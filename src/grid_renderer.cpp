#include "astra/grid_renderer.h"

#include "astra/cyberdeck.h"
#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/grid_camera.h"
#include "astra/grid_ice.h"
#include "astra/grid_session.h"
#include "astra/grid_theme.h"
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

namespace astra::grid_theme {

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

} // namespace astra::grid_theme

namespace astra::grid_renderer {

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

bool is_connectable(GridTile t) {
    return t == GridTile::Wall || t == GridTile::Connector;
}

const char* glyph_for(GridTile t) {
    using namespace grid_theme;
    switch (t) {
        case GridTile::Floor:           return floor_glyph;
        case GridTile::Firewall:        return firewall_glyph;
        case GridTile::DataNode:        return data_node_glyph;
        case GridTile::Gateway:         return gateway_glyph;
        case GridTile::ExitNode:        return exit_glyph;
        case GridTile::EncryptedFile:   return encrypted_glyph;
        case GridTile::Wall:            return " ";
        case GridTile::Connector:       return connector_glyph;
        case GridTile::DeepGridGateway: return deep_grid_gateway_glyph;
        case GridTile::WarpAnchor:      return warp_anchor_glyph;
        case GridTile::DeviceAvatar:    return " ";
    }
    return " ";
}

Color color_for(GridTile t) {
    using namespace grid_theme;
    switch (t) {
        case GridTile::Floor:           return floor;
        case GridTile::Firewall:        return firewall;
        case GridTile::DataNode:        return data_node;
        case GridTile::Gateway:         return gateway;
        case GridTile::ExitNode:        return exit_node;
        case GridTile::EncryptedFile:   return encrypted;
        case GridTile::Wall:            return floor;
        case GridTile::Connector:       return connector;
        case GridTile::DeepGridGateway: return deep_grid_gateway;
        case GridTile::WarpAnchor:      return warp_anchor;
        case GridTile::DeviceAvatar:    return Color::BrightWhite;
    }
    return Color::White;
}

// ---------------------------------------------------------------------------
// Window geometry
// ---------------------------------------------------------------------------

struct WindowRect    { int x, y, w, h; };
struct PlayfieldRect { int x, y, w, h; };
struct LogPaneRect   { int x, y, w, h; };

constexpr Color kChrome  = Color::Cyan;
constexpr int   kLogPaneW = 36;

WindowRect compute_window_rect(int screen_w, int screen_h) {
    int w = screen_w * 7 / 10;
    int h = screen_h * 7 / 10;
    if (w < 50) w = std::min(50, screen_w);
    if (h < 18) h = std::min(18, screen_h);
    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;
    return {x, y, w, h};
}

PlayfieldRect compute_playfield_rect(const WindowRect& wr) {
    return { wr.x + 1,
             wr.y + 5,
             wr.w - 2 - kLogPaneW - 1,
             wr.h - 5 - 3 };
}

LogPaneRect compute_log_pane_rect(const WindowRect& wr) {
    return { wr.x + wr.w - 1 - kLogPaneW,
             wr.y + 5,
             kLogPaneW,
             wr.h - 5 - 3 };
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

void draw_window_chrome(Renderer& r, const WindowRect& wr) {
    r.draw_glyph(wr.x,            wr.y,            "\xe2\x95\x94", kChrome);
    r.draw_glyph(wr.x + wr.w - 1, wr.y,            "\xe2\x95\x97", kChrome);
    r.draw_glyph(wr.x,            wr.y + wr.h - 1, "\xe2\x95\x9a", kChrome);
    r.draw_glyph(wr.x + wr.w - 1, wr.y + wr.h - 1, "\xe2\x95\x9d", kChrome);
    for (int i = 1; i < wr.w - 1; ++i) {
        r.draw_glyph(wr.x + i, wr.y,            "\xe2\x95\x90", kChrome);
        r.draw_glyph(wr.x + i, wr.y + wr.h - 1, "\xe2\x95\x90", kChrome);
    }
    for (int j = 1; j < wr.h - 1; ++j) {
        r.draw_glyph(wr.x,            wr.y + j, "\xe2\x95\x91", kChrome);
        r.draw_glyph(wr.x + wr.w - 1, wr.y + j, "\xe2\x95\x91", kChrome);
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

void draw_column_split(Renderer& r, const WindowRect& wr) {
    int x = wr.x + wr.w - 1 - kLogPaneW - 1;
    for (int j = wr.y + 5; j < wr.y + wr.h - 3; ++j) {
        r.draw_glyph(x, j, "\xe2\x95\x91", kChrome);
    }
    r.draw_glyph(x, wr.y + 4,           "\xe2\x95\xa6", kChrome); // ╦
    r.draw_glyph(x, wr.y + wr.h - 3,    "\xe2\x95\xa9", kChrome); // ╩
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
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
        if (b < 0x80) {
            r.draw_char(cursor, y, static_cast<char>(b), c);
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
            r.draw_glyph(cursor, y, buf, c);
            i += len;
            ++cursor;
        }
    }
}

// Visual cell width of a UTF-8 string: ASCII = 1, every multi-byte code
// point = 1. Caller is responsible for advancing x by this much.
int visual_width(const std::string& text) {
    int w = 0;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
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

void draw_top_status(Game& game, Renderer& r, const WindowRect& wr,
                     const GridSession& s) {
    const int y = wr.y + 1;
    int x = wr.x + 2;

    std::string prefix = "\xe2\x96\xb6 GRID  ";  // ▶ + space + GRID + 2 spaces = 8 visual cells
    draw_colored_string(r, x, y, prefix, Color::Cyan);
    x += visual_width(prefix);

    const auto& meta = game.world().lan_metadata();
    std::string region = upper(meta.display_name.empty()
                               ? std::string("UNKNOWN")
                               : meta.display_name);
    draw_colored_string(r, x, y, region, Color::Cyan);
    x += visual_width(region);

    // Sub-segment: device hostname when in a Subnet, ATLAS / YOUR.ANCHOR
    // when in the deep-Grid anchor.
    const auto& net = game.world().grid_network();
    const GridNode* node = net.find(s.current_node);
    std::string ip_str;
    if (node) {
        if (node->kind == GridNodeKind::Subnet) {
            ip_str = node->label;  // node label IS the IP for Subnet nodes
            if (auto parsed = parse_ip(node->label)) {
                if (auto* h = game.world().find_hackable_by_ip(*parsed)) {
                    std::string host = lan_hostname(*h, meta);
                    std::string sub_label = upper(short_host_label(host));
                    std::string sub = " \xe2\x80\xba " + sub_label; // ›
                    draw_colored_string(r, x, y, sub, Color::Cyan);
                    x += visual_width(sub);
                }
            }
        } else if (node->kind == GridNodeKind::DeepGridAnchor) {
            std::string sub = " \xe2\x80\xba ";
            sub += (node->owned_by_consciousness_id != 0) ? "YOUR.ANCHOR" : "ATLAS";
            draw_colored_string(r, x, y, sub, Color::Cyan);
            x += visual_width(sub);
        }
    }
    if (ip_str.empty()) ip_str = format_ip(meta.subnet_base);

    x += 2;
    if (!ip_str.empty()) {
        draw_colored_string(r, x, y, ip_str, Color::Cyan);
    }

    // Trace gauge — right-justified within the top status row.
    // Layout: "TRACE " (6) + 5-seg bar + " NNN%" (5) = 16 cells; reserve a
    // 1-cell margin to clear the right border. Anchor at wr.w - 17.
    const int trace_label_x = wr.x + wr.w - 17;
    if (trace_label_x > x + 1) {
        draw_colored_string(r, trace_label_x, y, "TRACE ", Color::Cyan);
        int filled = s.trace * 5 / 100;
        if (filled > 5) filled = 5;
        draw_block_gauge(r, trace_label_x + 6, y, 5, filled, trace_fill_color(s.trace));
        char pct_buf[16];
        std::snprintf(pct_buf, sizeof(pct_buf), " %3d%%", s.trace);
        draw_colored_string(r, trace_label_x + 11, y, pct_buf, Color::Cyan);
    }
}

// ---------------------------------------------------------------------------
// Deck strip
// ---------------------------------------------------------------------------

void draw_deck_strip(Game& game, Renderer& r, const WindowRect& wr,
                     const GridSession& s) {
    const int y = wr.y + 3;
    int x = wr.x + 2;

    // HP — 10-segment bar
    draw_colored_string(r, x, y, "HP ", Color::Cyan);
    x += 3;
    int hp_filled = s.avatar_hp_max > 0 ? s.avatar_hp * 10 / s.avatar_hp_max : 0;
    if (hp_filled > 10) hp_filled = 10;
    draw_block_gauge(r, x, y, 10, hp_filled, Color::Cyan);
    x += 10;
    char hp_buf[24];
    std::snprintf(hp_buf, sizeof(hp_buf), " %3d/%-3d ", s.avatar_hp, s.avatar_hp_max);
    draw_colored_string(r, x, y, hp_buf, Color::Cyan);
    x += static_cast<int>(std::strlen(hp_buf));

    // RAM — 5-segment bar
    draw_colored_string(r, x, y, " RAM ", Color::Cyan);
    x += 5;
    int ram_filled = s.ram_max > 0 ? s.ram * 5 / s.ram_max : 0;
    if (ram_filled > 5) ram_filled = 5;
    Color ram_col = (s.ram < 5) ? Color::DarkGray : Color::Cyan;
    draw_block_gauge(r, x, y, 5, ram_filled, ram_col);
    x += 5;
    char ram_buf[24];
    std::snprintf(ram_buf, sizeof(ram_buf), " %2d/%-2d ", s.ram, s.ram_max);
    draw_colored_string(r, x, y, ram_buf, ram_col);
    x += static_cast<int>(std::strlen(ram_buf));

    // HEAT — 5-segment bar (queries equipped cyberdeck)
    int heat_cur = 0, heat_cap = 0;
    if (auto* deck_slot = game.player().equipment.equipped_cyberdeck()) {
        if (*deck_slot && (*deck_slot)->deck) {
            const auto& cd = *(*deck_slot)->deck;
            heat_cur = cd.heat_current;
            heat_cap = cd.stats.heat_cap;
        }
    }
    draw_colored_string(r, x, y, " HEAT ", Color::Cyan);
    x += 6;
    int heat_filled = heat_cap > 0 ? heat_cur * 5 / heat_cap : 0;
    if (heat_filled > 5) heat_filled = 5;
    Color heat_col = (heat_cap > 0 && heat_cur * 100 / heat_cap > 80)
                     ? Color::Magenta : Color::Cyan;
    draw_block_gauge(r, x, y, 5, heat_filled, heat_col);
    x += 5;
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
    if (line.empty()) return 0;
    if (line[0] == '[') {
        size_t close = line.find(']');
        if (close != std::string::npos &&
            close + 1 < line.size() &&
            line[close + 1] == ' ') {
            return static_cast<int>(close + 2);  // e.g. "[ERR] " → 6
        }
        return 0;
    }
    if (line[0] == '>') {
        size_t i = 0;
        while (i < line.size() && line[i] == '>') ++i;
        if (i < line.size() && line[i] == ' ') return static_cast<int>(i + 1);
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

void draw_log_pane(Renderer& r, const LogPaneRect& lr, const GridSession& s) {
    draw_colored_string(r, lr.x + 1, lr.y, "[F1] Messages", Color::Cyan);

    int rows = lr.h - 1;
    if (rows < 1) return;
    int max_w = lr.w - 2;
    if (max_w < 4) return;

    // Build the full wrapped buffer (oldest → newest), then take the
    // last `rows` entries. Older messages get dropped when they scroll off.
    std::vector<std::string> wrapped;
    wrapped.reserve(s.log_lines.size() * 2);
    for (const auto& line : s.log_lines) {
        auto pieces = wrap_log_line(line, max_w);
        for (auto& p : pieces) wrapped.push_back(std::move(p));
    }

    int total = static_cast<int>(wrapped.size());
    int start = std::max(0, total - rows);
    for (int i = 0; i < rows && start + i < total; ++i) {
        draw_colored_string(r, lr.x + 1, lr.y + 1 + i, wrapped[start + i], Color::Cyan);
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
                      const GridSession& s) {
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
                    const GridSession& s) {
    static GridCamera s_camera;
    s_camera.viewport_w = pr.w;
    s_camera.viewport_h = pr.h;
    s_camera.follow(s.avatar_x, s.avatar_y, s.sector.w, s.sector.h);

    auto neigh = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= s.sector.w || y >= s.sector.h) return false;
        return is_connectable(s.sector.at(x, y));
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
            if (tx < 0 || ty < 0 || tx >= s.sector.w || ty >= s.sector.h) continue;
            GridTile t = s.sector.at(tx, ty);
            const char* glyph;
            Color       color;
            if (t == GridTile::DeviceAvatar) {
                glyph = grid_theme::device_avatar_glyph(s.sector.source_fixture_type);
                color = Color::BrightWhite;
            } else if (t == GridTile::Wall || t == GridTile::Connector) {
                glyph = wall_glyph_for_neighbours(
                    neigh(tx, ty - 1), neigh(tx, ty + 1),
                    neigh(tx + 1, ty), neigh(tx - 1, ty));
                color = (t == GridTile::Connector) ? grid_theme::connector : grid_theme::floor;
            } else {
                glyph = glyph_for(t);
                color = color_for(t);
            }
            r.draw_glyph(pr.x + x, pr.y + y, glyph, color);
        }
    }

    for (const auto& ice : s.ice) {
        int sx, sy;
        if (!cull(ice.x, ice.y, sx, sy)) continue;
        const char* g = ice.color == IceColor::White ? grid_theme::white_ice_glyph
                      : ice.color == IceColor::Gray  ? grid_theme::gray_ice_glyph
                      :                                 grid_theme::black_ice_glyph;
        Color c = ice.color == IceColor::White ? grid_theme::white_ice
                : ice.color == IceColor::Gray  ? grid_theme::gray_ice
                :                                grid_theme::black_ice;
        r.draw_glyph(pr.x + sx, pr.y + sy, g, c);
    }

    {
        int sx, sy;
        if (cull(s.avatar_x, s.avatar_y, sx, sy)) {
            r.draw_glyph(pr.x + sx, pr.y + sy,
                         grid_theme::avatar_glyph, grid_theme::avatar);
        }
    }

    if (game.telegraph().active()) {
        game.telegraph().render(&r, s_camera.cam_x, s_camera.cam_y,
                                pr.w, pr.h, pr.x, pr.y);
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
    PlayfieldRect pr = compute_playfield_rect(wr);
    LogPaneRect   lr = compute_log_pane_rect(wr);

    // Make the window opaque first so the monochrome world UI behind
    // doesn't bleed through into the Tron HUD.
    clear_window_interior(r, wr);

    // Chrome — outer border + horizontal separators + column split.
    draw_window_chrome(r, wr);
    draw_horizontal_separator(r, wr, 2);              // below top status (row 1)
    draw_horizontal_separator(r, wr, 4);              // below deck strip (row 3)
    draw_horizontal_separator(r, wr, wr.h - 3);       // above program bar
    draw_column_split(r, wr);

    // Populated layout slots.
    draw_top_status(game, r, wr, *sess);
    draw_deck_strip(game, r, wr, *sess);

    // Plan 7 §3b: while a DeviceShell is open via the in-Grid doorway,
    // swap the playfield content for the shell terminal. HUD chrome
    // (Trace/Heat panes, log pane, program bar, borders) stays put — the
    // log pane keeps streaming Trace/Heat updates so the player can see
    // the cost of long-channels in-Grid.
    const auto& shell = game.hacking().device_shell();
    bool grid_shell = shell.is_open() && shell.via() == ShellVia::Grid;
    if (grid_shell) {
        shell.render_into(&r, Rect{pr.x, pr.y, pr.w, pr.h}, game);
    } else {
        draw_playfield(game, r, pr, *sess);
    }
    draw_log_pane(r, lr, *sess);
    draw_program_bar(game, r, wr, *sess);
}

} // namespace astra::grid_renderer
