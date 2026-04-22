#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra::dungeon {

enum class StyleId : uint8_t {
    SimpleRoomsAndCorridors = 0,   // slice 1 smoke-test
    PrecursorRuin           = 1,   // reserved — follow-up slice (Archive migration)
    OpenCave                = 2,   // reserved — cave slice
    TunnelCave              = 3,   // reserved — cave slice
    DerelictStation         = 4,   // reserved — station slice
};

enum class LayoutKind : uint8_t {
    BSPRooms,
    OpenCave,
    TunnelCave,
    DerelictStationBSP,
    RuinStamps,
};

enum class OverlayKind : uint8_t {
    None          = 0,
    BattleScarred = 1,
    Infested      = 2,
    Flooded       = 3,
    Vacuum        = 4,  // reserved — no-op stub in slice 1
};

enum class StairsStrategy : uint8_t {
    EntryExitRooms,     // distinct regions (BSP, multi-room ruin)
    FurthestPair,       // one big region, stairs at max-distance pair (open cave)
    CorridorEndpoints,  // corridor-only (tunnel cave)
};

struct DungeonStyle {
    StyleId         id;
    const char*     debug_name;               // "simple_rooms", shown in :dungen
    std::string     backdrop_material;        // "rock","sand","plating","cavern_floor"
    LayoutKind      layout;
    StairsStrategy  stairs_strategy;
    std::vector<OverlayKind> allowed_overlays;
    std::string     decoration_pack;          // "ruin_debris","cave_flora","station_scrap","natural_minimal"
    bool            connectivity_required;    // true => apply_connectivity verifies reachability
};

// Registry lookup. Asserts on unknown id.
const DungeonStyle& style_config(StyleId id);

// Try-parse for dev-console. Returns false if unknown.
bool parse_style_id(const std::string& debug_name, StyleId& out);

} // namespace astra::dungeon
