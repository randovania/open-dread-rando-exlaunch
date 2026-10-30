#include "map.hpp"

#include "lib.hpp"
#include "game_functions.hpp"

namespace odr::map {

	using game::Resolve;
	using game::hashString;

	// Minimap manager
	typedef void* (*GetMapManagerFn)();
	typedef void (*AddDoneMapTutoFn)(void* mapManager, u32 type);

	static GetMapManagerFn getMapManager = NULL;
	static AddDoneMapTutoFn addDoneMapTuto = NULL;

	// Map icon table (saved as TGlobalMapIcons). Two levels:
	// - the table itself: one entry per area (at most 9), grown by GrowMapIconTable
	// - inside each area entry: a list of that area's icons, e.g. its teleporters
	typedef void (*GrowMapIconTableFn)(void* table, u32 newCapacity);
	static GrowMapIconTableFn growMapIconTable = NULL;

	// Offsets inside the map manager
	static const ptrdiff_t MAPMGR_ICON_TABLE = 0xd642a0;
	static const ptrdiff_t ICON_TABLE_CAPACITY = 0x14;
	// Minimum table capacity: one entry per area, and there are 9 areas. One more for safety.
	static const u32 ICON_TABLE_MIN_CAPACITY = 10;

	// Blackboard
	struct STypedValue {
		void* type;
		void* value;
	};

	/** A reflected Blackboard property descriptor. Only the fields used here */
	struct SPropDescriptor {
		void* vtable;
		void* next;
		CStrId key;      // full key "[prefix:]GROUP:name", built on first use
		u8 unk18[0x10];
		CStrId name;
		void* type;      // reflection type of the value
	};

	typedef void* (*GetOrCreateSectionFn)(void* blackboard, const CStrId* section);
	typedef bool (*SectionGetVector3DFn)(void* section, const CStrId* key, float* out);
	typedef void (*SectionSetPropFn)(void* section, const CStrId* key, STypedValue* value);
	typedef void (*BuildPropKeyFn)(CStrId* outKey, SPropDescriptor* prop, const CStrId* prefix);

	// GOT slots hold the address of a global; dereference once more for its value.
	static void*** gameSlot = NULL;
	static char*** stringPoolSlot = NULL;
	static CStrId** keyPrefixSlot = NULL;
	static SPropDescriptor** propLastPlayerPosSlot = NULL;

	static GetOrCreateSectionFn getOrCreateSection = NULL;
	static SectionGetVector3DFn sectionGetVector3D = NULL;
	static SectionSetPropFn sectionSetProp = NULL;
	static BuildPropKeyFn buildPropKey = NULL;

	// Offsets inside the game object (same in 1.0.0 and 2.1.0).
	static const ptrdiff_t GAME_BLACKBOARD = 0x968;
	static const ptrdiff_t GAME_BLACKBOARD_LOCKED = 0xa50;

	static const luaL_Reg luaLibrary[] = {
		{"MarkMapTutoDone", MarkMapTutoDone},
		{"MarkAreaVisited", MarkAreaVisited},
		{NULL, NULL},
	};

	static void* GetMapManagerOrNull() {
		return getMapManager != NULL ? getMapManager() : NULL;
	}

	/**
	 * Fixes a crash when using a teleporter with global teleport unlocked.
	 *
	 * The game goes through the table area by area and adds each used teleporter's partner to the
	 * partner's area. Example: table full with [Itorash, Hanubia, Ferenia]; at Ferenia it adds Burenia
	 * (never visited) -> table moves to new memory -> the loop keeps reading the freed old memory -> crash.
	 * The base game never hits this, since all teleporter areas are visited before global teleport unlocks.
	 *
	 * Fix: make room first, so adding an area never moves the table.
	 */
	HOOK_DEFINE_TRAMPOLINE(SyncTeleporterPairs) {
		static void Callback() {
			char* mapManager = (char*) GetMapManagerOrNull();
			if (mapManager != NULL) {
				char* table = mapManager + MAPMGR_ICON_TABLE;
				u32 capacity = *(u32*)(table + ICON_TABLE_CAPACITY);
				if (capacity < ICON_TABLE_MIN_CAPACITY) {
					growMapIconTable(table, ICON_TABLE_MIN_CAPACITY);
				}
			}
			Orig();
		}
	};

	/** Returns the descriptor's full key, building it the same way the game does if it is still empty. */
	static const CStrId* GetPropKey(SPropDescriptor* prop) {
		void* emptyString = **stringPoolSlot + 0x10;
		if ((void*) prop->key.value == emptyString) {
			buildPropKey(&prop->key, prop, *keyPrefixSlot);
		}
		return &prop->key;
	}

};

void odr::map::InstallFunctions(const functionOffsets* offsets) {
	if (offsets->GetMapManager != 0) {
		getMapManager = Resolve<GetMapManagerFn>(offsets->GetMapManager);
	}
	if (offsets->AddDoneMapTuto != 0) {
		addDoneMapTuto = Resolve<AddDoneMapTutoFn>(offsets->AddDoneMapTuto);
	}
	if (getMapManager != NULL && offsets->SyncTeleporterPairs != 0 && offsets->GrowMapIconTable != 0) {
		growMapIconTable = Resolve<GrowMapIconTableFn>(offsets->GrowMapIconTable);
		SyncTeleporterPairs::InstallAtOffset(offsets->SyncTeleporterPairs);
	}

	bool blackboardKnown = offsets->GameGlobalSlot != 0 && offsets->StringPoolSlot != 0
		&& offsets->BlackboardKeyPrefixSlot != 0 && offsets->PropLastPlayerPosSlot != 0
		&& offsets->HashString != 0 && offsets->BlackboardGetOrCreateSection != 0
		&& offsets->BlackboardSectionGetVector3D != 0 && offsets->BlackboardSectionSetProp != 0
		&& offsets->BlackboardBuildPropKey != 0;
	if (blackboardKnown) {
		gameSlot = Resolve<void***>(offsets->GameGlobalSlot);
		stringPoolSlot = Resolve<char***>(offsets->StringPoolSlot);
		keyPrefixSlot = Resolve<CStrId**>(offsets->BlackboardKeyPrefixSlot);
		propLastPlayerPosSlot = Resolve<SPropDescriptor**>(offsets->PropLastPlayerPosSlot);
		getOrCreateSection = Resolve<GetOrCreateSectionFn>(offsets->BlackboardGetOrCreateSection);
		sectionGetVector3D = Resolve<SectionGetVector3DFn>(offsets->BlackboardSectionGetVector3D);
		sectionSetProp = Resolve<SectionSetPropFn>(offsets->BlackboardSectionSetProp);
		buildPropKey = Resolve<BuildPropKeyFn>(offsets->BlackboardBuildPropKey);
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

int odr::map::MarkAreaVisited(lua_State* L) {
	if (!lua_isstring(L, 1)) {
		lua_pushboolean(L, false);
		return 1;
	}
	const char* scenario = lua_tostring(L, 1);
	bool hasPos = lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4);
	if (!hasPos && !lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}
	float pos[4] = {0, 0, 0, 0};
	if (hasPos) {
		pos[0] = (float) lua_tonumber(L, 2);
		pos[1] = (float) lua_tonumber(L, 3);
		pos[2] = (float) lua_tonumber(L, 4);
	}

	if (gameSlot == NULL || *gameSlot == NULL || **gameSlot == NULL || *propLastPlayerPosSlot == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
	char* game = (char*) **gameSlot;

	// The game skips Blackboard writes while this flag is set
	if (*(bool*)(game + GAME_BLACKBOARD_LOCKED)) {
		lua_pushboolean(L, false);
		return 1;
	}

	CStrId sectionId = {NULL};
	hashString(&sectionId, scenario, true);
	void* section = getOrCreateSection(game + GAME_BLACKBOARD, &sectionId);
	if (section == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}

	// The world map only lets you select areas that have MINIMAP:LastPlayerPos.
	SPropDescriptor* posProp = *propLastPlayerPosSlot;
	const CStrId* posKey = GetPropKey(posProp);
	float existingPos[4] = {0, 0, 0, 0};
	if (hasPos || !sectionGetVector3D(section, posKey, existingPos)) {
		STypedValue posValue = {posProp->type, pos};
		sectionSetProp(section, posKey, &posValue);
	}

	lua_pushboolean(L, true);
	return 1;
}
