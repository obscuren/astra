#pragma once

#include "astra/faction_map.h"

#include <cstdint>
#include <string>

namespace astra {

struct NavigationData;
struct StarSystem;

// ─── Generation ──────────────────────────────────────────────────

// Assign every system a controlling_faction deterministically from the
// provided seed, then populate nav.faction_map for renderer lookups.
// Idempotent — calling twice overwrites. Safe to call after
// apply_lore_to_galaxy (and should be).
void assign_system_factions(NavigationData& nav, uint32_t seed);

// ─── Queries ─────────────────────────────────────────────────────

inline bool is_unclaimed(const StarSystem& s);

// Galaxy-space coord → faction label. Returns "" for Unclaimed or out-of-bounds.
// Cheap O(1) lookup against the precomputed FactionMap.
std::string faction_at_coord(const NavigationData& nav, float gx, float gy);

// Enum ↔ string helpers.
FactionTerritory faction_enum_from_name(const std::string& faction);
const char* faction_name_from_enum(FactionTerritory t);

} // namespace astra

// Inline definition must see the full StarSystem definition.
#include "astra/star_chart.h"

namespace astra {
inline bool is_unclaimed(const StarSystem& s) {
    return s.controlling_faction.empty();
}
} // namespace astra
