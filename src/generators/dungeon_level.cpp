#include "astra/dungeon_level_generator.h"

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"
#include "astra/dungeon/pipeline.h"
#include "astra/map_properties.h"
#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <utility>

namespace astra {

// ---------------------------------------------------------------------------
// Top-level free functions — signatures and behavior unchanged.
// ---------------------------------------------------------------------------

uint32_t dungeon_level_seed(uint32_t world_seed, const LocationKey& k) {
    uint32_t s = world_seed;
    s ^= static_cast<uint32_t>(std::get<0>(k)) * 73856093u;
    s ^= static_cast<uint32_t>(std::get<1>(k) + 1) * 19349663u;
    s ^= static_cast<uint32_t>(std::get<2>(k) + 1) * 83492791u;
    s ^= static_cast<uint32_t>(std::get<4>(k) + 1) * 2654435761u;
    s ^= static_cast<uint32_t>(std::get<5>(k) + 1) * 40503u;
    s ^= static_cast<uint32_t>(std::get<6>(k)) * 6271u;   // depth
    return s;
}

std::pair<int, int> find_stairs_up(const TileMap& m) {
    const auto& ids = m.fixture_ids();
    const int w = m.width();
    const int h = m.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int fid = ids[y * w + x];
            if (fid < 0) continue;
            if (m.fixture(fid).type == FixtureType::StairsUp) {
                return {x, y};
            }
        }
    }
    return {-1, -1};
}

std::pair<int, int> find_stairs_down(const TileMap& m) {
    const auto& ids = m.fixture_ids();
    const int w = m.width();
    const int h = m.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int fid = ids[y * w + x];
            if (fid < 0) continue;
            if (m.fixture(fid).type == FixtureType::StairsDown) {
                return {x, y};
            }
        }
    }
    return {-1, -1};
}

void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed,
                            std::pair<int, int> entered_from) {
    if (depth < 1 || depth > static_cast<int>(recipe.levels.size())) return;

    const auto& spec = recipe.levels[depth - 1];

    const MapType dtype = MapType::DerelictStation;
    MapProperties props = default_properties(dtype);
    props.biome = Biome::Dungeon;
    props.difficulty = std::max(1, spec.enemy_tier);

    map = TileMap(props.width, props.height, dtype);

    dungeon::LevelContext ctx;
    ctx.depth        = depth;
    ctx.seed         = seed;
    ctx.entered_from = entered_from;

    const dungeon::DungeonStyle& style = dungeon::style_config(spec.style_id);
    const CivConfig& civ               = civ_config_by_name(spec.civ_name);

    dungeon::run(map, style, civ, spec, ctx);
}

} // namespace astra
