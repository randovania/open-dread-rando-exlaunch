#pragma once

#include "lib.hpp"
#include "lib/util/modules.hpp"
#include "dread_types.hpp"

/** Game functions that several modules use. Resolved once at startup. */
namespace odr::game {

	/** Returns the address of a function or global at the given offset into the game's main module. */
	template<typename T>
	inline T Resolve(ptrdiff_t offset) {
		return (T) exl::util::modules::GetTargetOffset(offset);
	}

	/** Calculates the CRC64 hash of a string. */
	typedef u64 (*Crc64Fn)(const char* str, u64 size);
	/** Builds a CStrId from a C string. */
	typedef void (*HashStringFn)(CStrId* out, const char* str, bool addToPool);

	extern Crc64Fn crc64;
	extern HashStringFn hashString;

	/** Must run before the other modules install. */
	void InstallFunctions(const functionOffsets* offsets);

};
