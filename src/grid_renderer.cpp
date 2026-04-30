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

const char* glyph_for(GridTile t) {
    using namespace grid_theme;
    switch (t) {
        case GridTile::Floor:         return floor_glyph;
        case GridTile::Firewall:      return firewall_glyph;
        case GridTile::DataNode:      return data_node_glyph;
        case GridTile::Gateway:       return gateway_glyph;
        case GridTile::ExitNode:      return exit_glyph;
        case GridTile::EncryptedFile: return encrypted_glyph;
        case GridTile::Wall:          return " ";
    }
    return " ";
}

Color color_for(GridTile t) {
    using namespace grid_theme;
    switch (t) {
        case GridTile::Floor:         return floor;
        case GridTile::Firewall:      return firewall;
        case GridTile::DataNode:      return data_node;
        case GridTile::Gateway:       return gateway;
        case GridTile::ExitNode:      return exit_node;
        case GridTile::EncryptedFile: return encrypted;
        case GridTile::Wall:          return Color::Black;
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

    // Tiles — only iterate the viewport, source from camera-shifted sector.
    for (int y = 0; y < s_camera.viewport_h; ++y) {
        for (int x = 0; x < s_camera.viewport_w; ++x) {
            int tx = x + s_camera.cam_x;
            int ty = y + s_camera.cam_y;
            if (tx < 0 || ty < 0 || tx >= s.sector.w || ty >= s.sector.h) continue;
            GridTile t = s.sector.at(tx, ty);
            r.draw_glyph(origin_x + x, origin_y + y, glyph_for(t), color_for(t));
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
