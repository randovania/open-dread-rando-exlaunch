#pragma once

#include <stddef.h>

/**
 * Holds offset pointers for Dread functions that we either hook or call manually
 */
typedef struct {
    /** Calculates the CRC64 hash of a string */
    ptrdiff_t crc64;

	/** Function that creates a CStrId.  */
	ptrdiff_t GetCStrId;

	/** Function that releases a CStringInstance and frees it if it is not referenced. */
	ptrdiff_t DiscardStringInstance;

	/** Singleton that manages CStrId */
	ptrdiff_t StringPoolPtr;

	/** Singleton that stores all reflection data */
	ptrdiff_t ReflectionManagerPtr;

	/** Function that gets a CClass from its hashed name */
	ptrdiff_t GetCClassPtr;

    /** Constructor for `CFilePathStrId` class used when the game loads files */
    ptrdiff_t CFilePathStrIdCtor;

    /** Function that registers Lua libraries (e.g. the `Game` library, etc.). Hooked to register our own. */
    ptrdiff_t luaRegisterGlobals;

    /** Dread's custom lua_pcall function, used to call raw Lua code in protected mode */
    ptrdiff_t lua_pcall;

    /** The lua_CFunction for `Game.LogWarn`, which is stubbed out in vanilla Dread. */
	ptrdiff_t LogWarn;

	/** Used by the game engine to call a Lua function (by its qualified name) from C++ using varargs arguments. */
	ptrdiff_t CallFunctionWithArguments;

	// Pickups
	/** Called when the player first collides with a Pickup item. */
    ptrdiff_t OnCollectPickup;

	/** Called by the CPickableItemComponent class to play the appropriate sound for collecting a pickup. */
	ptrdiff_t PlayPickupSound;

	/** Called by the CPickableItemComponent class to show a pop-up dialog with a message for a newly-collected pickup. */
    ptrdiff_t ShowItemPickupMessage;

	// Audio
	/** Plays a sound effect with specific flags and a callback function that is invoked when the sound finishes playing. */
	ptrdiff_t PlaySoundWithCallback;

	/** A large array of common CStrId in alpha order */
	ptrdiff_t StaticStringBank;

	/** Function that generates all CClasses. Can be hooked to access class data. */
	ptrdiff_t GenerateReflection;

	/** Function that registers a variable to a CClass. Usually called in fields() function. */
	ptrdiff_t RegisterVariable;
} functionOffsets;

typedef unsigned long long crc64_t;

// TODO implement CClass
typedef ptrdiff_t CClass;

// TODO implement ReflectionManager
typedef ptrdiff_t ReflectionManager;

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

// TODO implement fully; this is just to provide access to nullStr for CStrId-related purposes.
struct SStringPool {
	void* vtable;
	ptrdiff_t unk1;
	CStringInstance *nullStr;
};