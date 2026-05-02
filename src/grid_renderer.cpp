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

#include <cstdio>
#include <string>

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
    }
    return Color::White;
}

} // namespace

void render(Game& game, Renderer& r) {
    const auto* sess = game.hacking().session();
    if (!sess) return;
    const auto& s = *sess;

    r.clear();

    static GridCamera s_camera;
    s_camera.follow(s.avatar_x, s.avatar_y, s.sector.w, s.sector.h);

    const int origin_x = 1;
    const int origin_y = 1;

    auto neigh = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= s.sector.w || y >= s.sector.h) return false;
        return is_connectable(s.sector.at(x, y));
    };

    // Tiles — only iterate the viewport, source from camera-shifted sector.
    for (int y = 0; y < s_camera.viewport_h; ++y) {
        for (int x = 0; x < s_camera.viewport_w; ++x) {
            int tx = x + s_camera.cam_x;
            int ty = y + s_camera.cam_y;
            if (tx < 0 || ty < 0 || tx >= s.sector.w || ty >= s.sector.h) continue;
            GridTile t = s.sector.at(tx, ty);
            const char* glyph;
            Color       color;
            if (t == GridTile::Wall || t == GridTile::Connector) {
                glyph = wall_glyph_for_neighbours(
                    neigh(tx, ty - 1), neigh(tx, ty + 1),
                    neigh(tx + 1, ty), neigh(tx - 1, ty));
                color = (t == GridTile::Connector) ? grid_theme::connector : grid_theme::floor;
            } else {
                glyph = glyph_for(t);
                color = color_for(t);
            }
            r.draw_glyph(origin_x + x, origin_y + y, glyph, color);
        }
    }

    auto cull = [&](int wx, int wy, int& sx, int& sy) {
        sx = wx - s_camera.cam_x;
        sy = wy - s_camera.cam_y;
        return sx >= 0 && sy >= 0 && sx < s_camera.viewport_w && sy < s_camera.viewport_h;
    };

    // ICE
    for (const auto& ice : s.ice) {
        int sx, sy;
        if (!cull(ice.x, ice.y, sx, sy)) continue;
        const char* g = ice.color == IceColor::White ? grid_theme::white_ice_glyph
                      : ice.color == IceColor::Gray  ? grid_theme::gray_ice_glyph
                      :                                 grid_theme::black_ice_glyph;
        Color c = ice.color == IceColor::White ? grid_theme::white_ice
                : ice.color == IceColor::Gray  ? grid_theme::gray_ice
                :                                grid_theme::black_ice;
        r.draw_glyph(origin_x + sx, origin_y + sy, g, c);
    }

    // Avatar — always within the viewport thanks to the deadzone follow.
    {
        int sx, sy;
        if (cull(s.avatar_x, s.avatar_y, sx, sy)) {
            r.draw_glyph(origin_x + sx, origin_y + sy,
                         grid_theme::avatar_glyph, grid_theme::avatar);
        }
    }

    // HUD: stack hp/ram/trace/heat at top of right pane.
    const int hud_x = origin_x + s_camera.viewport_w + 2;
    int hy = origin_y;

    auto bar = [&](const char* label, int v, int max) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%-6s %3d/%3d", label, v, max);
        r.draw_string(hud_x, hy, buf);
        ++hy;
    };

    bar("HP",    s.avatar_hp, s.avatar_hp_max);
    bar("RAM",   s.ram,       s.ram_max);
    bar("Trace", s.trace,     100);

    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
        const auto& cd = *(*deck_slot)->deck;
        bar("Heat", cd.heat_current, cd.stats.heat_cap);
    }

    r.draw_string(hud_x, hy + 1, "GRID");
    r.draw_string(hud_x, hy + 2, "[`] dev console");
}

} // namespace astra::grid_renderer
