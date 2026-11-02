#include "sound.hpp"

#include "lib.hpp"
#include "game_functions.hpp"
#include "offsets.hpp"

namespace odr::sound {

	using game::Resolve;
	using game::GotSlot;

	struct Offsets {
		/** GOT slot that points to the global holding the sound manager. */
		ptrdiff_t SoundManagerSlot;
		/** Pauses or resumes whole sound channels by bit mask. void(soundManager, pause, &channelMask). */
		ptrdiff_t PauseSoundChannels;
	};

	static constexpr offsets::Versioned<Offsets> OFFSETS = {
		.v1_0_0 = {
			.SoundManagerSlot = 0x1c635f8,
			.PauseSoundChannels = 0x108244,
		},
		.v2_1_0 = {
			.SoundManagerSlot = 0x1cbafd0,
			.PauseSoundChannels = 0x108464,
		},
	};

	// The game's channel pause: void(soundManager, pause, &channelMask). Also used by Game.PauseSounds.
	using PauseChannelsFn = void (*)(void* soundManager, u32 pause, u32* channelMask);

	static GotSlot<void*> soundManager;
	static PauseChannelsFn pauseChannels = nullptr;

	static const luaL_Reg luaLibrary[] = {
		{"PauseChannel", PauseChannel},
		{nullptr, nullptr},
	};

};

void odr::sound::InstallFunctions() {
	const Offsets& offsets = OFFSETS.Get();
	if (offsets::HasOffsets(offsets)) {
		soundManager.Install(offsets.SoundManagerSlot);
		pauseChannels = Resolve<PauseChannelsFn>(offsets.PauseSoundChannels);
	}
}

void odr::sound::InstallLuaLibrary(lua_State* L) {
	luaL_register(L, "OdrSound", luaLibrary);
}

int odr::sound::PauseChannel(lua_State* L) {
	lua_Integer channel = luaL_checkinteger(L, 1);
	bool pause = lua_toboolean(L, 2);

	if (channel < 0 || channel >= (lua_Integer) 8) {
		return luaL_error(L, "Channel must be between 0 and %d, got %d", 7, (int) channel);
	}

	void* manager = soundManager.Get();
	if (pauseChannels == nullptr || manager == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	u32 mask = 1u << channel;
	pauseChannels(manager, pause ? 1 : 0, &mask);

	lua_pushboolean(L, true);
	return 1;
}
