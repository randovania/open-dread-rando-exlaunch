#include "spin_attack.hpp"

#include <cstring>

#include "lib.hpp"
#include "dread_types.hpp"
#include "game_functions.hpp"
#include "offsets.hpp"

namespace odr::spin_attack {

	using game::Resolve;
	using game::GotSlot;
	using game::crc64;
	using game::hashString;

	struct Offsets {
		/** Starts the spin-jump attack and plays its effect for the current beam. void(gunComponent). */
		ptrdiff_t StartSpinAttack;
		/** Returns the current beam's name. Only used to stop the spin-jump effect. CStrId*(gunComponent). */
		ptrdiff_t GetSpinAttackGunName;
		/** Returns the current beam's name. CStrId*(gunComponent). */
		ptrdiff_t GetCurrentGunName;
		/** Plays an FX on an actor. void(actor, &fxName, unknown). */
		ptrdiff_t PlayFx;
		/** GOT slots of the spin-jump FX names for PowerBeam/WideBeam, PlasmaBeam and WaveBeam. */
		ptrdiff_t SpinAttackFxWideSlot;
		ptrdiff_t SpinAttackFxPlasmaSlot;
		ptrdiff_t SpinAttackFxWaveSlot;
	};

	static constexpr offsets::Versioned<Offsets> OFFSETS = {
		.v1_0_0 = {
			.StartSpinAttack = 0x857b9c,
			.GetSpinAttackGunName = 0x83e748,
			.GetCurrentGunName = 0x83e71c,
			.PlayFx = 0xd69d28,
			.SpinAttackFxWideSlot = 0x1c9f410,
			.SpinAttackFxPlasmaSlot = 0x1c9f408,
			.SpinAttackFxWaveSlot = 0x1c9f418,
		},
		.v2_1_0 = {
			.StartSpinAttack = 0x859d0c,
			.GetSpinAttackGunName = 0x840828,
			.GetCurrentGunName = 0x8407fc,
			.PlayFx = 0xd7c828,
			.SpinAttackFxWideSlot = 0x1cf6e40,
			.SpinAttackFxPlasmaSlot = 0x1cf6e38,
			.SpinAttackFxWaveSlot = 0x1cf6e48,
		},
	};

	/** Samus's gun component. Only used through pointers into game memory. */
	struct CGunComponent {
		u8 unk0[0x30];
		/** The actor that owns the component. */
		void* actor;
	};
	static_assert(offsetof(CGunComponent, actor) == 0x30);

	/** Returns the name of the current beam. CStrId*(gunComponent). */
	using GetCurrentGunNameFn = CStrId* (*)(CGunComponent* gunComponent);
	/** Plays an FX on an actor. void(actor, &fxName, unknown). */
	using PlayFxFn = void (*)(void* actor, CStrId* fxName, u32 unknown);

	static GetCurrentGunNameFn getCurrentGunName = nullptr;
	static PlayFxFn playFx = nullptr;

	enum VanillaBeam {
		BEAM_WIDE,
		BEAM_PLASMA,
		BEAM_WAVE,
		BEAM_COUNT,
		BEAM_NONE = BEAM_COUNT,
	};

	static const char* const VANILLA_NAMES[BEAM_COUNT] = { "WideBeam", "PlasmaBeam", "WaveBeam" };

	// Spin-jump FX name globals for each vanilla beam (PowerBeam shares the WideBeam one)
	static GotSlot<CStrId> fxNames[BEAM_COUNT];

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
		if (gunName == nullptr || gunName->value == nullptr) {
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
		static void Callback(CGunComponent* gunComponent) {
			Orig(gunComponent);

			void* actor = gunComponent->actor;
			if (actor == nullptr) {
				return;
			}
			VanillaBeam beam = GetVanillaBeam(getCurrentGunName(gunComponent));
			if (beam != BEAM_NONE) {
				playFx(actor, fxNames[beam].Address(), 0);
			}
		}
	};

	/**
	 * Returns the current beam's name. CStrId*(gunComponent).
	 * Only the code that stops the spin-jump effect uses it, and it stops the effect by beam name.
	 * Return the vanilla name for split beams, so their effect gets stopped too.
	 */
	HOOK_DEFINE_TRAMPOLINE(GetSpinAttackGunName) {
		static CStrId* Callback(CGunComponent* gunComponent) {
			CStrId* name = Orig(gunComponent);
			VanillaBeam beam = GetVanillaBeam(name);
			if (beam == BEAM_NONE) {
				return name;
			}
			if (vanillaIds[beam].value == nullptr) {
				hashString(&vanillaIds[beam], VANILLA_NAMES[beam], true);
			}
			return &vanillaIds[beam];
		}
	};

};

void odr::spin_attack::InstallHooks() {
	const Offsets& offsets = OFFSETS.Get();
	if (!offsets::HasOffsets(offsets)) {
		return;
	}

	getCurrentGunName = Resolve<GetCurrentGunNameFn>(offsets.GetCurrentGunName);
	playFx = Resolve<PlayFxFn>(offsets.PlayFx);
	fxNames[BEAM_WIDE].Install(offsets.SpinAttackFxWideSlot);
	fxNames[BEAM_PLASMA].Install(offsets.SpinAttackFxPlasmaSlot);
	fxNames[BEAM_WAVE].Install(offsets.SpinAttackFxWaveSlot);

	for (SplitBeam& beam : splitBeams) {
		beam.hash = crc64(beam.name, strlen(beam.name));
	}

	StartSpinAttack::InstallAtOffset(offsets.StartSpinAttack);
	GetSpinAttackGunName::InstallAtOffset(offsets.GetSpinAttackGunName);
}
