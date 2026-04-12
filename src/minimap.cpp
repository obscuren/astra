#include "astra/minimap.h"
#include "astra/faction.h"
#include "astra/poi_placement.h"

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
        case Tile::OW_Barren:
            return Color::DarkGray;
        case Tile::OW_Mountains:
            return Color::White;
        case Tile::OW_Forest:
        case Tile::OW_Fungal:
            return Color::Green;
        case Tile::OW_River:
        case Tile::OW_Lake:
            return Color::Blue;
        case Tile::OW_Swamp:
            return static_cast<Color>(22);
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
                   const Player& player,
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

    // Each minimap cell covers a 3x3 block of map tiles.
    // Each terminal row covers 2 such blocks vertically (half-blocks),
    // so one terminal cell = 3 wide x 6 tall map tiles.
    static constexpr int SCALE = 3;
    int view_w = panel_w * SCALE;       // map tiles visible horizontally
    int view_h = panel_h * 2 * SCALE;   // map tiles visible vertically

    // Player-centered viewport origin (map coords), clamped to edges
    int vx = player_x - view_w / 2;
    int vy = player_y - view_h / 2;
    vx = std::clamp(vx, 0, std::max(0, map_w - view_w));
    vy = std::clamp(vy, 0, std::max(0, map_h - view_h));

    // Resolve a map cell to its minimap color.
    // No FOV distinction — all explored tiles use dim colors.
    // The center tile of each 3x3 block is sampled.
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

        // All explored/visible tiles rendered at dim — no FOV on minimap
        Tile t = map.get(mx2, my2);
        // Render-side invariant: undiscovered hidden POIs show as underlying biome.
        if (map_type == MapType::Overworld) {
            if (const auto* hidden = map.find_hidden_poi(mx2, my2)) {
                t = hidden->underlying_tile;
            }
        }
        Color c = tile_color(t, map_type);
        if (c == Color::Default) return Color::Black;
        return dim_color(c);
    };

    // Render half-block cells — each cell samples the center of a 3x3 block
    for (int ty = 0; ty < panel_h; ++ty) {
        for (int tx = 0; tx < panel_w; ++tx) {
            // Center of each 3x3 block (offset by 1 into the block)
            int mx = vx + tx * SCALE + SCALE / 2;
            int my_top = vy + (ty * 2) * SCALE + SCALE / 2;
            int my_bot = vy + (ty * 2 + 1) * SCALE + SCALE / 2;

            Color top = cell_color(mx, my_top);
            Color bot = cell_color(mx, my_bot);

            if (top == Color::Black && bot == Color::Black)
                continue;

            ctx.put(tx, ty, UPPER_HALF, top, bot);
        }
    }

    // Draw exits/portals — scan each 3x3 block for any exit tile
    for (int ty = 0; ty < panel_h; ++ty) {
        for (int tx = 0; tx < panel_w; ++tx) {
            int bx = vx + tx * SCALE;
            int by_top = vy + (ty * 2) * SCALE;
            int by_bot = vy + (ty * 2 + 1) * SCALE;

            auto block_has_exit = [&](int bx0, int by0) -> bool {
                for (int dy = 0; dy < SCALE; ++dy) {
                    for (int dx = 0; dx < SCALE; ++dx) {
                        int ex = bx0 + dx, ey = by0 + dy;
                        if (ex < 0 || ex >= map_w || ey < 0 || ey >= map_h) continue;
                        if (vis.get(ex, ey) == Visibility::Unexplored) continue;
                        if (is_exit_tile(map.get(ex, ey), map, ex, ey)) return true;
                    }
                }
                return false;
            };

            bool top_exit = block_has_exit(bx, by_top);
            bool bot_exit = block_has_exit(bx, by_bot);
            if (!top_exit && !bot_exit) continue;

            // Use dim exit color (no FOV distinction on minimap)
            Color exit_color = dim_color(Color::Magenta);
            int mx = vx + tx * SCALE + SCALE / 2;
            int my_top = vy + (ty * 2) * SCALE + SCALE / 2;
            int my_bot = vy + (ty * 2 + 1) * SCALE + SCALE / 2;

            Color top_c = top_exit ? exit_color : cell_color(mx, my_top);
            Color bot_c = bot_exit ? exit_color : cell_color(mx, my_bot);

            ctx.put(tx, ty, UPPER_HALF, top_c, bot_c);
        }
    }

    // Draw NPCs (requires Scout's Eye skill)
    if (flags.scouts_eye) {
        for (const auto& npc : npcs) {
            if (!npc.alive()) continue;
            if (npc.x < 0 || npc.x >= map_w || npc.y < 0 || npc.y >= map_h) continue;
            if (vis.get(npc.x, npc.y) == Visibility::Unexplored) continue;

            bool hostile = is_hostile_to_player(npc.faction, player);

            int sx = (npc.x - vx) / SCALE;
            int sy = (npc.y - vy) / SCALE;
            int sy_cell = sy / 2;
            bool top_half = (sy % 2 == 0);

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
        int px = (player_x - vx) / SCALE;
        int py = (player_y - vy) / SCALE;
        int py_cell = py / 2;
        bool top_half = (py % 2 == 0);

        if (px >= 0 && px < panel_w && py_cell >= 0 && py_cell < panel_h) {
            if (top_half)
                ctx.put(px, py_cell, UPPER_HALF, Color::BrightYellow, Color::Black);
            else
                ctx.put(px, py_cell, UPPER_HALF, Color::Black, Color::BrightYellow);
        }
    }
}

} // namespace astra
