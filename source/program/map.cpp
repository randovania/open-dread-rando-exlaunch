#include "map.hpp"

#include "lib.hpp"
#include "lib/util/modules.hpp"

namespace odr::map {

	// Minimap manager
	typedef void* (*GetMapManagerFn)();
	typedef void (*AddDoneMapTutoFn)(void* mapManager, u32 type);

	static GetMapManagerFn getMapManager = NULL;
	static AddDoneMapTutoFn addDoneMapTuto = NULL;

	static const luaL_Reg luaLibrary[] = {
		{"MarkMapTutoDone", MarkMapTutoDone},
		{NULL, NULL},
	};

	template<typename T>
	static T Resolve(ptrdiff_t offset) {
		return (T) exl::util::modules::GetTargetOffset(offset);
	}

	static void* GetMapManagerOrNull() {
		return getMapManager != NULL ? getMapManager() : NULL;
	}

};

void odr::map::InstallFunctions(const functionOffsets* offsets) {
	if (offsets->GetMapManager != 0) {
		getMapManager = Resolve<GetMapManagerFn>(offsets->GetMapManager);
	}
	if (offsets->AddDoneMapTuto != 0) {
		addDoneMapTuto = Resolve<AddDoneMapTutoFn>(offsets->AddDoneMapTuto);
	}
}

void odr::map::InstallLuaLibrary(lua_State* L) {
	luaL_register(L, "OdrMap", luaLibrary);
}

int odr::map::MarkMapTutoDone(lua_State* L) {
	u32 type = (u32) luaL_checkinteger(L, 1);

	void* mapManager = GetMapManagerOrNull();
	if (mapManager == NULL || addDoneMapTuto == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}

	// Adds the type to the manager's done list and saves it to Blackboard MINIMAP:DoneMapTutos.
	addDoneMapTuto(mapManager, type);
	lua_pushboolean(L, true);
	return 1;
}
