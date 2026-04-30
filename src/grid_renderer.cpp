#include "astra/grid_renderer.h"

#include "astra/cyberdeck.h"
#include "astra/game.h"
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

    const int origin_x = 1;
    const int origin_y = 1;

    // Tiles
    for (int y = 0; y < s.sector.h; ++y) {
        for (int x = 0; x < s.sector.w; ++x) {
            GridTile t = s.sector.at(x, y);
            r.draw_glyph(origin_x + x, origin_y + y, glyph_for(t), color_for(t));
        }
    }

    // ICE
    for (const auto& ice : s.ice) {
        const char* g = ice.color == IceColor::White ? grid_theme::white_ice_glyph
                      : ice.color == IceColor::Gray  ? grid_theme::gray_ice_glyph
                      :                                 grid_theme::black_ice_glyph;
        Color c = ice.color == IceColor::White ? grid_theme::white_ice
                : ice.color == IceColor::Gray  ? grid_theme::gray_ice
                :                                grid_theme::black_ice;
        r.draw_glyph(origin_x + ice.x, origin_y + ice.y, g, c);
    }

    // Avatar
    r.draw_glyph(origin_x + s.avatar_x, origin_y + s.avatar_y,
                 grid_theme::avatar_glyph, grid_theme::avatar);

    // HUD: stack hp/ram/trace/heat at top of right pane.
    const int hud_x = origin_x + s.sector.w + 2;
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
