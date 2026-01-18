#include "item_pickups.hpp"

#include "lib.hpp"
#include "lib/util/modules.hpp"
#include "lua_helper.hpp"

namespace odr::pickups {

	static void QueueAsyncPopup(const char* message);
	static void SoundCallbackHook(void* ptr1, void* ptr2, u32 param1, u32 param2);

	static bool disableItemPopups = false;
	static bool nextPopupIsAsync = false;
	static bool hookNextSoundCallback = false;
	static SoundCallbackFunction hookedCallback = NULL;

	/**
	 * Hooks the function that is called by CPickableItemComponent when the player picks up an item.
	 * This function will call the "show item popup" function that we hook below, and we need the
	 * next invocation of that to be handled by our hook.
	 */
	HOOK_DEFINE_TRAMPOLINE(OnCollectPickup) {
		static void Callback(void* pickup, double d1, double d2, float f1, float f2) {
			if (disableItemPopups) {
				nextPopupIsAsync = true;
			}

			Orig(pickup, d1, d2, f1, f2);
		}
	};

	/**
	 * Hooks the function that is called by CPickableItemComponent to play its pickup sound.
	 * We need to signal our hook for the "play sound" function to intercept the "sound completed"
	 * callback, where we will manually restore environment music.
	 */
	HOOK_DEFINE_TRAMPOLINE(PlayPickupSound) {
		static float Callback(void* pickup, u64 isKnownItem) {
			if (nextPopupIsAsync) {
				hookNextSoundCallback = true;
			}

			return Orig(pickup, isKnownItem);
		}
	};

	/**
	 * Hooks the function that shows the actual popup dialog when collecting an item.
	 * When our "OnCollectPickup" hook above signals it, this hook will redirect the call to instead
	 * queue an "async popup" similar to what is displayed when receiving items in a multiworld session.
	 * In this case, this hook must immediately call the "popup dismissed" callback, as that callback
	 * is responsible for some extra item pickup processing.
	 */
	HOOK_DEFINE_TRAMPOLINE(ShowItemPickupMessage) {
		static void Callback(void* popupComposition, const CStrId* pickupMessage, PopupCallbackInfo* callbackInfo, double d1, double d2, float f1, float f2, bool flag, u32 u1) {
			if (nextPopupIsAsync) {
				nextPopupIsAsync = false;

				// Show the pickup message at the top of the screen (like incoming multiworld items)
				if (pickupMessage != NULL) {
					const char* pickupMessageText = pickupMessage->value->string.str;

					QueueAsyncPopup(pickupMessageText);
				}

				// Make sure to call the "after picked up" callback!
				if (callbackInfo != NULL) {
					// Dread calls the callback with values of "0.0, 0.0, 1.0f, 1.0f" in the vanilla game
					callbackInfo->function(callbackInfo->receiver, 0.0, 0.0, 1.0f, 1.0f);
				}

				return;
			}

			Orig(popupComposition, pickupMessage, callbackInfo, d1, d2, f1, f2, flag, u1);
		}
	};

	/**
	 * Hooks the function responsible for playing certain sound effects and invoking a callback once the
	 * sound has finished playing. When the "PlayPickupSound" hook signals it, this hook will intercept the
	 * "sound completed" callback for a sound, and use it to queue the restoration of environmental music.
	 * This task is normally performed by the "popup dismissed" callback above, but because that callback
	 * is invoked immediately after the item is picked up in the case of our hooks, it tries to restore
	 * music while the pickup sound is still playing (which does nothing). By waiting for the item sound
	 * to finish playing, and then waiting an extra ~0.5 seconds, we can consistently restore music after
	 * the sound plays.
	 */
	HOOK_DEFINE_TRAMPOLINE(PlaySoundWithCallback) {
		static void Callback(void* musicManager, CRntString *soundName, u32 layer, u32 *flags, float param_5, float param_6, u32 param_7, u32 startTimeMs, SoundCallbackFunction callback) {
			SoundCallbackFunction callbackToUse = callback;

			if (hookNextSoundCallback) {
				// Wrap callback with our own, so we can be notified when the sound finishes playing
				hookNextSoundCallback = false;
				hookedCallback = callback;
				callbackToUse = SoundCallbackHook;
			}

			Orig(musicManager, soundName, layer, flags, param_5, param_6, param_7, startTimeMs, callbackToUse);
		}
	};

};

void odr::pickups::InstallHooks(const functionOffsets* offsets) {
	OnCollectPickup::InstallAtOffset(offsets->OnCollectPickup);
	PlayPickupSound::InstallAtOffset(offsets->PlayPickupSound);
    ShowItemPickupMessage::InstallAtOffset(offsets->ShowItemPickupMessage);
	PlaySoundWithCallback::InstallAtOffset(offsets->PlaySoundWithCallback);
}

int odr::pickups::SetItemPopupsEnabled(lua_State* L) {
	if (lua_gettop(L) < 1) {
		return luaL_error(L, "Expected a single boolean argument, but got none");
	}

	disableItemPopups = !lua_toboolean(L, 1);
	return 0;
}

void odr::pickups::QueueAsyncPopup(const char* message) {
	lua::CallFunctionWithArguments("Scenario.QueueAsyncPopup", "s", message);
}

void odr::pickups::SoundCallbackHook(void* ptr1, void* ptr2, u32 param1, u32 param2) {
	if (hookedCallback != NULL) {
		hookedCallback(ptr1, ptr2, param1, param2);
		hookedCallback = NULL;
	}

	// After half a second, call the "PlayCurrentEnvironmentMusic" function to restore music after an item pickup sound plays.
	// (for some reason, it doesn't work consistently if we just call it immediately in this hook)
	lua::CallFunctionWithArguments("Game.AddGUISF", "fss", 0.5f, "Game.PlayCurrentEnvironmentMusic", "");
}