#pragma once

#include "astra/star_chart.h"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astra {

enum class ChartZoom : uint8_t;

// Arm label for galaxy-level rendering
struct GVArmLabel {
    std::string_view name;
    float gx, gy;
};

// Quest target info for a specific body
struct GVBodyQuest {
    int body_index;
    std::string title;
};

// Describes the galaxy map visualization — just the star field / orbital diagram.
// No panel chrome, no info sidebar. Renders into whatever rect it's given.
// Constructed per-frame, consumed immediately — span lifetimes are frame-scoped.
struct GalaxyMapDesc {
    ChartZoom zoom;
    std::span<const StarSystem> systems;
    std::span<const GVArmLabel> arm_labels;

    // Player location
    int player_system_index = -1;
    int player_body_index = -1;
    int player_moon_index = -1;
    bool at_station = false;
    bool on_ship = false;

    // Viewport center (galactic coordinates)
    float view_cx = 0.0f;
    float view_cy = 0.0f;

    // Cursor (interactive mode, -1 = none)
    int cursor_system_index = -1;
    int body_cursor = -1;
    int sub_cursor = -1;

    // Highlight (non-interactive, e.g. quest preview — -1 = none)
    int highlight_system_index = -1;

    // Quest markers
    std::vector<uint32_t> quest_system_ids;
    std::vector<GVBodyQuest> quest_body_targets;

    // Navigation range (for warp radius display)
    float navi_range = 1.0f;

    // System zoom: precomputed station host body index
    int station_host_body_index = -1;
};

} // namespace astra
