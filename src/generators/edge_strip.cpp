// src/generators/edge_strip.cpp
#include "astra/edge_strip.h"

namespace astra {

EdgeStrip extract_edge_strip(const TileMap& map, EdgeDirection dir, int strip_depth) {
    const int w = map.width();
    const int h = map.height();

    EdgeStrip strip;
    strip.depth = strip_depth;

    switch (dir) {
        case EdgeDirection::North:
        case EdgeDirection::South:
            strip.length = w;
            break;
        case EdgeDirection::East:
        case EdgeDirection::West:
            strip.length = h;
            break;
    }

    strip.cells.resize(strip.depth * strip.length);

    for (int d = 0; d < strip.depth; ++d) {
        for (int a = 0; a < strip.length; ++a) {
            int x = 0, y = 0;

            switch (dir) {
                case EdgeDirection::North:
                    // Row 0 of map = depth 0 (boundary), row strip_depth-1 = deepest
                    x = a;
                    y = d;
                    break;
                case EdgeDirection::South:
                    // Last row of map = depth 0 (boundary), going inward
                    x = a;
                    y = (h - 1) - d;
                    break;
                case EdgeDirection::East:
                    // Last col of map = depth 0, going inward
                    x = (w - 1) - d;
                    y = a;
                    break;
                case EdgeDirection::West:
                    // Col 0 of map = depth 0, going inward
                    x = d;
                    y = a;
                    break;
            }

            EdgeStripCell& cell = strip.cells[d * strip.length + a];
            Tile t = map.get(x, y);
            // Fixtures are stored separately — record the underlying floor tile
            cell.tile = (t == Tile::Fixture) ? Tile::Floor : t;
            cell.glyph_override = map.glyph_override(x, y);
            cell.custom_flags = map.get_custom_flags(x, y);

            int fid = map.fixture_id(x, y);
            if (fid >= 0) {
                cell.fixture = map.fixture(fid);
            }
        }
    }

    return strip;
}

} // namespace astra
