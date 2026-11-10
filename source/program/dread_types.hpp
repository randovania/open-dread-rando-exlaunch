#pragma once

#include <cstddef>

#include "common.hpp"

using crc64_t = unsigned long long;

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

/** The game's string pool. */
struct CStringPool {
	u8 unk0[0x10];
	/** The shared instance that every empty CStrId points to. */
	CStringInstance emptyString;
};
static_assert(offsetof(CStringPool, emptyString) == 0x10);

/** The game object. */
struct CGame {
	u8 unk0[0x968];
	/** Start of the CBlackboard. Its layout is unknown, only its address is used. */
	u8 blackboard[0xa50 - 0x968];
	/** The game skips Blackboard writes while this is set. */
	bool blackboardLocked;
};
static_assert(offsetof(CGame, blackboard) == 0x968);
static_assert(offsetof(CGame, blackboardLocked) == 0xa50);
