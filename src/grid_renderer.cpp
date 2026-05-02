#include "astra/grid_renderer.h"

#include "astra/cyberdeck.h"
#include "astra/game.h"
#include "astra/grid_camera.h"
#include "astra/grid_ice.h"
#include "astra/grid_session.h"
#include "astra/grid_theme.h"
#include "astra/hacking_system.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <cstdio>
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
        case GridTile::Wall:            return " ";   // overridden by neighbour-aware path in render()
        case GridTile::Connector:       return connector_glyph;        // overridden by neighbour-aware path
        case GridTile::DeepGridGateway: return deep_grid_gateway_glyph;
        case GridTile::WarpAnchor:      return warp_anchor_glyph;
        case GridTile::DeviceAvatar:    return " ";   // overridden in render() via device_avatar_glyph
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
        case GridTile::Wall:            return floor;             // walls share floor's Blue per spec §6 substrate
        case GridTile::Connector:       return connector;         // DarkGray
        case GridTile::DeepGridGateway: return deep_grid_gateway; // Cyan
        case GridTile::WarpAnchor:      return warp_anchor;       // BrightWhite
        case GridTile::DeviceAvatar:    return Color::BrightWhite; // device-avatar always BrightWhite
    }
    return Color::White;
}

} // namespace

namespace {

// Window geometry — 70% × 70% of screen, centered.
struct WindowRect { int x, y, w, h; };

WindowRect compute_window_rect(int screen_w, int screen_h) {
    int w = screen_w * 7 / 10;
    int h = screen_h * 7 / 10;
    if (w < 50) w = std::min(50, screen_w);
    if (h < 18) h = std::min(18, screen_h);
    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;
    return {x, y, w, h};
}

// Tron palette helpers — used by every chrome path.
constexpr Color kChrome = Color::Cyan;

void draw_window_chrome(Renderer& r, const WindowRect& wr) {
    // Corners
    r.draw_glyph(wr.x,            wr.y,            "\xe2\x95\x94", kChrome); // ╔
    r.draw_glyph(wr.x + wr.w - 1, wr.y,            "\xe2\x95\x97", kChrome); // ╗
    r.draw_glyph(wr.x,            wr.y + wr.h - 1, "\xe2\x95\x9a", kChrome); // ╚
    r.draw_glyph(wr.x + wr.w - 1, wr.y + wr.h - 1, "\xe2\x95\x9d", kChrome); // ╝
    // Top + bottom edges
    for (int i = 1; i < wr.w - 1; ++i) {
        r.draw_glyph(wr.x + i,            wr.y,            "\xe2\x95\x90", kChrome); // ═
        r.draw_glyph(wr.x + i,            wr.y + wr.h - 1, "\xe2\x95\x90", kChrome);
    }
    // Left + right edges
    for (int j = 1; j < wr.h - 1; ++j) {
        r.draw_glyph(wr.x,            wr.y + j,        "\xe2\x95\x91", kChrome); // ║
        r.draw_glyph(wr.x + wr.w - 1, wr.y + j,        "\xe2\x95\x91", kChrome);
    }
}

void draw_horizontal_separator(Renderer& r, const WindowRect& wr, int y_in_window) {
    int y = wr.y + y_in_window;
    r.draw_glyph(wr.x,            y, "\xe2\x95\xa0", kChrome); // ╠
    r.draw_glyph(wr.x + wr.w - 1, y, "\xe2\x95\xa3", kChrome); // ╣
    for (int i = 1; i < wr.w - 1; ++i) {
        r.draw_glyph(wr.x + i, y, "\xe2\x95\x90", kChrome); // ═
    }
}

} // namespace

void render(Game& game, Renderer& r) {
    const auto* sess = game.hacking().session();
    if (!sess) return;

    int sw = r.get_width();
    int sh = r.get_height();
    WindowRect wr = compute_window_rect(sw, sh);

    // Chrome — outer border + horizontal separators between layout rows.
    draw_window_chrome(r, wr);
    draw_horizontal_separator(r, wr, 2);              // below top status (row 1)
    draw_horizontal_separator(r, wr, 4);              // below deck strip (row 3)
    draw_horizontal_separator(r, wr, wr.h - 3);       // above program bar (last-1)

    // Stubbed slots — populated in Cut 3.
    r.draw_string(wr.x + 2, wr.y + 1,        "TOP STATUS [stub]");
    r.draw_string(wr.x + 2, wr.y + 3,        "DECK STRIP [stub]");
    r.draw_string(wr.x + 2, wr.y + 5,        "PLAYFIELD [stub]");
    r.draw_string(wr.x + 2, wr.y + wr.h - 2, "PROGRAM BAR [stub]");
}

} // namespace astra::grid_renderer
