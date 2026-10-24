#pragma once

#include "lib.hpp"
#include "lua-5.1.5/src/lua.hpp"
#include "dread_types.hpp"

namespace odr::map {

	/** Values of the game's EMapTutoType enum. */
	enum EMapTutoType : u32 {
		HINT_ZONE = 0,
		EMMY_ZONE = 1,
		TELEPORTER_NET = 2,
	};

	void InstallFunctions(const functionOffsets* offsets);
	void InstallLuaLibrary(lua_State* L);

	/** OdrMap.MarkMapTutoDone(type) -> bool. Marks a map tutorial (EMapTutoType) as already shown. */
	int MarkMapTutoDone(lua_State* L);

	/**
	 * OdrMap.MarkAreaVisited(scenario[, x, y, z]) -> bool. Saves MINIMAP:LastPlayerPos for the area,
	 * which makes it selectable on the world map.
	 * With a position it is always written, otherwise (0,0,0) is written only if the proeprty is missing.
	 */
	int MarkAreaVisited(lua_State* L);

};
