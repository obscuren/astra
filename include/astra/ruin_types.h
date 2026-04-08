#pragma once

#include "astra/settlement_types.h"
#include "astra/lore_types.h"

#include <string>
#include <vector>

namespace astra {

// --- Civilization visual config for ruins ---

struct CivConfig {
    std::string name;

    // Glyph palettes (UTF-8 strings for overworld + detail rendering)
    std::vector<std::string> wall_glyphs;    // primary wall characters
    std::vector<std::string> accent_glyphs;  // decorative/interior characters

    // Color indices (terminal 256-color)
    int color_primary   = 250;   // main wall color
    int color_secondary = 245;   // accent/highlight color
    int color_tint      = 245;   // ruin tint for custom_flags_

    // Architectural tendencies
    float wall_thickness_bias = 1.0f;  // multiplier on base thickness (>1 = thicker)
    float split_regularity    = 0.5f;  // 0 = organic BSP, 1 = geometric BSP

    // Room preferences (weighted)
    std::vector<BuildingType> preferred_rooms;

    // Map to lore architecture
    Architecture architecture = Architecture::Geometric;
};

// --- Room discovered by flood-fill ---

struct RuinRoom {
    Rect bounds;                   // bounding rect of the room
    std::vector<std::pair<int,int>> floor_tiles;  // all floor positions
    BuildingType theme = BuildingType::GreatHall;  // assigned theme
    bool is_nucleus = false;       // part of a deliberate nucleus cluster
};

// --- Post-processing stamp types ---

enum class RuinStampType : uint8_t {
    BattleScarred,
    Infested,
    Flooded,
    Excavated,
};

struct RuinStampConfig {
    RuinStampType type;
    float intensity = 0.5f;  // 0-1, how severe
};

// --- BSP node ---

struct BspNode {
    Rect area;
    int depth = 0;
    bool is_leaf = false;
    bool is_nucleus = false;
    int child_a = -1;  // index into flat vector
    int child_b = -1;
};

// --- Full ruin plan ---

struct RuinPlan {
    Rect footprint;
    CivConfig civ;
    std::vector<BspNode> bsp_nodes;
    std::vector<RuinRoom> rooms;
    std::vector<RuinStampConfig> stamps;
    float base_decay = 0.5f;

    // Edge continuity: which edges connect to other ruin tiles
    bool edge_n = false;
    bool edge_s = false;
    bool edge_e = false;
    bool edge_w = false;
};

// --- Civilization config registry ---

CivConfig civ_config_monolithic();
CivConfig civ_config_baroque();
CivConfig civ_config_crystal();
CivConfig civ_config_industrial();
CivConfig civ_config_for_architecture(Architecture arch);
CivConfig civ_config_by_name(const std::string& name);

} // namespace astra
