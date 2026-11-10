#pragma once

#include <utility>

#include "lib.hpp"
#include "lib/nx/kernel/svc.h"
#include "lua-5.1.5/src/lua.hpp"

namespace odr::debug {
	void InstallHooks();
	void InstallLuaLibrary(lua_State* L);
	int SetLoggingEnabled(lua_State* L);
};
