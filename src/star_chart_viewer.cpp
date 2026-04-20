#include "astra/star_chart_viewer.h"

#include "astra/celestial_body.h"
#include "astra/galaxy_map_desc.h"
#include "astra/poi_budget.h"
#include "astra/ui_types.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace astra {

// ---------------------------------------------------------------------------
// Arm labels for galaxy view
// ---------------------------------------------------------------------------

static const GVArmLabel arm_labels[] = {
    {"Sagittarius",  140.0f,  -30.0f},
    {"Perseus",      -30.0f,  140.0f},
    {"Norma-Outer", -140.0f,   30.0f},
    {"Scutum-Cent.", 30.0f, -140.0f},
};

// ---------------------------------------------------------------------------
// Construction / open / close
// ---------------------------------------------------------------------------

StarChartViewer::StarChartViewer(NavigationData* nav, Renderer* renderer, WorldManager* world)
    : nav_(nav), renderer_(renderer), world_(world) {}

void StarChartViewer::open() {
    open_ = true;
    center_on_sol();

    // Start zoomed into the current system
    if (nav_) {
        for (size_t i = 0; i < nav_->systems.size(); ++i) {
            if (nav_->systems[i].id == nav_->current_system_id) {
                cursor_index_ = static_cast<int>(i);
                auto& sys = nav_->systems[i];
                generate_system_bodies(sys);
                if (!sys.bodies.empty() || sys.has_station) {
                    zoom_ = ChartZoom::System;
                    bool station_only = sys.bodies.empty() && sys.has_station;
                    // Select the body/station we're currently at
                    if (nav_->at_station) {
                        int host = station_host_body(sys);
                        body_cursor_ = host;
                        sub_cursor_ = 0;   // select station regardless of host
                    } else if (nav_->current_body_index >= 0) {
                        body_cursor_ = nav_->current_body_index;
                        // If on a moon, select it
                        sub_cursor_ = (nav_->current_moon_index >= 0)
                                      ? nav_->current_moon_index + 1 : -1;
                    } else if (station_only) {
                        // Arrived in a bodyless station-only system (e.g. just warped in).
                        body_cursor_ = -1;
                        sub_cursor_ = 0;
                    } else if (nav_->on_ship) {
                        body_cursor_ = 0; // in orbit around host star
                        sub_cursor_ = -1;
                    } else {
                        body_cursor_ = 0;
                        sub_cursor_ = -1;
                    }
                } else {
                    zoom_ = ChartZoom::Local;
                    body_cursor_ = -1;
                    sub_cursor_ = -1;
                }
                return;
            }
        }
    }

    zoom_ = ChartZoom::Galaxy;
    cursor_index_ = -1;
}

void StarChartViewer::close() {
    open_ = false;
}

bool StarChartViewer::has_pending_action() const {
    return pending_action_.type != ChartActionType::None;
}

ChartAction StarChartViewer::consume_action() {
    ChartAction a = pending_action_;
    pending_action_ = {};
    return a;
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

#ifdef ASTRA_DEV_MODE
    // Dev-only: toggle station-type overlay with 'T'
    if (dev_mode_ && key == 'T') {
        show_station_types_ = !show_station_types_;
        return true;
    }
#endif

    switch (zoom_) {
        case ChartZoom::Galaxy: {
            // Per-cell navigation: step exactly one renderer cell (cached from
            // the most recent draw, which knows the real map-area width).
            switch (key) {
                case KEY_UP:    view_cy_ -= galaxy_cell_h_; return true;
                case KEY_DOWN:  view_cy_ += galaxy_cell_h_; return true;
                case KEY_LEFT:  view_cx_ -= galaxy_cell_w_; return true;
                case KEY_RIGHT: view_cx_ += galaxy_cell_w_; return true;
                case '+': case '=': case '\n': case '\r':
                    zoom_ = ChartZoom::Region;
                    cursor_index_ = find_nearest_system(view_cx_, view_cy_);
                    return true;
                case 'h': // Home — center on Sol
                    center_on_sol();
                    return true;
                case 'F':
                case 'f':
                    show_faction_tint_ = !show_faction_tint_;
                    return true;
                case '\033': case 'q':
                    close();
                    return true;
            }
            break;
        }
        case ChartZoom::Region: {
            switch (key) {
                case KEY_UP:    view_cy_ -= 5.0f; return true;
                case KEY_DOWN:  view_cy_ += 5.0f; return true;
                case KEY_LEFT:  view_cx_ -= 5.0f; return true;
                case KEY_RIGHT: view_cx_ += 5.0f; return true;
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
                case 's':
                    scan_system();
                    return true;
                case 'h':
                    center_on_sol();
                    return true;
                case '\033': case 'q':
                    close();
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
                case '\t':
                    cycle_cursor();
                    center_on_cursor();
                    return true;
                case '+': case '=': case '\n': case '\r':
                    if (cursor_index_ >= 0) {
                        auto& sys = nav_->systems[cursor_index_];
                        if (sys.discovered && sys.id != 0) {
                            generate_system_bodies(sys);
                            if (!sys.bodies.empty() || sys.has_station) {
                                zoom_ = ChartZoom::System;
                                if (sys.bodies.empty()) {
                                    // Standalone station — sub_cursor=0 selects the station
                                    body_cursor_ = -1;
                                    sub_cursor_ = 0;
                                } else {
                                    body_cursor_ = 0;
                                    sub_cursor_ = -1;
                                }
                            }
                        }
                    }
                    return true;
                case '-': case '\b':
                    zoom_ = ChartZoom::Region;
                    return true;
                case 's':
                    scan_system();
                    return true;
                case 'w': {
                    if (cursor_index_ < 0 || cursor_index_ >= static_cast<int>(nav_->systems.size())) break;
                    auto& target = nav_->systems[cursor_index_];
                    if (!target.discovered) {
                        scan_message_ = "System not yet scanned.";
                        scan_message_timer_ = 90;
                        break;
                    }
                    if (target.id == nav_->current_system_id) {
                        scan_message_ = "Already in this system.";
                        scan_message_timer_ = 90;
                        break;
                    }
                    // Check warp range (dev mode bypasses — infinite range).
#ifdef ASTRA_DEV_MODE
                    const bool bypass_range = dev_mode_;
#else
                    const bool bypass_range = false;
#endif
                    if (!bypass_range) {
                        const StarSystem* current_sys = nullptr;
                        for (const auto& s : nav_->systems) {
                            if (s.id == nav_->current_system_id) { current_sys = &s; break; }
                        }
                        if (current_sys) {
                            float dist = system_distance(*current_sys, target);
                            float max_range = static_cast<float>(nav_->navi_range) * 20.0f;
                            if (dist > max_range) {
                                scan_message_ = "Out of warp range.";
                                scan_message_timer_ = 90;
                                break;
                            }
                        }
                    }
                    if (view_only_) {
                        scan_message_ = "Board your ship to travel.";
                        scan_message_timer_ = 90;
                        break;
                    }
                    pending_action_ = {ChartActionType::WarpToSystem, cursor_index_, -1, -1, true};
                    close();
                    return true;
                }
                case 'h':
                    center_on_sol();
                    cursor_index_ = find_nearest_system(view_cx_, view_cy_);
                    return true;
                case '\033': case 'q':
                    close();
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
            int cur_moons = (body_cursor_ >= 0 && body_cursor_ < body_count)
                            ? sys.bodies[body_cursor_].moons : 0;
            bool hosts_station = sys.has_station &&
                                 body_cursor_ == station_host_body(sys);
            bool station_only = sys.has_station && sys.bodies.empty();
            switch (key) {
                case KEY_LEFT:
                    if (station_only) return true;   // nothing to cycle
                    if (body_cursor_ > 0) { --body_cursor_; sub_cursor_ = -1; }
                    return true;
                case KEY_RIGHT:
                    if (station_only) return true;
                    if (body_cursor_ < body_count - 1) { ++body_cursor_; sub_cursor_ = -1; }
                    return true;
                case KEY_UP:
                    if (station_only) { sub_cursor_ = 0; return true; }
                    if (sub_cursor_ > 1) {
                        // Move up through moons
                        --sub_cursor_;
                    } else if (sub_cursor_ == 1) {
                        // From first moon back to body
                        sub_cursor_ = -1;
                    } else if (sub_cursor_ == -1 && hosts_station) {
                        // From body up to station
                        sub_cursor_ = 0;
                    }
                    return true;
                case KEY_DOWN:
                    if (station_only) { sub_cursor_ = 0; return true; }
                    if (sub_cursor_ == 0) {
                        // From station back to body
                        sub_cursor_ = -1;
                    } else if (sub_cursor_ == -1 && cur_moons > 0) {
                        // From body down to first moon
                        sub_cursor_ = 1;
                    } else if (sub_cursor_ >= 1 && sub_cursor_ < cur_moons) {
                        // Move down through moons
                        ++sub_cursor_;
                    }
                    return true;
                case 't': {
                    // Travel within current system only
                    if (sys.id != nav_->current_system_id) {
                        scan_message_ = "Must warp to this system first.";
                        scan_message_timer_ = 90;
                        break;
                    }
                    if (view_only_) {
                        scan_message_ = "Board your ship to travel.";
                        scan_message_timer_ = 90;
                        break;
                    }
                    if (sub_cursor_ == 0 && hosts_station) {
                        // Station selected
                        if (nav_->at_station) {
                            scan_message_ = "Already docked here.";
                            scan_message_timer_ = 90;
                            break;
                        }
                        pending_action_ = {ChartActionType::TravelToStation, cursor_index_, -1, -1, true};
                        close();
                        return true;
                    } else if (sub_cursor_ == -1 && body_cursor_ >= 0 && body_cursor_ < body_count) {
                        // Body selected
                        const auto& body = sys.bodies[body_cursor_];
                        if (!body.landable) {
                            scan_message_ = "Not landable.";
                            scan_message_timer_ = 90;
                            break;
                        }
                        if (!nav_->at_station && nav_->current_body_index == body_cursor_ &&
                            nav_->current_moon_index < 0) {
                            scan_message_ = "Already here.";
                            scan_message_timer_ = 90;
                            break;
                        }
                        pending_action_ = {ChartActionType::TravelToBody, cursor_index_, body_cursor_, -1, false};
                        close();
                        return true;
                    } else if (sub_cursor_ >= 1) {
                        // Moon selected — travel to moon on this body
                        if (body_cursor_ >= 0 && body_cursor_ < body_count) {
                            int moon_idx = sub_cursor_ - 1;
                            pending_action_ = {ChartActionType::TravelToBody, cursor_index_, body_cursor_, moon_idx, false};
                            close();
                            return true;
                        }
                    }
                    break;
                }
                case '-': case '\b':
                    zoom_ = ChartZoom::Local;
                    body_cursor_ = -1;
                    sub_cursor_ = -1;
                    return true;
                case '\033': case 'q':
                    close();
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

void StarChartViewer::scan_system() {
    if (!nav_ || cursor_index_ < 0 ||
        cursor_index_ >= static_cast<int>(nav_->systems.size()))
        return;

    auto& sys = nav_->systems[cursor_index_];
    if (sys.discovered) return; // already known

    sys.discovered = true;
    generate_system_bodies(sys);

    scan_message_ = "Scanned: " + sys.name;
    scan_message_timer_ = 90; // ~1.5s at 60fps
}

void StarChartViewer::center_on_cursor() {
    if (cursor_index_ >= 0 && nav_) {
        view_cx_ = nav_->systems[cursor_index_].gx;
        view_cy_ = nav_->systems[cursor_index_].gy;
    }
}

// ---------------------------------------------------------------------------
// Station host
// ---------------------------------------------------------------------------

int StarChartViewer::station_host_body(const StarSystem& sys) const {
    // Station orbits the first gas giant; fallback to last body
    for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
        if (sys.bodies[i].type == BodyType::GasGiant) return i;
    }
    return sys.bodies.empty() ? -1 : static_cast<int>(sys.bodies.size()) - 1;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void StarChartViewer::draw(int screen_w, int screen_h) {
    if (!open_ || !nav_ || !renderer_) return;

    // Ensure bodies generated for system zoom
    if ((zoom_ == ChartZoom::System || zoom_ == ChartZoom::Local) &&
        cursor_index_ >= 0 && cursor_index_ < static_cast<int>(nav_->systems.size())) {
        generate_system_bodies(nav_->systems[cursor_index_]);
    }

    // Title based on zoom level
    const char* zoom_name = "Galaxy";
    if (zoom_ == ChartZoom::Region) zoom_name = "Region";
    else if (zoom_ == ChartZoom::Local) zoom_name = "Local";
    else if (zoom_ == ChartZoom::System) zoom_name = "System";
    std::string title = std::string("Star Chart [") + zoom_name + "]";

    // Footer hints
    std::string footer;
    switch (zoom_) {
        case ChartZoom::Galaxy:
            footer = "[Arrows] Pan  [+] Zoom in  [H] Home  [ESC] Close";
            break;
        case ChartZoom::Region:
            footer = "[Arrows] Pan  [Tab] Select  [s] Scan  [+] Zoom in  [-] Zoom out  [H] Home  [ESC] Close";
            break;
        case ChartZoom::Local:
            footer = "[Arrows] Select  [Tab] Cycle  [s] Scan  [w] Warp  [+] View  [-] Zoom out  [H] Home  [ESC] Close";
            break;
        case ChartZoom::System:
            footer = "[Left/Right] Select  [Up/Down] Sub  [t] Travel  [-] Back  [ESC] Close";
            break;
    }
#ifdef ASTRA_DEV_MODE
    if (dev_mode_) {
        footer += "  [T] Station types";
    }
#endif

    // Full-screen context with inset padding
    UIContext screen(renderer_, Rect{2, 2, screen_w - 4, screen_h - 4});
    auto content = screen.panel({.title = title, .footer = footer});

    // Split: map | info (info panel has its own internal spine)
    int info_width = std::max(22, content.width() * 20 / 100);
    auto cols = content.columns({fill(), fixed(info_width)});
    auto& map_area = cols[0];
    auto& info = cols[1];

    // Cache galaxy cell size so input-handler pan steps match the renderer's
    // grid (which uses map_area.width(), not the full renderer width).
    if (zoom_ == ChartZoom::Galaxy && map_area.width() > 0) {
        galaxy_cell_w_ = kGalaxyViewWidthGu / static_cast<float>(map_area.width());
        galaxy_cell_h_ = 2.0f * galaxy_cell_w_;
    }

    // Layer 1: galaxy map primitive
    map_area.galaxy_map(build_map_desc());

#ifdef ASTRA_DEV_MODE
    // Dev overlay: station-type distribution tally at bottom of map
    if (show_station_types_) {
        int n_normal = 0, n_scav = 0, n_pirate = 0, n_abandoned = 0, n_infested = 0;
        int total = 0;
        for (const auto& sys : nav_->systems) {
            if (!sys.has_station) continue;
            ++total;
            switch (sys.station.type) {
                case StationType::NormalHub:  ++n_normal;   break;
                case StationType::Scav:       ++n_scav;     break;
                case StationType::Pirate:     ++n_pirate;   break;
                case StationType::Abandoned:  ++n_abandoned; break;
                case StationType::Infested:   ++n_infested; break;
            }
        }
        if (total > 0) {
            auto pct = [&](int n) {
                return static_cast<int>(std::round(100.0f * n / total));
            };
            std::string tally = "Stations:";
            if (n_normal   > 0) tally += " " + std::to_string(pct(n_normal))   + "%N";
            if (n_scav     > 0) tally += " " + std::to_string(pct(n_scav))     + "%S";
            if (n_pirate   > 0) tally += " " + std::to_string(pct(n_pirate))   + "%P";
            if (n_abandoned> 0) tally += " " + std::to_string(pct(n_abandoned))+ "%A";
            if (n_infested > 0) tally += " " + std::to_string(pct(n_infested)) + "%I";
            tally += "  (n=" + std::to_string(total) + ")";
            int ty = map_area.height() - 1;
            if (ty >= 0) {
                map_area.text(0, ty, tally, Color::DarkGray);
            }
        }
    }
#endif

    // Scan feedback overlay on the map
    if (scan_message_timer_ > 0) {
        int msg_x = (map_area.width() - static_cast<int>(scan_message_.size())) / 2;
        if (msg_x < 0) msg_x = 0;
        int msg_y = map_area.height() - 2;
        if (msg_y >= 0) {
            map_area.text(msg_x, msg_y, scan_message_, Color::Cyan);
        }
        --scan_message_timer_;
    }

    // Info sidebar
    draw_info_panel(info);
}

// ---------------------------------------------------------------------------
// build_map_desc
// ---------------------------------------------------------------------------

GalaxyMapDesc StarChartViewer::build_map_desc() const {
    GalaxyMapDesc desc;
    desc.zoom = zoom_;
    desc.systems = std::span<const StarSystem>(nav_->systems);

    // Arm labels (static data)
    desc.arm_labels = arm_labels;

    // Player location
    for (size_t i = 0; i < nav_->systems.size(); ++i) {
        if (nav_->systems[i].id == nav_->current_system_id) {
            desc.player_system_index = static_cast<int>(i);
            break;
        }
    }
    desc.player_body_index = nav_->current_body_index;
    desc.player_moon_index = nav_->current_moon_index;
    desc.at_station = nav_->at_station;
    desc.on_ship = nav_->on_ship;

    // Viewport
    desc.view_cx = view_cx_;
    desc.view_cy = view_cy_;

    // Cursor
    desc.cursor_system_index = cursor_index_;
    desc.body_cursor = body_cursor_;
    desc.sub_cursor = sub_cursor_;

    // Quest markers
    if (world_) {
        auto ids = world_->quest_target_system_ids();
        desc.quest_system_ids.assign(ids.begin(), ids.end());

        if (zoom_ == ChartZoom::System && cursor_index_ >= 0 &&
            cursor_index_ < static_cast<int>(nav_->systems.size())) {
            const auto& sys = nav_->systems[cursor_index_];
            for (int bi = 0; bi < static_cast<int>(sys.bodies.size()); ++bi) {
                if (world_->is_quest_target_body(sys.id, bi)) {
                    desc.quest_body_targets.push_back({
                        bi, world_->quest_title_for_body(sys.id, bi)
                    });
                }
            }
        }
    }

    // Lore markers — discovered systems with tier 2+
    for (const auto& sys : nav_->systems) {
        if (sys.discovered && sys.lore.lore_tier >= 2) {
            desc.lore_system_ids.push_back(sys.id);
        }
    }

    // Navigation
    desc.navi_range = static_cast<float>(nav_->navi_range);

    // Station host (for System zoom)
    if (zoom_ == ChartZoom::System && cursor_index_ >= 0 &&
        cursor_index_ < static_cast<int>(nav_->systems.size())) {
        const auto& sys = nav_->systems[cursor_index_];
        if (sys.has_station) {
            desc.station_host_body_index = station_host_body(sys);
        }
    }

    // Faction tint (Galaxy zoom only)
    desc.show_faction_tint = show_faction_tint_;

#ifdef ASTRA_DEV_MODE
    desc.show_station_types = show_station_types_;
#endif

    return desc;
}

// ---------------------------------------------------------------------------
// Info panel
// ---------------------------------------------------------------------------

// Draw the interior vertical bar that runs the height of the info panel
static void draw_info_spine(UIContext& ctx) {
    for (int y = 0; y < ctx.height(); ++y) {
        ctx.put(0, y, BoxDraw::V, Color::DarkGray);
    }
}

// Draw a double-line title row: ╞═══════════════╡  (line below the name)
static void draw_info_title(UIContext& ctx, int& y, const std::string& name, Color name_color) {
    ctx.text(2, y++, name, name_color);
    // Replace the vertical spine at this row with ╞, fill with ═
    ctx.put(0, y, "\xe2\x95\x9e", Color::DarkGray);  // ╞
    for (int x = 1; x < ctx.width(); ++x) {
        ctx.put(x, y, BoxDraw::DH, Color::DarkGray);
    }
    ++y;
}

// Draw a section divider: ├─┤ LABEL ├────────
// If glyph != 0, prefix the label with the glyph in the given color
static void draw_info_section(UIContext& ctx, int& y, const std::string& label,
                              Color label_color, char glyph = 0, Color glyph_color = Color::White) {
    int w = ctx.width();
    // Left: ├─┤
    ctx.put(0, y, BoxDraw::LT, Color::DarkGray);  // ├
    ctx.put(1, y, BoxDraw::H, Color::DarkGray);   // ─
    ctx.put(2, y, BoxDraw::RT, Color::DarkGray);  // ┤
    int tx = 4;
    if (glyph != 0) {
        ctx.put(tx++, y, glyph, glyph_color);
        ++tx;  // space after glyph
    }
    ctx.text(tx, y, label, label_color);
    int after = tx + static_cast<int>(label.size()) + 1;
    // Right: ├──────
    ctx.put(after, y, BoxDraw::LT, Color::DarkGray);  // ├
    for (int x = after + 1; x < w; ++x) {
        ctx.put(x, y, BoxDraw::H, Color::DarkGray);
    }
    ++y;
}

void StarChartViewer::draw_info_panel(UIContext& ctx) {
    // Draw the vertical spine first — everything else overwrites it where needed
    draw_info_spine(ctx);

    int y = 1;

    switch (zoom_) {
        case ChartZoom::Galaxy: {
            draw_info_title(ctx, y, "MILKY WAY", Color::White);
            y++;
            ctx.text(2, y++, "Zoom: Galaxy", Color::DarkGray);

            int total = static_cast<int>(nav_->systems.size());
            int discovered = 0;
            for (const auto& s : nav_->systems) {
                if (s.discovered) ++discovered;
            }
            ctx.text(2, y++, "Systems: " + std::to_string(total), Color::DarkGray);
            ctx.text(2, y++, "Discovered: " + std::to_string(discovered), Color::Cyan);
            y++;

            for (const auto& s : nav_->systems) {
                if (s.id == nav_->current_system_id) {
                    ctx.text(2, y++, "Current: " + s.name, Color::Yellow);
                    break;
                }
            }
            break;
        }
        case ChartZoom::Region: {
            draw_info_title(ctx, y, "REGION VIEW", Color::White);
            y++;

            if (cursor_index_ >= 0 && cursor_index_ < static_cast<int>(nav_->systems.size())) {
                const auto& sys = nav_->systems[cursor_index_];
                if (sys.discovered) {
                    draw_system_info_text(ctx, sys, y, ctx.height(), /*use_title=*/false);
                } else {
                    draw_info_section(ctx, y, "UNKNOWN", Color::DarkGray);
                    y++;
                    ctx.text(2, y++, "Scan or visit", Color::DarkGray);
                    ctx.text(2, y++, "to reveal", Color::DarkGray);
                }
            } else {
                ctx.text(2, y++, "Use [Tab] to", Color::DarkGray);
                ctx.text(2, y++, "select a system", Color::DarkGray);
            }
            break;
        }
        case ChartZoom::Local: {
            draw_info_title(ctx, y, "LOCAL VIEW", Color::White);
            y++;

            if (cursor_index_ >= 0 && cursor_index_ < static_cast<int>(nav_->systems.size())) {
                const auto& sys = nav_->systems[cursor_index_];
                if (sys.discovered) {
                    draw_system_info_text(ctx, sys, y, ctx.height(), /*use_title=*/false);
                } else {
                    draw_info_section(ctx, y, "UNKNOWN", Color::DarkGray);
                    y++;
                    ctx.text(2, y++, "Scan or visit", Color::DarkGray);
                    ctx.text(2, y++, "to reveal", Color::DarkGray);
                }
            }
            break;
        }
        case ChartZoom::System: {
            if (cursor_index_ < 0 || cursor_index_ >= static_cast<int>(nav_->systems.size())) break;
            const auto& sys = nav_->systems[cursor_index_];

            // Title: star name
            draw_info_title(ctx, y, sys.name, Color::White);
            y++;

            // Star details
            ctx.text(2, y, "Class", Color::DarkGray);
            ctx.text(10, y++, star_class_name(sys.star_class), star_class_color(sys.star_class));
            ctx.text(2, y, "Type", Color::DarkGray);
            ctx.text(10, y++, sys.binary ? "Binary" : "Single star", Color::White);
            ctx.text(2, y, "Bodies", Color::DarkGray);
            ctx.text(10, y++, std::to_string(sys.bodies.size()), Color::White);

            if (sys.has_station) {
                Color sc = (sys.station.type == StationType::Abandoned) ? Color::Red : Color::Cyan;
                std::string slabel = sys.station.name;
                if (sys.station.type == StationType::Abandoned) slabel += " [!]";
                ctx.text(2, y, "Station", Color::DarkGray);
                ctx.text(10, y++, slabel, sc);
            }
            ctx.text(2, y, "Faction", Color::DarkGray);
            if (!sys.controlling_faction.empty()) {
                ctx.text(10, y++, sys.controlling_faction, Color::White);
            } else {
                ctx.text(10, y++, "Unclaimed space", Color::DarkGray);
            }
            y++;

            // Selected item details — body/moon/station
            if (sub_cursor_ == 0 && sys.has_station) {
                draw_station_info_text(ctx, sys, y);
            } else if (sub_cursor_ >= 1 && body_cursor_ >= 0 &&
                       body_cursor_ < static_cast<int>(sys.bodies.size())) {
                // Moon selected
                const auto& body = sys.bodies[body_cursor_];
                int mi = sub_cursor_ - 1;
                std::string mname = (mi >= 0 && mi < static_cast<int>(body.moon_names.size()))
                                    ? body.moon_names[mi]
                                    : body.name + " Moon " + std::to_string(sub_cursor_);
                draw_info_section(ctx, y, mname, Color::White, '*', Color::DarkGray);
                y++;
                ctx.text(2, y, "Type", Color::DarkGray);
                ctx.text(10, y++, "Moon", Color::White);
                ctx.text(2, y, "Orbits", Color::DarkGray);
                ctx.text(10, y++, body.name, body_display_color(body));
            } else if (body_cursor_ >= 0 && body_cursor_ < static_cast<int>(sys.bodies.size())) {
                draw_body_info_text(ctx, sys.bodies[body_cursor_], sys, y, body_cursor_);
            }

            // Quest target indicator
            if (world_ && body_cursor_ >= 0 && world_->is_quest_target_body(sys.id, body_cursor_)) {
                std::string qt = world_->quest_title_for_body(sys.id, body_cursor_);
                int qy = ctx.height() - 2;
                ctx.text(2, qy, "! " + (qt.empty() ? "QUEST TARGET" : qt), Color::BrightYellow);
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Info text helpers
// ---------------------------------------------------------------------------

void StarChartViewer::draw_system_info_text(UIContext& ctx, const StarSystem& sys, int start_y, int max_h, bool use_title) {
    int y = start_y;

    if (use_title) {
        // Title: star name as the main heading
        draw_info_title(ctx, y, sys.name, Color::White);
        y++;
    } else {
        // Section header: star name as a sub-section (for Region/Local views)
        draw_info_section(ctx, y, sys.name, Color::White, '*',
                          star_class_color(sys.star_class));
        y++;
    }

    // Star details
    ctx.text(2, y, "Class", Color::DarkGray);
    ctx.text(10, y++, star_class_name(sys.star_class), star_class_color(sys.star_class));

    ctx.text(2, y, "Type", Color::DarkGray);
    ctx.text(10, y++, sys.binary ? "Binary" : "Single star", Color::White);

    if (sys.has_station) {
        ctx.text(2, y, "Station", Color::DarkGray);
        Color sc = (sys.station.type == StationType::Abandoned) ? Color::Red : Color::Cyan;
        std::string slabel = sys.station.name;
        if (sys.station.type == StationType::Abandoned) slabel += " [!]";
        ctx.text(10, y++, slabel, sc);
    } else {
        ctx.text(2, y, "Station", Color::DarkGray);
        ctx.text(10, y++, "None", Color::DarkGray);
    }

    ctx.text(2, y, "Planets", Color::DarkGray);
    ctx.text(10, y++, std::to_string(sys.planet_count), Color::White);

    if (sys.asteroid_belts > 0) {
        ctx.text(2, y, "Belts", Color::DarkGray);
        ctx.text(10, y++, std::to_string(sys.asteroid_belts), Color::White);
    }

    ctx.text(2, y, "Danger", Color::DarkGray);
    std::string danger_bar;
    for (int i = 0; i < 10; ++i) {
        danger_bar += (i < sys.danger_level) ? '=' : '-';
    }
    danger_bar += " " + std::to_string(sys.danger_level) + "/10";
    Color danger_color = Color::Green;
    if (sys.danger_level >= 7) danger_color = Color::Red;
    else if (sys.danger_level >= 4) danger_color = Color::Yellow;
    ctx.text(10, y++, danger_bar, danger_color);

    for (const auto& s : nav_->systems) {
        if (s.id == nav_->current_system_id) {
            float dist = system_distance(sys, s);
            int d10 = static_cast<int>(dist * 10.0f);
            std::string dist_str = std::to_string(d10 / 10) + "." + std::to_string(d10 % 10) + " ly";
            ctx.text(2, y, "Dist", Color::DarkGray);
            ctx.text(10, y++, dist_str, Color::White);
            break;
        }
    }

    ctx.text(2, y, "Faction", Color::DarkGray);
    if (!sys.controlling_faction.empty()) {
        ctx.text(10, y++, sys.controlling_faction, Color::White);
    } else {
        ctx.text(10, y++, "Unclaimed space", Color::DarkGray);
    }

    if (sys.lore.has_megastructure) {
        ctx.text(2, y++, "\xe2\x97\x88 Megastructure", Color::Yellow);
    }
    if (sys.lore.beacon) {
        ctx.text(2, y++, "\xe2\x8c\xbe Sgr A* Beacon", Color::Cyan);
    }

    y++;

    // BODIES section
    if (!sys.bodies.empty() && y + 2 < max_h) {
        draw_info_section(ctx, y, "BODIES", Color::White);
        y++;

        int panel_w = ctx.width();
        for (const auto& body : sys.bodies) {
            if (y >= max_h) break;

            char glyph = body_type_glyph(body.type);
            Color color = body_display_color(body);
            ctx.put(2, y, glyph, color);

            int name_max = panel_w - 5;
            std::string display_name = body.name;
            if (static_cast<int>(display_name.size()) > name_max) {
                display_name = display_name.substr(0, name_max - 1) + ".";
            }

            if (body.landable && static_cast<int>(display_name.size()) + 4 <= name_max) {
                display_name += " [L]";
            }

            ctx.text(4, y, display_name, color);
            ++y;
        }
    }
}

void StarChartViewer::draw_body_info_text(UIContext& ctx, const CelestialBody& body,
                                          const StarSystem& sys, int start_y, int body_index) {
    int y = start_y;

    // Body section header: ├─┤ o Mars ├──────
    char glyph = body_type_glyph(body.type);
    Color color = body_display_color(body);
    draw_info_section(ctx, y, body.name, Color::White, glyph, color);
    y++;

    ctx.text(2, y, "Type", Color::DarkGray);
    ctx.text(10, y++, body_type_name(body.type), color);

    ctx.text(2, y, "Size", Color::DarkGray);
    {
        std::string size_bar;
        for (int i = 0; i < 10; ++i) size_bar += (i < body.size) ? '=' : '-';
        ctx.text(10, y++, size_bar, Color::White);
    }

    ctx.text(2, y, "Atmo", Color::DarkGray);
    ctx.text(10, y++, atmosphere_name(body.atmosphere), Color::White);

    ctx.text(2, y, "Temp", Color::DarkGray);
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
        ctx.text(2, y, "Moons", Color::DarkGray);
        ctx.text(10, y++, std::to_string(body.moons), Color::White);
    }

    {
        int d10 = static_cast<int>(body.orbital_distance * 10.0f);
        std::string dist_str = std::to_string(d10 / 10) + "." + std::to_string(d10 % 10) + " AU";
        ctx.text(2, y, "Orbit", Color::DarkGray);
        ctx.text(10, y++, dist_str, Color::White);
    }

    ctx.text(2, y, "Danger", Color::DarkGray);
    {
        std::string danger_bar;
        for (int i = 0; i < 10; ++i) danger_bar += (i < body.danger_level) ? '=' : '-';
        Color dc = Color::Green;
        if (body.danger_level >= 7) dc = Color::Red;
        else if (body.danger_level >= 4) dc = Color::Yellow;
        ctx.text(10, y++, danger_bar, dc);
    }

    if (body.landable) {
        ctx.text(2, y++, "LANDABLE", Color::Green);
    } else {
        ctx.text(2, y++, "Not landable", Color::DarkGray);
    }

    if (body.has_dungeon) {
        ctx.text(2, y++, "Dungeon detected", Color::Yellow);
    }

    // Lore features (system-level, shown on landable bodies)
    if (body.landable && sys.lore.lore_tier > 0) {
        if (sys.lore.beacon)
            ctx.text(2, y++, "\xe2\x8c\xbe Sgr A* Beacon", Color::Cyan);
        if (sys.lore.has_megastructure)
            ctx.text(2, y++, "\xe2\x97\x88 Megastructure", Color::Yellow);
        if (sys.lore.terraformed)
            ctx.text(2, y++, "\xe2\x88\x97 Terraformed", Color::Green);
        if (sys.lore.battle_site)
            ctx.text(2, y++, "\xe2\x9a\xa0 Battle site", Color::Red);
        if (sys.lore.weapon_test_site)
            ctx.text(2, y++, "\xe2\x9a\xa0 Weapon test site", Color::Red);
        if (sys.lore.plague_origin)
            ctx.text(2, y++, "\xe2\x9a\xa0 Plague origin", Color::Magenta);
    }

    // RESOURCES section
    if (body.resources != 0) {
        y++;
        draw_info_section(ctx, y, "RESOURCES", Color::White);
        y++;
        if (has_resource(body.resources, Resource::Metals))     ctx.text(3, y++, "Metals", Color::White);
        if (has_resource(body.resources, Resource::RareMetals)) ctx.text(3, y++, "Rare Metals", Color::BrightMagenta);
        if (has_resource(body.resources, Resource::Water))      ctx.text(3, y++, "Water", Color::Blue);
        if (has_resource(body.resources, Resource::Fuel))       ctx.text(3, y++, "Fuel", Color::Yellow);
        if (has_resource(body.resources, Resource::Organics))   ctx.text(3, y++, "Organics", Color::Green);
        if (has_resource(body.resources, Resource::Crystals))   ctx.text(3, y++, "Crystals", Color::Cyan);
        if (has_resource(body.resources, Resource::Radioactive))ctx.text(3, y++, "Radioactive", Color::Red);
        if (has_resource(body.resources, Resource::Gas))        ctx.text(3, y++, "Gas", static_cast<Color>(208));
    }

    // SCANNER REPORT section — shows PoiBudget if the overworld has been visited
    y++;
    draw_info_section(ctx, y, "SCANNER REPORT", Color::Cyan);
    y++;

    // Look up the overworld map for this body in the location cache.
    // Key: {system_id, body_index, moon_index=-1, is_station=false, ow_x=-1, ow_y=-1, depth=0}
    const TileMap* owm = nullptr;
    if (world_ && body_index >= 0) {
        LocationKey key{sys.id, body_index, -1, false, -1, -1, 0};
        auto it = world_->location_cache().find(key);
        if (it != world_->location_cache().end() &&
            it->second.map.map_type() == MapType::Overworld) {
            owm = &it->second.map;
        }
        // Also check if the currently-loaded map is this body's overworld
        if (!owm && world_->on_overworld() &&
            world_->navigation().current_system_id == sys.id &&
            world_->navigation().current_body_index == body_index &&
            world_->navigation().current_moon_index == -1) {
            owm = &world_->map();
        }
    }

    if (owm) {
        std::string report = format_poi_budget(owm->poi_budget());
        size_t start = 0;
        while (start < report.size()) {
            size_t nl = report.find('\n', start);
            std::string line = report.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            if (!line.empty()) {
                ctx.text(3, y++, line, Color::White);
            }
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    } else {
        ctx.text(3, y++, "(not scanned \xe2\x80\x94 visit the planet to survey)", Color::DarkGray);
    }
}

void StarChartViewer::draw_station_info_text(UIContext& ctx, const StarSystem& sys, int start_y) {
    int y = start_y;

    bool abandoned = (sys.station.type == StationType::Abandoned);
    char glyph = abandoned ? '#' : 'H';
    Color color = abandoned ? Color::Red : Color::Cyan;
    draw_info_section(ctx, y, sys.station.name, Color::White, glyph, color);
    y++;

    ctx.text(2, y, "Type", Color::DarkGray);
    if (abandoned) {
        ctx.text(10, y++, "Derelict", Color::Red);
    } else {
        ctx.text(10, y++, "Station", Color::Cyan);
    }

    int host = station_host_body(sys);
    if (host >= 0 && host < static_cast<int>(sys.bodies.size())) {
        ctx.text(2, y, "Orbits", Color::DarkGray);
        ctx.text(10, y++, sys.bodies[host].name, body_display_color(sys.bodies[host]));
    }

    y++;
    if (abandoned) {
        ctx.text(2, y++, "DERELICT", Color::Red);
        ctx.text(2, y++, "Power systems offline", Color::DarkGray);
        ctx.text(2, y++, "Structural integrity", Color::DarkGray);
        ctx.text(2, y++, "compromised", Color::DarkGray);
    } else {
        ctx.text(2, y++, "OPERATIONAL", Color::Green);
        ctx.text(2, y++, "Docking available", Color::DarkGray);
    }
}

} // namespace astra
