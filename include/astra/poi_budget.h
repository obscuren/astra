#pragma once

#include "astra/cave_entrance_types.h"
#include "astra/crashed_ship_types.h"
#include "astra/tilemap.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

struct MapProperties; // forward decl

enum class RuinFormation : uint8_t {
    Solo,
    Connected,
};

struct RuinRequest {
    std::string civ;
    RuinFormation formation = RuinFormation::Solo;
    bool hidden = false;
};

struct ShipRequest {
    ShipClass klass = ShipClass::EscapePod;
};

struct PoiBudget {
    int settlements = 0;
    int outposts = 0;

    struct CaveCounts {
        int natural = 0;
        int mine = 0;
        int excavation = 0;
    } caves;

    std::vector<RuinRequest> ruins;
    std::vector<ShipRequest> ships;

    int beacons = 0;
    int megastructures = 0;

    int total_caves() const { return caves.natural + caves.mine + caves.excavation; }
    int visible_ruin_count() const;
    int hidden_ruin_count() const;
};

// Roll a budget from planet context. Deterministic given rng state.
PoiBudget roll_poi_budget(const MapProperties& props, std::mt19937& rng);

// Format as a multi-line human-readable summary.
std::string format_poi_budget(const PoiBudget& budget);

// Build a best-effort PoiBudget by scanning already-placed POI tiles on an
// overworld map. Used for legacy save reconstruction; variant data is unknown.
PoiBudget reconstruct_poi_budget_from_map(const TileMap& overworld);

} // namespace astra
