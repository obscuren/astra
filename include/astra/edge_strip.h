// include/astra/edge_strip.h
#pragma once

#include "astra/tilemap.h"

#include <optional>
#include <vector>

namespace astra {

struct EdgeStripCell {
    Tile tile = Tile::Empty;
    std::optional<FixtureData> fixture;
    uint8_t glyph_override = 0;
    uint8_t custom_flags = 0;
};

enum class EdgeDirection : uint8_t { North, South, East, West };

struct EdgeStrip {
    int length = 0;   // 360 for N/S edges, 150 for E/W edges
    int depth  = 0;   // number of rows/cols captured (20)

    // Row-major: cells[depth_idx * length + along_idx]
    // depth_idx 0 = the shared boundary line, depth_idx (depth-1) = deepest into neighbor
    std::vector<EdgeStripCell> cells;

    const EdgeStripCell& at(int depth_idx, int along_idx) const {
        return cells[depth_idx * length + along_idx];
    }
};

// Extract an edge strip from a TileMap.
// dir = which edge of the source map to read.
// strip_depth = how many rows/cols to capture (typically 20).
EdgeStrip extract_edge_strip(const TileMap& map, EdgeDirection dir, int strip_depth);

} // namespace astra
