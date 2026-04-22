#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "astra/dungeon/puzzles.h"

namespace astra::dungeon {

enum class StyleId : uint8_t {
    SimpleRoomsAndCorridors = 0,
    PrecursorRuin           = 1,
    OpenCave                = 2,
    TunnelCave              = 3,
    DerelictStation         = 4,
};

enum class LayoutKind : uint8_t {
    BSPRooms,
    OpenCave,
    TunnelCave,
    DerelictStationBSP,
    RuinStamps,
    PrecursorVault,     // authored per-depth topology (Archive migration)
};

enum class OverlayKind : uint8_t {
    None          = 0,
    BattleScarred = 1,
    Infested      = 2,
    Flooded       = 3,
    Vacuum        = 4,
};

enum class StairsStrategy : uint8_t {
    EntryExitRooms,
    FurthestPair,
    CorridorEndpoints,
};

// Layer 6.iii catalog (Archive migration).
enum class FixtureKind : uint8_t {
    Plinth,
    Altar,
    Inscription,
    Pillar,
    ResonancePillar,
    Brazier,
};

enum class PlacementSlot : uint8_t {
    SanctumCenter,    // center of the single terminal chamber
    ChapelCenter,     // centers of chapel-tagged rooms
    EachRoomOnce,     // one per non-terminal room
    WallAttached,     // attached to an interior wall of any room
    FlankPair,        // two copies flanking the nearest previously-placed target
};

struct IntRange { int min; int max; };

struct RequiredFixture {
    FixtureKind   kind;
    PlacementSlot where;
    IntRange      count;
    uint32_t      depth_mask;   // bit 0 = depth 1, bit 1 = depth 2, ...
};

// Helpers for authoring depth_mask entries.
constexpr uint32_t depth_mask_bit(int depth) {
    return (depth >= 1 && depth <= 32) ? (1u << (depth - 1)) : 0u;
}
constexpr uint32_t depth_mask_all(int max_depth) {
    uint32_t m = 0;
    for (int d = 1; d <= max_depth; ++d) m |= depth_mask_bit(d);
    return m;
}

struct DungeonStyle {
    StyleId                      id;
    const char*                  debug_name;
    std::string                  backdrop_material;
    LayoutKind                   layout;
    StairsStrategy               stairs_strategy;
    std::vector<OverlayKind>     allowed_overlays;
    std::string                  decoration_pack;
    bool                         connectivity_required;
    std::vector<RequiredFixture> required_fixtures;   // layer 6.iii catalog
    std::vector<RequiredPuzzle>  required_puzzles;   // layer 7 catalog
};

const DungeonStyle& style_config(StyleId id);
bool parse_style_id(const std::string& debug_name, StyleId& out);

} // namespace astra::dungeon
