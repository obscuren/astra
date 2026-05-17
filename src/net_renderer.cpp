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

constexpr Color kChrome  = Color::Cyan;

WindowRect compute_window_rect(int screen_w, int screen_h) {
    int w = screen_w * 8 / 10;
    int h = screen_h * 8 / 10;
    if (w < 50) w = std::min(50, screen_w);
    if (h < 18) h = std::min(18, screen_h);
    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;
    return {x, y, w, h};
}

// ---------------------------------------------------------------------------
// Band geometry  (Phase 5 horizontal layout)
// ---------------------------------------------------------------------------

struct Rect { int x, y, w, h; };
struct NetBands {
    Rect header;   // 1 row
    Rect field;    // elastic
    Rect caption;  // 1 row
    Rect deck;     // 1 header row + eff_slots rows
    Rect vitals;   // 1 row
    Rect log;      // kLogRows rows
    Rect footer;   // 1 row — row for the meatworld-clock footer — consumed in Slice 1 Task 8
};
constexpr int kLogRows = 3;

// `deck_slots` = effective cyberdeck slots (a [ DECK ] header row is added on top).
NetBands compute_bands(const WindowRect& wr, int deck_slots) {
    const int ix = wr.x + 1;          // interior left
    const int iw = wr.w - 2;          // interior width
    const int deck_h = 1 + deck_slots;
    // bottom_block = caption(1) + sep + deck(deck_h) + sep + vitals(1) + sep + log(kLogRows) + sep + footer(1)
    // = deck_h + kLogRows + 7; footer is pinned at wr.h-2, no separator below it.
    const int bottom_block = 1 /*footer*/ + 1 + kLogRows + 1 + 1 /*vitals*/
                           + 1 + deck_h + 1 + 1 /*caption*/;
    NetBands b{};
    b.header  = { ix, wr.y + 1, iw, 1 };
    int y = wr.y + 3;                 // after header (row +1) + its separator (row +2)
    const int field_h = std::max(1, wr.h - 2 /*chrome*/ - 1 /*header*/
                                 - 1 /*hdr sep*/ - bottom_block);
    b.field   = { ix, y, iw, field_h };          y += field_h;
    b.caption = { ix, y, iw, 1 };                y += 1 + 1; // caption + its sep
    b.deck    = { ix, y, iw, deck_h };           y += deck_h + 1;
    b.vitals  = { ix, y, iw, 1 };                y += 1 + 1;
    b.log     = { ix, y, iw, kLogRows };         y += kLogRows + 1;
    b.footer  = { ix, wr.y + wr.h - 2, iw, 1 };
    // Sub-minimum-window safety: the footer is pinned at wr.h-2 independently
    // of the top-down accumulator, so on windows below the documented minimum
    // the log band (kLogRows tall, drawn unconditionally by draw_log_pane) can
    // run into the pinned footer or the bottom chrome row. Clamp its height so
    // it never overruns the footer. <=0 height makes draw_log_pane no-op.
    if (b.log.y + b.log.h > b.footer.y)
        b.log.h = std::max(0, b.footer.y - b.log.y);
    // NOTE: compute_bands assumes wr.h >= deck_h + kLogRows + 12 for all bands
    // to fit without collision. Row budget, top->bottom:
    //   top border(1) + header(1) + sep(1) + field(1) + caption(1) + sep(1)
    //   + deck(deck_h) + sep(1) + vitals(1) + sep(1) + log(kLogRows) + sep(1)
    //   + footer(1) + bottom border(1)  =  deck_h + kLogRows + 12
    // The single sep between caption and deck is the only sep among the
    // field/caption pair — there is NO separator between field and caption
    // (caption sits directly under the field). At exactly the minimum,
    // field_h == 1 and sep_log == wr.h-3, so footer/border do not collide.
    // Below the minimum the field shrinks to 1 (std::max), the log band is
    // clamped above, and render() skips the band separators entirely.
    return b;
}

// Mirrors the eff_slots logic in draw_deck_panel so render() can size the
// deck band before calling the draw functions.
int effective_deck_slots(Game& game, const NetSession& s) {
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
        const auto& cd = *(*deck_slot)->deck;
        return std::min(kCyberdeckMaxSlots,
                        cd.stats.slots + (s.skill_daemon_mastery ? 1 : 0));
    }
    return kCyberdeckMaxSlots;
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

    // Border tear: Hunted+ states fracture short contiguous segments of
    // the frame into corruption / box-break glyphs (incl. dropouts) that
    // churn fast, relocate, and heal at irregular intervals — the rest of
    // the frame stays clean. Same burst grammar as the title-bar glitch.
    const bool corrupt = (ws == WindowState::Hunted ||
                          ws == WindowState::Critical ||
                          ws == WindowState::Blackwall);
    auto mix = [](uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16; return x;
    };
    static const char* const kFrag[] = {
        "\xc2\xa7",        // §
        "\xc2\xa4",        // ¤
        "\xe2\x80\xa1",    // ‡
        "\xce\xa3",        // Σ
        "\xce\xa8",        // Ψ
        "\xe2\x95\xb3",    // ╳
        "\xe2\x95\xaa",    // ╪
        "\xe2\x95\xab",    // ╫
        "\xe2\x96\x93",    // ▓
        "\xe2\x96\x88",    // █
        "\xe2\x96\x9a",    // ▚
        " ",               // dropout
    };
    constexpr uint32_t kFragN = 12;
    const uint32_t t  = static_cast<uint32_t>(phase);
    const uint32_t ep = t / 3u;                 // tear relocates ~every 3 ticks
    // Is cell p of an N-long edge `eid` inside this epoch's tear?
    auto torn = [&](uint32_t eid, int p, int N) -> bool {
        if (!corrupt || N <= 2) return false;
        uint32_t hh = mix(eid * 2654435761u ^ (ep + 1u) * 40503u);
        if ((hh % 3u) == 0u) return false;              // ~1/3 epochs: clean
        int len = 2 + static_cast<int>((hh >> 3) % 6u); // tear span 2..7
        int s   = static_cast<int>((hh >> 8) % static_cast<uint32_t>(N));
        return p >= s && p < s + len;
    };
    auto frag = [&](int key) -> const char* {           // churns every tick
        uint32_t h = mix(static_cast<uint32_t>(key) * 0x9e3779b9u
                       ^ (t * 2654435761u));
        return kFrag[(h >> 13) % kFragN];
    };
    for (int i = 1; i < wr.w - 1; ++i) {
        const int p = i - 1, N = wr.w - 2;
        bool gt = torn(0, p, N), gb = torn(1, p, N);
        r.draw_glyph(wr.x + i, wr.y,
                     gt ? frag(p)       : "\xe2\x95\x90",
                     gt ? Color::Magenta : kChrome);
        r.draw_glyph(wr.x + i, wr.y + wr.h - 1,
                     gb ? frag(p + 777)  : "\xe2\x95\x90",
                     gb ? Color::Magenta : kChrome);
    }
    for (int j = 1; j < wr.h - 1; ++j) {
        const int p = j - 1, N = wr.h - 2;
        bool gl = torn(2, p, N), gr = torn(3, p, N);
        r.draw_glyph(wr.x, wr.y + j,
                     gl ? frag(p + 1111) : "\xe2\x95\x91",
                     gl ? Color::Magenta : kChrome);
        r.draw_glyph(wr.x + wr.w - 1, wr.y + j,
                     gr ? frag(p + 2222) : "\xe2\x95\x91",
                     gr ? Color::Magenta : kChrome);
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

// Decode the UTF-8 sequence at text[i] into a codepoint; sets `len` to its
// byte length (1..4, clamped to the buffer). Malformed lead bytes decode as
// a 1-byte replacement so callers always make progress.
uint32_t decode_utf8(const std::string& text, size_t i, int& len) {
    unsigned char b = static_cast<unsigned char>(text[i]);
    if (b < 0x80) { len = 1; return b; }
    auto cont = [&](size_t k) -> uint32_t {
        return (i + k < text.size())
                   ? (static_cast<unsigned char>(text[i + k]) & 0x3Fu) : 0u;
    };
    if ((b & 0xE0) == 0xC0) { len = 2; return ((b & 0x1Fu) << 6) | cont(1); }
    if ((b & 0xF0) == 0xE0) { len = 3; return ((b & 0x0Fu) << 12) | (cont(1) << 6) | cont(2); }
    if ((b & 0xF8) == 0xF0) { len = 4; return ((b & 0x07u) << 18) | (cont(1) << 12) | (cont(2) << 6) | cont(3); }
    len = 1; return b;
}

// A combining mark stacks on the preceding base glyph and adds ZERO advance
// width (zalgo titles/labels). Covers the blocks zalgo draws from.
bool is_combining_cp(uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||  // Combining Diacritical Marks
           (cp >= 0x0483 && cp <= 0x0489) ||  // Combining Cyrillic
           (cp >= 0x1AB0 && cp <= 0x1AFF) ||  // ...Extended
           (cp >= 0x1DC0 && cp <= 0x1DFF) ||  // ...Supplement
           (cp >= 0x20D0 && cp <= 0x20FF) ||  // ...for Symbols
           (cp >= 0xFE20 && cp <= 0xFE2F);    // Combining Half Marks
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
        // Build one cell = a base glyph plus any combining marks that
        // immediately follow it. Marks overstrike the base in the same
        // cell and never advance the cursor (so column math, the title
        // border, and centered room labels stay aligned). The terminal
        // Cell holds 4 content bytes — an ASCII base + one 2-byte mark
        // fits; whole marks past capacity are dropped (zalgo still
        // reads). A sequence is only ever copied IN FULL — never split
        // mid-codepoint, which would emit invalid UTF-8 (renders as ).
        constexpr int kCellBytes = 4;       // TerminalRenderer::Cell::ch holds 4 + NUL
        char buf[kCellBytes + 1] = {0};
        int  used = 0;
        auto append_full = [&](size_t at, int len) {
            if (used + len > kCellBytes) return;          // whole-sequence-or-nothing
            for (int k = 0; k < len && at + k < text.size(); ++k)
                buf[used++] = text[at + k];
        };
        int  base_len = 1;
        uint32_t base_cp = decode_utf8(text, i, base_len);
        if (is_combining_cp(base_cp)) {
            // Orphan combining mark with no preceding base on this cell:
            // overstrike the current cell without advancing the cursor.
            append_full(i, base_len);
            buf[used] = '\0';
            if (used > 0) r.draw_glyph(cursor, y, buf, cur);
            i += base_len;
            continue;  // cursor unchanged — next glyph lands on the same cell
        }
        append_full(i, base_len);
        i += base_len;
        // Absorb trailing combining marks into the same cell (whole marks only).
        while (i < text.size()) {
            unsigned char nb = static_cast<unsigned char>(text[i]);
            if (nb == static_cast<unsigned char>(COLOR_BEGIN) ||
                nb == static_cast<unsigned char>(COLOR_END)) break;
            int mlen = 1;
            uint32_t mcp = decode_utf8(text, i, mlen);
            if (!is_combining_cp(mcp)) break;
            append_full(i, mlen);
            i += mlen;  // consume the mark even if it didn't fit (drop whole mark)
        }
        buf[used] = '\0';
        if (base_cp < 0x80 && used == 1)
            r.draw_char(cursor, y, buf[0], cur);
        else if (used > 0)
            r.draw_glyph(cursor, y, buf, cur);
        ++cursor;
    }
}

// Visual cell width of a UTF-8 string: ASCII / base multi-byte glyph = 1,
// combining mark = 0 (it overstrikes the previous cell). COLOR_BEGIN/
// COLOR_END markers are zero-width metadata and skipped. Caller advances x
// by this much.
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
        int len = 1;
        uint32_t cp = decode_utf8(text, i, len);
        if (!is_combining_cp(cp)) ++w;   // combining marks add 0 width
        i += len;
    }
    return w;
}

// Clip a UTF-8 string (which may contain COLOR_BEGIN/COLOR_END markers) to at
// most `max_cells` visual cells, cutting only at a code-point boundary. Color
// markers are zero-width and never cut mid-sequence. Used by draw_ghost_dialog
// to safely truncate lore/choice lines that exceed the panel's inner width.
std::string utf8_clip(const std::string& text, int max_cells) {
    int cells = 0;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
        if (b == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < text.size()) {
            i += 2; continue;  // zero-width marker — never clips here
        }
        if (b == static_cast<unsigned char>(COLOR_END)) {
            ++i; continue;     // zero-width marker
        }
        int seq = 1;
        uint32_t cp = decode_utf8(text, i, seq);
        if (is_combining_cp(cp)) { i += seq; continue; }  // rides with prev base; 0 cells
        if (cells + 1 > max_cells) break;  // would overflow — cut before this base
        ++cells;
        i += seq;
    }
    return text.substr(0, i);
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

        std::string status_token =
            (s.netspace.combat_status == Netspace::CombatStatus::Combat)
            ? " :: COMBAT"
            : " :: OPEN";
        Color status_color =
            (s.netspace.combat_status == Netspace::CombatStatus::Combat)
            ? Color::Magenta
            : Color::Cyan;
        draw_colored_string(r, x, y, status_token, status_color);
        x += visual_width(status_token);
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

    // Time-dilation badge — shown on the title row only when the netspace
    // has no subtitle string (Phase 5 removed the subtitle row entirely;
    // has_subtitle suppresses the badge when the field is non-empty).
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

    // Title-bar glitch: Stressed+ states run a burst cycle on the title
    // row — pick a random spot + random length (3..8) + random on-time,
    // hold that spot/length while the glyphs churn fast, then restore and
    // wait a random gap before the next burst.
    WindowState ws = s.netspace.window_state;
    if (ws == WindowState::Stressed || ws == WindowState::Hunted ||
        ws == WindowState::Critical || ws == WindowState::Blackwall) {
        static const char* const kGlitch[] = {
            "\xc2\xa7",        // §
            "\xc2\xa4",        // ¤
            "\xe2\x80\xa1",    // ‡
            "\xce\xa3",        // Σ
            "\xce\xa8",        // Ψ
            "\xce\x9e",        // Ξ
            "\xe2\x97\x8a",    // ◊
            "\xe2\x95\xb3",    // ╳
        };
        constexpr uint32_t kPalette = 8;
        auto mix = [](uint32_t x) {
            x ^= x >> 16; x *= 0x7feb352du;
            x ^= x >> 15; x *= 0x846ca68bu;
            x ^= x >> 16; return x;
        };
        const uint32_t t   = static_cast<uint32_t>(
                                 game.hacking().blink_phase());
        constexpr uint32_t kCycle = 6u;              // ticks per burst cycle
        const uint32_t cyc = t / kCycle;             // which burst
        const uint32_t off = t % kCycle;             // offset within it
        const uint32_t hc  = mix(cyc);               // per-cycle randomness
        const uint32_t on_dur = 2u + (hc % 4u);      // on-time: 2..5 ticks
        if (off < on_dur) {                          // (1)-(4): burst ON
            const int len = 3 + static_cast<int>((hc >> 4) % 6u);   // 3..8
            int maxstart = wr.w - 2 - len;
            if (maxstart < 1) maxstart = 1;
            const int start = wr.x + 1 + static_cast<int>(           // fixed
                                  (hc >> 8) % static_cast<uint32_t>(maxstart));
            for (int g = 0; g < len; ++g) {
                // glyph keyed off t too → randomizes quickly each tick
                uint32_t h = mix(hc
                               ^ (static_cast<uint32_t>(g) * 0x9e3779b9u)
                               ^ (t * 2654435761u));
                r.draw_glyph(start + g, y,
                             kGlitch[(h >> 19) % kPalette], Color::Magenta);
            }
        }
        // else (5): restored — title row left untouched until next cycle.
    }
}

// ---------------------------------------------------------------------------
// Deck strip
// ---------------------------------------------------------------------------

void draw_deck_strip(Game& game, Renderer& r, const Rect& vit,
                     const NetSession& s) {
    const int y = vit.y;
    int x = vit.x + 1;

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

void draw_meatworld_footer(Renderer& r, const Rect& f, const NetSession& s) {
    long secs = static_cast<long>(s.meat_clock_base_secs)
              + static_cast<long>(s.net_turn)
                * std::max(1, s.netspace.time_dilation);
    long tod = ((secs % 86400) + 86400) % 86400;   // wrap to a day
    int hh = static_cast<int>(tod / 3600);
    int mm = static_cast<int>((tod % 3600) / 60);
    int ss = static_cast<int>(tod % 60);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "meatworld clock %02d:%02d:%02d", hh, mm, ss);
    int x = f.x + 1;
    draw_colored_string(r, x, f.y, buf, Color::Cyan);
    x += static_cast<int>(std::strlen(buf));
    draw_colored_string(r, x, f.y, "   [net paused \xe2\x80\x94 body is not]",
                        Color::DarkGray);
}

void draw_field_caption(Renderer& r, const Rect& cap, const NetSession& s) {
    if (s.field_caption.empty()) return;
    std::string line = s.field_caption;
    // Truncate to interior width by visual cells (UTF-8/zalgo-safe).
    while (visual_width(line) > cap.w - 2 && !line.empty())
        line.pop_back();
    draw_colored_string(r, cap.x + 1, cap.y, line, Color::DarkGray);
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

void draw_log_pane(Renderer& r, const Rect& log, const NetSession& s,
                   int phase) {
    int rows = log.h;
    if (rows < 1) return;
    int max_w = log.w - 2;
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
        draw_colored_string(r, log.x + 1, log.y + i, wrapped[start + i], Color::White);
    }

    // Command-line glitch: Hunted+ states corrupt ~20% of the last visible
    // log row with § glyphs, crawling with the blink phase.
    if (s.netspace.window_state == WindowState::Hunted ||
        s.netspace.window_state == WindowState::Critical ||
        s.netspace.window_state == WindowState::Blackwall) {
        int last = std::min(rows, total) - 1;
        if (last >= 0) {
            int yrow = log.y + last;
            for (int cx = 0; cx < log.w - 2; ++cx) {
                if ((cx * 31 + phase) % 5 == 0)
                    r.draw_glyph(log.x + 1 + cx, yrow, "\xc2\xa7", Color::Magenta);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Ghost dialog modal
// ---------------------------------------------------------------------------

// Draws a compact centered bordered modal panel over the net overlay when
// s.ghost_dialog.open is true. Mirrors the BlackIceTakeover centered-box
// idiom (draw_window_sequence, ~line 1486) and uses draw_colored_string +
// draw_char(bg) inverse-video for the selected choice, matching the
// deck-panel active-slot highlight (draw_deck_panel).
void draw_ghost_dialog(Renderer& r, const WindowRect& wr, const NetSession& s) {
    if (!s.ghost_dialog.open) return;
    const auto& gd = s.ghost_dialog;

    // Panel dimensions: wide enough for lines + choices, min 36 cols.
    constexpr int kMinW = 36;
    constexpr int kPad  = 2;   // left/right interior padding

    // Measure content width using visual_width (UTF-8 + color-marker safe).
    // Mirror of the log-panel width scan at net_renderer.cpp ~line 680.
    int content_w = kMinW;
    for (const auto& line : gd.lines) {
        int lw = visual_width(line) + kPad * 2;
        if (lw > content_w) content_w = lw;
    }
    for (const auto& ch : gd.choices) {
        // "▸ " prefix occupies 2 cells + visual text + kPad*2
        int cw = visual_width(ch.text) + 2 + kPad * 2;
        if (cw > content_w) content_w = cw;
    }
    // Footer hint: "Space: choose   Esc: leave"
    constexpr int kFooterLen = 26;
    if (kFooterLen + kPad * 2 > content_w) content_w = kFooterLen + kPad * 2;

    // Clamp width to window interior.
    int max_w = wr.w - 4;
    if (content_w > max_w) content_w = max_w;
    int bw = content_w + 2;  // +2 for left/right border columns

    int n_lines   = static_cast<int>(gd.lines.size());
    int n_choices = static_cast<int>(gd.choices.size());
    // Fixed rows: footer separator (1) + footer (1) + top/bottom border (2) = 4.
    // Choice rows are always reserved first so they remain visible; lore rows
    // fill whatever space remains when content exceeds the window height.
    constexpr int kFixedRows = 4;  // top border + footer sep + footer + bottom border
    int sep_rows = (n_lines > 0 && n_choices > 0) ? 1 : 0;

    // Ideal height: all content + separator + fixed chrome.
    int ideal_inner_h = n_lines + sep_rows + n_choices + 2 /* footer sep+row */;
    int bh = ideal_inner_h + 2;  // +2 for top/bottom border
    if (bh < 6) bh = 6;

    // Clamp height to window interior (mirrors width clamp above).
    int max_h = wr.h - 4;
    if (bh > max_h) bh = max_h;

    // Recompute how many lore lines actually fit after clamping.
    // Choices are always drawn; lore gets the remaining rows.
    int inner_h   = bh - 2;  // rows inside the border
    int reserved  = n_choices + sep_rows + 2;  // footer sep + footer
    int lines_fit = std::max(0, inner_h - reserved);
    if (lines_fit > n_lines) lines_fit = n_lines;

    // Center the panel inside the window.
    int bx = wr.x + (wr.w - bw) / 2;
    int by = wr.y + (wr.h - bh) / 2;

    // Black interior.
    for (int j = 1; j < bh - 1; ++j)
        for (int i = 1; i < bw - 1; ++i)
            r.draw_char(bx + i, by + j, ' ', Color::White, Color::Black);

    // Border using double-line box-draw glyphs (same family as main chrome).
    r.draw_glyph(bx,          by,          "\xe2\x95\x94", Color::Cyan);  // ╔
    r.draw_glyph(bx + bw - 1, by,          "\xe2\x95\x97", Color::Cyan);  // ╗
    r.draw_glyph(bx,          by + bh - 1, "\xe2\x95\x9a", Color::Cyan);  // ╚
    r.draw_glyph(bx + bw - 1, by + bh - 1, "\xe2\x95\x9d", Color::Cyan);  // ╝
    for (int i = 1; i < bw - 1; ++i) {
        r.draw_glyph(bx + i, by,          "\xe2\x95\x90", Color::Cyan);   // ═
        r.draw_glyph(bx + i, by + bh - 1, "\xe2\x95\x90", Color::Cyan);   // ═
    }
    for (int j = 1; j < bh - 1; ++j) {
        r.draw_glyph(bx,          by + j, "\xe2\x95\x91", Color::Cyan);   // ║
        r.draw_glyph(bx + bw - 1, by + j, "\xe2\x95\x91", Color::Cyan);   // ║
    }

    // Content rows.
    int row = by + 1;
    const int text_x = bx + 1 + kPad;
    const int text_w = bw - 2 - kPad * 2;

    // Lore/flavour lines — clipped to lines_fit rows, each line clipped to
    // text_w visual cells via utf8_clip (never a raw byte substr).
    // Pattern mirrors draw_log_panel (net_renderer.cpp ~line 802) which also
    // passes pre-clipped strings to draw_colored_string for bounded rows.
    for (int li = 0; li < lines_fit; ++li) {
        std::string clamped = utf8_clip(gd.lines[li], text_w);
        draw_colored_string(r, text_x, row, clamped, Color::White);
        ++row;
    }

    // Separator between lore and choices.
    if (n_lines > 0 && n_choices > 0) {
        r.draw_glyph(bx,          row, "\xe2\x95\xa0", Color::Cyan);   // ╠
        r.draw_glyph(bx + bw - 1, row, "\xe2\x95\xa3", Color::Cyan);   // ╣
        for (int i = 1; i < bw - 1; ++i)
            r.draw_glyph(bx + i, row, "\xe2\x95\x90", Color::Cyan);    // ═
        ++row;
    }

    // Choices — selected choice gets ▸ marker + inverse-video highlight.
    // Choice text is clipped to (text_w - 1) cells via utf8_clip; the
    // selected-row draw uses draw_colored_string with the inverse colors
    // rather than the byte-unsafe `for (char c : label)` loop.
    for (int ci = 0; ci < n_choices; ++ci) {
        const auto& ch = gd.choices[ci];
        bool sel = (ci == gd.sel);
        // Clip choice label at a UTF-8 character boundary.
        std::string label = utf8_clip(ch.text, text_w - 1);
        if (sel) {
            // Inverse-video: fill the choice row background.
            for (int i = 1; i < bw - 1; ++i)
                r.draw_char(bx + i, row, ' ', Color::Black, Color::Cyan);
            // Cursor marker ▸ then text. Walk code points (not bytes) so
            // multi-byte glyphs are passed whole to draw_glyph, not split.
            r.draw_glyph(text_x - 1, row, "\xe2\x96\xb8", Color::Black, Color::Cyan);  // ▸
            int cx = text_x;
            for (size_t bi = 0; bi < label.size(); ) {
                unsigned char b = static_cast<unsigned char>(label[bi]);
                int seq = 1;
                if      ((b & 0xE0) == 0xC0) seq = 2;
                else if ((b & 0xF0) == 0xE0) seq = 3;
                else if ((b & 0xF8) == 0xF0) seq = 4;
                if (seq == 1) {
                    r.draw_char(cx, row, label[bi], Color::Black, Color::Cyan);
                } else {
                    char buf[5] = {0};
                    for (int k = 0; k < seq && bi + k < label.size(); ++k)
                        buf[k] = label[bi + k];
                    r.draw_glyph(cx, row, buf, Color::Black, Color::Cyan);
                }
                bi += seq;
                ++cx;
            }
        } else {
            r.draw_glyph(text_x - 1, row, " ", Color::Cyan);
            draw_colored_string(r, text_x, row, label, Color::Cyan);
        }
        ++row;
    }

    // Footer separator + hint.
    r.draw_glyph(bx,          row, "\xe2\x95\xa0", Color::Cyan);   // ╠
    r.draw_glyph(bx + bw - 1, row, "\xe2\x95\xa3", Color::Cyan);   // ╣
    for (int i = 1; i < bw - 1; ++i)
        r.draw_glyph(bx + i, row, "\xe2\x95\x90", Color::Cyan);    // ═
    ++row;

    draw_colored_string(r, text_x, row, "Space: choose   Esc: leave", Color::DarkGray);
}

// ---------------------------------------------------------------------------
// Deck panel  (Phase 5 Slice 1 — replaces the old single-row program bar)
// ---------------------------------------------------------------------------

// draw_deck_panel renders the [ DECK ] band: one header row then one row per
// slot.  Column layout (all columns within deck.w - 2 interior):
//
//   [n] NAME.exe         §   N RAM   ready
//   [n] ________         --  --      empty      (empty slot)
//
// Active slot: the "[n] NAME.exe" token is rendered in inverse video.
// Placeholder glyph § is used for all occupied programs.
// TODO Slice 8: per-program signature glyph (replace § with program-specific icon)
void draw_deck_panel(Game& game, Renderer& r, const Rect& deck,
                     const NetSession& s) {
    // Header row.
    draw_colored_string(r, deck.x + 1, deck.y, "[ DECK ]", Color::Cyan);

    const int deck_slots = deck.h - 1;   // band was sized 1 + deck_slots

    // Read the equipped cyberdeck (may be absent).
    auto* deck_slot_ptr = game.player().equipment.equipped_cyberdeck();
    const bool has_deck = deck_slot_ptr && *deck_slot_ptr && (*deck_slot_ptr)->deck;

    int eff_slots = 0;
    // cd_ptr points into the optional so we can reference it conditionally
    // below without a CyberdeckData copy.
    const CyberdeckData* cd_ptr = nullptr;
    if (has_deck) {
        cd_ptr    = &(*(*deck_slot_ptr)->deck);
        eff_slots = std::min(kCyberdeckMaxSlots,
                             cd_ptr->stats.slots + (s.skill_daemon_mastery ? 1 : 0));
    }

    // Fixed column offsets (relative to deck.x + 1, the interior left edge).
    //   col_name  : "[n] NAME.exe" — starts at interior x+0
    //   col_glyph : glyph / "--"   — fixed at interior x+22
    //   col_cost  : "N RAM" / "--" — fixed at interior x+26
    //   col_state : "ready"/"empty"— fixed at interior x+33
    const int ix         = deck.x + 1;
    const int col_glyph  = ix + 22;
    const int col_cost   = ix + 26;
    const int col_state  = ix + 33;

    for (int i = 0; i < deck_slots; ++i) {
        const int row = deck.y + 1 + i;
        if (row >= deck.y + deck.h) break;   // guard: clip to band

        // Slot number tag "[n] ".
        char slot_tag[5];
        std::snprintf(slot_tag, sizeof(slot_tag), "[%d] ", i + 1);

        // Resolve program for this slot (if equipped deck and slot occupied).
        bool occupied        = false;
        bool affordable      = false;
        std::string name_str = "________";
        int  ram_cost        = 0;

        if (has_deck && cd_ptr && i < eff_slots &&
            cd_ptr->loaded[i].program_def_id != 0) {
            Item probe = build_by_def_id(cd_ptr->loaded[i].program_def_id);
            if (probe.program) {
                ProgramId pid       = probe.program->id;
                const auto* def     = find_program(pid);
                if (def && def->kind != ProgramKind::Qh) {
                    occupied   = true;
                    name_str   = std::string(def->name) + ".exe";
                    ram_cost   = def->ram_cost;
                    affordable = (s.ram >= def->ram_cost) &&
                                 (cd_ptr->heat_current + def->heat_cost
                                  <= cd_ptr->stats.heat_cap);
                }
            }
        }

        if (occupied) {
            // "[n] NAME.exe" — active slot gets inverse-video treatment.
            const std::string full_label = std::string(slot_tag) + name_str;
            if (i == s.active_slot) {
                int cx = ix;
                for (char ch : full_label) {
                    r.draw_char(cx++, row, ch, Color::Black, Color::Cyan);
                }
            } else {
                Color name_col = affordable ? Color::Cyan : Color::DarkGray;
                draw_colored_string(r, ix, row, full_label, name_col);
            }

            // Glyph column — placeholder § until Slice 8 per-program table.
            // TODO Slice 8: per-program signature glyph
            r.draw_glyph(col_glyph, row, "\xc2\xa7", Color::Cyan);  // §

            // Cost + ready/dim columns.
            char cost_buf[12];
            std::snprintf(cost_buf, sizeof(cost_buf), "%d RAM", ram_cost);
            Color status_col = affordable ? Color::Cyan : Color::DarkGray;
            draw_colored_string(r, col_cost,  row, cost_buf, status_col);
            draw_colored_string(r, col_state, row, "ready",  status_col);
        } else {
            // Empty slot — everything dimmed.
            const std::string empty_label = std::string(slot_tag) + "________";
            draw_colored_string(r, ix,        row, empty_label, Color::DarkGray);
            draw_colored_string(r, col_glyph, row, "--",        Color::DarkGray);
            draw_colored_string(r, col_cost,  row, "--",        Color::DarkGray);
            draw_colored_string(r, col_state, row, "empty",     Color::DarkGray);
        }
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
    // A cell counts as "pipe-connected" for junction-glyph resolution if it
    // carries a pipe/port tile or is an opened port (make_passable) — so a
    // bend cell resolves to a real corner (╗ ╝ …) instead of a stray ─.
    auto pipe_neigh = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= s.netspace.w || y >= s.netspace.h) return false;
        const NetTile t = s.netspace.at(x, y);
        if (t == NetTile::PipeH || t == NetTile::PipeV ||
            t == NetTile::PipeJunc || t == NetTile::PipePortV ||
            t == NetTile::PipePortCornerTR || t == NetTile::PipePortDownD)
            return true;
        return s.netspace.passable_overrides.count({x, y}) > 0;
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
                    // Resolve from pipe neighbours: 2 arms → corner
                    // (╗ ╝ ╔ ╚), 3 → tee, 4 → ╬. Reuses the double-line
                    // glyph table so it matches the ═║ pipe aesthetic.
                    glyph = wall_glyph_for_neighbours(
                        pipe_neigh(tx, ty - 1), pipe_neigh(tx, ty + 1),
                        pipe_neigh(tx + 1, ty), pipe_neigh(tx - 1, ty));
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

                case NetTile::Glyph: {
                    // Render the per-cell override glyph (e.g. the ATM $-border,
                    // the turret █ arena wall). Color from glyph_color_overrides
                    // when set, else a neutral default. Empty string ⇒ blank.
                    auto gi = s.netspace.glyph_overrides.find({tx, ty});
                    glyph = (gi != s.netspace.glyph_overrides.end() && !gi->second.empty())
                                ? gi->second.c_str() : " ";
                    auto ci = s.netspace.glyph_color_overrides.find({tx, ty});
                    color = (ci != s.netspace.glyph_color_overrides.end())
                                ? ci->second : Color::White;
                    break;
                }
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

    // Ambient overlay scaffold — per-kind visual overlays that sit above
    // the tile grid but below ICE/avatar so they never obscure actors.
    if (s.netspace.target.kind == NetspaceTargetKind::Turret) {
        // Turret "rounds" band: animated > rows above and < rows below the
        // right side of the arena interior, phase-shifted per blink_phase.
        // Uses the same 9-frame cadence as the pipe animation so the
        // rounds appear to travel rightward (>) above and leftward (<) below.
        // Visual-only: does not alter tiles or passability.
        const int bp  = game.hacking().blink_phase();
        const int ph  = (bp / 2) & 7;          // fast rounds — phase steps ~every 2 frames
        // Draw two rows of > glyphs and two rows of < glyphs in the
        // right half of the arena interior, using floor cells only.
        // Bounds derived from arena dims in gen_turret_netspace.cpp:
        //   kArenaX=3, kArenaW=40 → arena right interior col = 3+40-2 = 41
        //   kArenaY=1, kArenaH=24 → arena y interior = 2..23
        //   ax0: kArenaX+1=4 (first interior col) + 12 (right-half offset into
        //        the ~34-col wide interior) — keep in sync with gen_turret_netspace.cpp
        //        arena consts if kArenaX / kArenaW / kArenaH change.
        const int arena_interior_left = 4;  // kArenaX + 1
        const int rounds_right_offset = 12; // shift band into right half of interior
        const int ax0 = arena_interior_left + rounds_right_offset; // first column of rounds band
        const int ax1 = 41; // kArenaX + kArenaW - 2 = 3 + 40 - 2 = 41 (last interior col)
        const int ay_above1 = 3;   // kArenaY + 2 = 1 + 2 = 3; first > row (above spine)
        const int ay_above2 = 4;   // kArenaY + 3 = 1 + 3 = 4; second > row
        const int ay_below1 = 22;  // kArenaY + kArenaH - 3 = 1 + 24 - 3 = 22; first < row
        const int ay_below2 = 23;  // kArenaY + kArenaH - 2 = 1 + 24 - 2 = 23; second < row
        for (int x = ax0; x <= ax1; ++x) {
            // Only draw over Floor/Void tiles (never overwrite walls, pipes, boxes).
            auto safe = [&](int cx, int cy) -> bool {
                if (!s.netspace.in_bounds(cx, cy)) return false;
                NetTile t = s.netspace.at(cx, cy);
                return t == NetTile::Floor || t == NetTile::Void;
            };
            // Phase-shifted: one column in four shows the glyph (when (x+ph)&3 == 0),
            // the rest are spaces — the active column marches right each phase step,
            // giving a crawling-rightward illusion for '>' and crawling-leftward for '<'.
            const char* fwd = ((x + ph) & 3) == 0 ? ">" : " ";
            const char* bwd = ((x - ph) & 3) == 0 ? "<" : " ";
            int sx, sy;
            if (safe(x, ay_above1) && cull(x, ay_above1, sx, sy))
                r.draw_glyph(pr.x + sx, pr.y + sy, fwd, Color::Red);
            if (safe(x, ay_above2) && cull(x, ay_above2, sx, sy))
                r.draw_glyph(pr.x + sx, pr.y + sy, fwd, Color::Red);
            if (safe(x, ay_below1) && cull(x, ay_below1, sx, sy))
                r.draw_glyph(pr.x + sx, pr.y + sy, bwd, Color::Red);
            if (safe(x, ay_below2) && cull(x, ay_below2, sx, sy))
                r.draw_glyph(pr.x + sx, pr.y + sy, bwd, Color::Red);
        }
    }

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
    const uint32_t ph = static_cast<uint32_t>(phase);
    auto mix = [](uint32_t v) {
        v ^= v >> 16; v *= 0x7feb352du;
        v ^= v >> 15; v *= 0x846ca68bu;
        v ^= v >> 16; return v;
    };
    auto fill = [&](const char* g, Color c) {
        for (int j=1;j<wr.h-1;++j) for (int i=1;i<wr.w-1;++i)
            r.draw_glyph(wr.x+i, wr.y+j, g, c);
    };
    auto center = [&](int row, const std::string& t, Color c) {
        int sx = wr.x + (wr.w - (int)t.size())/2;
        draw_colored_string(r, sx, wr.y + row, t, c);
    };
    if (q.kind == WindowSeqKind::Opening) {
        // Persistent title bar — drawn on EVERY Opening frame so it stays
        // visible through the whole ritual, and the per-frame body
        // animation is clipped strictly below its separator so it can
        // never overwrite the title row or the chrome.
        const int sep_in = 2;                        // title-bar bottom line (window-rel y)
        const int by0    = wr.y + sep_in + 1;       // first body row (screen y)
        const int by1    = wr.y + wr.h - 1;         // exclusive last body row
        const int bx0    = wr.x + 1;
        const int bx1    = wr.x + wr.w - 1;         // exclusive

        // Per-frame chrome header (design doc frames): handshake frames
        // keep "JACKING IN :: <target>" + heart rate; the invert /
        // dissolution / signal-acquired frames take the doc's header art.
        auto hdr_center = [&](const std::string& t, Color c) {
            int sx = wr.x + (wr.w - visual_width(t)) / 2;
            draw_colored_string(r, sx, wr.y+1, t, c);
        };
        switch (q.frame_index) {
            case 0: case 1: {
                int hr = 88 + q.frame_index*4 + (phase/5)%3;
                std::string hdr = "JACKING IN :: " + (s.netspace.title.empty()
                                  ? std::string("TARGET") : s.netspace.title);
                draw_colored_string(r, wr.x+2, wr.y+1, hdr, Color::Cyan);
                char hb[32]; std::snprintf(hb,sizeof(hb),"heart rate %d", hr);
                draw_colored_string(r, wr.x+wr.w-2-(int)std::strlen(hb), wr.y+1,
                                    hb, Color::Magenta);
                break;
            }
            case 2:   // doc No.3 — the screen inverts: full ▓ header bar
                for (int i=bx0;i<bx1;++i)
                    r.draw_glyph(i, wr.y+1, "\xe2\x96\x93", Color::Cyan);
                break;
            case 3:   // doc No.4 — dissolution: density gradient
                hdr_center("\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91 "
                           "\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92 "
                           "\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93 "
                           "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88 "
                           "\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93 "
                           "\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92 "
                           "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91",
                           Color::Cyan);
                break;
            case 4:   // doc No.5 — first glimpse: signal acquired
                hdr_center("\xe2\x96\x92\xe2\x96\x91  signal acquired  "
                           "\xe2\x96\x91\xe2\x96\x92", Color::Cyan);
                break;
            default: break;
        }
        draw_horizontal_separator(r, wr, sep_in);

        auto body_center = [&](const std::string& t, Color c) {
            int sx = wr.x + (wr.w - (int)t.size())/2;
            draw_colored_string(r, sx, (by0+by1)/2, t, c);
        };

        switch (q.frame_index) {
            case 0: case 1: {
                const char* msg[] = {"establishing handshake...",
                                     "parsing reality offset..."};
                body_center(msg[q.frame_index], Color::Cyan);
                break;
            }
            case 2: {
                // doc No.3 — reality inverts: a ▓ box around the
                // meatspace room, █ furniture, the @ pulled through a
                // density gradient, the jack cable dropping below it.
                int BW = std::min(41, bx1 - bx0);
                if (BW < 21) BW = bx1 - bx0;            // tiny-window fallback
                const int BH = 7;
                const int L  = wr.x + (wr.w - BW) / 2;
                const int T  = (by0 + by1 - BH) / 2;
                // The inverted reality buzzes — every block cell flickers
                // between ▓ █ ▒ each tick.
                static const char* const kInv[] = {
                    "\xe2\x96\x93", "\xe2\x96\x88", "\xe2\x96\x92" }; // ▓ █ ▒
                auto buzz = [&](int x, int y) {
                    return kInv[mix(static_cast<uint32_t>(x*131 + y*977) ^ ph)
                                % 3u];
                };
                for (int i=0;i<BW;++i){
                    r.draw_glyph(L+i, T,      buzz(L+i,T),      Color::Cyan);
                    r.draw_glyph(L+i, T+BH-1, buzz(L+i,T+BH-1), Color::Cyan);
                }
                for (int j=0;j<BH;++j){
                    r.draw_glyph(L,      T+j, buzz(L,T+j),      Color::Cyan);
                    r.draw_glyph(L+BW-1, T+j, buzz(L+BW-1,T+j), Color::Cyan);
                }
                const int fW=4, fH=3, fyt=T+2;
                const int flx=L+4, frx=L+BW-1-4-fW;
                for (int j=0;j<fH;++j) for (int i=0;i<fW;++i){
                    r.draw_glyph(flx+i, fyt+j, buzz(flx+i,fyt+j), Color::Cyan);
                    r.draw_glyph(frx+i, fyt+j, buzz(frx+i,fyt+j), Color::Cyan);
                }
                const std::string grad =
                    "\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93 "        // ▓▓▓
                    "\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92 @ "      // ▒▒▒ @
                    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91 "        // ░░░
                    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91";        // ░░░
                const int gy = T+2;
                const int gx = wr.x + (wr.w - visual_width(grad))/2;
                draw_colored_string(r, gx, gy, grad, Color::Cyan);
                const int atx = gx + visual_width(
                    "\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93 "
                    "\xe2\x96\x92\xe2\x96\x92\xe2\x96\x92 ");
                r.draw_glyph(atx, gy,  "@",            Color::BrightWhite);
                // cable pulses as consciousness is drawn down it
                const bool c1 = ((ph    ) & 1u) != 0u;
                const bool c2 = ((ph + 1u) & 1u) != 0u;
                r.draw_glyph(atx, T+3, c1 ? "\xe2\x94\x82" : " ", Color::Cyan);
                r.draw_glyph(atx, T+4, c2 ? "\xe2\x94\x82" : " ", Color::Cyan);
                r.draw_glyph(atx, T+5, "\xe2\x96\xbc", Color::Cyan); // ▼
                const std::string cap =
                    "> \xe2\x96\x91\xe2\x96\x91\xe2\x96\x91"
                    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91 neural sync "
                    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91"
                    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91";
                draw_colored_string(r, wr.x+(wr.w-visual_width(cap))/2,
                                    wr.y+wr.h-2, cap, Color::Cyan);
                break;
            }
            case 3: {
                // doc No.4 — dissolution: the @ floats in churning static.
                // Layout is the doc's authored scatter; the particles and
                // dots twinkle every tick so it reads as live noise.
                const std::string D="\xe2\x96\x91", M="\xe2\x96\x92",
                                  H="\xe2\x96\x93";
                const std::string rows[5] = {
                    ". .   .  "+D+" "+M+"  .   .   .  "+H+" "+M+"   .  . .",
                    "    .       "+D+"         "+D+"       .",
                    ".       "+D+"       @       "+D+"       .",
                    "    .       "+D+"         "+D+"       .",
                    ". .   .  "+H+" "+M+"  .   .   .  "+D+" "+M+"   .  . .",
                };
                int maxw=0;
                for (const auto& s2:rows) maxw=std::max(maxw,visual_width(s2));
                const int left = wr.x + (wr.w - maxw)/2;
                const int top  = (by0 + by1 - 5)/2;
                static const char* const dens[] = {
                    "\xe2\x96\x91","\xe2\x96\x92","\xe2\x96\x93"," " }; // ░▒▓ ·
                for (int k=0;k<5;++k){
                    const std::string& row = rows[k];
                    int col = 0;
                    for (size_t bi=0; bi<row.size(); ) {
                        unsigned char b = static_cast<unsigned char>(row[bi]);
                        std::string gch;
                        if (b < 0x80) { gch.assign(1, row[bi]); bi += 1; }
                        else          { gch = row.substr(bi, 3); bi += 3; }
                        const int cx = left + col; ++col;
                        if (gch == " ") continue;
                        if (gch == "@") {
                            r.draw_glyph(cx, top+k, "@", Color::BrightWhite);
                            continue;
                        }
                        uint32_t h = mix(static_cast<uint32_t>(
                            cx*131 + (top+k)*977) ^ ph);
                        if (gch == ".") {            // anchor dots twinkle
                            if (h & 1u)
                                r.draw_glyph(cx, top+k, ".", Color::Cyan);
                            continue;
                        }
                        r.draw_glyph(cx, top+k, dens[h % 4u], Color::Cyan);
                    }
                }
                const std::string cap =
                    "> "+M+D+" consciousness migrating "+D+M;
                draw_colored_string(r, wr.x+(wr.w-visual_width(cap))/2,
                                    wr.y+wr.h-2, cap, Color::Cyan);
                break;
            }
            case 4: {
                // Ghost room materialising: thin corners anchor it while
                // the ╌/╎ dashed edges shimmer (a gap crawls along them)
                // and the @ pulses — "not yet real".
                const int bw = 16, bh = 7;
                const int bx = wr.x + (wr.w - bw) / 2;
                const int by = (by0 + by1 - bh) / 2;
                const Color gc = Color::DarkGray;
                r.draw_glyph(bx,        by,        "\xe2\x94\x8c", gc); // ┌
                r.draw_glyph(bx+bw-1,   by,        "\xe2\x94\x90", gc); // ┐
                r.draw_glyph(bx,        by+bh-1,   "\xe2\x94\x94", gc); // └
                r.draw_glyph(bx+bw-1,   by+bh-1,   "\xe2\x94\x98", gc); // ┘
                auto edge = [&](int seed, const char* g) -> const char* {
                    return ((seed + static_cast<int>(ph)) & 3) == 0 ? " " : g;
                };
                for (int i = 1; i < bw-1; ++i) {
                    r.draw_glyph(bx+i, by,      edge(i,   "\xe2\x95\x8c"), gc);
                    r.draw_glyph(bx+i, by+bh-1, edge(i+2, "\xe2\x95\x8c"), gc);
                }
                for (int j = 1; j < bh-1; ++j) {
                    r.draw_glyph(bx,      by+j, edge(j+1, "\xe2\x95\x8e"), gc);
                    r.draw_glyph(bx+bw-1, by+j, edge(j+3, "\xe2\x95\x8e"), gc);
                }
                const Color ac = ((ph / 3u) & 1u) ? Color::BrightWhite
                                                  : Color::Cyan;
                r.draw_glyph(bx+bw/2, by+bh/2, "@", ac);  // pulsing @
                center(wr.h-2, "> resolving topology...", Color::Cyan); break;
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
    WindowRect wr = compute_window_rect(sw, sh);
    int deck_slots = effective_deck_slots(game, *sess);
    NetBands b = compute_bands(wr, deck_slots);

    // Make the window opaque first so the monochrome world UI behind
    // doesn't bleed through into the Tron HUD.
    clear_window_interior(r, wr);

    // Full-window scripted sequence (jack-in ritual, jack-out, takeover).
    // The sequence owns the entire interior; skip normal layout while active.
    if (sess->window_seq.active()) {
        // The ritual plays *inside* the In-Net UI window, so the frame
        // must be drawn — the window reads as "open" with the sequence
        // animating in its interior. The Black-ICE takeover is the one
        // deliberate exception (design doc: "the only time the window's
        // frame breaks"), so it stays frameless.
        if (sess->window_seq.kind != WindowSeqKind::BlackIceTakeover)
            draw_window_chrome(r, wr, sess->netspace.window_state,
                               game.hacking().blink_phase());
        draw_window_sequence(r, wr, *sess, game.hacking().blink_phase());
        return;   // sequence owns the whole window; skip normal layout
    }

    // Chrome — outer border + horizontal separators (one per band boundary).
    // At Critical/Blackwall the interior dividers stay drawn but violently
    // fracture into RED corruption — the UI shredding itself reads as
    // "critical", where a blank-disappearing separator just read as a bug.
    draw_window_chrome(r, wr, sess->netspace.window_state,
                       game.hacking().blink_phase());

    // Five band-boundary separators (window-relative rows).
    // Skip all of them if the window is too short to fit the full bottom block
    // (pathological resize) — avoids overwriting the bottom chrome border.
    const int kMinBandH = deck_slots + 1 + kLogRows + 12;
    const int sep_hdr     = (b.header.y  + b.header.h)  - wr.y; // under header
    const int sep_caption = (b.caption.y + b.caption.h) - wr.y; // under caption
    const int sep_deck    = (b.deck.y    + b.deck.h)    - wr.y; // under deck
    const int sep_vitals  = (b.vitals.y  + b.vitals.h)  - wr.y; // under vitals
    const int sep_log     = (b.log.y     + b.log.h)     - wr.y; // under log

    if (wr.h >= kMinBandH) {
        draw_horizontal_separator(r, wr, sep_hdr);
        draw_horizontal_separator(r, wr, sep_caption);
        draw_horizontal_separator(r, wr, sep_deck);
        draw_horizontal_separator(r, wr, sep_vitals);
        draw_horizontal_separator(r, wr, sep_log);

        if (sess->netspace.window_state == WindowState::Critical ||
            sess->netspace.window_state == WindowState::Blackwall) {
            auto mix = [](uint32_t v) {
                v ^= v >> 16; v *= 0x7feb352du;
                v ^= v >> 15; v *= 0x846ca68bu;
                v ^= v >> 16; return v;
            };
            static const char* const kBreak[] = {
                "\xe2\x95\xb3",    // ╳
                "\xe2\x95\xaa",    // ╪
                "\xe2\x95\xab",    // ╫
                "\xe2\x96\x93",    // ▓
                "\xe2\x96\x88",    // █
                "\xc2\xa7",        // §
                "\xce\xa3",        // Σ
                " ",               // dropout
            };
            constexpr uint32_t kBreakN = 8;
            const uint32_t t  = static_cast<uint32_t>(game.hacking().blink_phase());
            const uint32_t ep = t / 2u;                  // tear relocates fast
            auto fracture_row = [&](int yr, uint32_t eid) {
                const int N = wr.w - 2;
                if (N <= 4) return;
                uint32_t hh = mix(eid * 2654435761u ^ (ep + 1u) * 40503u);
                if ((hh % 4u) == 0u) return;             // ~1/4 epochs: clean
                int len = 4 + static_cast<int>((hh >> 3) % 10u);   // 4..13
                if (len > N) len = N;
                int s = static_cast<int>((hh >> 8)
                                         % static_cast<uint32_t>(N - len + 1));
                for (int k = 0; k < len; ++k) {
                    uint32_t g = mix(static_cast<uint32_t>(s + k) * 0x9e3779b9u
                                   ^ (t * 2654435761u));
                    r.draw_glyph(wr.x + 1 + s + k, wr.y + yr,
                                 kBreak[(g >> 13) % kBreakN], Color::Red);
                }
            };
            fracture_row(sep_hdr,  11u);
            fracture_row(sep_deck, 22u);
            fracture_row(sep_log,  33u);
        }
    }

    // Populated layout slots.
    draw_top_status(game, r, wr, *sess);
    // subtitle row removed this slice (Phase 5 folds it away)
    draw_deck_strip(game, r, b.vitals, *sess);

    draw_playfield(game, r, PlayfieldRect{ b.field.x, b.field.y,
                                          b.field.w, b.field.h }, *sess);
    draw_field_caption(r, b.caption, *sess);
    draw_log_pane(r, b.log, *sess, game.hacking().blink_phase());
    draw_meatworld_footer(r, b.footer, *sess);
    draw_deck_panel(game, r, b.deck, *sess);

    if (sess->netspace.window_state == WindowState::Blackwall &&
        !sess->window_seq.active()) {
        int ph = game.hacking().blink_phase();
        const char* zal[] = { "\xce\xa3", "\xce\xa8" };   // Σ Ψ — out-of-vocabulary
        for (int k=0;k<5;++k) {
            int i = (ph*3 + k*29) % (wr.w-2) + 1;
            r.draw_glyph(wr.x+i, wr.y, zal[k&1], Color::Green);
        }
    }

    // Phase 4: ghost dialog modal — drawn last so it composites above everything.
    if (sess->ghost_dialog.open)
        draw_ghost_dialog(r, wr, *sess);
}

// ---------------------------------------------------------------------------
// Slice 1 band-geometry self-test (pure — no rendering)
// ---------------------------------------------------------------------------

bool selftest_bands(std::string& err) {
    struct TestCase { int sw, sh, deck_slots; };
    static const TestCase kCases[] = {
        { 80,  40, 1 }, {  80,  40, 4 }, {  80,  40, 6 },
        { 100, 50, 1 }, { 100,  50, 4 }, { 100,  50, 6 },
        { 120, 60, 1 }, { 120,  60, 4 }, { 120,  60, 6 },
        {  60, 30, 1 }, {  60,  30, 4 }, {  60,  30, 6 },
    };

    for (const auto& tc : kCases) {
        const int sw = tc.sw, sh = tc.sh, deck_slots = tc.deck_slots;
        WindowRect wr = compute_window_rect(sw, sh);
        const int kmin = deck_slots + 1 + kLogRows + 12;
        if (wr.h < kmin) continue;   // below documented minimum — geometry intentionally degrades

        NetBands b = compute_bands(wr, deck_slots);

        auto fail = [&](const char* what) -> bool {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "(%d,%d,ds=%d) %s", sw, sh, deck_slots, what);
            err = buf;
            return false;
        };

        // Fixed-height bands.
        if (b.header.h  != 1)           return fail("header.h!=1");
        if (b.caption.h != 1)           return fail("caption.h!=1");
        if (b.vitals.h  != 1)           return fail("vitals.h!=1");
        if (b.footer.h  != 1)           return fail("footer.h!=1");
        if (b.log.h     != kLogRows)    return fail("log.h!=kLogRows");
        if (b.deck.h    != 1 + deck_slots) return fail("deck.h!=1+deck_slots");
        if (b.field.h   < 1)            return fail("field.h<1");

        // Interior containment for every band.
        const int ix = wr.x + 1;
        const int ir = wr.x + wr.w - 1;   // one past last interior column
        const int iy = wr.y + 1;
        const int ib = wr.y + wr.h - 1;   // one past last interior row
        auto check_rect = [&](const Rect& r2, const char* name) -> bool {
            char buf[64];
            if (r2.x < ix) { std::snprintf(buf, sizeof(buf), "%s.x<ix", name); return fail(buf); }
            if (r2.x + r2.w > ir) { std::snprintf(buf, sizeof(buf), "%s right>ir", name); return fail(buf); }
            if (r2.y < iy) { std::snprintf(buf, sizeof(buf), "%s.y<iy", name); return fail(buf); }
            if (r2.y + r2.h > ib) { std::snprintf(buf, sizeof(buf), "%s bottom>ib", name); return fail(buf); }
            return true;
        };
        if (!check_rect(b.header,  "header"))  return false;
        if (!check_rect(b.field,   "field"))   return false;
        if (!check_rect(b.caption, "caption")) return false;
        if (!check_rect(b.deck,    "deck"))    return false;
        if (!check_rect(b.vitals,  "vitals"))  return false;
        if (!check_rect(b.log,     "log"))     return false;
        if (!check_rect(b.footer,  "footer"))  return false;

        // Footer pinned at wr.y + wr.h - 2.
        if (b.footer.y != wr.y + wr.h - 2) return fail("footer.y!=wr.y+wr.h-2");

        // Strict top→bottom order (no overlap for adjacent pairs in render order).
        // header→field: a separator row sits between them, so field.y >= header.y+header.h+1.
        if (b.field.y < b.header.y + b.header.h + 1) return fail("field.y<header bottom+1");
        // Remaining adjacent pairs: next.y >= cur.y + cur.h (no overlap; separator may follow).
        if (b.caption.y < b.field.y   + b.field.h)   return fail("caption overlaps field");
        if (b.deck.y    < b.caption.y + b.caption.h)  return fail("deck overlaps caption");
        if (b.vitals.y  < b.deck.y    + b.deck.h)     return fail("vitals overlaps deck");
        if (b.log.y     < b.vitals.y  + b.vitals.h)   return fail("log overlaps vitals");
        if (b.footer.y  < b.log.y     + b.log.h)      return fail("footer overlaps log");
    }
    return true;
}


} // namespace astra::net_renderer
