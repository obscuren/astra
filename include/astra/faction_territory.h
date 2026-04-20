#pragma once

#include "astra/faction_map.h"

#include <cstdint>
#include <string>

namespace astra {

struct NavigationData;
struct StarSystem;

// Assign every system a controlling_faction deterministically from the
// provided seed. Idempotent — calling twice overwrites. Safe to call after
// apply_lore_to_galaxy (and should be).
void assign_system_factions(NavigationData& nav, uint32_t seed);

inline bool is_unclaimed(const StarSystem& s);

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
