#pragma once

#include "astra/tilemap.h"

namespace astra {

// Per-FixtureType OS identity for the device shell. Plan 7 §9 layer 2.
// Phase A ships entries for the few fixture types we can demo end-to-end:
// Door, Camera, Vending, generic Console, Lamp/Conduit power nodes, and a
// generic fallback. Phase B fills in the rest.
struct FixtureOsId {
    const char* os_name      = "DEV-OS";   // banner OS string e.g. "TURRET-OS"
    const char* version      = "v1.0";     // banner version string
    const char* prompt_user  = "root";     // unused at guest tier
    const char* prompt_host  = "device";   // host portion of prompt
    const char* prompt_glyph = "$";        // guest glyph; root uses '#'
};

const FixtureOsId& os_id_for(FixtureType f);

// Convenience: also exposed for NPC-implant turrets (Mobile + Weaponized).
const FixtureOsId& os_id_for_implant();

} // namespace astra
