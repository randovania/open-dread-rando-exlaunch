#include "sound.hpp"

#include "lib.hpp"
#include "game_functions.hpp"

namespace odr::sound {

	using game::Resolve;

	// The game's channel pause: void(soundManager, pause, &channelMask). Also used by Game.PauseSounds.
	typedef void (*PauseChannelsFn)(void* soundManager, u32 pause, u32* channelMask);

	// GOT slot holds the address of the global; dereference once more for the sound manager.
	static void*** soundManagerSlot = NULL;
	static PauseChannelsFn pauseChannels = NULL;

	static const luaL_Reg luaLibrary[] = {
		{"PauseChannel", PauseChannel},
		{NULL, NULL},
	};

};

void odr::sound::InstallFunctions(const functionOffsets* offsets) {
	if (offsets->SoundManagerSlot != 0 && offsets->PauseSoundChannels != 0) {
		soundManagerSlot = Resolve<void***>(offsets->SoundManagerSlot);
		pauseChannels = Resolve<PauseChannelsFn>(offsets->PauseSoundChannels);
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

	if (pauseChannels == NULL || soundManagerSlot == NULL || *soundManagerSlot == NULL || **soundManagerSlot == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}

	u32 mask = 1u << channel;
	pauseChannels(**soundManagerSlot, pause ? 1 : 0, &mask);

	lua_pushboolean(L, true);
	return 1;
}
