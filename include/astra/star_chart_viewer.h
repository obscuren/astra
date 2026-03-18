#pragma once

#include "astra/renderer.h"
#include "astra/star_chart.h"
#include "astra/ui.h"

namespace astra {

enum class ChartZoom { Galaxy, Region, Local };

class StarChartViewer {
public:
    StarChartViewer() = default;
    StarChartViewer(NavigationData* nav, Renderer* renderer);

    bool is_open() const { return open_; }
    void open();
    void close();

    // Returns true if input was consumed
    bool handle_input(int key);

    void draw(int screen_w, int screen_h);

private:
    NavigationData* nav_ = nullptr;
    Renderer* renderer_ = nullptr;
    bool open_ = false;
    ChartZoom zoom_ = ChartZoom::Galaxy;

    // Viewport center in galactic coordinates
    float view_cx_ = 0.0f;
    float view_cy_ = 0.0f;

    // System cursor (index into nav_->systems, -1 = none)
    int cursor_index_ = -1;

    // Rendering
    void draw_galaxy_view(DrawContext& map_ctx, DrawContext& info_ctx);
    void draw_region_view(DrawContext& map_ctx, DrawContext& info_ctx);
    void draw_local_view(DrawContext& map_ctx, DrawContext& info_ctx);
    void draw_system_info(DrawContext& ctx, const StarSystem& sys, int start_y);

    // Map projection helpers
    int to_screen_x(float gx, float view_left, float view_width, int screen_w) const;
    int to_screen_y(float gy, float view_top, float view_height, int screen_h) const;

    // Cursor navigation
    void move_cursor_direction(int dx, int dy);
    void cycle_cursor();
    void center_on_cursor();
    void center_on_sol();

    // Find nearest system to a galactic coordinate
    int find_nearest_system(float gx, float gy, float max_dist = 1e9f) const;
};

} // namespace astra
