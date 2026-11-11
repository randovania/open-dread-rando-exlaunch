#include "demo_blocks.hpp"

#include "offsets.hpp"

namespace odr::demo_blocks {
    struct Offsets {
        /** Function called when a CDemolitionBlockLifeComponent is damaged */
        ptrdiff_t OnHit;
    };

    static constexpr offsets::Versioned<Offsets> OFFSETS = {
        .v1_0_0 = { .OnHit = 0x8c45fc },
        .v2_1_0 = { .OnHit = 0x8c6bcc },
    };

    /** Bombs damage CDemolitionBlockLifeComponent through a separate path from missiles/beams. This path does
        not seem to check life comp's damage sources against the bomb's damage flags and relies on the
        collision layer, which may create a race condition against an active PB. Adding this guard
        seems to resolve the inconsistency. */
    HOOK_DEFINE_TRAMPOLINE(CDemolitionBlockLifeComponent_OnHit) {
        static int Callback(uint* oLIFE, uint* hit) {
            if ((oLIFE[144] & hit[45]) == 0) { // oLife->eDamageSources at 0x240, hit->damageFlags at 0xb4
                return 0;
            }

            return Orig(oLIFE, hit);
        }
    };

    void InstallFunctions() {
        CDemolitionBlockLifeComponent_OnHit::InstallAtOffset(OFFSETS.Get().OnHit);
    }
}