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

// Returns true if placing an impassable fixture at (x, y) would block a
// narrow passage. Mirror of the fixtures-layer helper: a "safe" tile has
// >= 3 passable orthogonal neighbors, which excludes 1-wide corridors.
bool safe_for_impassable_prop(const TileMap& m, int x, int y) {
    static const int dxs[4] = { 1, -1, 0, 0 };
    static const int dys[4] = { 0, 0, 1, -1 };
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        int nx = x + dxs[i], ny = y + dys[i];
        if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
        if (m.passable(nx, ny)) ++n;
    }
    return n >= 3;
}

// Precursor vault — interior ruin decoration for the Archive.
// Non-destructive (never collapses walls). Paints:
//   - CF_RUIN_TINT on interior walls (glyph-etched rune feel)
//   - FixtureType::Debris on floor tiles (passable ',' clutter)
//   - Toppled furniture on floor tiles at heavy decay (impassable, only in
//     wide cells so they never block a 1-tile-wide passage)
// Density scales with decay_level (0 pristine -> 3 heavy ruin).
// Runs before the fixtures layer, so it must avoid claiming the whole map —
// stairs and required fixtures still need open floor afterwards.
void decorate_precursor_vault(TileMap& map, const CivConfig& civ,
                              const DungeonLevelSpec& spec,
                              std::mt19937& rng) {
    (void)civ;
    const int decay = std::clamp(spec.decay_level, 0, 3);

    // ---- Wall runes ----
    const int wall_runes_per_room =
        (decay == 0) ? 1 :
        (decay == 1) ? 2 :
        (decay == 2) ? 3 : 5;

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

    // ---- Floor debris (passable ',' clutter) ----
    //   0 -> 0%   1 -> 1.5%   2 -> 4%   3 -> 7%
    const int debris_bp =
        (decay == 0) ? 0   :
        (decay == 1) ? 150 :
        (decay == 2) ? 400 : 700;   // parts per 10,000 (i.e. basis points)

    // ---- Toppled furniture (impassable, only at heavy decay) ----
    //   <=1 -> 0%   2 -> 0.3%   3 -> 0.8%
    const int furniture_bp =
        (decay <= 1) ? 0  :
        (decay == 2) ? 30 : 80;

    static const FixtureType furniture_kinds[] = {
        FixtureType::Crate,
        FixtureType::Shelf,
        FixtureType::Table,
    };

    std::uniform_int_distribution<int> d10k(0, 9999);
    std::uniform_int_distribution<int> dkind(0, 2);

    for (int y = 1; y < map.height() - 1; ++y) {
        for (int x = 1; x < map.width() - 1; ++x) {
            if (!map.passable(x, y)) continue;
            if (map.fixture_id(x, y) >= 0) continue;

            int roll = d10k(rng);
            if (roll < debris_bp) {
                map.add_fixture(x, y, make_fixture(FixtureType::Debris));
                continue;
            }
            int roll2 = d10k(rng);
            if (roll2 < furniture_bp && safe_for_impassable_prop(map, x, y)) {
                FixtureType ft = furniture_kinds[dkind(rng)];
                map.add_fixture(x, y, make_fixture(ft));
            }
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
