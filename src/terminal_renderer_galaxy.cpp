#include "astra/terminal_renderer.h"
#include "astra/galaxy_map_desc.h"
#include "astra/star_chart_viewer.h"  // for ChartZoom enum
#include "astra/ui.h"
#include "astra/celestial_body.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// File-local projection helpers
// ---------------------------------------------------------------------------

static int to_screen_x(float gx, float view_left, float view_width, int sw) {
    return static_cast<int>((gx - view_left) / view_width * static_cast<float>(sw));
}

static int to_screen_y(float gy, float view_top, float view_height, int sh) {
    return static_cast<int>((gy - view_top) / view_height * static_cast<float>(sh));
}

// ---------------------------------------------------------------------------
// File-local quest helpers
// ---------------------------------------------------------------------------

static bool is_quest_system(const GalaxyMapDesc& desc, uint32_t sys_id) {
    for (auto id : desc.quest_system_ids)
        if (id == sys_id) return true;
    return false;
}

static bool is_quest_body(const GalaxyMapDesc& desc, int body_index) {
    for (const auto& qt : desc.quest_body_targets)
        if (qt.body_index == body_index) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Galaxy zoom
// ---------------------------------------------------------------------------

static void render_galaxy_zoom(DrawContext& ctx, const GalaxyMapDesc& desc) {
    int mw = ctx.width();
    int mh = ctx.height();
    if (mw <= 0 || mh <= 0) return;

    float view_w = 440.0f;
    float view_h = view_w * (static_cast<float>(mh) / static_cast<float>(mw)) * 2.0f;
    float view_left = desc.view_cx - view_w / 2.0f;
    float view_top = desc.view_cy - view_h / 2.0f;

    // Draw all systems
    for (size_t i = 0; i < desc.systems.size(); ++i) {
        const auto& sys = desc.systems[i];
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;

        if (sys.id == 0) {
            // Sgr A*
            ctx.put(sx, sy, '+', Color::BrightMagenta);
        } else if (sys.id == 1) {
            // Sol
            ctx.put(sx, sy, '*', Color::Yellow);
        } else if (desc.player_system_index >= 0 &&
                   static_cast<int>(i) == desc.player_system_index) {
            ctx.put(sx, sy, '@', Color::Green);
        } else if (sys.discovered) {
            ctx.put(sx, sy, '.', star_class_color(sys.star_class));
        } else {
            ctx.put(sx, sy, '.', Color::DarkGray);
        }
    }

    // Quest target markers
    for (const auto& sys : desc.systems) {
        if (!is_quest_system(desc, sys.id)) continue;
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;
        if (sx + 1 < mw) ctx.put(sx + 1, sy, '!', Color::BrightYellow);
    }

    // Highlight system marker
    if (desc.highlight_system_index >= 0 &&
        desc.highlight_system_index < static_cast<int>(desc.systems.size())) {
        const auto& hs = desc.systems[desc.highlight_system_index];
        int sx = to_screen_x(hs.gx, view_left, view_w, mw);
        int sy = to_screen_y(hs.gy, view_top, view_h, mh);
        if (sx >= 0 && sx < mw && sy >= 0 && sy < mh) {
            ctx.put(sx, sy, '*', Color::BrightYellow);
        }
    }

    // Arm labels
    for (const auto& label : desc.arm_labels) {
        int lx = to_screen_x(label.gx, view_left, view_w, mw);
        int ly = to_screen_y(label.gy, view_top, view_h, mh);
        if (lx >= 0 && lx < mw - 6 && ly >= 0 && ly < mh) {
            ctx.text(lx, ly, label.name, Color::DarkGray);
        }
    }

    // Sgr A* label
    {
        int cx = to_screen_x(0.0f, view_left, view_w, mw);
        int cy = to_screen_y(0.0f, view_top, view_h, mh);
        if (cx >= 0 && cx < mw - 6 && cy >= 1 && cy < mh) {
            ctx.text(cx - 2, cy - 1, "Sgr A*", Color::BrightMagenta);
        }
    }

    // Sol label
    {
        int sx = to_screen_x(180.0f, view_left, view_w, mw);
        int sy = to_screen_y(0.0f, view_top, view_h, mh);
        if (sx >= 0 && sx < mw - 3 && sy < mh - 1) {
            ctx.text(sx - 1, sy + 1, "Sol", Color::Yellow);
        }
    }

    // Crosshair reticle at viewport center
    {
        int cx = mw / 2;
        int cy = mh / 2;
        if (cx > 1)      ctx.put(cx - 2, cy, '-', Color::White);
        if (cx > 0)      ctx.put(cx - 1, cy, '-', Color::White);
        if (cx < mw - 1) ctx.put(cx + 1, cy, '-', Color::White);
        if (cx < mw - 2) ctx.put(cx + 2, cy, '-', Color::White);
        if (cy > 0)      ctx.put(cx, cy - 1, '|', Color::White);
        if (cy < mh - 1) ctx.put(cx, cy + 1, '|', Color::White);
        ctx.put(cx, cy, '+', Color::White);
    }
}

// ---------------------------------------------------------------------------
// Region zoom
// ---------------------------------------------------------------------------

static void render_region_zoom(DrawContext& ctx, const GalaxyMapDesc& desc) {
    int mw = ctx.width();
    int mh = ctx.height();
    if (mw <= 0 || mh <= 0) return;

    float view_w = 40.0f;
    float view_h = view_w * (static_cast<float>(mh) / static_cast<float>(mw)) * 2.0f;
    float view_left = desc.view_cx - view_w / 2.0f;
    float view_top = desc.view_cy - view_h / 2.0f;

    for (size_t i = 0; i < desc.systems.size(); ++i) {
        const auto& sys = desc.systems[i];
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;

        bool is_player = (desc.player_system_index >= 0 &&
                          static_cast<int>(i) == desc.player_system_index);
        bool is_cursor = (static_cast<int>(i) == desc.cursor_system_index);

        if (is_player) {
            ctx.put(sx, sy, '@', Color::Green);
            if (sy + 1 < mh) {
                ctx.text(sx - 3, sy + 1, "(YOU)", Color::Green);
            }
        } else if (is_cursor) {
            char glyph = sys.discovered ? '*' : '?';
            ctx.put(sx, sy, glyph, Color::White);
            if (sx > 0) ctx.put(sx - 1, sy, '[', Color::White);
            if (sx < mw - 1) ctx.put(sx + 1, sy, ']', Color::White);
        } else if (sys.discovered) {
            ctx.put(sx, sy, '*', star_class_color(sys.star_class));
            int name_len = static_cast<int>(sys.name.size());
            int nx = sx - name_len / 2;
            if (sy + 1 < mh && nx >= 0 && nx + name_len < mw) {
                ctx.text(nx, sy + 1, sys.name, Color::DarkGray);
            }
        } else {
            ctx.put(sx, sy, '.', Color::DarkGray);
        }
    }

    // Quest target markers
    for (const auto& sys : desc.systems) {
        if (!is_quest_system(desc, sys.id)) continue;
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;
        if (sy > 0) ctx.put(sx, sy - 1, '!', Color::BrightYellow);
    }

    // Highlight system marker
    if (desc.highlight_system_index >= 0 &&
        desc.highlight_system_index != desc.cursor_system_index &&
        desc.highlight_system_index < static_cast<int>(desc.systems.size())) {
        const auto& hs = desc.systems[desc.highlight_system_index];
        int sx = to_screen_x(hs.gx, view_left, view_w, mw);
        int sy = to_screen_y(hs.gy, view_top, view_h, mh);
        if (sx >= 0 && sx < mw && sy >= 0 && sy < mh) {
            ctx.put(sx, sy, '*', Color::BrightYellow);
        }
    }

    // Crosshair when no cursor selected
    if (desc.cursor_system_index < 0) {
        int cx = mw / 2;
        int cy = mh / 2;
        if (cx > 0)      ctx.put(cx - 1, cy, '-', Color::DarkGray);
        if (cx < mw - 1) ctx.put(cx + 1, cy, '-', Color::DarkGray);
        if (cy > 0)      ctx.put(cx, cy - 1, '|', Color::DarkGray);
        if (cy < mh - 1) ctx.put(cx, cy + 1, '|', Color::DarkGray);
        ctx.put(cx, cy, '+', Color::DarkGray);
    }
}

// ---------------------------------------------------------------------------
// Local zoom
// ---------------------------------------------------------------------------

static void render_local_zoom(DrawContext& ctx, const GalaxyMapDesc& desc) {
    int mw = ctx.width();
    int mh = ctx.height();
    if (mw <= 0 || mh <= 0) return;

    float view_w = 15.0f;
    float view_h = view_w * (static_cast<float>(mh) / static_cast<float>(mw)) * 2.0f;
    float view_left = desc.view_cx - view_w / 2.0f;
    float view_top = desc.view_cy - view_h / 2.0f;

    for (size_t i = 0; i < desc.systems.size(); ++i) {
        const auto& sys = desc.systems[i];
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;

        bool is_player = (desc.player_system_index >= 0 &&
                          static_cast<int>(i) == desc.player_system_index);
        bool is_cursor = (static_cast<int>(i) == desc.cursor_system_index);

        if (is_player) {
            ctx.put(sx, sy, '@', Color::Green);
            if (sy + 1 < mh) {
                int lx = sx - static_cast<int>(sys.name.size()) / 2;
                if (lx < 0) lx = 0;
                ctx.text(lx, sy + 1, sys.name, Color::Green);
            }
        } else if (is_cursor) {
            char glyph = sys.discovered ? '*' : '?';
            Color c = sys.discovered ? star_class_color(sys.star_class) : Color::White;
            ctx.put(sx, sy, glyph, c);
            if (sx > 0) ctx.put(sx - 1, sy, '[', Color::White);
            if (sx < mw - 1) ctx.put(sx + 1, sy, ']', Color::White);
            if (sys.discovered && sy + 1 < mh) {
                int lx = sx - static_cast<int>(sys.name.size()) / 2;
                if (lx < 0) lx = 0;
                ctx.text(lx, sy + 1, sys.name, star_class_color(sys.star_class));
            }
        } else if (sys.discovered) {
            ctx.put(sx, sy, '*', star_class_color(sys.star_class));
            if (sy + 1 < mh) {
                int lx = sx - static_cast<int>(sys.name.size()) / 2;
                if (lx < 0) lx = 0;
                ctx.text(lx, sy + 1, sys.name, Color::DarkGray);
            }
        } else {
            ctx.put(sx, sy, '.', Color::DarkGray);
        }
    }

    // Quest target markers
    for (const auto& sys : desc.systems) {
        if (!is_quest_system(desc, sys.id)) continue;
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;
        if (sy > 0) ctx.put(sx, sy - 1, '!', Color::BrightYellow);
    }

    // Highlight system marker
    if (desc.highlight_system_index >= 0 &&
        desc.highlight_system_index != desc.cursor_system_index &&
        desc.highlight_system_index < static_cast<int>(desc.systems.size())) {
        const auto& hs = desc.systems[desc.highlight_system_index];
        int sx = to_screen_x(hs.gx, view_left, view_w, mw);
        int sy = to_screen_y(hs.gy, view_top, view_h, mh);
        if (sx >= 0 && sx < mw && sy >= 0 && sy < mh) {
            ctx.put(sx, sy, '*', Color::BrightYellow);
        }
    }
}

// ---------------------------------------------------------------------------
// System zoom — orbital diagram with body selection
// ---------------------------------------------------------------------------

static void render_system_zoom(DrawContext& ctx, const GalaxyMapDesc& desc) {
    if (desc.cursor_system_index < 0 ||
        desc.cursor_system_index >= static_cast<int>(desc.systems.size())) return;
    const auto& sys = desc.systems[desc.cursor_system_index];

    int mw = ctx.width();
    int mh = ctx.height();
    if (mw <= 0 || mh <= 0) return;

    // Star sits on the left; bodies spread across the full width
    int cy = mh / 2;
    int star_x = 2;
    Color star_color = star_class_color(sys.star_class);
    ctx.put(star_x, cy, '*', star_color);
    if (star_x > 0) ctx.put(star_x - 1, cy, '*', star_color);
    if (star_x + 1 < mw) ctx.put(star_x + 1, cy, '*', star_color);

    // System name above star
    {
        int lx = std::max(0, star_x - static_cast<int>(sys.name.size()) / 2);
        if (cy - 2 >= 0) ctx.text(lx, cy - 2, sys.name, star_color);
    }

    // Draw bodies across the width using log scale for spacing
    if (!sys.bodies.empty()) {
        float max_dist = sys.bodies.back().orbital_distance;
        if (max_dist < 1.0f) max_dist = 1.0f;
        float log_max = std::log(1.0f + max_dist);

        int orbit_left = star_x + 5;
        int orbit_right = mw - 14;
        int orbit_span = orbit_right - orbit_left;
        if (orbit_span < 4) orbit_span = 4;

        int station_host = desc.station_host_body_index;

        // --- Pass 1: compute screen x for each body ---
        std::vector<int> body_sx(sys.bodies.size());
        for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
            float t = std::log(1.0f + sys.bodies[i].orbital_distance) / log_max;
            body_sx[i] = orbit_left + static_cast<int>(t * orbit_span);
        }

        // --- Pass 2: compute max label width per body based on gap to next ---
        std::vector<int> max_label(sys.bodies.size(), 12);
        for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
            if (i + 1 < static_cast<int>(sys.bodies.size())) {
                int gap = body_sx[i + 1] - body_sx[i];
                max_label[i] = std::max(3, gap - 1);
                if (max_label[i] > 12) max_label[i] = 12;
            }
        }

        // --- Pass 3: draw everything ---
        for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
            const auto& body = sys.bodies[i];
            int bx = body_sx[i];
            if (bx < 0 || bx >= mw) continue;

            bool is_cursor = (i == desc.body_cursor);
            bool body_selected = is_cursor && desc.sub_cursor == -1;
            char glyph = body_type_glyph(body.type);
            Color color = body_type_color(body.type);

            // --- Station above (if this body hosts it) ---
            if (i == station_host) {
                bool st_sel = is_cursor && desc.sub_cursor == 0;
                char st_glyph = sys.station.derelict ? '#' : 'H';
                Color st_color = sys.station.derelict ? Color::DarkGray : Color::Cyan;

                int st_y = cy - 3;
                if (st_y >= 0) {
                    if (st_sel) {
                        ctx.put(bx, st_y, st_glyph, Color::White);
                        if (bx > 0) ctx.put(bx - 1, st_y, '[', Color::White);
                        if (bx < mw - 1) ctx.put(bx + 1, st_y, ']', Color::White);
                    } else {
                        ctx.put(bx, st_y, st_glyph, st_color);
                    }
                }

                if (st_y - 1 >= 0) {
                    std::string slabel = sys.station.name;
                    if (slabel.size() > 20) slabel = slabel.substr(0, 19) + ".";
                    int lx = bx - static_cast<int>(slabel.size()) / 2;
                    if (lx < 0) lx = 0;
                    ctx.text(lx, st_y - 1, slabel,
                             st_sel ? Color::White : st_color);
                }

                for (int ly = st_y + 1; ly < cy - 1; ++ly) {
                    if (ly >= 0 && ly < mh)
                        ctx.put(bx, ly, ':', Color::DarkGray);
                }
            }

            // --- Body glyph (one row above track) ---
            int by = cy - 1;
            if (by >= 0 && by < mh) {
                if (body_selected) {
                    ctx.put(bx, by, glyph, Color::White);
                    if (bx > 0) ctx.put(bx - 1, by, '[', Color::White);
                    if (bx < mw - 1) ctx.put(bx + 1, by, ']', Color::White);
                } else {
                    ctx.put(bx, by, glyph, color);
                }
            }

            // --- Quest target marker above body ---
            if (is_quest_body(desc, i)) {
                int qy = cy - 2;
                if (qy >= 0 && qy < mh)
                    ctx.put(bx, qy, '!', Color::BrightYellow);
            }

            // --- Orbital track dot ---
            if (cy >= 0 && cy < mh) ctx.put(bx, cy, '.', Color::DarkGray);

            // --- Body name (cy+1) — truncated; selected body drawn later on top ---
            if (cy + 1 < mh && !body_selected) {
                std::string label = body.name;
                int mlw = max_label[i];
                if (static_cast<int>(label.size()) > mlw)
                    label = label.substr(0, mlw - 1) + ".";
                int lx = bx - static_cast<int>(label.size()) / 2;
                if (lx < 0) lx = 0;
                if (lx + static_cast<int>(label.size()) >= mw)
                    lx = mw - static_cast<int>(label.size()) - 1;
                ctx.text(lx, cy + 1, label, Color::DarkGray);
            }

            // --- Moons below: colon on cy+2, then alternating moon/colon ---
            if (body.moons > 0) {
                int next_bx = mw;
                if (i + 1 < static_cast<int>(sys.bodies.size()))
                    next_bx = body_sx[i + 1];
                int moon_label_max = next_bx - bx - 3;
                if (moon_label_max < 3) moon_label_max = 3;

                int row = cy + 2;
                for (int m = 0; m < body.moons; ++m) {
                    if (row >= mh) break;
                    ctx.put(bx, row, ':', Color::DarkGray);
                    ++row;

                    if (row >= mh) break;
                    bool moon_sel = is_cursor && desc.sub_cursor == m + 1;
                    if (moon_sel) {
                        ctx.put(bx, row, '*', Color::White);
                        if (bx > 0) ctx.put(bx - 1, row, '[', Color::White);
                        if (bx < mw - 1) ctx.put(bx + 1, row, ']', Color::White);
                    } else {
                        ctx.put(bx, row, '*', Color::DarkGray);
                    }

                    std::string mlabel = (m < static_cast<int>(body.moon_names.size()))
                                         ? body.moon_names[m]
                                         : "Moon " + std::to_string(m + 1);
                    if (!moon_sel && static_cast<int>(mlabel.size()) > moon_label_max)
                        mlabel = mlabel.substr(0, moon_label_max - 1) + ".";
                    int mlx = bx + 2;
                    if (mlx + static_cast<int>(mlabel.size()) < mw) {
                        ctx.text(mlx, row, mlabel,
                                 moon_sel ? Color::White : Color::DarkGray);
                    }
                    ++row;
                }
            }
        }

        // --- Draw selected item's full name on top, padded with spaces ---
        if (desc.body_cursor >= 0 &&
            desc.body_cursor < static_cast<int>(sys.bodies.size())) {
            int sel_bx = body_sx[desc.body_cursor];
            const auto& sel_body = sys.bodies[desc.body_cursor];

            if (desc.sub_cursor == -1 && cy + 1 < mh && sel_bx >= 0 && sel_bx < mw) {
                std::string label = " " + sel_body.name + " ";
                if (static_cast<int>(label.size()) > 14)
                    label = " " + sel_body.name.substr(0, 11) + ". ";
                int lx = sel_bx - static_cast<int>(label.size()) / 2;
                if (lx < 0) lx = 0;
                if (lx + static_cast<int>(label.size()) >= mw)
                    lx = mw - static_cast<int>(label.size()) - 1;
                ctx.text(lx, cy + 1, label, Color::White);
            } else if (desc.sub_cursor >= 1 && sel_bx >= 0 && sel_bx < mw) {
                int m = desc.sub_cursor - 1;
                int moon_row = cy + 2 + desc.sub_cursor * 2 - 1;
                if (moon_row < mh) {
                    std::string mlabel = (m < static_cast<int>(sel_body.moon_names.size()))
                                         ? sel_body.moon_names[m]
                                         : "Moon " + std::to_string(m + 1);
                    std::string padded = " " + mlabel + " ";
                    int mlx = sel_bx + 2;
                    if (mlx - 1 >= 0 && mlx + static_cast<int>(padded.size()) - 1 < mw) {
                        ctx.text(mlx - 1, moon_row, padded, Color::White);
                    }
                }
            }
        }

        // --- Draw player location marker '@' ---
        if (desc.cursor_system_index == desc.player_system_index) {
            int station_host_idx = desc.station_host_body_index;
            if (desc.at_station && sys.has_station && station_host_idx >= 0 &&
                station_host_idx < static_cast<int>(body_sx.size())) {
                int px = body_sx[station_host_idx];
                int py = cy - 3;
                if (px + 2 < mw && py >= 0)
                    ctx.put(px + 2, py, '@', Color::Green);
            } else if (desc.player_body_index >= 0 &&
                       desc.player_body_index < static_cast<int>(body_sx.size())) {
                int px = body_sx[desc.player_body_index];
                if (desc.player_moon_index >= 0) {
                    int py = cy + 3 + desc.player_moon_index * 2;
                    const auto& body = sys.bodies[desc.player_body_index];
                    int mi = desc.player_moon_index;
                    std::string mname = (mi < static_cast<int>(body.moon_names.size()))
                                        ? body.moon_names[mi]
                                        : "Moon " + std::to_string(mi + 1);
                    int ax = px + 2 + static_cast<int>(mname.size()) + 1;
                    if (ax < mw && py >= 0 && py < mh)
                        ctx.put(ax, py, '@', Color::Green);
                } else {
                    int py = cy - 1;
                    if (px + 2 < mw && py >= 0)
                        ctx.put(px + 2, py, '@', Color::Green);
                }
            } else if (desc.on_ship && !body_sx.empty()) {
                int px = body_sx[0];
                int py = cy - 1;
                if (px + 2 < mw && py >= 0)
                    ctx.put(px + 2, py, '@', Color::Green);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_galaxy_map(const Rect& bounds, const GalaxyMapDesc& desc) {
    DrawContext ctx(this, bounds);
    switch (desc.zoom) {
        case ChartZoom::Galaxy: render_galaxy_zoom(ctx, desc); break;
        case ChartZoom::Region: render_region_zoom(ctx, desc); break;
        case ChartZoom::Local:  render_local_zoom(ctx, desc);  break;
        case ChartZoom::System: render_system_zoom(ctx, desc); break;
    }
}

} // namespace astra
