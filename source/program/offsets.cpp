#include "offsets.hpp"

#include <cstring>
#include "lib.hpp"
#include <nn.hpp>

namespace odr::offsets {

	static bool versionDetected = false;
	static GameVersion gameVersion = GameVersion::V1_0_0;

	GameVersion GetGameVersion() {
		if (!versionDetected) {
			nn::oe::DisplayVersion dispVer;
			nn::oe::GetDisplayVersion(&dispVer);
			gameVersion = strcmp(dispVer.displayVersion, "2.1.0") == 0 ? GameVersion::V2_1_0 : GameVersion::V1_0_0;
			versionDetected = true;
		}
		return gameVersion;
	}

};
