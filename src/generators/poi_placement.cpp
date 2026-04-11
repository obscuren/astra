#include "astra/poi_placement.h"

#include "astra/celestial_body.h"
#include "astra/map_properties.h"

#include <algorithm>

namespace astra {

namespace {

// Settlement/outpost spacing (same as legacy).
constexpr int kDefaultSpacing = 8;
// Tighter spacing for ships, ruins, caves near their own kind.
constexpr int kCloseSpacing = 6;

PoiTerrainRequirements reqs_for_settlement() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kDefaultSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_outpost() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kDefaultSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_cave(CaveVariant v) {
    PoiTerrainRequirements r;
    r.min_spacing = kCloseSpacing;
    if (v == CaveVariant::NaturalCave || v == CaveVariant::AbandonedMine)
        r.needs_cliff = true;
    // Excavation: no strict cliff requirement.
    return r;
}

PoiTerrainRequirements reqs_for_ship() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kCloseSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_ruin() {
    PoiTerrainRequirements r;
    r.min_spacing = kCloseSpacing;
    return r;
}

} // namespace

std::vector<PoiRequest> expand_budget_to_requests(const PoiBudget& budget,
                                                   const MapProperties& props,
                                                   std::mt19937& /*rng*/) {
    std::vector<PoiRequest> out;

    // Settlements
    for (int i = 0; i < budget.settlements; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Settlement;
        r.reqs = reqs_for_settlement();
        r.priority = (i == 0 && props.body_type == BodyType::Terrestrial)
                         ? PoiPriority::Required
                         : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Outposts
    for (int i = 0; i < budget.outposts; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Outpost;
        r.reqs = reqs_for_outpost();
        r.priority = PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Caves — natural
    for (int i = 0; i < budget.caves.natural; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::NaturalCave;
        r.reqs = reqs_for_cave(CaveVariant::NaturalCave);
        r.priority = PoiPriority::Normal;
        out.push_back(std::move(r));
    }
    // Caves — mine
    for (int i = 0; i < budget.caves.mine; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::AbandonedMine;
        r.reqs = reqs_for_cave(CaveVariant::AbandonedMine);
        r.priority = (props.lore_tier >= 2) ? PoiPriority::Required
                                             : PoiPriority::Normal;
        out.push_back(std::move(r));
    }
    // Caves — excavation
    for (int i = 0; i < budget.caves.excavation; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::AncientExcavation;
        r.reqs = reqs_for_cave(CaveVariant::AncientExcavation);
        r.priority = (props.lore_tier >= 3) ? PoiPriority::Required
                                             : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Ruins
    int half_hidden = std::max(0, budget.hidden_ruin_count() / 2);
    int hidden_placed = 0;
    for (const auto& ruin : budget.ruins) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Ruins;
        r.reqs = reqs_for_ruin();
        r.ruin_civ = ruin.civ;
        r.ruin_formation = ruin.formation;
        r.ruin_hidden = ruin.hidden;
        if (ruin.hidden && props.lore_tier >= 3 && hidden_placed < half_hidden) {
            r.priority = PoiPriority::Required;
            ++hidden_placed;
        } else {
            r.priority = PoiPriority::Normal;
        }
        out.push_back(std::move(r));
    }

    // Ships
    for (size_t i = 0; i < budget.ships.size(); ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CrashedShip;
        r.ship_class = budget.ships[i].klass;
        r.reqs = reqs_for_ship();
        r.priority = (i == 0 && props.lore_battle_site) ? PoiPriority::Required
                                                         : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    return out;
}

void run_poi_placement(TileMap& /*overworld*/, const MapProperties& /*props*/,
                       std::mt19937& /*rng*/) {
    // Filled in by Task 7.
}

} // namespace astra
