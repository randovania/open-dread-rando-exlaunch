#pragma once

#include "lib.hpp"
#include "dread_types.hpp"

namespace odr::spin_attack {

	/**
	 * Restores the charged spin-jump visual for the randomizer's split beams.
	 *
	 * The game picks the spin-jump effect by the current beam's name and only knows
	 * PowerBeam, WideBeam, PlasmaBeam and WaveBeam. The split beams (SoloWideBeam, AllBeams, ...)
	 * have names the vanilla game doesn't know, so it shows no effect for them.
	 */
	void InstallHooks(const functionOffsets* offsets);

};
