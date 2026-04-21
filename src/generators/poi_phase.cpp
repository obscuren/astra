#include "astra/poi_phase.h"
#include "astra/placement_scorer.h"
#include "astra/ruin_generator.h"
#include "astra/settlement_planner.h"
#include "astra/outpost_planner.h"
#include "astra/crashed_ship_generator.h"
#include "astra/cave_entrance_generator.h"
#include "astra/building_generator.h"
#include "astra/path_router.h"
#include "astra/perimeter_builder.h"
#include "astra/exterior_decorator.h"

namespace astra {

Rect poi_phase(TileMap& map, const TerrainChannels& channels,
               const MapProperties& props, std::mt19937& rng) {
    if (!props.detail_has_poi)
        return {};

    if (props.detail_poi_type == Tile::OW_Ruins) {
        RuinGenerator ruin_gen;
        std::string civ = props.detail_ruin_civ;
        if (props.detail_poi_anchor.valid && !props.detail_poi_anchor.ruin_civ.empty())
            civ = props.detail_poi_anchor.ruin_civ;
        return ruin_gen.generate(map, channels, props, rng, civ);
    }

    if (props.detail_poi_type == Tile::OW_Outpost) {
        constexpr int kOutpostFootW = 44;
        constexpr int kOutpostFootH = 32;

        PlacementScorer scorer;
        auto placement = scorer.score(channels, map, kOutpostFootW, kOutpostFootH);
        if (!placement.valid) return {};

        TerrainChannels mutable_channels = channels;

        OutpostPlanner outpost_planner;
        auto plan = outpost_planner.plan(placement, mutable_channels, map, props, rng);

        // Apply terrain clears
        for (const auto& mod : plan.terrain_mods) {
            const auto& area = mod.area;
            for (int y = area.y; y < area.y + area.h; ++y) {
                for (int x = area.x; x < area.x + area.w; ++x) {
                    if (x < 0 || x >= map.width() || y < 0 || y >= map.height())
                        continue;
                    if (mod.type == TerrainModType::Clear) {
                        mutable_channels.struc(x, y) = StructureMask::None;
                        if (map.get(x, y) == Tile::Wall)
                            map.set(x, y, Tile::Floor);
                    }
                }
            }
        }

        BuildingGenerator builder;
        for (const auto& spec : plan.buildings) {
            builder.generate(map, spec, plan.style, rng);
        }

        PathRouter router;
        router.route(map, plan);

        PerimeterBuilder perimeter;
        perimeter.build(map, plan, rng);

        ExteriorDecorator decorator;
        decorator.decorate(map, plan, rng);

        // Post-stamp: tents, campfires, fence glyph overrides.
        outpost_planner.post_stamp(map, plan, props.biome, rng);

        return placement.footprint;
    }

    if (props.detail_poi_type == Tile::OW_CrashedShip) {
        CrashedShipGenerator ship_gen;
        return ship_gen.generate(map, channels, props, rng);
    }

    if (props.detail_poi_type == Tile::OW_CaveEntrance) {
        CaveEntranceGenerator cave_gen;
        return cave_gen.generate(map, channels, props, rng);
    }

    if (props.detail_poi_type == Tile::OW_PrecursorArchive) {
        // Generate a full Precursor ruin, then drop a quest-flagged
        // DungeonHatch in a back chamber. The player spawns at the map
        // center on detail-map entry (game_world.cpp:enter_detail_map),
        // so placing the hatch at the room whose centroid is furthest
        // from center means the player has to traverse the ruin and
        // fight through the Conclave Sentry patrols to reach it.
        RuinGenerator ruin_gen;
        std::string civ = props.detail_ruin_civ;
        if (props.detail_poi_anchor.valid && !props.detail_poi_anchor.ruin_civ.empty())
            civ = props.detail_poi_anchor.ruin_civ;
        Rect footprint = ruin_gen.generate(map, channels, props, rng, civ);

        const int cx = map.width() / 2;
        const int cy = map.height() / 2;

        // Compute per-region centroids from the region_ids grid.
        const int rc = map.region_count();
        std::vector<long long> sum_x(rc, 0), sum_y(rc, 0);
        std::vector<int> count(rc, 0);
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                int rid = map.region_id(x, y);
                if (rid < 0 || rid >= rc) continue;
                sum_x[rid] += x;
                sum_y[rid] += y;
                count[rid] += 1;
            }
        }

        // Pick the Room region whose centroid is furthest from map center.
        int best_rid = -1;
        int best_d2 = -1;
        for (int r = 0; r < rc; ++r) {
            if (map.region(r).type != RegionType::Room) continue;
            if (count[r] <= 0) continue;
            int rcx = static_cast<int>(sum_x[r] / count[r]);
            int rcy = static_cast<int>(sum_y[r] / count[r]);
            int dx = rcx - cx, dy = rcy - cy;
            int d2 = dx * dx + dy * dy;
            if (d2 > best_d2) { best_d2 = d2; best_rid = r; }
        }

        int hx = cx, hy = cy;
        if (best_rid >= 0) {
            int rx = 0, ry = 0;
            std::mt19937 pick_rng(static_cast<uint32_t>(best_rid) * 2654435761u);
            if (map.find_open_spot_in_region(best_rid, rx, ry, {}, &pick_rng)) {
                hx = rx; hy = ry;
            }
        }

        // Clear the target tile so the hatch sits on passable floor.
        map.set(hx, hy, Tile::IndoorFloor);
        if (map.fixture_id(hx, hy) >= 0) map.remove_fixture(hx, hy);

        FixtureData hatch = make_fixture(FixtureType::DungeonHatch);
        hatch.interactable = true;
        hatch.quest_fixture_id = "conclave_archive_entrance";
        map.add_fixture(hx, hy, hatch);

        return footprint;
    }

    // Stubbed POI types — generate terrain only, implementation pending
    if (props.detail_poi_type == Tile::OW_Beacon ||
        props.detail_poi_type == Tile::OW_Megastructure) {
        return {};
    }

    if (props.detail_poi_type != Tile::OW_Settlement)
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
