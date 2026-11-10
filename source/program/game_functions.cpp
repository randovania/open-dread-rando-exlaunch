#include "game_functions.hpp"

#include "offsets.hpp"

namespace odr::game {

	struct Offsets {
		/** Calculates the CRC64 hash of a string. */
		ptrdiff_t crc64;
		/** Builds a CStrId from a C string. */
		ptrdiff_t HashString;
	};

	static constexpr offsets::Versioned<Offsets> OFFSETS = {
		.v1_0_0 = {
			.crc64 = 0x1570,
			.HashString = 0x3d4,
		},
		.v2_1_0 = {
			.crc64 = 0x1570,
			.HashString = 0x3d4,
		},
	};

	Crc64Fn crc64 = nullptr;
	HashStringFn hashString = nullptr;

};

void odr::game::InstallFunctions() {
	const Offsets& offsets = OFFSETS.Get();
	crc64 = Resolve<Crc64Fn>(offsets.crc64);
	hashString = Resolve<HashStringFn>(offsets.HashString);
}
