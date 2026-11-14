#pragma once

#include "lib.hpp"

namespace odr::demo_blocks {
	void InstallFunctions();

	struct CDemolitionBlockLifeComponent {
		u8 unk0[0x240];
		uint eDamageSources;
	};
	static_assert(offsetof(CDemolitionBlockLifeComponent, eDamageSources) == 0x240);

	struct SHit {
		u8 unk0[0xb4];
		uint eDamageSources;
	};
	static_assert(offsetof(SHit, eDamageSources) == 0xb4);
};
