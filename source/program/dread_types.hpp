#pragma once

#include <stddef.h>

/**
 * Holds offset pointers for Dread functions that we either hook or call manually
 */
typedef struct {
    /** Calculates the CRC64 hash of a string */
    ptrdiff_t crc64;

    /** Constructor for `CFilePathStrId` class used when the game loads files */
    ptrdiff_t CFilePathStrIdCtor;

    /** Function that registers Lua libraries (e.g. the `Game` library, etc.). Hooked to register our own. */
    ptrdiff_t luaRegisterGlobals;

    /** Dread's custom lua_pcall function, used to call raw Lua code in protected mode */
    ptrdiff_t lua_pcall;

    /** The lua_CFunction for `Game.LogWarn`, which is stubbed out in vanilla Dread. */
	ptrdiff_t LogWarn;
	ptrdiff_t CallFunctionWithArguments;

	// Pickups
    ptrdiff_t OnCollectPickup;
	ptrdiff_t PlayPickupSound;
    ptrdiff_t ShowItemPickupMessage;

	// Audio
	ptrdiff_t PlaySoundWithCallback;
} functionOffsets;

typedef unsigned long long crc64_t;

struct CRntString {
	char* str;
	int length;
	void* allocator;
	bool usesMainAllocator;
	crc64_t hash;
	bool isEmpty;
};

struct CStringInstance {
	void* stringPoolEntry;
	unsigned int uses;
	CRntString string;
	bool storeInPool;
	unsigned int unknown;
};

struct CStrId {
	CStringInstance *value;
};
