#include "spin_attack.hpp"

#include <cstring>

#include "lib.hpp"
#include "game_functions.hpp"

namespace odr::spin_attack {

	using game::Resolve;
	using game::crc64;
	using game::hashString;

	/** Returns the name of the current beam. CStrId*(gunComponent). */
	typedef CStrId* (*GetCurrentGunNameFn)(void* gunComponent);
	/** Plays an FX on an actor. void(actor, &fxName, unknown). */
	typedef void (*PlayFxFn)(void* actor, CStrId* fxName, u32 unknown);

	static GetCurrentGunNameFn getCurrentGunName = NULL;
	static PlayFxFn playFx = NULL;

	// Offset inside Samus's gun component
	static const ptrdiff_t GUN_COMPONENT_ACTOR = 0x30;

	enum VanillaBeam {
		BEAM_WIDE,
		BEAM_PLASMA,
		BEAM_WAVE,
		BEAM_COUNT,
		BEAM_NONE = BEAM_COUNT,
	};

	static const char* const VANILLA_NAMES[BEAM_COUNT] = { "WideBeam", "PlasmaBeam", "WaveBeam" };

	// GOT slots of the spin-jump FX name for each vanilla beam (PowerBeam shares the WideBeam one)
	static CStrId** fxSlots[BEAM_COUNT] = { NULL, NULL, NULL };

	// Vanilla beam names, created on first use and kept for the whole session
	static CStrId vanillaIds[BEAM_COUNT] = {};

	struct SplitBeam {
		const char* name;
		VanillaBeam vanilla;
		u64 hash;
	};

	// Gun names added by open-dread-rando's split_pickups.py, mapped to the vanilla beam whose effect they use
	static SplitBeam splitBeams[] = {
		{ "SoloWideBeam", BEAM_WIDE, 0 },
		{ "SoloPlasmaBeam", BEAM_PLASMA, 0 },
		{ "WidePlasmaBeam", BEAM_PLASMA, 0 },
		{ "SoloWaveBeam", BEAM_WAVE, 0 },
		{ "WideWaveBeam", BEAM_WAVE, 0 },
		{ "PlasmaWaveBeam", BEAM_WAVE, 0 },
		{ "AllBeams", BEAM_WAVE, 0 },
	};

	static VanillaBeam GetVanillaBeam(CStrId* gunName) {
		if (gunName == NULL || gunName->value == NULL) {
			return BEAM_NONE;
		}
		u64 hash = gunName->value->string.hash;
		// check if a SplitBeam is active
		for (const SplitBeam& beam : splitBeams) {
			if (beam.hash == hash) {
				return beam.vanilla;
			}
		}
		return BEAM_NONE;
	}

	/**
	 * Starts the charged spin-jump attack. void(gunComponent).
	 * The original turns the attack on and plays the effect for vanilla beams. For split beams, play it here.
	 */
	HOOK_DEFINE_TRAMPOLINE(StartSpinAttack) {
		static void Callback(void* gunComponent) {
			Orig(gunComponent);

			void* actor = *(void**)((char*) gunComponent + GUN_COMPONENT_ACTOR);
			if (actor == NULL) {
				return;
			}
			VanillaBeam beam = GetVanillaBeam(getCurrentGunName(gunComponent));
			if (beam != BEAM_NONE) {
				playFx(actor, *fxSlots[beam], 0);
			}
		}
	};

	/**
	 * Returns the current beam's name. CStrId*(gunComponent).
	 * Only the code that stops the spin-jump effect uses it, and it stops the effect by beam name.
	 * Return the vanilla name for split beams, so their effect gets stopped too.
	 */
	HOOK_DEFINE_TRAMPOLINE(GetSpinAttackGunName) {
		static CStrId* Callback(void* gunComponent) {
			CStrId* name = Orig(gunComponent);
			VanillaBeam beam = GetVanillaBeam(name);
			if (beam == BEAM_NONE) {
				return name;
			}
			if (vanillaIds[beam].value == NULL) {
				hashString(&vanillaIds[beam], VANILLA_NAMES[beam], true);
			}
			return &vanillaIds[beam];
		}
	};

};

void odr::spin_attack::InstallHooks(const functionOffsets* offsets) {
	if (offsets->StartSpinAttack == 0 || offsets->GetSpinAttackGunName == 0 || offsets->GetCurrentGunName == 0
		|| offsets->PlayFx == 0 || offsets->SpinAttackFxWideSlot == 0 || offsets->SpinAttackFxPlasmaSlot == 0
		|| offsets->SpinAttackFxWaveSlot == 0) {
		return;
	}

	getCurrentGunName = Resolve<GetCurrentGunNameFn>(offsets->GetCurrentGunName);
	playFx = Resolve<PlayFxFn>(offsets->PlayFx);
	fxSlots[BEAM_WIDE] = Resolve<CStrId**>(offsets->SpinAttackFxWideSlot);
	fxSlots[BEAM_PLASMA] = Resolve<CStrId**>(offsets->SpinAttackFxPlasmaSlot);
	fxSlots[BEAM_WAVE] = Resolve<CStrId**>(offsets->SpinAttackFxWaveSlot);

	for (SplitBeam& beam : splitBeams) {
		beam.hash = crc64(beam.name, strlen(beam.name));
	}

	StartSpinAttack::InstallAtOffset(offsets->StartSpinAttack);
	GetSpinAttackGunName::InstallAtOffset(offsets->GetSpinAttackGunName);
}
