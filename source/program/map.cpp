#include "map.hpp"

#include "lib.hpp"
#include "dread_types.hpp"
#include "game_functions.hpp"
#include "offsets.hpp"

namespace odr::map {

	using game::Resolve;
	using game::GotSlot;
	using game::hashString;

	struct MapManagerOffsets {
		/** Returns the game's minimap manager. */
		ptrdiff_t GetMapManager;
		/** Marks an EMapTutoType as shown and saves it to Blackboard MINIMAP:DoneMapTutos. */
		ptrdiff_t AddDoneMapTuto;
	};

	struct TeleporterOffsets {
		/** Global teleport: adds each used teleporter's partner to the map manager's icon table. void(). */
		ptrdiff_t SyncTeleporterPairs;
		/** Grows a map icon table to the given capacity. void(table, newCapacity). */
		ptrdiff_t GrowMapIconTable;
	};

	struct BlackboardOffsets {
		/** GOT slot that points to the global holding the game object pointer. */
		ptrdiff_t GameGlobalSlot;
		/** Returns the Blackboard section with the given name, creating it if needed. */
		ptrdiff_t GetOrCreateSection;
		/** Reads a CVector3D property from a section; returns whether it exists. */
		ptrdiff_t SectionGetVector3D;
		/** Writes a typed property into a section. */
		ptrdiff_t SectionSetProp;
		/** Builds a property descriptor's full key ("[prefix:]GROUP:name") on first use. */
		ptrdiff_t BuildPropKey;
		/** GOT slot of the global key prefix passed to BuildPropKey. */
		ptrdiff_t KeyPrefixSlot;
		/** GOT slot of the string pool. */
		ptrdiff_t StringPoolSlot;
		/** GOT slot of the MINIMAP:LastPlayerPos property descriptor. */
		ptrdiff_t PropLastPlayerPosSlot;
	};

	static constexpr offsets::Versioned<MapManagerOffsets> MAP_MANAGER_OFFSETS = {
		.v1_0_0 = {
			.GetMapManager = 0xe4f410,
			.AddDoneMapTuto = 0xe67dc8,
		},
		.v2_1_0 = {
			.GetMapManager = 0xe8d8b0,
			.AddDoneMapTuto = 0xea6314,
		},
	};

	static constexpr offsets::Versioned<TeleporterOffsets> TELEPORTER_OFFSETS = {
		.v1_0_0 = {
			.SyncTeleporterPairs = 0xb3b7e8,
			.GrowMapIconTable = 0xb41e70,
		},
		.v2_1_0 = {
			.SyncTeleporterPairs = 0xb422a8,
			.GrowMapIconTable = 0xb48944,
		},
	};

	static constexpr offsets::Versioned<BlackboardOffsets> BLACKBOARD_OFFSETS = {
		.v1_0_0 = {
			.GameGlobalSlot = 0x1c626b8,
			.GetOrCreateSection = 0x10057a0,
			.SectionGetVector3D = 0x3954bc,
			.SectionSetProp = 0x332544,
			.BuildPropKey = 0xae6c10,
			.KeyPrefixSlot = 0x1c626c0,
			.StringPoolSlot = 0x1c62698,
			.PropLastPlayerPosSlot = 0x1cb22b0,
		},
		.v2_1_0 = {
			.GameGlobalSlot = 0x1cba088,
			.GetOrCreateSection = 0x10472e0,
			.SectionGetVector3D = 0x395d3c,
			.SectionSetProp = 0x332c80,
			.BuildPropKey = 0xae9a10,
			.KeyPrefixSlot = 0x1cba090,
			.StringPoolSlot = 0x1cba068,
			.PropLastPlayerPosSlot = 0x1d09dc8,
		},
	};

	/**
	 * Map icon table (saved as TGlobalMapIcons). Two levels:
	 * - the table itself: one entry per area (at most 9), grown by GrowMapIconTable
	 * - inside each area entry: a list of that area's icons, e.g. its teleporters
	 */
	struct CMapIconTable {
		u8 unk0[0x14];
		u32 capacity;
	};
	static_assert(offsetof(CMapIconTable, capacity) == 0x14);

	/** The game's minimap manager. Only used through pointers into game memory. */
	struct CMapManager {
		u8 unk0[0xd642a0];
		CMapIconTable iconTable;
	};
	static_assert(offsetof(CMapManager, iconTable) == 0xd642a0);

	// Minimap manager
	using GetMapManagerFn = CMapManager* (*)();
	using AddDoneMapTutoFn = void (*)(CMapManager* mapManager, u32 type);

	static GetMapManagerFn getMapManager = nullptr;
	static AddDoneMapTutoFn addDoneMapTuto = nullptr;

	using GrowMapIconTableFn = void (*)(CMapIconTable* table, u32 newCapacity);
	static GrowMapIconTableFn growMapIconTable = nullptr;

	// Minimum icon table capacity: one entry per area, and there are 9 areas. One more for safety.
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
	static_assert(offsetof(SPropDescriptor, key) == 0x10);
	static_assert(offsetof(SPropDescriptor, name) == 0x28);
	static_assert(offsetof(SPropDescriptor, type) == 0x30);

	using GetOrCreateSectionFn = void* (*)(void* blackboard, const CStrId* section);
	using SectionGetVector3DFn = bool (*)(void* section, const CStrId* key, float* out);
	using SectionSetPropFn = void (*)(void* section, const CStrId* key, STypedValue* value);
	using BuildPropKeyFn = void (*)(CStrId* outKey, SPropDescriptor* prop, const CStrId* prefix);

	static GotSlot<CGame*> gameGlobal;
	static GotSlot<CStringPool*> stringPool;
	static GotSlot<CStrId> keyPrefix;
	static GotSlot<SPropDescriptor> propLastPlayerPos;

	static GetOrCreateSectionFn getOrCreateSection = nullptr;
	static SectionGetVector3DFn sectionGetVector3D = nullptr;
	static SectionSetPropFn sectionSetProp = nullptr;
	static BuildPropKeyFn buildPropKey = nullptr;

	static const luaL_Reg luaLibrary[] = {
		{"MarkMapTutoDone", MarkMapTutoDone},
		{"MarkAreaVisited", MarkAreaVisited},
		{nullptr, nullptr},
	};

	static CMapManager* GetMapManagerOrNull() {
		return getMapManager != nullptr ? getMapManager() : nullptr;
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
			CMapManager* mapManager = GetMapManagerOrNull();
			if (mapManager != nullptr) {
				CMapIconTable* table = &mapManager->iconTable;
				if (table->capacity < ICON_TABLE_MIN_CAPACITY) {
					growMapIconTable(table, ICON_TABLE_MIN_CAPACITY);
				}
			}
			Orig();
		}
	};

	/** Returns the descriptor's full key, building it the same way the game does if it is still empty. */
	static const CStrId* GetPropKey(SPropDescriptor* prop) {
		CStringPool* pool = stringPool.Get();
		if (pool != nullptr && prop->key.value == &pool->emptyString) {
			buildPropKey(&prop->key, prop, keyPrefix.Address());
		}
		return &prop->key;
	}

};

void odr::map::InstallFunctions() {
	const MapManagerOffsets& mapManagerOffsets = MAP_MANAGER_OFFSETS.Get();
	if (offsets::HasOffsets(mapManagerOffsets)) {
		getMapManager = Resolve<GetMapManagerFn>(mapManagerOffsets.GetMapManager);
		addDoneMapTuto = Resolve<AddDoneMapTutoFn>(mapManagerOffsets.AddDoneMapTuto);
	}

	const TeleporterOffsets& teleporterOffsets = TELEPORTER_OFFSETS.Get();
	if (getMapManager != nullptr && offsets::HasOffsets(teleporterOffsets)) {
		growMapIconTable = Resolve<GrowMapIconTableFn>(teleporterOffsets.GrowMapIconTable);
		SyncTeleporterPairs::InstallAtOffset(teleporterOffsets.SyncTeleporterPairs);
	}

	const BlackboardOffsets& blackboardOffsets = BLACKBOARD_OFFSETS.Get();
	if (hashString != nullptr && offsets::HasOffsets(blackboardOffsets)) {
		gameGlobal.Install(blackboardOffsets.GameGlobalSlot);
		stringPool.Install(blackboardOffsets.StringPoolSlot);
		keyPrefix.Install(blackboardOffsets.KeyPrefixSlot);
		propLastPlayerPos.Install(blackboardOffsets.PropLastPlayerPosSlot);
		getOrCreateSection = Resolve<GetOrCreateSectionFn>(blackboardOffsets.GetOrCreateSection);
		sectionGetVector3D = Resolve<SectionGetVector3DFn>(blackboardOffsets.SectionGetVector3D);
		sectionSetProp = Resolve<SectionSetPropFn>(blackboardOffsets.SectionSetProp);
		buildPropKey = Resolve<BuildPropKeyFn>(blackboardOffsets.BuildPropKey);
	}
}

void odr::map::InstallLuaLibrary(lua_State* L) {
	luaL_register(L, "OdrMap", luaLibrary);
}

int odr::map::MarkMapTutoDone(lua_State* L) {
	u32 type = (u32) luaL_checkinteger(L, 1);

	CMapManager* mapManager = GetMapManagerOrNull();
	if (mapManager == nullptr || addDoneMapTuto == nullptr) {
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

	CGame* game = gameGlobal.Get();
	SPropDescriptor* posProp = propLastPlayerPos.Address();
	if (game == nullptr || posProp == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	if (game->blackboardLocked) {
		lua_pushboolean(L, false);
		return 1;
	}

	CStrId sectionId = {nullptr};
	hashString(&sectionId, scenario, true);
	void* section = getOrCreateSection(&game->blackboard, &sectionId);
	if (section == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	// The world map only lets you select areas that have MINIMAP:LastPlayerPos.
	const CStrId* posKey = GetPropKey(posProp);
	float existingPos[4] = {0, 0, 0, 0};
	if (hasPos || !sectionGetVector3D(section, posKey, existingPos)) {
		STypedValue posValue = {posProp->type, pos};
		sectionSetProp(section, posKey, &posValue);
	}

	lua_pushboolean(L, true);
	return 1;
}
