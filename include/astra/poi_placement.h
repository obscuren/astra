#pragma once

#include "astra/cave_entrance_types.h"
#include "astra/crashed_ship_types.h"
#include "astra/poi_budget.h"
#include "astra/tilemap.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace astra {

struct MapProperties;

enum class AnchorDirection : uint8_t {
    None,
    North, South, East, West,
    NorthEast, NorthWest, SouthEast, SouthWest,
};

enum class AnchorReason : uint8_t {
    None,
    CliffAdjacent,
    WaterAdjacent,
    Flat,
    Open,
};

struct PoiAnchorHint {
    bool valid = false;
    AnchorReason reason = AnchorReason::None;
    AnchorDirection direction = AnchorDirection::None;
    CaveVariant cave_variant = CaveVariant::None;
    ShipClass ship_class = ShipClass::EscapePod;
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
};

enum class PoiPriority : uint8_t { Required, Normal, Opportunistic };

struct PoiTerrainRequirements {
    bool needs_cliff = false;
    bool needs_flat = false;
    bool needs_water_adjacent = false;
    int min_spacing = 8;
};

struct PoiRequest {
    Tile poi_tile = Tile::Empty;
    PoiTerrainRequirements reqs;
    PoiPriority priority = PoiPriority::Normal;
    // Variant payload — whichever fields match poi_tile are meaningful.
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
    bool ruin_hidden = false;
    ShipClass ship_class = ShipClass::EscapePod;
    CaveVariant cave_variant = CaveVariant::None;
};

struct HiddenPoi {
    int x = 0;
    int y = 0;
    Tile underlying_tile = Tile::OW_Plains;
    Tile real_tile = Tile::OW_Ruins;
    bool discovered = false;
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
};

// Run the placement pass against an overworld TileMap. Reads budget from the
// map (must be set ahead of time) and mutates the map: stamps POI tiles,
// writes anchor hints, and appends hidden POIs.
void run_poi_placement(TileMap& overworld, const MapProperties& props,
                       std::mt19937& rng);

// Build a PoiRequest vector from a budget. Visible units use the map's
// standard priority; lore-driven items may be flagged Required by the caller.
std::vector<PoiRequest> expand_budget_to_requests(const PoiBudget& budget,
                                                   const MapProperties& props,
                                                   std::mt19937& rng);

} // namespace astra
