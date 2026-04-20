#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

struct NavigationData;
struct StarSystem;

// ─── FactionMap ──────────────────────────────────────────────────
// Precomputed galaxy-space → faction-index grid. Used by the renderer
// to tint the background of every cell on the galaxy chart, including
// the empty space between stars.

constexpr int kFactionMapWidth  = 256;
constexpr int kFactionMapHeight = 256;

// Tight packing — one byte per cell, index into the faction palette.
enum class FactionTerritory : uint8_t {
    Unclaimed        = 0,
    StellariConclave = 1,
    TerranFederation = 2,
    KrethMiningGuild = 3,
    VeldraniAccord   = 4,
};

struct FactionMap {
    // Row-major, size = kFactionMapWidth * kFactionMapHeight.
    // Empty before assign_system_factions runs.
    std::vector<FactionTerritory> cells;

    // Galaxy-space bounds the grid covers (min/max gx, gy). Used to map
    // floating-point coords to cell indexes.
    float gx_min = 0.0f;
    float gx_max = 0.0f;
    float gy_min = 0.0f;
    float gy_max = 0.0f;

    bool empty() const { return cells.empty(); }
};

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
