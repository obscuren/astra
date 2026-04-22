#include "astra/dungeon/backdrop.h"

#include "astra/ruin_types.h"
#include "astra/tilemap.h"

namespace astra::dungeon {

void apply_backdrop(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, std::mt19937& rng) {
    (void)style;  // material is informational in slice 1
    (void)civ;    // palette used at render time, not here
    (void)rng;

    const int w = map.width();
    const int h = map.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            map.set(x, y, Tile::Wall);
        }
    }
    map.set_biome(Biome::Dungeon);
}

} // namespace astra::dungeon
