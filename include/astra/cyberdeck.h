#pragma once

#include "astra/program_compiler.h"   // CompiledProgram

#include <array>
#include <cstdint>
#include <optional>

namespace astra {

// Per-deck stats.
struct CyberdeckStats {
    int  ram_max       = 8;
    int  cpu           = 1;
    int  slots         = 3;       // number of program slots
    int  stealth       = 0;       // additive bonus to Trace reduction (Plan 3)
    int  cooling_rate  = 1;       // heat decay per turn (Plan 3)
    int  heat_cap      = 10;      // max heat (Plan 3)
};

// Fixed-size slot array. CyberdeckData is held inline on Item via
// std::optional, so we keep the size deterministic for serialization
// and avoid an extra heap allocation per equipped deck.
// Slot count from CyberdeckStats::slots gates how many of these are live.
inline constexpr int kCyberdeckMaxSlots = 6;

// A loaded program slot. Either:
//   - holds a legacy program reference (program_def_id != 0, compiled empty) —
//     used by hardcoded loot drops which can be rebuilt via build_by_def_id, or
//   - holds a compiled program payload directly (compiled.has_value()) — used
//     for player-compiled programs (def_id = 0) since those have no def_id.
// `slot_is_empty(s)` is the canonical "is this slot free?" check.
struct CyberdeckSlot {
    uint16_t program_def_id = 0;
    std::optional<CompiledProgram> compiled;
};

inline bool slot_is_empty(const CyberdeckSlot& s) {
    return s.program_def_id == 0 && !s.compiled.has_value();
}

struct CyberdeckData {
    CyberdeckStats stats;
    int  ram_current = 0;          // current RAM available (regenerates between sessions)
    int  heat_current = 0;         // Plan 3
    // Loaded programs; program_def_id == 0 means empty. Index < stats.slots is live.
    std::array<CyberdeckSlot, kCyberdeckMaxSlots> loaded;
};

// Tier presets.
CyberdeckStats cyberdeck_stats_tier1();
CyberdeckStats cyberdeck_stats_tier2();

// Heat helpers (Plan 3). Mutate CyberdeckData::heat_current.
void cyberdeck_add_heat(CyberdeckData& cd, int amount);
bool cyberdeck_decay_heat(CyberdeckData& cd);                 // -= cooling_rate, clamp 0; returns true if fully cooled
bool cyberdeck_overheated(const CyberdeckData& cd);           // heat > heat_cap
void cyberdeck_force_reboot(CyberdeckData& cd);               // ram_current = 0; heat_current = 0

} // namespace astra
