#include "game_functions.hpp"

namespace odr::game {

	Crc64Fn crc64 = NULL;
	HashStringFn hashString = NULL;

};

void odr::game::InstallFunctions(const functionOffsets* offsets) {
	crc64 = Resolve<Crc64Fn>(offsets->crc64);
	hashString = Resolve<HashStringFn>(offsets->HashString);
}
