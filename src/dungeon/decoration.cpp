#include "astra/dungeon/decoration.h"

#include "astra/dungeon_recipe.h"
#include "astra/ruin_decay.h"
#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace astra::dungeon {

namespace {

void decorate_ruin_debris(TileMap& map, const CivConfig& civ,
                          int decay_level, std::mt19937& rng) {
    // Map decay_level (0..3) to intensity (0.0..1.0).
    const float intensity = std::clamp(
        static_cast<float>(decay_level) / 3.0f, 0.0f, 1.0f);
    apply_decay(map, civ, intensity, rng);
}

// Precursor vault — tile-only decoration pack for the Archive dungeon.
// Paints rubble (wall→floor collapse) and wall runes (CF_RUIN_TINT) scaled
// by decay_level. Decorative fixtures (pillars, braziers) are handled by
// the prop layer; this pack adds no fixtures. Reuses the tile-painting
// subset of ruin_decay: wall-to-floor collapse + CF_RUIN_TINT on walls.
// Floor runes are intentionally skipped — no tile-level representation
// exists for them in this slice.
void decorate_precursor_vault(TileMap& map, const CivConfig& civ,
                              const DungeonLevelSpec& spec,
                              std::mt19937& rng) {
    (void)civ;
    const int decay = std::clamp(spec.decay_level, 0, 3);

    // Rubble (wall collapse) chance per wall tile, in percent.
    const int rubble_chance =
        (decay == 0) ? 0 :
        (decay == 1) ? 2 :
        (decay == 2) ? 4 : 12;

    // Wall runes per room (CF_RUIN_TINT marks on interior walls).
    const int wall_runes_per_room =
        (decay == 0) ? 1 :
        (decay == 1) ? 2 :
        (decay == 2) ? 2 : 3;

    std::uniform_int_distribution<int> d100(0, 99);

    // 1. Rubble — collapse walls to floor with probability scaled by decay.
    //    Mirrors ruin_decay.cpp stage 1 (wall → Floor), minus the fixture
    //    emission (precursor_vault is tiles-only).
    if (rubble_chance > 0) {
        for (int y = 1; y < map.height() - 1; ++y) {
            for (int x = 1; x < map.width() - 1; ++x) {
                Tile t = map.get(x, y);
                if (t != Tile::Wall && t != Tile::StructuralWall) continue;
                if (map.fixture_id(x, y) >= 0) continue;
                if (d100(rng) < rubble_chance) {
                    map.set(x, y, Tile::Floor);
                }
            }
        }
    }

    // 2. Wall runes — tag a handful of interior walls per room with
    //    CF_RUIN_TINT (the same wall-mark mechanism used by ruin_decay
    //    stage 4). The renderer colour-shifts tagged walls; for the
    //    Archive this reads as glyph-etched runes.
    static const int dxs[4] = {1, -1, 0, 0};
    static const int dys[4] = {0, 0, 1, -1};

    for (int rid = 0; rid < map.region_count(); ++rid) {
        if (map.region(rid).type != RegionType::Room) continue;

        std::vector<std::pair<int,int>> walls;
        for (int y = 1; y < map.height() - 1; ++y) {
            for (int x = 1; x < map.width() - 1; ++x) {
                if (map.passable(x, y)) continue;
                Tile t = map.get(x, y);
                if (t != Tile::Wall && t != Tile::StructuralWall) continue;
                bool touches = false;
                for (int k = 0; k < 4; ++k) {
                    int nx = x + dxs[k], ny = y + dys[k];
                    if (nx < 0 || ny < 0 ||
                        nx >= map.width() || ny >= map.height()) continue;
                    if (map.passable(nx, ny) && map.region_id(nx, ny) == rid) {
                        touches = true;
                        break;
                    }
                }
                if (touches) walls.emplace_back(x, y);
            }
        }

        std::shuffle(walls.begin(), walls.end(), rng);
        const int n = std::min(wall_runes_per_room,
                               static_cast<int>(walls.size()));
        for (int i = 0; i < n; ++i) {
            map.set_custom_flag(walls[i].first, walls[i].second,
                                CF_RUIN_TINT);
        }
    }
}

void decorate_natural_minimal(TileMap& map, const CivConfig& civ,
                              int decay_level, std::mt19937& rng) {
    (void)map; (void)civ; (void)decay_level; (void)rng;
    // Deferred: cave flora / mushroom stamps. Reserved for cave slice.
}

void decorate_station_scrap(TileMap&, const CivConfig&, int, std::mt19937&) {
    // Deferred: station slice.
}

void decorate_cave_flora(TileMap&, const CivConfig&, int, std::mt19937&) {
    // Deferred: cave slice.
}

} // namespace

void apply_decoration(TileMap& map, const DungeonStyle& style,
                      const CivConfig& civ, const DungeonLevelSpec& spec,
                      std::mt19937& rng) {
    const std::string& pack = style.decoration_pack;
    if      (pack == "ruin_debris")      decorate_ruin_debris(map, civ, spec.decay_level, rng);
    else if (pack == "precursor_vault")  decorate_precursor_vault(map, civ, spec, rng);
    else if (pack == "natural_minimal")  decorate_natural_minimal(map, civ, spec.decay_level, rng);
    else if (pack == "station_scrap")    decorate_station_scrap(map, civ, spec.decay_level, rng);
    else if (pack == "cave_flora")       decorate_cave_flora(map, civ, spec.decay_level, rng);
    // Unknown pack: silently skip.
}

} // namespace astra::dungeon
