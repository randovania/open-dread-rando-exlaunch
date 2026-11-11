#pragma once

#include "lib.hpp"

namespace odr::suit_change {

	/**
	 * Prevents a crash when an actor's model is changed at runtime, e.g. Samus's suit via MODELUPDATER.sModelAlias.
	 *
	 * Material effects (e.g. the damage flash in lava) keep a list of the model data they draw on.
	 * Changing the model rebuilds it without updating those lists, so the renderer reads the old model data
	 * and crashes. The hook takes the actor's material effects off the old model and puts them back afterwards.
	 */
	void InstallHooks();

};
