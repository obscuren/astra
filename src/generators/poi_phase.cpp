#include "astra/poi_phase.h"
#include "astra/placement_scorer.h"
#include "astra/settlement_planner.h"
#include "astra/building_generator.h"
#include "astra/path_router.h"
#include "astra/perimeter_builder.h"
#include "astra/exterior_decorator.h"

namespace astra {

Rect poi_phase(TileMap& map, const TerrainChannels& channels,
               const MapProperties& props, std::mt19937& rng) {
    // Only run for settlement POIs
    if (!props.detail_has_poi || props.detail_poi_type != Tile::OW_Settlement)
        return {};

    // --- Determine footprint size from biome ---
    int foot_w = 100;
    int foot_h = 60;

    switch (props.biome) {
        case Biome::Volcanic:
        case Biome::ScarredScorched:
        case Biome::ScarredGlassed:
        case Biome::Ice:
            foot_w = 70;
            foot_h = 45;
            break;
        case Biome::Forest:
        case Biome::Jungle:
        case Biome::Grassland:
        case Biome::Marsh:
            if (props.lore_tier >= 2) {
                foot_w = 130;
                foot_h = 80;
            }
            break;
        default:
            break;
    }

    // --- Placement scoring ---
    PlacementScorer scorer;
    auto placement = scorer.score(channels, map, foot_w, foot_h);
    if (!placement.valid) return {};

    // --- Mutable copy of channels for terrain sculpting ---
    TerrainChannels mutable_channels = channels;

    // --- Plan the settlement ---
    SettlementPlanner planner;
    auto plan = planner.plan(placement, mutable_channels, map, props, rng);

    // --- Apply terrain modifications ---
    for (const auto& mod : plan.terrain_mods) {
        const auto& area = mod.area;
        for (int y = area.y; y < area.y + area.h; ++y) {
            for (int x = area.x; x < area.x + area.w; ++x) {
                if (x < 0 || x >= map.width() || y < 0 || y >= map.height())
                    continue;

                switch (mod.type) {
                    case TerrainModType::Level:
                        mutable_channels.elev(x, y) = mod.target_elevation;
                        if (map.get(x, y) == Tile::Wall)
                            map.set(x, y, Tile::Floor);
                        break;

                    case TerrainModType::RaiseBluff:
                        mutable_channels.elev(x, y) = mod.target_elevation;
                        // Edges become walls, interior becomes floor
                        if (x == area.x || x == area.x + area.w - 1 ||
                            y == area.y || y == area.y + area.h - 1) {
                            map.set(x, y, Tile::Wall);
                        } else {
                            map.set(x, y, Tile::Floor);
                        }
                        break;

                    case TerrainModType::CutBank:
                        mutable_channels.elev(x, y) = mod.target_elevation;
                        if (map.get(x, y) == Tile::Wall)
                            map.set(x, y, Tile::Floor);
                        break;

                    case TerrainModType::Clear:
                        mutable_channels.struc(x, y) = StructureMask::None;
                        if (map.get(x, y) == Tile::Wall)
                            map.set(x, y, Tile::Floor);
                        break;
                }
            }
        }
    }

    // --- Generate buildings ---
    BuildingGenerator builder;
    for (const auto& spec : plan.buildings) {
        builder.generate(map, spec, plan.style, rng);
    }

    // --- Route paths ---
    PathRouter router;
    router.route(map, plan);

    // --- Build perimeter ---
    PerimeterBuilder perimeter;
    perimeter.build(map, plan, rng);

    // --- Decorate exterior ---
    ExteriorDecorator decorator;
    decorator.decorate(map, plan, rng);

    return placement.footprint;
}

} // namespace astra
