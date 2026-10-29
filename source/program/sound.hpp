#pragma once

#include "lib.hpp"
#include "lua-5.1.5/src/lua.hpp"
#include "dread_types.hpp"

namespace odr::sound {

	void InstallFunctions(const functionOffsets* offsets);
	void InstallLuaLibrary(lua_State* L);

	/**
	 * OdrSound.PauseChannel(channel, pause) -> bool. Pauses or resumes a whole sound channel (0-7).
	 * Returns false if the sound manager isn't ready.
	 */
	int PauseChannel(lua_State* L);

};
