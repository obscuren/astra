#include "astra/minimap.h"

#include <algorithm>

namespace astra {

// Upper half-block: ▀ (U+2580)
// When rendered with fg=top_color, bg=bottom_color, this packs two rows per cell.
static constexpr const char* UPPER_HALF = "\xe2\x96\x80";

Color Minimap::tile_color(Tile t, MapType map_type) {
    switch (t) {
        // Indoor tiles
        case Tile::Floor:
        case Tile::IndoorFloor:
            return Color::DarkGray;
        case Tile::Wall:
        case Tile::StructuralWall:
            return Color::White;
        case Tile::Portal:
            return Color::Magenta;
        case Tile::Water:
            return Color::Blue;
        case Tile::Ice:
            return Color::Cyan;
        case Tile::Fixture:
            return Color::DarkGray;

        // Overworld / detail terrain
        case Tile::OW_Plains:
        case Tile::OW_Desert:
            return Color::Yellow;
        case Tile::OW_Mountains:
            return Color::White;
        case Tile::OW_Forest:
        case Tile::OW_Fungal:
            return Color::Green;
        case Tile::OW_River:
        case Tile::OW_Lake:
        case Tile::OW_Swamp:
            return Color::Blue;
        case Tile::OW_IceField:
            return Color::Cyan;
        case Tile::OW_LavaFlow:
            return Color::Red;
        case Tile::OW_Crater:
            return Color::DarkGray;

        // Overworld POIs — bright markers
        case Tile::OW_CaveEntrance:
            return Color::Cyan;
        case Tile::OW_Ruins:
            return Color::Magenta;
        case Tile::OW_Settlement:
            return Color::BrightYellow;
        case Tile::OW_CrashedShip:
            return Color::BrightMagenta;
        case Tile::OW_Outpost:
            return Color::BrightYellow;
        case Tile::OW_Landing:
            return Color::Green;

        default:
            return Color::Default; // don't draw
    }
}

Color Minimap::dim_color(Color c) {
    switch (c) {
        case Color::White:        return Color::DarkGray;
        case Color::Yellow:       return static_cast<Color>(58);  // dim olive
        case Color::Green:        return static_cast<Color>(22);  // dark green
        case Color::Blue:         return static_cast<Color>(17);  // dark blue
        case Color::Cyan:         return static_cast<Color>(23);  // dark cyan
        case Color::Red:          return static_cast<Color>(52);  // dark red
        case Color::Magenta:      return static_cast<Color>(53);  // dark magenta
        case Color::BrightYellow: return static_cast<Color>(58);  // dim olive
        case Color::BrightMagenta:return static_cast<Color>(53);  // dark magenta
        case Color::DarkGray:     return static_cast<Color>(236); // very dark gray
        default:                  return Color::DarkGray;
    }
}

bool Minimap::is_exit_tile(Tile t, const TileMap& map, int x, int y) {
    if (t == Tile::Portal) return true;
    if (t == Tile::Fixture) {
        int fid = map.fixture_id(x, y);
        if (fid >= 0) {
            auto ft = map.fixture(fid).type;
            return ft == FixtureType::DungeonHatch || ft == FixtureType::StairsUp;
        }
    }
    if (t == Tile::OW_CaveEntrance) return true;
    return false;
}

void Minimap::draw(UIContext& ctx,
                   const TileMap& map,
                   const VisibilityMap& vis,
                   int player_x, int player_y,
                   const std::vector<Npc>& npcs,
                   const MinimapFlags& flags) {
    int panel_w = ctx.width();
    int panel_h = ctx.height();
    if (panel_w <= 0 || panel_h <= 0) return;

    int map_w = map.width();
    int map_h = map.height();
    if (map_w <= 0 || map_h <= 0) return;

    MapType map_type = map.map_type();
    bool schematic = (map_type == MapType::SpaceStation ||
                      map_type == MapType::Starship);

    // Each terminal row shows 2 map rows via half-blocks.
    int view_w = panel_w;
    int view_h = panel_h * 2;

    // Player-centered viewport origin, clamped to edges
    int vx = player_x - view_w / 2;
    int vy = player_y - view_h / 2;
    vx = std::clamp(vx, 0, std::max(0, map_w - view_w));
    vy = std::clamp(vy, 0, std::max(0, map_h - view_h));

    // Resolve a map cell to its minimap color (visibility-aware)
    auto cell_color = [&](int mx2, int my2) -> Color {
        if (mx2 < 0 || mx2 >= map_w || my2 < 0 || my2 >= map_h)
            return Color::Black;

        Visibility v = vis.get(mx2, my2);
        if (v == Visibility::Unexplored) {
            if (schematic) {
                Tile t = map.get(mx2, my2);
                if (t == Tile::Wall || t == Tile::StructuralWall)
                    return static_cast<Color>(236);
            }
            return Color::Black;
        }

        Tile t = map.get(mx2, my2);
        Color c = tile_color(t, map_type);
        if (c == Color::Default) return Color::Black;

        if (v == Visibility::Explored)
            return dim_color(c);
        return c;
    };

    // Render half-block cells
    for (int ty = 0; ty < panel_h; ++ty) {
        for (int tx = 0; tx < panel_w; ++tx) {
            int mx = vx + tx;
            int my_top = vy + ty * 2;
            int my_bot = vy + ty * 2 + 1;

            Color top = cell_color(mx, my_top);
            Color bot = cell_color(mx, my_bot);

            if (top == Color::Black && bot == Color::Black)
                continue;

            ctx.put(tx, ty, UPPER_HALF, top, bot);
        }
    }

    // Draw exits/portals — always visible (base feature)
    for (int ty = 0; ty < panel_h; ++ty) {
        for (int tx = 0; tx < panel_w; ++tx) {
            int mx = vx + tx;
            int my_top = vy + ty * 2;
            int my_bot = vy + ty * 2 + 1;

            auto check_exit = [&](int ex, int ey) -> bool {
                if (ex < 0 || ex >= map_w || ey < 0 || ey >= map_h) return false;
                Visibility v = vis.get(ex, ey);
                if (v == Visibility::Unexplored) return false;
                return is_exit_tile(map.get(ex, ey), map, ex, ey);
            };

            bool top_exit = check_exit(mx, my_top);
            bool bot_exit = check_exit(mx, my_bot);
            if (!top_exit && !bot_exit) continue;

            Color exit_color = Color::Magenta;
            Color exit_dim = dim_color(Color::Magenta);

            // Preserve terrain color for non-exit half
            Color top_c = top_exit ? exit_color : cell_color(mx, my_top);
            Color bot_c = bot_exit ? exit_color : cell_color(mx, my_bot);

            if (top_exit && my_top < map_h && vis.get(mx, my_top) == Visibility::Explored)
                top_c = exit_dim;
            if (bot_exit && my_bot < map_h && vis.get(mx, my_bot) == Visibility::Explored)
                bot_c = exit_dim;

            ctx.put(tx, ty, UPPER_HALF, top_c, bot_c);
        }
    }

    // Draw NPCs (gated by MinimapFlags)
    if (flags.show_enemies || flags.show_npcs) {
        for (const auto& npc : npcs) {
            if (!npc.alive()) continue;
            if (npc.x < 0 || npc.x >= map_w || npc.y < 0 || npc.y >= map_h) continue;
            if (vis.get(npc.x, npc.y) != Visibility::Visible) continue;

            bool hostile = (npc.disposition == Disposition::Hostile);
            if (hostile && !flags.show_enemies) continue;
            if (!hostile && !flags.show_npcs) continue;

            int sx = npc.x - vx;
            int sy_cell = (npc.y - vy) / 2;
            bool top_half = ((npc.y - vy) % 2 == 0);

            if (sx < 0 || sx >= panel_w || sy_cell < 0 || sy_cell >= panel_h)
                continue;

            Color npc_color = hostile ? Color::Red : Color::Cyan;
            if (top_half)
                ctx.put(sx, sy_cell, UPPER_HALF, npc_color, Color::Black);
            else
                ctx.put(sx, sy_cell, UPPER_HALF, Color::Black, npc_color);
        }
    }

    // Draw player — always on top, bright yellow
    {
        int px = player_x - vx;
        int py_cell = (player_y - vy) / 2;
        bool top_half = ((player_y - vy) % 2 == 0);

        if (px >= 0 && px < panel_w && py_cell >= 0 && py_cell < panel_h) {
            if (top_half)
                ctx.put(px, py_cell, UPPER_HALF, Color::BrightYellow, Color::Black);
            else
                ctx.put(px, py_cell, UPPER_HALF, Color::Black, Color::BrightYellow);
        }
    }
}

} // namespace astra
