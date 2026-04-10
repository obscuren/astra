#include "astra/overworld_stamps.h"

namespace astra {

const char* stamp_glyph(uint8_t index) {
    static const char* table[] = {
        nullptr,                   // 0  SG_None
        "\xe2\x99\xa6",           // 1  ♦
        "\xce\xa9",               // 2  Ω
        "\xce\x98",               // 3  Θ
        "=",                      // 4  =
        "%",                      // 5  %
        "\xc2\xa4",               // 6  ¤
        "+",                      // 7  +
        "#",                      // 8  #
        "\xcf\x80",               // 9  π
        "\xce\xa3",               // 10 Σ
        "\xc2\xa7",               // 11 §
        "\xe2\x96\xbc",           // 12 ▼
        "|",                      // 13 |
        "\xc2\xb7",               // 14 ·
        "!",                      // 15 ! (quest marker)
    };
    if (index == 0 || index >= SG_COUNT) return nullptr;
    return table[index];
}

// ---------------------------------------------------------------------------
// Settlement stamps
// ---------------------------------------------------------------------------

static const StampCell settlement_1x1[] = {
    {0, 0, Tile::OW_Settlement, SG_None},
};

static const StampCell settlement_2x2[] = {
    {0, 0, Tile::OW_Settlement, SG_Diamond},
    {1, 0, Tile::OW_Settlement, SG_Omega},
    {0, 1, Tile::OW_Settlement, SG_Diamond},
    {1, 1, Tile::OW_Settlement, SG_Diamond},
};

static const StampCell settlement_3x3[] = {
    {0, 0, Tile::OW_Settlement, SG_Diamond},
    {1, 0, Tile::OW_Settlement, SG_Omega},
    {2, 0, Tile::OW_Settlement, SG_Diamond},
    {0, 1, Tile::OW_Settlement, SG_Diamond},
    {1, 1, Tile::OW_Settlement, SG_Diamond},
    {2, 1, Tile::OW_Settlement, SG_Diamond},
    {1, 2, Tile::OW_Settlement, SG_Diamond},
};

static const StampCell settlement_walled[] = {
    {0, 0, Tile::OW_Settlement, SG_Hash},
    {1, 0, Tile::OW_Settlement, SG_Diamond},
    {2, 0, Tile::OW_Settlement, SG_Hash},
    {0, 1, Tile::OW_Settlement, SG_Diamond},
    {1, 1, Tile::OW_Settlement, SG_Diamond},
    {2, 1, Tile::OW_Settlement, SG_Diamond},
};

const StampDef settlement_stamps[] = {
    {1, 1, settlement_1x1, 1},
};
const int settlement_stamp_count = 1;

// ---------------------------------------------------------------------------
// Space habitat stamps (airless worlds)
// ---------------------------------------------------------------------------

static const StampCell habitat_1x1[] = {
    {0, 0, Tile::OW_Outpost, SG_Theta},
};

static const StampCell habitat_3x2[] = {
    {0, 0, Tile::OW_Outpost, SG_Theta},
    {1, 0, Tile::OW_Outpost, SG_Equals},
    {2, 0, Tile::OW_Outpost, SG_Theta},
    {1, 1, Tile::OW_Outpost, SG_Theta},
};

static const StampCell habitat_2x2[] = {
    {0, 0, Tile::OW_Outpost, SG_Theta},
    {1, 0, Tile::OW_Outpost, SG_Plus},
    {0, 1, Tile::OW_Outpost, SG_Theta},
};

const StampDef habitat_stamps[] = {
    {1, 1, habitat_1x1, 1},
    {3, 2, habitat_3x2, 4},
    {2, 2, habitat_2x2, 3},
};
const int habitat_stamp_count = 3;

// ---------------------------------------------------------------------------
// Ruin stamps
// ---------------------------------------------------------------------------

static const StampCell ruin_1x1_a[] = {
    {0, 0, Tile::OW_Ruins, SG_Pi},
};

static const StampCell ruin_1x1_b[] = {
    {0, 0, Tile::OW_Ruins, SG_Omega},
};

static const StampCell ruin_2x2[] = {
    {0, 0, Tile::OW_Ruins, SG_Pi},
    {1, 0, Tile::OW_Ruins, SG_Omega},
    {1, 1, Tile::OW_Ruins, SG_Section},
};

static const StampCell ruin_3x3[] = {
    {0, 0, Tile::OW_Ruins, SG_Omega},
    {2, 0, Tile::OW_Ruins, SG_Omega},
    {1, 1, Tile::OW_Ruins, SG_Pi},
    {1, 2, Tile::OW_Ruins, SG_Section},
};

const StampDef ruin_stamps[] = {
    {1, 1, ruin_1x1_a, 1},
    {1, 1, ruin_1x1_b, 1},
    {2, 2, ruin_2x2, 3},
    {3, 3, ruin_3x3, 4},
};
const int ruin_stamp_count = 4;

// ---------------------------------------------------------------------------
// Crashed ship stamps
// ---------------------------------------------------------------------------

static const StampCell crash_1x1[] = {
    {0, 0, Tile::OW_CrashedShip, SG_None},
};

const StampDef crashed_ship_stamps[] = {
    {1, 1, crash_1x1, 1},
};
const int crashed_ship_stamp_count = 1;

// ---------------------------------------------------------------------------
// Cave stamps
// ---------------------------------------------------------------------------

static const StampCell cave_1x1_a[] = {
    {0, 0, Tile::OW_CaveEntrance, SG_DownTriangle},
};

static const StampCell cave_1x1_b[] = {
    {0, 0, Tile::OW_CaveEntrance, SG_Theta},
};

const StampDef cave_stamps[] = {
    {1, 1, cave_1x1_a, 1},
    {1, 1, cave_1x1_b, 1},
};
const int cave_stamp_count = 2;

// ---------------------------------------------------------------------------
// Outpost stamps
// ---------------------------------------------------------------------------

static const StampCell outpost_1x1[] = {
    {0, 0, Tile::OW_Outpost, SG_Plus},
};

static const StampCell outpost_2x2[] = {
    {0, 0, Tile::OW_Outpost, SG_Plus},
    {1, 0, Tile::OW_Outpost, SG_Equals},
    {1, 1, Tile::OW_Outpost, SG_Plus},
};

const StampDef outpost_stamps[] = {
    {1, 1, outpost_1x1, 1},
    {2, 2, outpost_2x2, 3},
};
const int outpost_stamp_count = 2;

} // namespace astra
