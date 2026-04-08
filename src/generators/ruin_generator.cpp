#include "astra/ruin_generator.h"
#include "astra/bsp_generator.h"
#include "astra/room_identifier.h"

#include "astra/ruin_decay.h"
#include "astra/noise.h"

#include <algorithm>
#include <functional>

namespace astra {
namespace {

// ---------------------------------------------------------------------------
// Helper: select civilization configuration
// ---------------------------------------------------------------------------
CivConfig select_civ(const MapProperties& props, std::mt19937& rng,
                      const std::string& civ_name) {
    if (!civ_name.empty()) {
        return civ_config_by_name(civ_name);
    }
    if (props.lore_primary_civ_index >= 0) {
        return civ_config_for_architecture(props.lore_alien_architecture);
    }
    // Random pick from the four civ configs
    std::uniform_int_distribution<int> dist(0, 3);
    switch (dist(rng)) {
        case 0:  return civ_config_monolithic();
        case 1:  return civ_config_baroque();
        case 2:  return civ_config_crystal();
        default: return civ_config_industrial();
    }
}

// ---------------------------------------------------------------------------
// Helper: select post-processing stamps based on map properties
// ---------------------------------------------------------------------------
std::vector<RuinStampConfig> select_stamps(const MapProperties& props,
                                           std::mt19937& rng) {
    std::vector<RuinStampConfig> stamps;

    // Battle sites always get the battle-scarred stamp
    if (props.lore_battle_site) {
        std::uniform_real_distribution<float> intensity(0.5f, 0.8f);
        stamps.push_back({RuinStampType::BattleScarred, intensity(rng)});
    }

    // Non-extreme biomes may be infested (30% chance)
    if (props.biome != Biome::Ice && props.biome != Biome::Volcanic) {
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(rng) < 0.3f) {
            std::uniform_real_distribution<float> intensity(0.3f, 0.7f);
            stamps.push_back({RuinStampType::Infested, intensity(rng)});
        }
    }

    // Wet biomes may be flooded (25% chance)
    if (props.biome == Biome::Marsh || props.biome == Biome::Jungle ||
        props.biome == Biome::Forest) {
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(rng) < 0.25f) {
            std::uniform_real_distribution<float> intensity(0.3f, 0.6f);
            stamps.push_back({RuinStampType::Flooded, intensity(rng)});
        }
    }

    return stamps;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// generate — full ruin pipeline
// ---------------------------------------------------------------------------
Rect RuinGenerator::generate(TileMap& map, const TerrainChannels& channels,
                             const MapProperties& props, std::mt19937& rng,
                             const std::string& civ_name) const {
    // 1. Select civilization
    CivConfig civ = select_civ(props, rng, civ_name);

    // 2. Footprint = full map. Ruins are megastructures that extend edge to edge.
    PlacementResult result;
    result.footprint = {0, 0, map.width(), map.height()};
    result.valid = true;

    // 4. Build the ruin plan
    RuinPlan plan;
    plan.footprint = result.footprint;
    plan.civ = std::move(civ);
    plan.stamps = select_stamps(props, rng);
    plan.base_decay = 0.2f;

    // Edge connectivity — check if neighboring tiles are also ruins
    plan.edge_n = (props.detail_neighbor_n == Tile::OW_Ruins);
    plan.edge_s = (props.detail_neighbor_s == Tile::OW_Ruins);
    plan.edge_e = (props.detail_neighbor_e == Tile::OW_Ruins);
    plan.edge_w = (props.detail_neighbor_w == Tile::OW_Ruins);

    // 5. BSP wall network
    BspGenerator bsp;
    bsp.generate(map, plan, rng);

    // 6. Room identification
    RoomIdentifier identifier;
    identifier.identify(map, plan, rng);

    // 7. Furnish each room
    for (const auto& room : plan.rooms) {
        place_room_furniture(map, room, plan.civ, rng);
    }

    // 8. Decay with gradient + sectoral
    DecayContext decay_ctx;
    decay_ctx.age_decay = plan.base_decay;
    decay_ctx.use_gradient = true;
    decay_ctx.gradient_footprint = plan.footprint;
    decay_ctx.use_sectoral = true;
    decay_ctx.sectoral_variance = 0.3f;

    RuinDecay decay;
    decay.apply(map, plan.footprint, decay_ctx, props.biome, rng);

    // 9. Post-processing stamps
    apply_ruin_stamps(map, plan, props.biome, rng);

    // 10. Edge continuity for multi-tile ruins
    apply_edge_continuity(map, plan, props);

    // 11. Return the footprint
    return plan.footprint;
}

// ---------------------------------------------------------------------------
// place_room_furniture — populate a room with thematic fixtures
// ---------------------------------------------------------------------------
void RuinGenerator::place_room_furniture(TileMap& map, const RuinRoom& room,
                                         const CivConfig& civ,
                                         std::mt19937& rng) const {
    // Build a ruin-flavored CivStyle for furniture palette lookup
    CivStyle ruin_style;
    ruin_style.name = civ.name;
    ruin_style.wall_tile = Tile::Wall;
    ruin_style.floor_tile = Tile::IndoorFloor;

    // Most fixture roles map to Debris in ruins
    ruin_style.seating   = FixtureType::Debris;
    ruin_style.cooking   = FixtureType::Debris;
    ruin_style.knowledge = FixtureType::Debris;
    ruin_style.display   = FixtureType::Debris;

    // Storage and lighting keep their identity
    ruin_style.storage  = FixtureType::Crate;
    ruin_style.lighting = FixtureType::Torch;

    ruin_style.decay = 0.5f;

    // Get the palette for this room's theme
    auto palette = furniture_palette(room.theme, ruin_style);

    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    for (const auto& group : palette.groups) {
        if (chance(rng) >= group.frequency) continue;

        // Determine how many fixtures to place
        int count = group.min_count;
        if (group.max_count > group.min_count) {
            std::uniform_int_distribution<int> cnt(group.min_count,
                                                   group.max_count);
            count = cnt(rng);
        }

        // Place each fixture at a random floor tile
        std::uniform_int_distribution<int> tile_pick(
            0, static_cast<int>(room.floor_tiles.size()) - 1);

        for (int i = 0; i < count; ++i) {
            bool placed = false;
            for (int attempt = 0; attempt < 10 && !placed; ++attempt) {
                auto [fx, fy] = room.floor_tiles[tile_pick(rng)];
                if (map.get(fx, fy) == Tile::IndoorFloor &&
                    map.fixture_id(fx, fy) < 0) {
                    map.add_fixture(fx, fy, make_fixture(group.primary));
                    placed = true;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// apply_edge_continuity — generate matching wall stubs at shared edges
// ---------------------------------------------------------------------------
void RuinGenerator::apply_edge_continuity(TileMap& map, const RuinPlan& plan,
                                          const MapProperties& props) const {
    auto shared_seed = [](int a, int b) -> uint32_t {
        int lo = std::min(a, b);
        int hi = std::max(a, b);
        // Simple hash combining both coordinates
        return static_cast<uint32_t>(
            std::hash<int>()(lo) ^ (std::hash<int>()(hi) << 16));
    };

    auto place_stubs = [&](int edge_coord, bool horizontal, bool inward_positive,
                           uint32_t seed) {
        std::mt19937 edge_rng(seed);
        std::uniform_int_distribution<int> stub_count(3, 6);
        int n_stubs = stub_count(edge_rng);

        int span = horizontal ? plan.footprint.w : plan.footprint.h;
        int origin = horizontal ? plan.footprint.x : plan.footprint.y;

        std::uniform_int_distribution<int> pos_dist(origin, origin + span - 1);
        std::uniform_int_distribution<int> len_dist(3, 8);
        std::uniform_int_distribution<int> thick_dist(2, 4);

        for (int s = 0; s < n_stubs; ++s) {
            int start_pos = pos_dist(edge_rng);
            int length = len_dist(edge_rng);
            int thickness = thick_dist(edge_rng);

            for (int t = 0; t < thickness; ++t) {
                for (int l = 0; l < length; ++l) {
                    int x, y;
                    if (horizontal) {
                        x = start_pos + l;
                        y = inward_positive ? (edge_coord + t)
                                            : (edge_coord - t);
                    } else {
                        y = start_pos + l;
                        x = inward_positive ? (edge_coord + t)
                                            : (edge_coord - t);
                    }

                    if (x < 0 || x >= map.width() ||
                        y < 0 || y >= map.height()) continue;

                    // Skip water tiles
                    if (map.get(x, y) == Tile::Water) continue;

                    map.set(x, y, Tile::Wall);
                    map.set_custom_flag(x, y, CF_RUIN_TINT);
                }
            }
        }
    };

    // North edge: stubs grow inward from y=0
    if (plan.edge_n) {
        uint32_t seed = shared_seed(
            props.overworld_x * 1000 + props.overworld_y,
            props.overworld_x * 1000 + (props.overworld_y - 1));
        place_stubs(0, true, true, seed);
    }

    // South edge: stubs grow inward from y=height-1
    if (plan.edge_s) {
        uint32_t seed = shared_seed(
            props.overworld_x * 1000 + props.overworld_y,
            props.overworld_x * 1000 + (props.overworld_y + 1));
        place_stubs(map.height() - 1, true, false, seed);
    }

    // West edge: stubs grow inward from x=0
    if (plan.edge_w) {
        uint32_t seed = shared_seed(
            props.overworld_x * 1000 + props.overworld_y,
            (props.overworld_x - 1) * 1000 + props.overworld_y);
        place_stubs(0, false, true, seed);
    }

    // East edge: stubs grow inward from x=width-1
    if (plan.edge_e) {
        uint32_t seed = shared_seed(
            props.overworld_x * 1000 + props.overworld_y,
            (props.overworld_x + 1) * 1000 + props.overworld_y);
        place_stubs(map.width() - 1, false, false, seed);
    }
}

} // namespace astra
