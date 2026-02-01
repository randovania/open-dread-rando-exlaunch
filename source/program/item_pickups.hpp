#pragma once

#include "lib.hpp"
#include "lua-5.1.5/src/lua.hpp"
#include "dread_types.hpp"

namespace odr::pickups {

	typedef void (*PopupCallbackFunction)(void* receiver, double d1, double d2, float f1, float f2);
	typedef void (*SoundCallbackFunction)(void* ptr1, void* ptr2, u32 param1, u32 param2);

	struct PopupCallbackInfo {
		void* receiver;
		PopupCallbackFunction function;
	};

	void InstallHooks(const functionOffsets *offsets);
	void InstallLuaLibrary(lua_State* L);
	int SetItemPopupsEnabled(lua_State* L);

};