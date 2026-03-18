#include "astra/star_chart_viewer.h"

#include "astra/celestial_body.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace astra {

// ---------------------------------------------------------------------------
// Arm labels for galaxy view
// ---------------------------------------------------------------------------

struct ArmLabel {
    const char* name;
    float gx, gy; // approximate label position
};

static const ArmLabel arm_labels[] = {
    {"Sagittarius",  140.0f,  -30.0f},
    {"Perseus",      -30.0f,  140.0f},
    {"Norma-Outer", -140.0f,   30.0f},
    {"Scutum-Cent.", 30.0f, -140.0f},
};

// ---------------------------------------------------------------------------
// Construction / open / close
// ---------------------------------------------------------------------------

StarChartViewer::StarChartViewer(NavigationData* nav, Renderer* renderer)
    : nav_(nav), renderer_(renderer) {}

void StarChartViewer::open() {
    open_ = true;
    zoom_ = ChartZoom::Galaxy;
    cursor_index_ = -1;
    center_on_sol();
}

void StarChartViewer::close() {
    open_ = false;
}

void StarChartViewer::center_on_sol() {
    if (!nav_) return;
    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        if (nav_->systems[i].id == nav_->current_system_id) {
            view_cx_ = nav_->systems[i].gx;
            view_cy_ = nav_->systems[i].gy;
            return;
        }
    }
    view_cx_ = 0.0f;
    view_cy_ = 0.0f;
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

bool StarChartViewer::handle_input(int key) {
    if (!open_) return false;

    switch (zoom_) {
        case ChartZoom::Galaxy: {
            switch (key) {
                case KEY_UP:    case 'w': view_cy_ -= 15.0f; return true;
                case KEY_DOWN:  case 's': view_cy_ += 15.0f; return true;
                case KEY_LEFT:  case 'a': view_cx_ -= 15.0f; return true;
                case KEY_RIGHT: case 'd': view_cx_ += 15.0f; return true;
                case '+': case '=': case '\n': case '\r':
                    zoom_ = ChartZoom::Region;
                    cursor_index_ = find_nearest_system(view_cx_, view_cy_);
                    return true;
                case 'h': // Home — center on Sol
                    center_on_sol();
                    return true;
                case '\033': case 'q':
                    close();
                    return true;
            }
            break;
        }
        case ChartZoom::Region: {
            switch (key) {
                case KEY_UP:    case 'w': view_cy_ -= 5.0f; return true;
                case KEY_DOWN:  case 's': view_cy_ += 5.0f; return true;
                case KEY_LEFT:  case 'a': view_cx_ -= 5.0f; return true;
                case KEY_RIGHT: case 'd': view_cx_ += 5.0f; return true;
                case '\t':
                    cycle_cursor();
                    return true;
                case '+': case '=': case '\n': case '\r':
                    if (cursor_index_ >= 0) {
                        view_cx_ = nav_->systems[cursor_index_].gx;
                        view_cy_ = nav_->systems[cursor_index_].gy;
                        zoom_ = ChartZoom::Local;
                    }
                    return true;
                case '-': case '\b':
                    zoom_ = ChartZoom::Galaxy;
                    cursor_index_ = -1;
                    return true;
                case 'h':
                    center_on_sol();
                    return true;
                case '\033': case 'q':
                    zoom_ = ChartZoom::Galaxy;
                    cursor_index_ = -1;
                    return true;
            }
            break;
        }
        case ChartZoom::Local: {
            switch (key) {
                case KEY_UP:    move_cursor_direction(0, -1); return true;
                case KEY_DOWN:  move_cursor_direction(0,  1); return true;
                case KEY_LEFT:  move_cursor_direction(-1, 0); return true;
                case KEY_RIGHT: move_cursor_direction( 1, 0); return true;
                case 'w': move_cursor_direction(0, -1); return true;
                case 's': move_cursor_direction(0,  1); return true;
                case 'a': move_cursor_direction(-1, 0); return true;
                case 'd': move_cursor_direction( 1, 0); return true;
                case '\t':
                    cycle_cursor();
                    center_on_cursor();
                    return true;
                case '+': case '=': case '\n': case '\r':
                    if (cursor_index_ >= 0) {
                        auto& sys = nav_->systems[cursor_index_];
                        if (sys.discovered && sys.id != 0) {
                            generate_system_bodies(sys);
                            if (!sys.bodies.empty()) {
                                zoom_ = ChartZoom::System;
                                body_cursor_ = 0;
                            }
                        }
                    }
                    return true;
                case '-': case '\b':
                    zoom_ = ChartZoom::Region;
                    return true;
                case 'h':
                    center_on_sol();
                    cursor_index_ = find_nearest_system(view_cx_, view_cy_);
                    return true;
                case '\033': case 'q':
                    zoom_ = ChartZoom::Region;
                    return true;
            }
            break;
        }
        case ChartZoom::System: {
            if (cursor_index_ < 0 || cursor_index_ >= static_cast<int>(nav_->systems.size())) {
                zoom_ = ChartZoom::Local;
                return true;
            }
            const auto& sys = nav_->systems[cursor_index_];
            int body_count = static_cast<int>(sys.bodies.size());
            switch (key) {
                case KEY_UP: case 'w':
                    if (body_cursor_ > 0) --body_cursor_;
                    return true;
                case KEY_DOWN: case 's':
                    if (body_cursor_ < body_count - 1) ++body_cursor_;
                    return true;
                case KEY_LEFT: case 'a':
                    if (body_cursor_ > 0) --body_cursor_;
                    return true;
                case KEY_RIGHT: case 'd':
                    if (body_cursor_ < body_count - 1) ++body_cursor_;
                    return true;
                case '-': case '\b': case '\033': case 'q':
                    zoom_ = ChartZoom::Local;
                    body_cursor_ = -1;
                    return true;
            }
            break;
        }
    }
    return true; // consume all input while open
}

// ---------------------------------------------------------------------------
// Cursor helpers
// ---------------------------------------------------------------------------

int StarChartViewer::find_nearest_system(float gx, float gy, float max_dist) const {
    if (!nav_) return -1;
    int best = -1;
    float best_d2 = max_dist * max_dist;
    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        float dx = nav_->systems[i].gx - gx;
        float dy = nav_->systems[i].gy - gy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void StarChartViewer::cycle_cursor() {
    if (!nav_ || nav_->systems.empty()) return;

    // Collect systems visible in current viewport
    float half_w, half_h;
    if (zoom_ == ChartZoom::Region) {
        half_w = 20.0f; half_h = 15.0f;
    } else {
        half_w = 7.5f; half_h = 6.0f;
    }

    std::vector<int> visible;
    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        float dx = std::abs(nav_->systems[i].gx - view_cx_);
        float dy = std::abs(nav_->systems[i].gy - view_cy_);
        if (dx <= half_w && dy <= half_h) {
            visible.push_back(static_cast<int>(i));
        }
    }
    if (visible.empty()) return;

    // Find current in visible list, advance to next
    auto it = std::find(visible.begin(), visible.end(), cursor_index_);
    if (it == visible.end() || ++it == visible.end()) {
        cursor_index_ = visible[0];
    } else {
        cursor_index_ = *it;
    }
}

void StarChartViewer::move_cursor_direction(int dx, int dy) {
    if (!nav_ || nav_->systems.empty()) return;

    if (cursor_index_ < 0) {
        cursor_index_ = find_nearest_system(view_cx_, view_cy_);
        center_on_cursor();
        return;
    }

    const auto& cur = nav_->systems[cursor_index_];
    float best_d2 = 1e18f;
    int best = -1;

    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        if (static_cast<int>(i) == cursor_index_) continue;
        const auto& s = nav_->systems[i];
        float sdx = s.gx - cur.gx;
        float sdy = s.gy - cur.gy;

        // Check direction alignment
        bool aligned = false;
        if (dx > 0 && sdx > 0.5f) aligned = true;
        if (dx < 0 && sdx < -0.5f) aligned = true;
        if (dy > 0 && sdy > 0.5f) aligned = true;
        if (dy < 0 && sdy < -0.5f) aligned = true;
        if (!aligned) continue;

        // Limit search radius in local view
        float d2 = sdx * sdx + sdy * sdy;
        if (d2 > 30.0f * 30.0f) continue;

        if (d2 < best_d2) {
            best_d2 = d2;
            best = static_cast<int>(i);
        }
    }

    if (best >= 0) {
        cursor_index_ = best;
        center_on_cursor();
    }
}

void StarChartViewer::center_on_cursor() {
    if (cursor_index_ >= 0 && nav_) {
        view_cx_ = nav_->systems[cursor_index_].gx;
        view_cy_ = nav_->systems[cursor_index_].gy;
    }
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

int StarChartViewer::to_screen_x(float gx, float view_left, float view_width, int sw) const {
    return static_cast<int>((gx - view_left) / view_width * static_cast<float>(sw));
}

int StarChartViewer::to_screen_y(float gy, float view_top, float view_height, int sh) const {
    return static_cast<int>((gy - view_top) / view_height * static_cast<float>(sh));
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void StarChartViewer::draw(int screen_w, int screen_h) {
    if (!open_ || !nav_ || !renderer_) return;

    // Title based on zoom level
    const char* zoom_name = "Galaxy";
    if (zoom_ == ChartZoom::Region) zoom_name = "Region";
    else if (zoom_ == ChartZoom::Local) zoom_name = "Local";
    else if (zoom_ == ChartZoom::System) zoom_name = "System";

    std::string title = std::string("Star Chart [") + zoom_name + "]";

    // Full-screen window
    Window win(renderer_, Rect{0, 0, screen_w, screen_h}, title);

    // Footer hints
    switch (zoom_) {
        case ChartZoom::Galaxy:
            win.set_footer("[Arrows] Pan  [+] Zoom in  [H] Home  [Esc] Close");
            break;
        case ChartZoom::Region:
            win.set_footer("[Arrows] Pan  [Tab] Select  [+] Zoom in  [-] Zoom out  [H] Home  [Esc] Back");
            break;
        case ChartZoom::Local:
            win.set_footer("[Arrows] Select  [Tab] Cycle  [+] View system  [-] Zoom out  [H] Home  [Esc] Back");
            break;
        case ChartZoom::System:
            win.set_footer("[Up/Down] Select body  [-] Back  [Esc] Back");
            break;
    }

    win.draw();
    DrawContext content = win.content();

    // Split content: left 70% = map, right 30% = info panel
    int info_width = std::max(28, content.width() * 30 / 100);
    int map_width = content.width() - info_width - 1; // 1 for separator

    Rect map_rect = {content.bounds().x, content.bounds().y,
                     map_width, content.height()};
    Rect sep_rect = {content.bounds().x + map_width, content.bounds().y,
                     1, content.height()};
    Rect info_rect = {content.bounds().x + map_width + 1, content.bounds().y,
                      info_width, content.height()};

    DrawContext map_ctx(renderer_, map_rect);
    DrawContext sep_ctx(renderer_, sep_rect);
    DrawContext info_ctx(renderer_, info_rect);

    sep_ctx.vline(0, '|');

    switch (zoom_) {
        case ChartZoom::Galaxy: draw_galaxy_view(map_ctx, info_ctx); break;
        case ChartZoom::Region: draw_region_view(map_ctx, info_ctx); break;
        case ChartZoom::Local:  draw_local_view(map_ctx, info_ctx);  break;
        case ChartZoom::System: draw_system_view(map_ctx, info_ctx); break;
    }
}

// ---------------------------------------------------------------------------
// Galaxy view
// ---------------------------------------------------------------------------

void StarChartViewer::draw_galaxy_view(DrawContext& map_ctx, DrawContext& info_ctx) {
    int mw = map_ctx.width();
    int mh = map_ctx.height();
    if (mw <= 0 || mh <= 0) return;

    // Viewport: show full galaxy with pan offset
    float view_w = 440.0f;  // full galaxy diameter
    float view_h = view_w * (static_cast<float>(mh) / static_cast<float>(mw)) * 2.0f; // chars are ~2:1 aspect
    float view_left = view_cx_ - view_w / 2.0f;
    float view_top = view_cy_ - view_h / 2.0f;

    // Draw all systems
    for (const auto& sys : nav_->systems) {
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;

        if (sys.id == 0) {
            // Sgr A*
            map_ctx.put(sx, sy, '+', Color::BrightMagenta);
        } else if (sys.id == 1) {
            // Sol
            map_ctx.put(sx, sy, '*', Color::Yellow);
        } else if (sys.id == nav_->current_system_id) {
            map_ctx.put(sx, sy, '@', Color::Green);
        } else if (sys.discovered) {
            map_ctx.put(sx, sy, '.', star_class_color(sys.star_class));
        } else {
            map_ctx.put(sx, sy, '.', Color::DarkGray);
        }
    }

    // Draw arm labels
    for (const auto& label : arm_labels) {
        int lx = to_screen_x(label.gx, view_left, view_w, mw);
        int ly = to_screen_y(label.gy, view_top, view_h, mh);
        if (lx >= 0 && lx < mw - 6 && ly >= 0 && ly < mh) {
            map_ctx.text(lx, ly, label.name, Color::DarkGray);
        }
    }

    // Draw Sgr A* label
    {
        int cx = to_screen_x(0.0f, view_left, view_w, mw);
        int cy = to_screen_y(0.0f, view_top, view_h, mh);
        if (cx >= 0 && cx < mw - 6 && cy >= 1 && cy < mh) {
            map_ctx.text(cx - 2, cy - 1, "Sgr A*", Color::BrightMagenta);
        }
    }

    // Sol label
    {
        int sx = to_screen_x(180.0f, view_left, view_w, mw);
        int sy = to_screen_y(0.0f, view_top, view_h, mh);
        if (sx >= 0 && sx < mw - 3 && sy < mh - 1) {
            map_ctx.text(sx - 1, sy + 1, "Sol", Color::Yellow);
        }
    }

    // Crosshair reticle at viewport center
    {
        int cx = mw / 2;
        int cy = mh / 2;
        if (cx > 1)      map_ctx.put(cx - 2, cy, '-', Color::White);
        if (cx > 0)      map_ctx.put(cx - 1, cy, '-', Color::White);
        if (cx < mw - 1) map_ctx.put(cx + 1, cy, '-', Color::White);
        if (cx < mw - 2) map_ctx.put(cx + 2, cy, '-', Color::White);
        if (cy > 0)      map_ctx.put(cx, cy - 1, '|', Color::White);
        if (cy < mh - 1) map_ctx.put(cx, cy + 1, '|', Color::White);
        map_ctx.put(cx, cy, '+', Color::White);
    }

    // Info panel
    info_ctx.text(1, 0, "MILKY WAY GALAXY", Color::White);
    info_ctx.hline(1, '~');

    int y = 3;
    info_ctx.text(1, y++, "Zoom: Galaxy", Color::DarkGray);

    int total = static_cast<int>(nav_->systems.size());
    int discovered = 0;
    for (const auto& s : nav_->systems) {
        if (s.discovered) ++discovered;
    }
    info_ctx.text(1, y++, "Systems: " + std::to_string(total), Color::DarkGray);
    info_ctx.text(1, y++, "Discovered: " + std::to_string(discovered), Color::Cyan);
    y++;

    // Current system
    for (const auto& s : nav_->systems) {
        if (s.id == nav_->current_system_id) {
            info_ctx.text(1, y++, "Current: " + s.name, Color::Yellow);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Region view
// ---------------------------------------------------------------------------

void StarChartViewer::draw_region_view(DrawContext& map_ctx, DrawContext& info_ctx) {
    int mw = map_ctx.width();
    int mh = map_ctx.height();
    if (mw <= 0 || mh <= 0) return;

    // ~40x30 unit area
    float view_w = 40.0f;
    float view_h = view_w * (static_cast<float>(mh) / static_cast<float>(mw)) * 2.0f;
    float view_left = view_cx_ - view_w / 2.0f;
    float view_top = view_cy_ - view_h / 2.0f;

    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        const auto& sys = nav_->systems[i];
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;

        bool is_cursor = (static_cast<int>(i) == cursor_index_);

        if (sys.id == nav_->current_system_id) {
            map_ctx.put(sx, sy, '@', Color::Green);
            if (sy + 1 < mh) {
                map_ctx.text(sx - 3, sy + 1, "(YOU)", Color::Green);
            }
        } else if (is_cursor) {
            char glyph = sys.discovered ? '*' : '?';
            map_ctx.put(sx, sy, glyph, Color::White);
            // Cursor brackets
            if (sx > 0) map_ctx.put(sx - 1, sy, '[', Color::White);
            if (sx < mw - 1) map_ctx.put(sx + 1, sy, ']', Color::White);
        } else if (sys.discovered) {
            map_ctx.put(sx, sy, '*', star_class_color(sys.star_class));
            // Name label
            int name_len = static_cast<int>(sys.name.size());
            int nx = sx - name_len / 2;
            if (sy + 1 < mh && nx >= 0 && nx + name_len < mw) {
                map_ctx.text(nx, sy + 1, sys.name, Color::DarkGray);
            }
        } else {
            map_ctx.put(sx, sy, '.', Color::DarkGray);
        }
    }

    // Crosshair reticle at viewport center when no cursor selected
    if (cursor_index_ < 0) {
        int cx = mw / 2;
        int cy = mh / 2;
        if (cx > 0)      map_ctx.put(cx - 1, cy, '-', Color::DarkGray);
        if (cx < mw - 1) map_ctx.put(cx + 1, cy, '-', Color::DarkGray);
        if (cy > 0)      map_ctx.put(cx, cy - 1, '|', Color::DarkGray);
        if (cy < mh - 1) map_ctx.put(cx, cy + 1, '|', Color::DarkGray);
        map_ctx.put(cx, cy, '+', Color::DarkGray);
    }

    // Info panel
    info_ctx.text(1, 0, "REGION VIEW", Color::White);
    info_ctx.hline(1, '~');

    int y = 3;
    // Count visible systems
    int visible_count = 0;
    int visible_discovered = 0;
    for (const auto& sys : nav_->systems) {
        float dx = std::abs(sys.gx - view_cx_);
        float dy = std::abs(sys.gy - view_cy_);
        if (dx <= view_w / 2 && dy <= view_h / 2) {
            ++visible_count;
            if (sys.discovered) ++visible_discovered;
        }
    }
    info_ctx.text(1, y++, "Systems: " + std::to_string(visible_count), Color::DarkGray);
    info_ctx.text(1, y++, "Known: " + std::to_string(visible_discovered), Color::Cyan);
    y++;

    if (cursor_index_ >= 0 && cursor_index_ < static_cast<int>(nav_->systems.size())) {
        const auto& sys = nav_->systems[cursor_index_];
        if (sys.discovered) {
            draw_system_info(info_ctx, sys, y);
        } else {
            info_ctx.text(1, y++, "UNKNOWN SYSTEM", Color::DarkGray);
            info_ctx.text(1, y + 1, "Scan or visit", Color::DarkGray);
            info_ctx.text(1, y + 2, "to reveal", Color::DarkGray);
        }
    } else {
        info_ctx.text(1, y, "Use [Tab] to", Color::DarkGray);
        info_ctx.text(1, y + 1, "select a system", Color::DarkGray);
    }
}

// ---------------------------------------------------------------------------
// Local view
// ---------------------------------------------------------------------------

void StarChartViewer::draw_local_view(DrawContext& map_ctx, DrawContext& info_ctx) {
    int mw = map_ctx.width();
    int mh = map_ctx.height();
    if (mw <= 0 || mh <= 0) return;

    // ~15x12 unit area
    float view_w = 15.0f;
    float view_h = view_w * (static_cast<float>(mh) / static_cast<float>(mw)) * 2.0f;
    float view_left = view_cx_ - view_w / 2.0f;
    float view_top = view_cy_ - view_h / 2.0f;

    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        const auto& sys = nav_->systems[i];
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;

        bool is_cursor = (static_cast<int>(i) == cursor_index_);

        if (sys.id == nav_->current_system_id) {
            map_ctx.put(sx, sy, '@', Color::Green);
            if (sy + 1 < mh) {
                int lx = sx - static_cast<int>(sys.name.size()) / 2;
                if (lx < 0) lx = 0;
                map_ctx.text(lx, sy + 1, sys.name, Color::Green);
            }
        } else if (is_cursor) {
            char glyph = sys.discovered ? '*' : '?';
            Color c = sys.discovered ? star_class_color(sys.star_class) : Color::White;
            map_ctx.put(sx, sy, glyph, c);
            // Cursor brackets
            if (sx > 0) map_ctx.put(sx - 1, sy, '[', Color::White);
            if (sx < mw - 1) map_ctx.put(sx + 1, sy, ']', Color::White);
            // Name below if discovered
            if (sys.discovered && sy + 1 < mh) {
                int lx = sx - static_cast<int>(sys.name.size()) / 2;
                if (lx < 0) lx = 0;
                map_ctx.text(lx, sy + 1, sys.name, star_class_color(sys.star_class));
            }
        } else if (sys.discovered) {
            map_ctx.put(sx, sy, '*', star_class_color(sys.star_class));
            if (sy + 1 < mh) {
                int lx = sx - static_cast<int>(sys.name.size()) / 2;
                if (lx < 0) lx = 0;
                map_ctx.text(lx, sy + 1, sys.name, Color::DarkGray);
            }
        } else {
            map_ctx.put(sx, sy, '.', Color::DarkGray);
        }
    }

    // Info panel
    int y = 0;
    if (cursor_index_ >= 0 && cursor_index_ < static_cast<int>(nav_->systems.size())) {
        auto& sys = nav_->systems[cursor_index_];
        if (sys.discovered) {
            generate_system_bodies(sys);
            draw_system_info(info_ctx, sys, y, info_ctx.height());
        } else {
            info_ctx.text(1, y++, "UNKNOWN SYSTEM", Color::DarkGray);
            info_ctx.hline(y++, '~');
            y++;
            info_ctx.text(1, y++, "Scan or visit", Color::DarkGray);
            info_ctx.text(1, y++, "to reveal", Color::DarkGray);
        }
    }
}

// ---------------------------------------------------------------------------
// System view — orbital diagram with body selection
// ---------------------------------------------------------------------------

void StarChartViewer::draw_system_view(DrawContext& map_ctx, DrawContext& info_ctx) {
    if (cursor_index_ < 0 || cursor_index_ >= static_cast<int>(nav_->systems.size())) return;
    auto& sys = nav_->systems[cursor_index_];
    generate_system_bodies(sys);

    int mw = map_ctx.width();
    int mh = map_ctx.height();
    if (mw <= 0 || mh <= 0) return;

    // Star sits on the left; bodies spread across the full width
    int cy = mh / 2;
    int star_x = 2;
    Color star_color = star_class_color(sys.star_class);
    map_ctx.put(star_x, cy, '*', star_color);
    if (star_x > 0) map_ctx.put(star_x - 1, cy, '*', star_color);
    if (star_x + 1 < mw) map_ctx.put(star_x + 1, cy, '*', star_color);

    // System name above star
    {
        int lx = std::max(0, star_x - static_cast<int>(sys.name.size()) / 2);
        if (cy - 2 >= 0) map_ctx.text(lx, cy - 2, sys.name, star_color);
    }

    // Draw bodies across the width using sqrt scale for spacing
    if (!sys.bodies.empty()) {
        float max_dist = sys.bodies.back().orbital_distance;
        if (max_dist < 1.0f) max_dist = 1.0f;
        float sqrt_max = std::sqrt(max_dist);

        // Bodies span from star_x+5 to mw-3
        int orbit_left = star_x + 5;
        int orbit_right = mw - 3;
        int orbit_span = orbit_right - orbit_left;
        if (orbit_span < 4) orbit_span = 4;

        for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
            const auto& body = sys.bodies[i];
            // sqrt scale so inner planets get more room
            float t = std::sqrt(body.orbital_distance) / sqrt_max;
            int bx = orbit_left + static_cast<int>(t * orbit_span);
            if (bx < 0 || bx >= mw) continue;

            bool selected = (i == body_cursor_);
            char glyph = body_type_glyph(body.type);
            Color color = body_type_color(body.type);

            // Draw orbital track marker
            if (cy < mh) map_ctx.put(bx, cy, '.', Color::DarkGray);

            // Draw body above the track
            int by = cy - 1;
            if (by < 0) by = 0;

            if (selected) {
                map_ctx.put(bx, by, glyph, Color::White);
                if (bx > 0) map_ctx.put(bx - 1, by, '[', Color::White);
                if (bx < mw - 1) map_ctx.put(bx + 1, by, ']', Color::White);
            } else {
                map_ctx.put(bx, by, glyph, color);
            }

            // Name below track (staggered to avoid overlap)
            int name_y = cy + 1 + (i % 2);
            if (name_y < mh) {
                std::string label = body.name;
                // Truncate long names
                if (static_cast<int>(label.size()) > 12) {
                    label = label.substr(0, 11) + ".";
                }
                int lx = bx - static_cast<int>(label.size()) / 2;
                if (lx < 0) lx = 0;
                if (lx + static_cast<int>(label.size()) >= mw) {
                    lx = mw - static_cast<int>(label.size()) - 1;
                }
                map_ctx.text(lx, name_y, label, selected ? Color::White : Color::DarkGray);
            }
        }
    }

    // Info panel — show selected body details or system overview
    int y = 0;
    info_ctx.text(1, y++, sys.name, Color::White);
    info_ctx.hline(y++, '~');
    y++;

    info_ctx.text(1, y, "Class", Color::DarkGray);
    info_ctx.text(10, y++, star_class_name(sys.star_class), star_class_color(sys.star_class));
    info_ctx.text(1, y, "Bodies", Color::DarkGray);
    info_ctx.text(10, y++, std::to_string(sys.bodies.size()), Color::White);
    y++;

    if (body_cursor_ >= 0 && body_cursor_ < static_cast<int>(sys.bodies.size())) {
        draw_body_info(info_ctx, sys.bodies[body_cursor_], sys, y);
    }
}

void StarChartViewer::draw_body_info(DrawContext& ctx, const CelestialBody& body,
                                      const StarSystem& sys, int start_y) {
    int y = start_y;
    int pw = ctx.width();

    // Body name with glyph
    char glyph = body_type_glyph(body.type);
    Color color = body_type_color(body.type);
    ctx.put(1, y, glyph, color);
    ctx.text(3, y++, body.name, Color::White);
    ctx.hline(y++, '-');
    y++;

    ctx.text(1, y, "Type", Color::DarkGray);
    ctx.text(10, y++, body_type_name(body.type), color);

    ctx.text(1, y, "Size", Color::DarkGray);
    {
        std::string size_bar;
        for (int i = 0; i < 10; ++i) size_bar += (i < body.size) ? '=' : '-';
        ctx.text(10, y++, size_bar, Color::White);
    }

    ctx.text(1, y, "Atmo", Color::DarkGray);
    ctx.text(10, y++, atmosphere_name(body.atmosphere), Color::White);

    ctx.text(1, y, "Temp", Color::DarkGray);
    {
        Color temp_color = Color::White;
        switch (body.temperature) {
            case Temperature::Frozen:    temp_color = Color::Cyan; break;
            case Temperature::Cold:      temp_color = Color::Blue; break;
            case Temperature::Temperate: temp_color = Color::Green; break;
            case Temperature::Hot:       temp_color = static_cast<Color>(208); break;
            case Temperature::Scorching: temp_color = Color::Red; break;
        }
        ctx.text(10, y++, temperature_name(body.temperature), temp_color);
    }

    if (body.moons > 0) {
        ctx.text(1, y, "Moons", Color::DarkGray);
        ctx.text(10, y++, std::to_string(body.moons), Color::White);
    }

    // Orbital distance
    {
        int d10 = static_cast<int>(body.orbital_distance * 10.0f);
        std::string dist_str = std::to_string(d10 / 10) + "." + std::to_string(d10 % 10) + " AU";
        ctx.text(1, y, "Orbit", Color::DarkGray);
        ctx.text(10, y++, dist_str, Color::White);
    }

    y++;

    // Landable
    if (body.landable) {
        ctx.text(1, y++, "LANDABLE", Color::Green);
    } else {
        ctx.text(1, y++, "Not landable", Color::DarkGray);
    }

    if (body.has_dungeon) {
        ctx.text(1, y++, "Dungeon detected", Color::Yellow);
    }

    // Danger
    y++;
    ctx.text(1, y, "Danger", Color::DarkGray);
    {
        std::string danger_bar;
        for (int i = 0; i < 10; ++i) danger_bar += (i < body.danger_level) ? '=' : '-';
        Color dc = Color::Green;
        if (body.danger_level >= 7) dc = Color::Red;
        else if (body.danger_level >= 4) dc = Color::Yellow;
        ctx.text(10, y++, danger_bar, dc);
    }

    // Resources
    y++;
    if (body.resources != 0) {
        ctx.text(1, y++, "RESOURCES", Color::White);
        ctx.hline(y++, '-');
        if (has_resource(body.resources, Resource::Metals))     ctx.text(2, y++, "Metals", Color::White);
        if (has_resource(body.resources, Resource::RareMetals)) ctx.text(2, y++, "Rare Metals", Color::BrightMagenta);
        if (has_resource(body.resources, Resource::Water))      ctx.text(2, y++, "Water", Color::Blue);
        if (has_resource(body.resources, Resource::Fuel))       ctx.text(2, y++, "Fuel", Color::Yellow);
        if (has_resource(body.resources, Resource::Organics))   ctx.text(2, y++, "Organics", Color::Green);
        if (has_resource(body.resources, Resource::Crystals))   ctx.text(2, y++, "Crystals", Color::Cyan);
        if (has_resource(body.resources, Resource::Radioactive))ctx.text(2, y++, "Radioactive", Color::Red);
        if (has_resource(body.resources, Resource::Gas))        ctx.text(2, y++, "Gas", static_cast<Color>(208));
    }
}

// ---------------------------------------------------------------------------
// System info panel
// ---------------------------------------------------------------------------

void StarChartViewer::draw_system_info(DrawContext& ctx, const StarSystem& sys, int start_y, int max_h) {
    int y = start_y;
    ctx.text(1, y++, sys.name, Color::White);
    ctx.hline(y++, '~');
    y++;

    ctx.text(1, y, "Class", Color::DarkGray);
    ctx.text(10, y++, star_class_name(sys.star_class), star_class_color(sys.star_class));

    ctx.text(1, y, "Type", Color::DarkGray);
    ctx.text(10, y++, sys.binary ? "Binary system" : "Single star", Color::White);

    if (sys.has_station) {
        ctx.text(1, y, "Station", Color::DarkGray);
        ctx.text(10, y++, "Yes", Color::Cyan);
    } else {
        ctx.text(1, y, "Station", Color::DarkGray);
        ctx.text(10, y++, "None", Color::DarkGray);
    }

    ctx.text(1, y, "Planets", Color::DarkGray);
    ctx.text(10, y++, std::to_string(sys.planet_count), Color::White);

    if (sys.asteroid_belts > 0) {
        ctx.text(1, y, "Belts", Color::DarkGray);
        ctx.text(10, y++, std::to_string(sys.asteroid_belts), Color::White);
    }

    y++;
    // Danger bar
    ctx.text(1, y, "Danger", Color::DarkGray);
    // Draw danger as a simple bar
    std::string danger_bar;
    for (int i = 0; i < 10; ++i) {
        danger_bar += (i < sys.danger_level) ? '=' : '-';
    }
    danger_bar += " " + std::to_string(sys.danger_level) + "/10";
    Color danger_color = Color::Green;
    if (sys.danger_level >= 7) danger_color = Color::Red;
    else if (sys.danger_level >= 4) danger_color = Color::Yellow;
    ctx.text(10, y++, danger_bar, danger_color);

    // Distance from current system
    y++;
    for (const auto& s : nav_->systems) {
        if (s.id == nav_->current_system_id) {
            float dist = system_distance(sys, s);
            // Format to 1 decimal
            int d10 = static_cast<int>(dist * 10.0f);
            std::string dist_str = std::to_string(d10 / 10) + "." + std::to_string(d10 % 10) + " ly";
            ctx.text(1, y, "Dist", Color::DarkGray);
            ctx.text(10, y++, dist_str, Color::White);
            break;
        }
    }

    // Celestial bodies list
    if (!sys.bodies.empty() && y + 2 < max_h) {
        y++;
        ctx.text(1, y++, "BODIES", Color::White);
        ctx.hline(y++, '-');

        int panel_w = ctx.width();
        for (const auto& body : sys.bodies) {
            if (y >= max_h) break;

            char glyph = body_type_glyph(body.type);
            Color color = body_type_color(body.type);
            ctx.put(1, y, glyph, color);

            // Name (truncated to fit)
            int name_max = panel_w - 4;
            std::string display_name = body.name;
            if (static_cast<int>(display_name.size()) > name_max) {
                display_name = display_name.substr(0, name_max - 1) + ".";
            }

            // Append landable marker
            if (body.landable && static_cast<int>(display_name.size()) + 4 <= name_max) {
                display_name += " [L]";
            }

            ctx.text(3, y, display_name, color);
            ++y;
        }
    }
}

} // namespace astra
