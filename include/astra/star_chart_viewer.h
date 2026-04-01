#pragma once

#include "astra/renderer.h"
#include "astra/star_chart.h"
#include "astra/galaxy_map_desc.h"
#include "astra/ui.h"

namespace astra {

class WorldManager;

enum class ChartZoom : uint8_t { Galaxy, Region, Local, System };

enum class ChartActionType { None, WarpToSystem, TravelToBody, TravelToStation };

struct ChartAction {
    ChartActionType type = ChartActionType::None;
    int system_index = -1;   // index into nav_->systems
    int body_index = -1;     // destination body
    int moon_index = -1;     // destination moon (-1 = body itself)
    bool to_station = false; // docking at station
};

class StarChartViewer {
public:
    StarChartViewer() = default;
    StarChartViewer(NavigationData* nav, Renderer* renderer, WorldManager* world = nullptr);

    bool is_open() const { return open_; }
    void open();
    void close();

    bool has_pending_action() const;
    ChartAction consume_action();

    // Returns true if input was consumed
    bool handle_input(int key);

    void draw(int screen_w, int screen_h);

private:
    NavigationData* nav_ = nullptr;
    Renderer* renderer_ = nullptr;
    WorldManager* world_ = nullptr;
    bool open_ = false;
    ChartZoom zoom_ = ChartZoom::Galaxy;

    // Viewport center in galactic coordinates
    float view_cx_ = 0.0f;
    float view_cy_ = 0.0f;

    // System cursor (index into nav_->systems, -1 = none)
    int cursor_index_ = -1;

    // Body cursor (index into system's bodies, -1 = none)
    int body_cursor_ = -1;

    // Sub-item cursor within a body (-1 = body itself, 0+ = sub-item like station/moon)
    int sub_cursor_ = -1;

    // Semantic rendering
    GalaxyMapDesc build_map_desc() const;
    void draw_info_panel(DrawContext& ctx);
    void draw_system_info_text(DrawContext& ctx, const StarSystem& sys, int start_y, int max_h = 100);
    void draw_body_info_text(DrawContext& ctx, const CelestialBody& body, const StarSystem& sys, int start_y);
    void draw_station_info_text(DrawContext& ctx, const StarSystem& sys, int start_y);

    // Find which body index the station orbits (first gas giant, or -1)
    int station_host_body(const StarSystem& sys) const;

    // Cursor navigation
    void move_cursor_direction(int dx, int dy);
    void cycle_cursor();
    void center_on_cursor();
    void center_on_sol();

    // Scanning
    void scan_system();
    std::string scan_message_;
    int scan_message_timer_ = 0;

    // View-only mode (observatory): can browse but not travel
    bool view_only_ = false;
public:
    void set_view_only(bool v) { view_only_ = v; }
    bool view_only() const { return view_only_; }
private:

    // Pending travel action
    ChartAction pending_action_;

    // Find nearest system to a galactic coordinate
    int find_nearest_system(float gx, float gy, float max_dist = 1e9f) const;
};

} // namespace astra
