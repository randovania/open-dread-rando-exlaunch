#include "debug_hooks.hpp"

namespace odr::debug {

	static const luaL_Reg luaLibrary[] = {
		{"SetLoggingEnabled", odr::debug::SetLoggingEnabled},
		{NULL, NULL},
	};

	static bool loggingEnabled = false;

	HOOK_DEFINE_REPLACE(LogWarn) {
		static int Callback(lua_State* L) {
			if (!loggingEnabled) {
				return 0;
			}

			int nargs = lua_gettop(L);

			if (nargs < 2) {
				return luaL_error(L, "not enough arguments (expected 2, got %d)", nargs);
			}

			lua_Integer arg1 = lua_tointeger(L, 1);
			const char* msg = lua_tostring(L, 2);
			static char buffer[256];

			int msgLen = snprintf(buffer, sizeof(buffer), "[LogWarn/%d] %s", (int) arg1, msg);

			svcOutputDebugString(buffer, msgLen);
			return 0;
		}
	};

};

void odr::debug::InstallHooks(functionOffsets* offsets)  {
	LogWarn::InstallAtOffset(offsets->LogWarn);
}

void odr::debug::InstallLuaLibrary(lua_State* L) {
	luaL_register(L, "OdrDebug", luaLibrary);
}

int odr::debug::SetLoggingEnabled(lua_State *L) {
	if (lua_gettop(L) < 1) {
		return luaL_error(L, "Expected a single boolean argument, but got none");
	}

	loggingEnabled = lua_toboolean(L, 1);
	return 0;
}