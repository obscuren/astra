#pragma once

#include "astra/map_properties.h"
#include "astra/rect.h"
#include "astra/tilemap.h"

#include <optional>
#include <string>
#include <vector>

namespace astra {

// --- Building & Anchor Enums ---

enum class BuildingType : uint8_t {
    MainHall,
    Market,
    Dwelling,
    Distillery,
    Lookout,
    Workshop,
    Storage,

    // Ruin structure types
    Temple,
    Vault,
    GreatHall,
    Archive,
    Observatory,
};

enum class AnchorType : uint8_t {
    Center,
    Waterfront,
    Elevated,
};

struct Anchor {
    int x = 0;
    int y = 0;
    AnchorType type = AnchorType::Center;
};

// --- Civilization Style ---

struct CivStyle {
    std::string name;

    // Tile types
    Tile wall_tile   = Tile::StructuralWall;
    Tile floor_tile  = Tile::IndoorFloor;
    Tile path_tile   = Tile::Floor;

    // Fixture roles
    FixtureType lighting  = FixtureType::Torch;
    FixtureType storage   = FixtureType::Crate;
    FixtureType seating   = FixtureType::Bench;
    FixtureType cooking   = FixtureType::CampStove;
    FixtureType knowledge = FixtureType::BookCabinet;
    FixtureType display   = FixtureType::Rack;

    // Perimeter
    Tile perimeter_wall = Tile::Wall;
    FixtureType gate    = FixtureType::Gate;

    // Bridge
    FixtureType bridge_rail  = FixtureType::BridgeRail;
    FixtureType bridge_floor = FixtureType::BridgeFloor;

    // Decay factor (0 = pristine, 1 = fully ruined)
    float decay = 0.0f;
};

CivStyle civ_frontier();
CivStyle civ_advanced();
CivStyle civ_ruined();
CivStyle civ_ruin_default();
CivStyle civ_ruin_heavy();
CivStyle select_civ_style(const MapProperties& props);

// --- Furniture ---

enum class PlacementRule : uint8_t {
    Anchor,       // prominent position (back wall center), placed first
    TableSet,     // table + bench on each side, rows in center
    WallShelf,    // 3-tile shelf structure against walls
    WallUniform,  // distributed evenly along all walls
    Corner,       // one per corner, skip near doors
    Center,       // free-standing in open floor
};

struct FurnitureGroup {
    PlacementRule rule = PlacementRule::Center;
    FixtureType primary   = FixtureType::Table;
    FixtureType secondary = FixtureType::Bench; // paired item (bench for TableSet, item for Shelf)
    int min_count = 1;
    int max_count = 1;
    float frequency = 1.0f;  // probability this group appears at all
};

struct FurniturePalette {
    std::vector<FurnitureGroup> groups;
};

FurniturePalette furniture_palette(BuildingType type, const CivStyle& style);

// --- Geometry ---

struct BuildingShape {
    Rect primary;
    std::vector<Rect> extensions;
    std::vector<std::pair<int, int>> door_positions;
};

struct BuildingSpec {
    BuildingType type = BuildingType::Dwelling;
    BuildingShape shape;
    Anchor anchor;
};

struct BridgeSpec {
    int start_x = 0;
    int start_y = 0;
    int end_x   = 0;
    int end_y   = 0;
    int width    = 1;
};

struct PathSpec {
    int from_x = 0;
    int from_y = 0;
    int to_x   = 0;
    int to_y   = 0;
    int width   = 1;
};

struct PerimeterSpec {
    Rect bounds;
    std::vector<std::pair<int, int>> gate_positions;
};

// --- Terrain Modification ---

enum class TerrainModType : uint8_t {
    Level,
    RaiseBluff,
    CutBank,
    Clear,
};

struct TerrainMod {
    TerrainModType type = TerrainModType::Level;
    Rect area;
    float target_elevation = 0.5f;
};

// --- Placement & Plan ---

struct PlacementResult {
    Rect footprint;
    std::vector<Anchor> anchors;
    bool valid = false;
};

struct SettlementPlan {
    PlacementResult placement;
    CivStyle style;
    std::vector<BuildingSpec> buildings;
    std::vector<PathSpec> paths;
    std::vector<BridgeSpec> bridges;
    std::optional<PerimeterSpec> perimeter;
    std::vector<TerrainMod> terrain_mods;
    int size_category = 0;  // 0=small, 1=medium, 2=large
};

} // namespace astra
