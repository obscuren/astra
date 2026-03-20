#pragma once

#include "astra/tilemap.h"

#include <cstdint>

namespace astra {

// Glyph indices for stamp cells.  0 = no override (use overworld_glyph()).
enum StampGlyph : uint8_t {
    SG_None         = 0,
    SG_Diamond      = 1,   // ♦
    SG_Omega        = 2,   // Ω
    SG_Theta        = 3,   // Θ
    SG_Equals       = 4,   // =
    SG_Percent      = 5,   // %
    SG_CurrencySign = 6,   // ¤
    SG_Plus         = 7,   // +
    SG_Hash         = 8,   // #
    SG_Pi           = 9,   // π
    SG_Sigma        = 10,  // Σ
    SG_Section      = 11,  // §
    SG_DownTriangle = 12,  // ▼
    SG_Pipe         = 13,  // |
    SG_MiddleDot    = 14,  // ·
    SG_COUNT
};

// Returns UTF-8 string for a glyph index, nullptr for SG_None.
const char* stamp_glyph(uint8_t index);

struct StampCell {
    int dx, dy;           // offset from anchor
    Tile tile;            // tile to place
    uint8_t glyph_index;  // 0 = use overworld_glyph()
};

struct StampDef {
    int width, height;          // bounding box
    const StampCell* cells;
    int cell_count;
};

// Stamp pools by POI category
extern const StampDef settlement_stamps[];
extern const int settlement_stamp_count;

extern const StampDef habitat_stamps[];
extern const int habitat_stamp_count;

extern const StampDef ruin_stamps[];
extern const int ruin_stamp_count;

extern const StampDef crashed_ship_stamps[];
extern const int crashed_ship_stamp_count;

extern const StampDef cave_stamps[];
extern const int cave_stamp_count;

extern const StampDef outpost_stamps[];
extern const int outpost_stamp_count;

} // namespace astra
