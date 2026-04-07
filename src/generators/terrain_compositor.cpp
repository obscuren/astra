#include "astra/terrain_compositor.h"

namespace astra {

void composite_terrain(TileMap& map, const TerrainChannels& channels,
                       const BiomeProfile& prof) {
    int w = channels.width;
    int h = channels.height;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float elev = channels.elev(x, y);
            float moist = channels.moist(x, y);
            StructureMask s = channels.struc(x, y);

            // Structure overrides take highest priority
            if (s == StructureMask::Wall)  { map.set(x, y, Tile::Wall);  continue; }
            if (s == StructureMask::Floor) { map.set(x, y, Tile::Floor); continue; }
            if (s == StructureMask::Water) { map.set(x, y, Tile::Water); continue; }

            // Moisture → water
            if (moist > prof.water_threshold && elev < prof.flood_level) {
                map.set(x, y, Tile::Water);
            } else if (elev > prof.wall_threshold) {
                map.set(x, y, Tile::Wall);
            } else {
                map.set(x, y, Tile::Floor);
            }
        }
    }
}

} // namespace astra
