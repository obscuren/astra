#include "astra/net_renderer.h"

#include "astra/cyberdeck.h"
#include "astra/net_window_anim.h"
#include "astra/game.h"
#include "astra/net_camera.h"
#include "astra/net_ice.h"
#include "astra/net_session.h"
#include "astra/net_theme.h"
#include "astra/hacking_system.h"
#include "astra/ip.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/lan.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/rect.h"
#include "astra/renderer.h"
#include "astra/telegraph.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace astra::net_theme {

// Plan 5 Cut 2.6: per-FixtureType wall-mounted device avatar glyph. All
// avatars render in BrightWhite via the renderer's DeviceAvatar branch.
const char* device_avatar_glyph(FixtureType type) {
    switch (type) {
        // Optics / consoles / data
        case FixtureType::Console:
        case FixtureType::CommandTerminal:    return "\xe2\x96\xa6";   // ▦ data block (ARIA flagship)
        case FixtureType::DataTerminal:       return "\xe2\x96\xa4";   // ▤ terminal screen
        case FixtureType::ShipTerminal:       return "\xe2\x89\xab";   // ≫ outbound arrow
        case FixtureType::StarChart:
        case FixtureType::StarChartL:
        case FixtureType::StarChartR:         return "\xe2\x80\xbb";   // ※ constellation

        // Doors / locks
        case FixtureType::Door:
        case FixtureType::Gate:               return "\xe2\x95\x91";   // ║ door bar

        // Power / lighting
        case FixtureType::Conduit:            return "\xe2\x89\x88";   // ≈ power flow
        case FixtureType::Lamp:
        case FixtureType::HoloLight:
        case FixtureType::Torch:              return "\xe2\x80\xbb";   // ※ lamp burst

        // Storage / commerce / health
        case FixtureType::Locker:
        case FixtureType::SupplyLocker:       return "\xe2\x96\xa3";   // ▣ locker slot
        case FixtureType::HealPod:            return "\xe2\x8a\x9e";   // ⊞ medical cross
        case FixtureType::FoodTerminal:       return "\xe2\x95\xa5";   // ╥ vending slot
        case FixtureType::WeaponDisplay:      return "\xe2\x95\xb3";   // ╳ weapon X
        case FixtureType::RepairBench:        return "\xce\xa0";       // Π workbench
        case FixtureType::RestPod:            return "\xe2\x97\x8b";   // ○ sleep capsule

        default:                              return "\xe2\x96\xa2";   // ▢ generic device
    }
}

} // namespace astra::net_theme

namespace astra::net_renderer {

namespace {

const char* wall_glyph_for_neighbours(bool n, bool s, bool e, bool w) {
    int code = (n?1:0) | (s?2:0) | (e?4:0) | (w?8:0);
    switch (code) {
        case 0:  return "\xe2\x80\xa2";              // • isolated (rare)
        case 1:  case 2:  case 3:  return "\xe2\x95\x91";   // ║ vertical
        case 4:  case 8:  case 12: return "\xe2\x95\x90";   // ═ horizontal
        case 5:  return "\xe2\x95\x9a";               // ╚  n + e
        case 6:  return "\xe2\x95\x94";               // ╔  s + e
        case 9:  return "\xe2\x95\x9d";               // ╝  n + w
        case 10: return "\xe2\x95\x97";               // ╗  s + w
        case 7:  return "\xe2\x95\xa0";               // ╠  n + s + e
        case 11: return "\xe2\x95\xa3";               // ╣  n + s + w
        case 13: return "\xe2\x95\xa9";               // ╩  n + e + w
        case 14: return "\xe2\x95\xa6";               // ╦  s + e + w
        case 15: return "\xe2\x95\xac";               // ╬  all four
        default: return "\xe2\x95\x91";               // fallback ║
    }
}

// GridTile dispatch (glyph_for / color_for / is_connectable /
// floor_color_for_zone_at) retired with the legacy sector. The renderer
// now switches on NetTile directly inside draw_playfield.

// ---------------------------------------------------------------------------
// Window geometry
// ---------------------------------------------------------------------------

struct WindowRect    { int x, y, w, h; };
struct PlayfieldRect { int x, y, w, h; };
struct LogPaneRect   { int x, y, w, h; };

constexpr Color kChrome  = Color::Cyan;
constexpr int   kLogPaneW = 40;

WindowRect compute_window_rect(int screen_w, int screen_h) {
    int w = screen_w * 8 / 10;
    int h = screen_h * 8 / 10;
    if (w < 50) w = std::min(50, screen_w);
    if (h < 18) h = std::min(18, screen_h);
    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;
    return {x, y, w, h};
}

// Chrome height grows by 1 when the netspace declares a subtitle line (a
// quote / flavour tag rendered below the title). All downstream rows
// (separator, deck strip, playfield, log pane, column split) shift down
// by `subtitle_rows` so the playfield contracts by that much.
int subtitle_rows_for(const Netspace& ns) {
    return ns.title_subtitle.empty() ? 0 : 1;
}

PlayfieldRect compute_playfield_rect(const WindowRect& wr, int subtitle_rows) {
    return { wr.x + 1,
             wr.y + 5 + subtitle_rows,
             wr.w - 2 - kLogPaneW - 1,
             wr.h - 5 - subtitle_rows - 3 };
}

LogPaneRect compute_log_pane_rect(const WindowRect& wr, int subtitle_rows) {
    return { wr.x + wr.w - 1 - kLogPaneW,
             wr.y + 5 + subtitle_rows,
             kLogPaneW,
             wr.h - 5 - subtitle_rows - 3 };
}

// ---------------------------------------------------------------------------
// Chrome
// ---------------------------------------------------------------------------

// Make the Tron window opaque — without this the monochrome world UI behind
// bleeds through every cell the chrome doesn't write to.
void clear_window_interior(Renderer& r, const WindowRect& wr) {
    for (int j = 0; j < wr.h; ++j) {
        for (int i = 0; i < wr.w; ++i) {
            r.draw_char(wr.x + i, wr.y + j, ' ');
        }
    }
}

void draw_window_chrome(Renderer& r, const WindowRect& wr,
                        WindowState ws, int phase) {
    // Corners are always drawn clean regardless of corruption state.
    r.draw_glyph(wr.x,            wr.y,            "\xe2\x95\x94", kChrome);
    r.draw_glyph(wr.x + wr.w - 1, wr.y,            "\xe2\x95\x97", kChrome);
    r.draw_glyph(wr.x,            wr.y + wr.h - 1, "\xe2\x95\x9a", kChrome);
    r.draw_glyph(wr.x + wr.w - 1, wr.y + wr.h - 1, "\xe2\x95\x9d", kChrome);

    // Border crawl: Hunted+ states replace ~1-in-7 edge cells with §
    // in a pattern that shifts with the blink phase.
    auto glitch_at = [&](int idx) {
        if (ws != WindowState::Hunted && ws != WindowState::Critical &&
            ws != WindowState::Blackwall) return false;
        return ((idx + phase / 3) % 7) == 0;
    };
    for (int i = 1; i < wr.w - 1; ++i) {
        bool g = glitch_at(i);
        r.draw_glyph(wr.x + i, wr.y,            g ? "\xc2\xa7" : "\xe2\x95\x90",
                     g ? Color::Magenta : kChrome);
        r.draw_glyph(wr.x + i, wr.y + wr.h - 1, g ? "\xc2\xa7" : "\xe2\x95\x90",
                     g ? Color::Magenta : kChrome);
    }
    for (int j = 1; j < wr.h - 1; ++j) {
        bool g = glitch_at(j + wr.w);
        r.draw_glyph(wr.x,            wr.y + j, g ? "\xc2\xa7" : "\xe2\x95\x91",
                     g ? Color::Magenta : kChrome);
        r.draw_glyph(wr.x + wr.w - 1, wr.y + j, g ? "\xc2\xa7" : "\xe2\x95\x91",
                     g ? Color::Magenta : kChrome);
    }
}

void draw_horizontal_separator(Renderer& r, const WindowRect& wr, int y_in_window) {
    int y = wr.y + y_in_window;
    r.draw_glyph(wr.x,            y, "\xe2\x95\xa0", kChrome);
    r.draw_glyph(wr.x + wr.w - 1, y, "\xe2\x95\xa3", kChrome);
    for (int i = 1; i < wr.w - 1; ++i) {
        r.draw_glyph(wr.x + i, y, "\xe2\x95\x90", kChrome);
    }
}

void draw_column_split(Renderer& r, const WindowRect& wr, int subtitle_rows) {
    int x = wr.x + wr.w - 1 - kLogPaneW - 1;
    for (int j = wr.y + 5 + subtitle_rows; j < wr.y + wr.h - 3; ++j) {
        r.draw_glyph(x, j, "\xe2\x95\x91", kChrome);
    }
    r.draw_glyph(x, wr.y + 4 + subtitle_rows, "\xe2\x95\xa6", kChrome); // ╦
    r.draw_glyph(x, wr.y + wr.h - 3,          "\xe2\x95\xa9", kChrome); // ╩
}

// ---------------------------------------------------------------------------
// Gauges + helpers
// ---------------------------------------------------------------------------

Color trace_fill_color(int pct) {
    if (pct < 25) return Color::Cyan;
    if (pct < 50) return Color::Cyan;          // bright variant unused — palette has Cyan + BrightCyan via xterm-256 elsewhere
    if (pct < 75) return Color::Magenta;
    return Color::BrightMagenta;
}

void draw_block_gauge(Renderer& r, int x, int y, int width,
                      int filled, Color fill_col) {
    for (int i = 0; i < width; ++i) {
        const char* glyph = (i < filled) ? "\xe2\x96\xae" : "\xe2\x96\xaf"; // ▮ ▯
        Color c = (i < filled) ? fill_col : Color::DarkGray;
        r.draw_glyph(x + i, y, glyph, c);
    }
}

// Renderer's draw_string takes no color parameter and writes byte-by-byte —
// multi-byte UTF-8 glyphs (▶, ›, ▮ …) collapse to one visual cell on screen
// but consume N buffer cells, throwing off downstream column math. Emit each
// glyph (ASCII or multi-byte) into one cell via draw_glyph, advancing the
// cursor one cell per *visible* glyph.
void draw_colored_string(Renderer& r, int x, int y, const std::string& text, Color c) {
    int cursor = x;
    Color cur = c;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
        if (b == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < text.size()) {
            cur = static_cast<Color>(static_cast<uint8_t>(text[i + 1]));
            i += 2;
            continue;
        }
        if (b == static_cast<unsigned char>(COLOR_END)) {
            cur = c;
            ++i;
            continue;
        }
        if (b < 0x80) {
            r.draw_char(cursor, y, static_cast<char>(b), cur);
            ++i;
            ++cursor;
        } else {
            int len = 1;
            if      ((b & 0xE0) == 0xC0) len = 2;
            else if ((b & 0xF0) == 0xE0) len = 3;
            else if ((b & 0xF8) == 0xF0) len = 4;
            char buf[5] = {0};
            for (int k = 0; k < len && i + k < text.size(); ++k) {
                buf[k] = text[i + k];
            }
            r.draw_glyph(cursor, y, buf, cur);
            i += len;
            ++cursor;
        }
    }
}

// Visual cell width of a UTF-8 string: ASCII = 1, every multi-byte code
// point = 1. COLOR_BEGIN/COLOR_END markers are zero-width metadata and
// skipped. Caller is responsible for advancing x by this much.
int visual_width(const std::string& text) {
    int w = 0;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
        if (b == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < text.size()) {
            i += 2;
            continue;
        }
        if (b == static_cast<unsigned char>(COLOR_END)) {
            ++i;
            continue;
        }
        if (b < 0x80) { ++w; ++i; }
        else if ((b & 0xE0) == 0xC0) { ++w; i += 2; }
        else if ((b & 0xF0) == 0xE0) { ++w; i += 3; }
        else if ((b & 0xF8) == 0xF0) { ++w; i += 4; }
        else                          { ++w; ++i; }
    }
    return w;
}

std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Extract the leading "<short-tag>-<host_octet>" piece from a hostname like
// "turret-13.tha.lan". Returns the part before the first '.'.
std::string short_host_label(const std::string& hostname) {
    size_t dot = hostname.find('.');
    return (dot == std::string::npos) ? hostname : hostname.substr(0, dot);
}

// ---------------------------------------------------------------------------
// Top status row
// ---------------------------------------------------------------------------

// Tier badge color follows the design doc's threat-tier palette: 1 reads
// as a routine civic lock (cyan), 2 as corp-tier (yellow), 3 as military /
// dangerous (red), 4+ as bossfight / blackwall-adjacent (bright magenta).
Color tier_badge_color(int tier) {
    if (tier <= 1) return Color::Cyan;
    if (tier == 2) return Color::Yellow;
    if (tier == 3) return Color::Red;
    return Color::BrightMagenta;
}

void draw_top_status(Game& game, Renderer& r, const WindowRect& wr,
                     const NetSession& s) {
    const int y = wr.y + 1;
    int x = wr.x + 2;

    std::string prefix = "\xe2\x96\xb6 GRID  ";  // ▶ + space + GRID + 2 spaces = 8 visual cells
    draw_colored_string(r, x, y, prefix, Color::Cyan);
    x += visual_width(prefix);

    const auto& meta = game.world().lan_metadata();
    std::string region = upper(meta.display_name.empty()
                               ? std::string("UNKNOWN")
                               : meta.display_name);
    draw_colored_string(r, x, y, region, Color::White);
    x += visual_width(region);

    // Sub-segment retired with multi-region geography. Per-target netspace
    // chrome titles (e.g. "MAGLOCK :: DOOR_47B :: TIER 1") will populate
    // from Netspace::title once Phase 1 grammars start setting it.
    std::string ip_str;
    if (!s.netspace.title.empty()) {
        std::string sep = " \xe2\x80\xba "; // ›
        draw_colored_string(r, x, y, sep, Color::Cyan);
        x += visual_width(sep);
        draw_colored_string(r, x, y, s.netspace.title, Color::White);
        x += visual_width(s.netspace.title);
    }

    if (ip_str.empty()) ip_str = format_ip(meta.subnet_base);

    x += 2;
    if (!ip_str.empty()) {
        draw_colored_string(r, x, y, ip_str, Color::Cyan);
    }

    // Right-side cluster grows from the right edge inward:
    //   [ ... TRACE gauge ... ][ TIME: N meatworld ][ TIER N ]
    // ‖ <- right border at wr.x + wr.w - 1; reserve 1 cell margin.
    int right_edge = wr.x + wr.w - 2;   // last writable column

    // Tier badge — always present.
    std::string tier_label = "TIER " + std::to_string(s.netspace.target.tier);
    int tier_w = visual_width(tier_label);
    int tier_x = right_edge - tier_w + 1;
    draw_colored_string(r, tier_x, y, tier_label,
                        tier_badge_color(s.netspace.target.tier));

    int cluster_left = tier_x;

    // Time-dilation badge — only on title row when there's no subtitle row
    // to host it. With a subtitle, draw_subtitle_row places it there.
    const bool has_subtitle = !s.netspace.title_subtitle.empty();
    if (s.netspace.time_dilation > 1 && !has_subtitle) {
        std::string time_label = "TIME: "
                               + std::to_string(s.netspace.time_dilation)
                               + " meatworld";
        int time_w = visual_width(time_label);
        int time_x = cluster_left - 2 - time_w;
        if (time_x > x + 1) {
            draw_colored_string(r, time_x, y, time_label, Color::DarkGray);
            cluster_left = time_x;
        }
    }

    // Trace gauge — right-justified to the left of the badge cluster.
    // Layout: "TRACE " (6) + 5-seg bar + " NNN%" (5) = 16 cells; keep
    // 2 cells of spacing between the gauge and whatever sits to its right.
    constexpr int kTraceW = 16;
    const int trace_label_x = cluster_left - 2 - kTraceW;
    if (trace_label_x > x + 1) {
        draw_colored_string(r, trace_label_x, y, "TRACE ", Color::Cyan);
        int filled = s.trace * 5 / 100;
        if (filled > 5) filled = 5;
        draw_block_gauge(r, trace_label_x + 6, y, 5, filled, trace_fill_color(s.trace));
        char pct_buf[16];
        std::snprintf(pct_buf, sizeof(pct_buf), " %3d%%", s.trace);
        draw_colored_string(r, trace_label_x + 11, y, pct_buf, Color::Cyan);
    }

    // Title-bar flicker: Stressed+ states scatter § glitches across the
    // title row. Stressed gets 1 glitch cell; Hunted/Critical/Blackwall get 3.
    WindowState ws = s.netspace.window_state;
    if (ws == WindowState::Stressed || ws == WindowState::Hunted ||
        ws == WindowState::Critical || ws == WindowState::Blackwall) {
        int ph = game.hacking().blink_phase();
        int n_glitch = (ws == WindowState::Stressed) ? 1 : 3;
        for (int g = 0; g < n_glitch; ++g) {
            int cx = wr.x + 2 + ((ph * 7 + g * 13) % std::max(1, wr.w - 6));
            if ((ph / 5 + g) % 3 == 0)
                r.draw_glyph(cx, y, "\xc2\xa7", Color::Magenta);
        }
    }
}

// Subtitle / second chrome row — flavour quote on the left, optional
// time-dilation indicator on the right. Called only when the netspace
// declares a non-empty title_subtitle (subtitle_rows_for() == 1).
void draw_subtitle_row(Renderer& r, const WindowRect& wr,
                       const NetSession& s) {
    const int y = wr.y + 2;
    const int x = wr.x + 2;
    if (!s.netspace.title_subtitle.empty()) {
        draw_colored_string(r, x, y, s.netspace.title_subtitle, Color::DarkGray);
    }

    // Time-dilation badge mirrors the title-row TIER position: anchored
    // at the right edge with 1 cell of margin. Only drawn when active.
    if (s.netspace.time_dilation > 1) {
        std::string time_label = "TIME: "
                               + std::to_string(s.netspace.time_dilation)
                               + " meatworld";
        int time_w = visual_width(time_label);
        int time_x = wr.x + wr.w - 2 - time_w + 1;
        int subtitle_end = x + visual_width(s.netspace.title_subtitle);
        if (time_x > subtitle_end + 1) {
            draw_colored_string(r, time_x, y, time_label, Color::DarkGray);
        }
    }
}

// ---------------------------------------------------------------------------
// Deck strip
// ---------------------------------------------------------------------------

void draw_deck_strip(Game& game, Renderer& r, const WindowRect& wr,
                     const NetSession& s, int subtitle_rows) {
    const int y = wr.y + 3 + subtitle_rows;
    int x = wr.x + 2;

    // One block per unit: bar width = attribute max. HP 4/4 = 4 blocks,
    // HEAT 0/12 = 12 blocks (all empty), etc.
    auto clamp_filled = [](int cur, int max) {
        if (cur < 0) return 0;
        if (cur > max) return max;
        return cur;
    };

    // HP
    draw_colored_string(r, x, y, "HP ", Color::Cyan);
    x += 3;
    draw_block_gauge(r, x, y, s.avatar_hp_max, clamp_filled(s.avatar_hp, s.avatar_hp_max), Color::Cyan);
    x += s.avatar_hp_max;
    int hp_pct = s.avatar_hp_max > 0
               ? (s.avatar_hp * 100 / s.avatar_hp_max) : 0;
    std::string hp_str = hp_lie(hp_pct, s.netspace.window_state,
                                static_cast<uint32_t>(game.hacking().blink_phase()));
    char hp_buf[24];
    std::snprintf(hp_buf, sizeof(hp_buf), " %s%% ", hp_str.c_str());
    draw_colored_string(r, x, y, hp_buf,
        (s.netspace.window_state == WindowState::Hunted ||
         s.netspace.window_state == WindowState::Critical ||
         s.netspace.window_state == WindowState::Blackwall)
            ? Color::Magenta : Color::Cyan);
    x += static_cast<int>(std::strlen(hp_buf));

    // RAM
    draw_colored_string(r, x, y, " RAM ", Color::Cyan);
    x += 5;
    Color ram_col = (s.ram < 5) ? Color::DarkGray : Color::Cyan;
    draw_block_gauge(r, x, y, s.ram_max, clamp_filled(s.ram, s.ram_max), ram_col);
    x += s.ram_max;
    int ram_shown = ram_lie(s.ram, s.ram_max, s.netspace.window_state, s.net_turn);
    char ram_buf[24];
    std::snprintf(ram_buf, sizeof(ram_buf), " %2d/%-2d ", ram_shown, s.ram_max);
    draw_colored_string(r, x, y, ram_buf, ram_col);
    x += static_cast<int>(std::strlen(ram_buf));

    // HEAT (queries equipped cyberdeck; plus active-session implant bonus)
    int heat_cur = 0, heat_cap = 0;
    if (auto* deck_slot = game.player().equipment.equipped_cyberdeck()) {
        if (*deck_slot && (*deck_slot)->deck) {
            const auto& cd = *(*deck_slot)->deck;
            heat_cur = cd.heat_current;
            heat_cap = cd.stats.heat_cap + s.heat_cap_bonus;
        }
    }
    draw_colored_string(r, x, y, " HEAT ", Color::Cyan);
    x += 6;
    Color heat_col = (heat_cap > 0 && heat_cur * 100 / heat_cap > 80)
                     ? Color::Magenta : Color::Cyan;
    draw_block_gauge(r, x, y, heat_cap, clamp_filled(heat_cur, heat_cap), heat_col);
    x += heat_cap;
    char heat_buf[24];
    std::snprintf(heat_buf, sizeof(heat_buf), " %2d/%-2d", heat_cur, heat_cap);
    draw_colored_string(r, x, y, heat_buf, heat_col);
}

// ---------------------------------------------------------------------------
// Right log pane
// ---------------------------------------------------------------------------

// Width (in visual cells) of the leading log-line prefix, used as the indent
// for wrapped continuation lines. Recognises the conventions in §4 of the
// spec: "> ", ">> ", and "[TAG] " (e.g. "[ERR] ", "[BLOCK] ").
int detect_log_prefix_width(const std::string& line) {
    // Walk past leading COLOR_BEGIN/END marker bytes so the visible prefix
    // pattern is matched even when the tag has been wrapped in colored().
    size_t i = 0;
    while (i < line.size()) {
        unsigned char b = static_cast<unsigned char>(line[i]);
        if (b == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < line.size()) {
            i += 2;
        } else if (b == static_cast<unsigned char>(COLOR_END)) {
            ++i;
        } else {
            break;
        }
    }
    if (i >= line.size()) return 0;

    if (line[i] == '[') {
        size_t j = i + 1;
        int visible = 1;
        while (j < line.size() && line[j] != ']') {
            unsigned char b = static_cast<unsigned char>(line[j]);
            if (b == static_cast<unsigned char>(COLOR_BEGIN) && j + 1 < line.size()) {
                j += 2;
            } else if (b == static_cast<unsigned char>(COLOR_END)) {
                ++j;
            } else {
                ++j;
                ++visible;
            }
        }
        if (j < line.size() && line[j] == ']') {
            ++visible;            // count ']'
            ++j;
            // Skip any markers between ']' and the trailing space.
            while (j < line.size()) {
                unsigned char b = static_cast<unsigned char>(line[j]);
                if (b == static_cast<unsigned char>(COLOR_END)) { ++j; continue; }
                if (b == static_cast<unsigned char>(COLOR_BEGIN) && j + 1 < line.size()) { j += 2; continue; }
                break;
            }
            if (j < line.size() && line[j] == ' ') {
                return visible + 1; // include the trailing space
            }
        }
        return 0;
    }
    if (line[i] == '>') {
        size_t j = i;
        int visible = 0;
        while (j < line.size() && line[j] == '>') { ++j; ++visible; }
        if (j < line.size() && line[j] == ' ') return visible + 1;
        return 0;
    }
    return 0;
}

// Word-wrap one log line to `width` visible cells. Continuation lines are
// indented by `detect_log_prefix_width(line)` so wrapped output reads as one
// message. Words longer than `width` are hard-broken at the column edge.
std::vector<std::string> wrap_log_line(const std::string& line, int width) {
    std::vector<std::string> out;
    if (width < 4) width = 4;
    int indent = detect_log_prefix_width(line);
    if (indent >= width - 4) indent = 0;  // fall back if prefix dominates

    std::string indent_str(static_cast<size_t>(indent), ' ');
    std::string current;
    int current_w = 0;
    bool first = true;

    auto flush = [&]() {
        if (!current.empty() || first) {
            out.push_back(current);
            current.clear();
            current_w = 0;
            first = false;
        }
    };

    auto emit_indent = [&]() {
        if (out.empty()) return;
        current = indent_str;
        current_w = indent;
    };

    // Tokenise on spaces; treat each contiguous non-space run as a word.
    size_t i = 0;
    while (i < line.size()) {
        // Skip a single leading space so word boundaries align.
        size_t word_start = i;
        while (i < line.size() && line[i] != ' ') ++i;
        std::string word = line.substr(word_start, i - word_start);
        // Consume the following space if any.
        bool had_space = (i < line.size() && line[i] == ' ');
        if (had_space) ++i;

        int word_w = visual_width(word);
        int target_max = first ? width : width;
        // First word on a fresh line just gets emitted (with continuation
        // indent if this is a wrap).
        if (current_w == 0) {
            emit_indent();
            if (word_w > target_max - current_w) {
                // Hard-break long word at column edge.
                size_t pos = 0;
                while (pos < word.size()) {
                    size_t take = 0;
                    int taken_w = 0;
                    while (pos + take < word.size() && taken_w < target_max - current_w) {
                        unsigned char b = static_cast<unsigned char>(word[pos + take]);
                        int len = 1;
                        if      ((b & 0xE0) == 0xC0) len = 2;
                        else if ((b & 0xF0) == 0xE0) len = 3;
                        else if ((b & 0xF8) == 0xF0) len = 4;
                        if (pos + take + len > word.size()) break;
                        take += len;
                        ++taken_w;
                    }
                    current += word.substr(pos, take);
                    current_w += taken_w;
                    pos += take;
                    if (pos < word.size()) {
                        flush();
                        emit_indent();
                    }
                }
            } else {
                current += word;
                current_w += word_w;
            }
        } else if (current_w + 1 + word_w <= target_max) {
            current += ' ';
            current += word;
            current_w += 1 + word_w;
        } else {
            // Word doesn't fit on the current line; flush and retry on a
            // fresh wrapped line.
            flush();
            emit_indent();
            if (word_w > target_max - current_w) {
                // Long word — hard-break.
                size_t pos = 0;
                while (pos < word.size()) {
                    size_t take = 0;
                    int taken_w = 0;
                    while (pos + take < word.size() && taken_w < target_max - current_w) {
                        unsigned char b = static_cast<unsigned char>(word[pos + take]);
                        int len = 1;
                        if      ((b & 0xE0) == 0xC0) len = 2;
                        else if ((b & 0xF0) == 0xE0) len = 3;
                        else if ((b & 0xF8) == 0xF0) len = 4;
                        if (pos + take + len > word.size()) break;
                        take += len;
                        ++taken_w;
                    }
                    current += word.substr(pos, take);
                    current_w += taken_w;
                    pos += take;
                    if (pos < word.size()) {
                        flush();
                        emit_indent();
                    }
                }
            } else {
                current += word;
                current_w += word_w;
            }
        }
        (void)had_space;
    }
    if (!current.empty()) out.push_back(current);
    if (out.empty()) out.push_back(line);
    return out;
}

// Map the leading "[TAG]" token in a log line to a color. Tags not in the
// table fall back to Cyan so unknown bracket tokens still read as metadata.
Color tag_color(const std::string& tag) {
    if (tag == "[ERR]")    return Color::Red;
    if (tag == "[WARN]")   return Color::Red;
    if (tag == "[BLOCK]")  return Color::Yellow;
    if (tag == "[INFO]")   return Color::Cyan;
    if (tag == "[ALERT]")  return Color::BrightMagenta;
    if (tag == "[SYS]")    return Color::Magenta;
    if (tag == "[OK]")     return Color::Green;
    return Color::Cyan;
}

// Wrap any leading "[TAG]" prefix in colored() so it renders in tag_color.
// No-op if the line doesn't start with a bracket tag, or if it's already
// been wrapped in markers.
std::string colorize_leading_tag(const std::string& line) {
    if (line.empty() || line[0] != '[') return line;
    size_t close = line.find(']');
    if (close == std::string::npos) return line;
    std::string tag = line.substr(0, close + 1);
    return colored(tag, tag_color(tag)) + line.substr(close + 1);
}

void draw_log_pane(Renderer& r, const LogPaneRect& lr, const NetSession& s,
                   int phase) {
    int rows = lr.h;
    if (rows < 1) return;
    int max_w = lr.w - 2;
    if (max_w < 4) return;

    // Build the full wrapped buffer (oldest → newest), then take the
    // last `rows` entries. Older messages get dropped when they scroll off.
    std::vector<std::string> wrapped;
    wrapped.reserve(s.log_lines.size() * 2);
    for (const auto& line : s.log_lines) {
        auto pieces = wrap_log_line(colorize_leading_tag(line), max_w);
        for (auto& p : pieces) wrapped.push_back(std::move(p));
    }

    int total = static_cast<int>(wrapped.size());
    int start = std::max(0, total - rows);
    for (int i = 0; i < rows && start + i < total; ++i) {
        draw_colored_string(r, lr.x + 1, lr.y + i, wrapped[start + i], Color::White);
    }

    // Command-line glitch: Hunted+ states corrupt ~20% of the last visible
    // log row with § glyphs, crawling with the blink phase.
    if (s.netspace.window_state == WindowState::Hunted ||
        s.netspace.window_state == WindowState::Critical ||
        s.netspace.window_state == WindowState::Blackwall) {
        int last = std::min(rows, total) - 1;
        if (last >= 0) {
            int yrow = lr.y + last;
            for (int cx = 0; cx < lr.w - 2; ++cx) {
                if ((cx * 31 + phase) % 5 == 0)
                    r.draw_glyph(lr.x + 1 + cx, yrow, "\xc2\xa7", Color::Magenta);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Program bar
// ---------------------------------------------------------------------------

const char* program_abbrev(ProgramId id) {
    switch (id) {
        case ProgramId::IcebreakerLite: return "ICE";
        case ProgramId::Breach:         return "BRC";
        case ProgramId::Decrypt:        return "DEC";
        case ProgramId::PulseHammer:    return "PUL";
        case ProgramId::DaemonHijack:   return "HIJ";
        case ProgramId::GhostTrace:     return "GHO";
        case ProgramId::Cooldown:       return "COD";
        default:                        return "   ";
    }
}

void draw_program_bar(Game& game, Renderer& r, const WindowRect& wr,
                      const NetSession& s) {
    const int y = wr.y + wr.h - 2;
    int x = wr.x + 2;
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) return;
    const auto& cd = *(*deck_slot)->deck;

    int eff_slots = std::min(kCyberdeckMaxSlots,
                             cd.stats.slots + (s.skill_daemon_mastery ? 1 : 0));

    for (int i = 0; i < 8; ++i) {
        char num = static_cast<char>('1' + i);
        r.draw_glyph(x,     y, "\xe2\x94\x83", Color::Cyan); // ┃
        r.draw_char(x + 1,  y, num, Color::Cyan);
        r.draw_glyph(x + 2, y, "\xe2\x94\x83", Color::Cyan);
        x += 3;

        const char* abbrev = "   ";
        Color label_col    = Color::DarkGray;

        if (i < eff_slots && cd.loaded[i].program_def_id != 0) {
            Item probe = build_by_def_id(cd.loaded[i].program_def_id);
            if (probe.program) {
                ProgramId pid  = probe.program->id;
                const auto* def = find_program(pid);
                if (def && def->kind != ProgramKind::Qh) {
                    abbrev = program_abbrev(pid);
                    bool affordable = (s.ram >= def->ram_cost) &&
                                      (cd.heat_current + def->heat_cost <= cd.stats.heat_cap);
                    label_col = affordable ? Color::Cyan : Color::DarkGray;
                }
            }
        }

        // Plan 6: active slot inverse-videos while its Telegraph is open.
        if (i == s.active_slot) {
            for (int k = 0; k < 3; ++k) {
                r.draw_char(x + k, y,
                            (k < static_cast<int>(std::strlen(abbrev))) ? abbrev[k] : ' ',
                            Color::Black, Color::Cyan);
            }
        } else {
            draw_colored_string(r, x, y, abbrev, label_col);
        }
        x += 3;
        x += 2;  // spacing between slots
    }
}

// ---------------------------------------------------------------------------
// Playfield
// ---------------------------------------------------------------------------

void draw_playfield(Game& game, Renderer& r, const PlayfieldRect& pr,
                    const NetSession& s) {
    static NetCamera s_camera;
    s_camera.viewport_w = pr.w;
    s_camera.viewport_h = pr.h;
    s_camera.follow(s.avatar_x, s.avatar_y, s.netspace.w, s.netspace.h);

    auto wall_neigh = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= s.netspace.w || y >= s.netspace.h) return false;
        return s.netspace.is_wall(x, y);
    };
    auto box_neigh = [&](int x, int y, NetTile style) -> bool {
        if (x < 0 || y < 0 || x >= s.netspace.w || y >= s.netspace.h) return false;
        const NetTile t = s.netspace.at(x, y);
        if (t == style) return true;
        // A pipe-port embedded in a box edge is transparent to corner /
        // edge resolution — the flanking box cells should render as if
        // the edge ran continuously through the port (so a ╦/╨/╜ port
        // doesn't strip the adjacent corners down to straight ║).
        return t == NetTile::PipePortV
            || t == NetTile::PipePortCornerTR
            || t == NetTile::PipePortDownD;
    };

    // Per-cell box-glyph resolver: pick corner / edge / junction from
    // the 4-neighbour mask. Only neighbours of the *same* box style
    // count, so adjacent BoxThin + BoxDouble rooms still render with
    // their own corners intact.
    auto box_glyph = [](const net_theme::BoxGlyphs& g, bool n, bool s_, bool e, bool w) -> const char* {
        // Vertical: only vertical neighbours.
        if ((n || s_) && !(e || w)) return g.v;
        // Horizontal: only horizontal neighbours.
        if ((e || w) && !(n || s_)) return g.h;
        // Corners — pick the one whose two arms point toward neighbours.
        if (s_ && e) return g.tl;
        if (s_ && w) return g.tr;
        if (n && e)  return g.bl;
        if (n && w)  return g.br;
        return g.h;  // isolated cell: fall back to horizontal
    };

    auto cull = [&](int wx, int wy, int& sx, int& sy) {
        sx = wx - s_camera.cam_x;
        sy = wy - s_camera.cam_y;
        return sx >= 0 && sy >= 0 && sx < pr.w && sy < pr.h;
    };

    for (int y = 0; y < pr.h; ++y) {
        for (int x = 0; x < pr.w; ++x) {
            int tx = x + s_camera.cam_x;
            int ty = y + s_camera.cam_y;
            if (!s.netspace.in_bounds(tx, ty)) continue;
            NetTile t = s.netspace.at(tx, ty);
            const char* glyph = " ";
            Color       color = net_theme::floor;
            switch (t) {
                case NetTile::Void:
                    glyph = " ";
                    break;
                case NetTile::Floor:
                case NetTile::JackIn:
                    glyph = net_theme::floor_glyph;
                    color = net_theme::floor;
                    break;
                case NetTile::Exit:
                    glyph = net_theme::exit_glyph;
                    color = net_theme::exit_node;
                    break;

                // Wall density gradient.
                case NetTile::WallDot:
                    glyph = net_theme::wall_dot_glyph;
                    color = net_theme::wall_dot;
                    break;
                case NetTile::WallLight:
                    glyph = net_theme::wall_light_glyph;
                    color = net_theme::wall_light;
                    break;
                case NetTile::WallMed:
                    glyph = net_theme::wall_med_glyph;
                    color = net_theme::wall_med;
                    break;
                case NetTile::WallHeavy:
                    glyph = net_theme::wall_heavy_glyph;
                    color = net_theme::wall_heavy;
                    break;
                case NetTile::WallSolid:
                    glyph = wall_glyph_for_neighbours(
                        wall_neigh(tx, ty - 1), wall_neigh(tx, ty + 1),
                        wall_neigh(tx + 1, ty), wall_neigh(tx - 1, ty));
                    color = net_theme::wall_solid;
                    break;

                case NetTile::Breakwall: {
                    auto it = s.netspace.breakwall_lookup.find({tx, ty});
                    if (it != s.netspace.breakwall_lookup.end()) {
                        const BreakwallGroup& g = s.netspace.breakwalls[it->second];
                        switch (g.current_density) {
                            case 1: glyph = net_theme::wall_dot_glyph;   color = net_theme::wall_dot;   break;
                            case 2: glyph = net_theme::wall_light_glyph; color = net_theme::wall_light; break;
                            case 3: glyph = net_theme::wall_med_glyph;   color = net_theme::wall_med;   break;
                            case 4: glyph = net_theme::wall_heavy_glyph; color = net_theme::wall_heavy; break;
                            case 5: glyph = net_theme::wall_solid_glyph; color = net_theme::wall_solid; break;
                            default: glyph = " "; color = net_theme::floor; break;
                        }
                    } else {
                        // Orphan Breakwall — shouldn't happen; render as solid wall defensively.
                        glyph = net_theme::wall_solid_glyph;
                        color = net_theme::wall_solid;
                    }
                    break;
                }

                // Box borders — resolve per-cell from same-style neighbours.
                case NetTile::BoxThin:
                    glyph = box_glyph(net_theme::box_thin,
                        box_neigh(tx, ty - 1, NetTile::BoxThin),
                        box_neigh(tx, ty + 1, NetTile::BoxThin),
                        box_neigh(tx + 1, ty, NetTile::BoxThin),
                        box_neigh(tx - 1, ty, NetTile::BoxThin));
                    color = net_theme::box_thin_color;
                    break;
                case NetTile::BoxDouble:
                    glyph = box_glyph(net_theme::box_double,
                        box_neigh(tx, ty - 1, NetTile::BoxDouble),
                        box_neigh(tx, ty + 1, NetTile::BoxDouble),
                        box_neigh(tx + 1, ty, NetTile::BoxDouble),
                        box_neigh(tx - 1, ty, NetTile::BoxDouble));
                    color = net_theme::box_double_color;
                    break;
                case NetTile::BoxBlock:
                    glyph = box_glyph(net_theme::box_block,
                        box_neigh(tx, ty - 1, NetTile::BoxBlock),
                        box_neigh(tx, ty + 1, NetTile::BoxBlock),
                        box_neigh(tx + 1, ty, NetTile::BoxBlock),
                        box_neigh(tx - 1, ty, NetTile::BoxBlock));
                    color = net_theme::box_block_color;
                    break;

                // Animated data pipes — phase keyed off the frame counter
                // (hacking_.blink_phase ticks ~60Hz regardless of world
                // turns) so the pulse keeps moving even while the player
                // is idle. 9 frames per phase ≈ 150ms cycle step.
                case NetTile::PipeH: {
                    int phase = (game.hacking().blink_phase() / 9) & 3;
                    glyph = net_theme::pipe_h_frames[phase];
                    color = net_theme::pipe_color;
                    break;
                }
                case NetTile::PipeV: {
                    int phase = (game.hacking().blink_phase() / 9) & 3;
                    glyph = net_theme::pipe_v_frames[phase];
                    color = net_theme::pipe_color;
                    break;
                }
                case NetTile::PipeJunc:
                    glyph = net_theme::pipe_junc_glyph;
                    color = net_theme::pipe_color;
                    break;
                case NetTile::PipePortV:
                    glyph = net_theme::pipe_port_v_glyph;
                    color = net_theme::pipe_color;
                    break;
                case NetTile::PipePortCornerTR:
                    glyph = net_theme::pipe_port_corner_tr_glyph;
                    color = net_theme::pipe_color;
                    break;
                case NetTile::PipePortDownD:
                    glyph = net_theme::pipe_port_down_d_glyph;
                    color = net_theme::pipe_color;
                    break;

                case NetTile::Glyph:
                    // Phase 1 plan §1: per-tile glyph overrides land with NetRoom
                    // content rendering in Step 2; until then this is a no-op.
                    glyph = " ";
                    break;
            }

            // NetBreakwallGlitch override: when a breakwall tile is mid-glitch,
            // override the resolved glyph/color with a chaos glyph from net_theme
            // and a density-keyed color. Self-pruning — query_effect returns
            // nullopt once the 800ms window expires.
            if (auto q = s.animations.query_effect(tx, ty)) {
                if (q->type == AnimationType::NetBreakwallGlitch) {
                    auto it = s.netspace.breakwall_lookup.find({tx, ty});
                    if (it != s.netspace.breakwall_lookup.end()) {
                        const BreakwallGroup& g = s.netspace.breakwalls[it->second];
                        glyph = net_theme::wall_glitch_glyph(tx, ty, q->frame_index);
                        // During the glitch window, paint in the pre-demote color — the
                        // post-demote color appears naturally once the animation ends.
                        // current_density has already been decremented by apply_breach_grid,
                        // so +1 recovers the prior density. Clamped just in case.
                        const uint8_t shade_density = static_cast<uint8_t>(
                            std::min<int>(5, g.current_density + 1));
                        color = net_theme::shade_for_density(shade_density, q->frame_index);
                    }
                }
            }

            r.draw_glyph(pr.x + x, pr.y + y, glyph, color);
        }
    }

    // Room overlay layer — interior text rows drawn on top of the tile
    // grid. The border tiles themselves were stamped at gen time and
    // rendered by the switch above; we only add the centered text here.
    auto draw_centered = [&](int rx, int ry, int rw,
                             const std::string& text, Color col) {
        if (text.empty()) return;
        int sx, sy;
        if (!cull(rx, ry, sx, sy)) return;
        // UTF-8-aware width: count non-continuation bytes.
        int vw = 0;
        for (unsigned char ch : text) if ((ch & 0xC0) != 0x80) ++vw;
        int start_screen_x = sx + (rw - vw) / 2;
        if (start_screen_x < 0) start_screen_x = 0;
        draw_colored_string(r, pr.x + start_screen_x, pr.y + sy, text, col);
    };
    for (const auto& room : s.netspace.rooms) {
        if (room.w < 3 || room.h < 3) continue;
        const int inner_w = room.w - 2;

        // Slot positions per interior height. See NetRoom comment.
        int top_y = 0, label_y = 0, bottom_y = 0;
        if (room.h >= 5) {
            top_y    = room.y + 1;
            label_y  = room.y + 2;
            bottom_y = room.y + 3;
        } else if (room.h == 4) {
            top_y    = -1;            // skip
            label_y  = room.y + 1;
            bottom_y = room.y + 2;
        } else { // h == 3 — a single interior row, prefer label
            top_y    = -1;
            label_y  = room.y + 1;
            bottom_y = -1;
        }

        if (top_y >= 0 && !room.top_content.empty()) {
            draw_centered(room.x + 1, top_y, inner_w,
                          room.top_content, room.top_color);
        }
        if (label_y >= 0 && !room.label.empty()) {
            draw_centered(room.x + 1, label_y, inner_w,
                          room.label, room.label_color);
        } else if (label_y >= 0 && !room.top_content.empty() && room.h == 3) {
            // h=3 with only top_content — fall back to it.
            draw_centered(room.x + 1, label_y, inner_w,
                          room.top_content, room.top_color);
        }
        if (bottom_y >= 0 && !room.bottom_content.empty()) {
            draw_centered(room.x + 1, bottom_y, inner_w,
                          room.bottom_content, room.bottom_color);
        }
    }

    // Ambient overlay scaffold — currently no variants ship. Later
    // phases (Blackwall drift, trace corruption) hook in here.

    for (const auto& ice : s.ice) {
        int sx, sy;
        if (!cull(ice.x, ice.y, sx, sy)) continue;
        const char* g = ice.color == IceColor::White ? net_theme::white_ice_glyph
                      : ice.color == IceColor::Gray  ? net_theme::gray_ice_glyph
                      :                                 net_theme::black_ice_glyph;
        Color c = ice.color == IceColor::White ? net_theme::white_ice
                : ice.color == IceColor::Gray  ? net_theme::gray_ice
                :                                net_theme::black_ice;
        r.draw_glyph(pr.x + sx, pr.y + sy, g, c);
    }

    {
        int sx, sy;
        if (cull(s.avatar_x, s.avatar_y, sx, sy)) {
            r.draw_glyph(pr.x + sx, pr.y + sy,
                         net_theme::avatar_glyph, net_theme::avatar);
        }
    }

    if (game.telegraph().active()) {
        game.telegraph().render(&r, s_camera.cam_x, s_camera.cam_y,
                                pr.w, pr.h, pr.x, pr.y);
    }
}

void draw_window_sequence(Renderer& r, const WindowRect& wr,
                          const NetSession& s, int phase) {
    const auto& q = s.window_seq;
    auto fill = [&](const char* g, Color c) {
        for (int j=1;j<wr.h-1;++j) for (int i=1;i<wr.w-1;++i)
            r.draw_glyph(wr.x+i, wr.y+j, g, c);
    };
    auto center = [&](int row, const std::string& t, Color c) {
        int sx = wr.x + (wr.w - (int)t.size())/2;
        draw_colored_string(r, sx, wr.y + row, t, c);
    };
    if (q.kind == WindowSeqKind::Opening) {
        switch (q.frame_index) {
            case 0: case 1: {
                int hr = 88 + q.frame_index*4 + (phase/5)%3;
                std::string hdr = "JACKING IN :: " + (s.netspace.title.empty()
                                  ? std::string("TARGET") : s.netspace.title);
                draw_colored_string(r, wr.x+2, wr.y+1, hdr, Color::Cyan);
                char hb[32]; std::snprintf(hb,sizeof(hb),"heart rate %d", hr);
                draw_colored_string(r, wr.x+wr.w-2-(int)std::strlen(hb), wr.y+1,
                                    hb, Color::Magenta);
                const char* msg[] = {"establishing handshake...",
                                     "parsing reality offset..."};
                center(wr.h/2, msg[q.frame_index], Color::Cyan);
                break;
            }
            case 2: fill("\xe2\x96\x93", Color::Cyan);
                    center(wr.h-2, "neural sync", Color::Cyan); break;
            case 3: {
                for (int j=1;j<wr.h-1;++j) for (int i=1;i<wr.w-1;++i)
                    if (((i*7+j*13+phase)%9)==0)
                        r.draw_glyph(wr.x+i,wr.y+j,"\xe2\x96\x92",Color::Cyan);
                center(wr.h/2, "@", Color::BrightWhite);
                center(wr.h-2, "consciousness migrating", Color::Cyan); break;
            }
            case 4: {
                int bx=wr.x+wr.w/2-7, by=wr.y+wr.h/2-3;
                for (int i=0;i<14;++i){ r.draw_glyph(bx+i,by,"\xe2\x95\x8c",Color::DarkGray);
                    r.draw_glyph(bx+i,by+6,"\xe2\x95\x8c",Color::DarkGray);}
                for (int j=0;j<6;++j){ r.draw_glyph(bx,by+j,"\xe2\x95\x8e",Color::DarkGray);
                    r.draw_glyph(bx+14,by+j,"\xe2\x95\x8e",Color::DarkGray);}
                r.draw_glyph(bx+7,by+3,"@",Color::BrightWhite);
                center(wr.h-2, "resolving topology...", Color::Cyan); break;
            }
            default: break;   // case 5: instant; loop clears, normal render next frame
        }
    }
    else if (q.kind == WindowSeqKind::ClosingNormal) {
        switch (q.frame_index) {
            case 1: {
                int bx = wr.x + wr.w/2 - 7, by = wr.y + wr.h/2 - 3;
                for (int i = 0; i < 14; ++i) {
                    r.draw_glyph(bx+i, by,   "\xe2\x95\x8c", Color::DarkGray);
                    r.draw_glyph(bx+i, by+6, "\xe2\x95\x8c", Color::DarkGray);
                }
                for (int j = 0; j < 6; ++j) {
                    r.draw_glyph(bx,    by+j, "\xe2\x95\x8e", Color::DarkGray);
                    r.draw_glyph(bx+14, by+j, "\xe2\x95\x8e", Color::DarkGray);
                }
                center(wr.h-2, "disconnecting...", Color::Cyan);
                break;
            }
            case 2:
                for (int j = 1; j < wr.h-1; ++j)
                    for (int i = 1; i < wr.w-1; ++i)
                        if (((i*7 + j*13 + phase) % 9) == 0)
                            r.draw_glyph(wr.x+i, wr.y+j, "\xe2\x96\x92", Color::Cyan);
                break;
            case 3: fill("\xe2\x96\x93", Color::Cyan); break;
            case 4: fill(" ", Color::DarkGray); break;
            default: break;
        }
    }
    else if (q.kind == WindowSeqKind::ClosingPanic) {
        if (q.frame_index >= 3) {
            fill("\xe2\x96\x88", Color::Red);
        } else {
            for (int j = 1; j < wr.h-1; ++j)
                for (int i = 1; i < wr.w-1; ++i)
                    if (((i + j + phase) % 2) == 0)
                        r.draw_glyph(wr.x+i, wr.y+j, "\xe2\x96\x93", Color::Red);
            center(wr.h/2, "DISCONNECT", Color::Red);
        }
    }
    else if (q.kind == WindowSeqKind::ForcedHold) {
        fill("\xe2\x96\x88", Color::Red);
        center(wr.h/2 - 1, "AVATAR LOST", Color::BrightWhite);
        center(wr.h/2 + 1, "connection severed", Color::Red);
    }
    else if (q.kind == WindowSeqKind::BlackIceTakeover) {
        // Black interior, then a centered heavy box + lethal announcement.
        for (int j=1;j<wr.h-1;++j) for (int i=1;i<wr.w-1;++i)
            r.draw_glyph(wr.x+i, wr.y+j, " ", Color::Black);
        int bw = 14, bh = 8;
        int bx = wr.x + (wr.w - bw)/2;
        int by = wr.y + (wr.h - bh)/2;
        for (int i=0;i<bw;++i) {
            r.draw_glyph(bx+i, by,        "\xe2\x96\x93", Color::Red);
            r.draw_glyph(bx+i, by+bh-1,   "\xe2\x96\x93", Color::Red);
        }
        for (int j=0;j<bh;++j) {
            r.draw_glyph(bx,        by+j, "\xe2\x96\x93", Color::Red);
            r.draw_glyph(bx+bw-1,   by+j, "\xe2\x96\x93", Color::Red);
        }
        center(wr.h/2 - 1, "YOU",  Color::BrightWhite);
        center(wr.h/2,     "ARE",  Color::BrightWhite);
        center(wr.h/2 + 1, "SEEN", Color::BrightWhite);
        center(wr.h/2 + 4, "BLACK ICE",        Color::Red);
        center(wr.h/2 + 5, "initiated lethal", Color::Red);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public render
// ---------------------------------------------------------------------------

void render(Game& game, Renderer& r) {
    const auto* sess = game.hacking().session();
    if (!sess) return;

    int sw = r.get_width();
    int sh = r.get_height();
    WindowRect    wr = compute_window_rect(sw, sh);
    const int     sub_rows = subtitle_rows_for(sess->netspace);
    PlayfieldRect pr = compute_playfield_rect(wr, sub_rows);
    LogPaneRect   lr = compute_log_pane_rect(wr, sub_rows);

    // Make the window opaque first so the monochrome world UI behind
    // doesn't bleed through into the Tron HUD.
    clear_window_interior(r, wr);

    // Full-window scripted sequence (jack-in ritual, jack-out, takeover).
    // The sequence owns the entire interior; skip normal layout while active.
    if (sess->window_seq.active()) {
        draw_window_sequence(r, wr, *sess, game.hacking().blink_phase());
        return;   // sequence owns the whole window; skip normal layout
    }

    // Chrome — outer border + horizontal separators + column split. With
    // a subtitle row inserted at row 2, every downstream row shifts down
    // by sub_rows; without one, the layout is unchanged.
    //
    // At Critical/Blackwall the interior separators flicker out on a
    // phase-dependent schedule so the chrome feels unstable while the
    // playfield and vitals remain fully legible.
    auto flicker_out = [&](int salt) {
        WindowState ws = sess->netspace.window_state;
        if (ws != WindowState::Critical && ws != WindowState::Blackwall) return false;
        int ph = game.hacking().blink_phase();
        return ((ph / 4 + salt * 3) % 5) == 0;   // ~1 frame in 5, staggered
    };
    draw_window_chrome(r, wr, sess->netspace.window_state,
                       game.hacking().blink_phase());
    if (!flicker_out(0)) draw_horizontal_separator(r, wr, 2 + sub_rows);       // below title (+ subtitle)
    if (!flicker_out(1)) draw_horizontal_separator(r, wr, 4 + sub_rows);       // below deck strip
    if (!flicker_out(2)) draw_horizontal_separator(r, wr, wr.h - 3);           // above program bar
    if (!flicker_out(3)) draw_column_split(r, wr, sub_rows);

    // Populated layout slots.
    draw_top_status(game, r, wr, *sess);
    if (sub_rows > 0) draw_subtitle_row(r, wr, *sess);
    draw_deck_strip(game, r, wr, *sess, sub_rows);

    draw_playfield(game, r, pr, *sess);
    draw_log_pane(r, lr, *sess, game.hacking().blink_phase());
    draw_program_bar(game, r, wr, *sess);

    if (sess->netspace.window_state == WindowState::Blackwall &&
        !sess->window_seq.active()) {
        int ph = game.hacking().blink_phase();
        const char* zal[] = { "\xce\xa3", "\xce\xa8" };   // Σ Ψ — out-of-vocabulary
        for (int k=0;k<5;++k) {
            int i = (ph*3 + k*29) % (wr.w-2) + 1;
            r.draw_glyph(wr.x+i, wr.y, zal[k&1], Color::Green);
        }
    }
}

} // namespace astra::net_renderer
