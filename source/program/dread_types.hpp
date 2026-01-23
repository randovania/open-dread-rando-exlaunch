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
} functionOffsets;
