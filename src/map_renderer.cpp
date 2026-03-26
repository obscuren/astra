#include "astra/map_renderer.h"
#include "astra/combat_system.h"
#include "astra/input_manager.h"
#include "astra/world_manager.h"
#include "astra/player.h"
#include "astra/overworld_stamps.h"

namespace astra {

static char star_at(int x, int y) {
    unsigned h = static_cast<unsigned>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    h ^= h >> 16;
    if ((h % 100) >= 3) return '\0';
    unsigned st = (h >> 8) % 10;
    if (st < 6) return '.';
    if (st < 9) return '*';
    return '+';
}

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
}

static char floor_scatter(int x, int y, Biome biome) {
    if (biome == Biome::Station) return '.';

    // Simple spatial hash
    unsigned h = static_cast<unsigned>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    h ^= h >> 16;
    int roll = static_cast<int>(h % 100);

    struct ScatterSet { int threshold; const char* glyphs; int count; };
    ScatterSet s;
    switch (biome) {
        case Biome::Rocky:    s = {15, ",:`",  3}; break;
        case Biome::Volcanic: s = {20, ",';" , 3}; break;
        case Biome::Ice:      s = {12, "'`,",  3}; break;
        case Biome::Sandy:    s = {20, ",`:",  3}; break;
        case Biome::Aquatic:  s = {10, ",:",   2}; break;
        case Biome::Fungal:   s = {18, "\",'", 3}; break;
        case Biome::Crystal:  s = {15, "*'`",  3}; break;
        case Biome::Corroded: s = {20, ",:;",  3}; break;
        case Biome::Forest:   s = {18, "\",'", 3}; break;
        case Biome::Grassland:s = {15, ",`.",  3}; break;
        case Biome::Jungle:   s = {22, "\",'", 3}; break;
        default: return '.';
    }
    if (roll >= s.threshold) return '.';
    return s.glyphs[h / 100 % s.count];
}

static Color overworld_tile_color(Tile tile, Biome biome) {
    switch (tile) {
        case Tile::OW_Plains:
            switch (biome) {
                case Biome::Ice:      return Color::White;
                case Biome::Rocky:    return Color::DarkGray;
                case Biome::Sandy:    return Color::Yellow;
                default:              return Color::Green;
            }
        case Tile::OW_Mountains:   return Color::White;
        case Tile::OW_Crater:      return Color::DarkGray;
        case Tile::OW_IceField:    return Color::Cyan;
        case Tile::OW_LavaFlow:    return Color::Red;
        case Tile::OW_Desert:      return Color::Yellow;
        case Tile::OW_Fungal:      return Color::Green;
        case Tile::OW_Forest:      return Color::Green;
        case Tile::OW_River:       return Color::Blue;
        case Tile::OW_Lake:        return Color::Cyan;
        case Tile::OW_Swamp:       return static_cast<Color>(58);
        case Tile::OW_CaveEntrance:return Color::Magenta;
        case Tile::OW_Ruins:       return Color::BrightMagenta;
        case Tile::OW_Settlement:  return Color::Yellow;
        case Tile::OW_CrashedShip: return Color::Cyan;
        case Tile::OW_Outpost:     return Color::Green;
        case Tile::OW_Landing:     return static_cast<Color>(14); // bright cyan
        default:                   return Color::White;
    }
}

void render_map(const MapRenderContext& rc) {
    DrawContext ctx(rc.renderer, rc.map_rect);

    for (int sy = 0; sy < rc.map_rect.h; ++sy) {
        for (int sx = 0; sx < rc.map_rect.w; ++sx) {
            int mx = rc.camera_x + sx;
            int my = rc.camera_y + sy;

            // Starfield backdrop — space stations only
            if (rc.world.map().biome() == Biome::Station && rc.world.map().get(mx, my) == Tile::Empty) {
                char star = star_at(mx, my);
                if (star) {
                    Color c = (star == '*' || star == '+') ? Color::White : Color::Cyan;
                    ctx.put(sx, sy, star, c);
                }
            }

            // Tiles respect FOV
            Visibility v = rc.world.visibility().get(mx, my);
            if (v == Visibility::Unexplored) continue;

            Tile tile_at = rc.world.map().get(mx, my);
            char g = tile_glyph(tile_at);
            if (g == ' ' && tile_at != Tile::Fixture) continue;

            // Overworld: no FOV dimming, use overworld colors + UTF-8 glyphs
            if (rc.world.map().map_type() == MapType::Overworld) {
                Color c = overworld_tile_color(tile_at, rc.world.map().biome());
                uint8_t gov = rc.world.map().glyph_override(mx, my);
                const char* og = (gov != 0) ? stamp_glyph(gov) : nullptr;
                if (!og) og = overworld_glyph(tile_at, mx, my);
                ctx.put(sx, sy, og, c);
                continue;
            }

            auto bc = biome_colors(rc.world.map().biome());
            Biome biome = rc.world.map().biome();
            if (v == Visibility::Visible) {
                Color c = bc.floor;
                const char* utf8 = nullptr;

                if (tile_at == Tile::StructuralWall) {
                    uint8_t mat = rc.world.map().glyph_override(mx, my);
                    switch (mat) {
                        case 1:  // Concrete
                            c = static_cast<Color>(245);  // medium gray
                            utf8 = "\xe2\x96\x93";        // ▓
                            break;
                        case 2:  // Wood
                            c = static_cast<Color>(137);   // brown/tan
                            utf8 = "\xe2\x96\x92";         // ▒
                            break;
                        case 3:  // Salvage
                            c = static_cast<Color>(240);   // dark gray
                            utf8 = "\xe2\x96\x91";         // ░
                            break;
                        default: // Metal (0)
                            c = Color::White;
                            utf8 = "\xe2\x96\x88";         // █
                            break;
                    }
                }
                else if (tile_at == Tile::Wall) {
                    c = bc.wall;
                    utf8 = dungeon_wall_glyph(biome, mx, my);
                }
                else if (tile_at == Tile::Portal) {
                    c = Color::Magenta;
                    utf8 = dungeon_portal_glyph();
                }
                else if (tile_at == Tile::Water) {
                    c = bc.water;
                    utf8 = dungeon_water_glyph(biome, mx, my);
                }
                else if (tile_at == Tile::Ice) {
                    c = static_cast<Color>(39);
                    utf8 = dungeon_water_glyph(biome, mx, my);
                }
                else if (tile_at == Tile::Fixture) {
                    int fid = rc.world.map().fixture_id(mx, my);
                    if (fid >= 0 && fid < rc.world.map().fixture_count()) {
                        const auto& f = rc.world.map().fixture(fid);
                        if (f.utf8_glyph) {
                            utf8 = f.utf8_glyph;
                        } else {
                            g = f.glyph;
                        }
                        c = f.color;
                    } else {
                        g = '?'; c = Color::Red;
                    }
                }
                else if (tile_at == Tile::IndoorFloor) {
                    c = static_cast<Color>(137);  // warm tan/brown — reads as plating
                    utf8 = "\xe2\x96\xaa";        // ▪ (small filled square)
                }
                else if (tile_at == Tile::Floor) {
                    char sg = floor_scatter(mx, my, biome);
                    if (sg != '.') {
                        g = sg;
                        c = bc.remembered; // dimmer shade for scatter
                    }
                }

                if (utf8) {
                    ctx.put(sx, sy, utf8, c);
                } else {
                    ctx.put(sx, sy, g, c);
                }
            } else {
                // Remembered tiles: use UTF-8 glyphs too
                const char* utf8 = nullptr;
                if (tile_at == Tile::StructuralWall) {
                    uint8_t mat = rc.world.map().glyph_override(mx, my);
                    switch (mat) {
                        case 1:  utf8 = "\xe2\x96\x93"; break; // ▓
                        case 2:  utf8 = "\xe2\x96\x92"; break; // ▒
                        case 3:  utf8 = "\xe2\x96\x91"; break; // ░
                        default: utf8 = "\xe2\x96\x88"; break; // █
                    }
                }
                else if (tile_at == Tile::Wall)
                    utf8 = dungeon_wall_glyph(biome, mx, my);
                else if (tile_at == Tile::IndoorFloor)
                    utf8 = "\xe2\x96\xaa";   // ▪
                else if (tile_at == Tile::Portal)
                    utf8 = dungeon_portal_glyph();
                else if (tile_at == Tile::Water || tile_at == Tile::Ice)
                    utf8 = dungeon_water_glyph(biome, mx, my);
                else if (tile_at == Tile::Fixture) {
                    int fid = rc.world.map().fixture_id(mx, my);
                    if (fid >= 0 && fid < rc.world.map().fixture_count()) {
                        const auto& f = rc.world.map().fixture(fid);
                        if (f.utf8_glyph) {
                            utf8 = f.utf8_glyph;
                        } else {
                            g = f.glyph;
                        }
                    }
                }

                if (utf8)
                    ctx.put(sx, sy, utf8, bc.remembered);
                else
                    ctx.put(sx, sy, g, bc.remembered);
            }
        }
    }

    // Draw visible ground items
    for (const auto& gi : rc.world.ground_items()) {
        if (rc.world.visibility().get(gi.x, gi.y) == Visibility::Visible) {
            ctx.put(gi.x - rc.camera_x, gi.y - rc.camera_y,
                    gi.item.glyph, gi.item.color);
        }
    }

    // Draw visible NPCs
    for (const auto& npc : rc.world.npcs()) {
        if (npc.alive() && rc.world.visibility().get(npc.x, npc.y) == Visibility::Visible) {
            ctx.put(npc.x - rc.camera_x, npc.y - rc.camera_y, npc.glyph, npc.color);
        }
    }

    // Draw player relative to camera
    ctx.put(rc.player.x - rc.camera_x, rc.player.y - rc.camera_y, '@', Color::Yellow);

    // Draw targeting line and reticule
    if (rc.combat.targeting()) {
        // Bresenham line from player to reticule
        int x0 = rc.player.x, y0 = rc.player.y;
        int x1 = rc.combat.target_x(), y1 = rc.combat.target_y();
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        // Determine weapon range for coloring
        int weapon_range = 0;
        const auto& rw = rc.player.equipment.missile;
        if (rw && rw->ranged) weapon_range = rw->ranged->max_range;

        int lx = x0, ly = y0;
        while (lx != x1 || ly != y1) {
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; lx += sx; }
            if (e2 <  dx) { err += dx; ly += sy; }
            if (lx == x1 && ly == y1) break; // don't draw on reticule pos
            int scx = lx - rc.camera_x, scy = ly - rc.camera_y;
            if (scx >= 0 && scx < rc.map_rect.w && scy >= 0 && scy < rc.map_rect.h) {
                int tile_dist = chebyshev_dist(x0, y0, lx, ly);
                Color line_color = (weapon_range > 0 && tile_dist <= weapon_range)
                    ? Color::Green : Color::Red;
                ctx.put(scx, scy, '*', line_color);
            }
        }

        // Reticule: blink only when over something interesting (NPC, item, etc.)
        int rx = rc.combat.target_x() - rc.camera_x, ry = rc.combat.target_y() - rc.camera_y;
        if (rx >= 0 && rx < rc.map_rect.w && ry >= 0 && ry < rc.map_rect.h) {
            bool has_entity = false;
            for (const auto& npc : rc.world.npcs()) {
                if (npc.alive() && npc.x == rc.combat.target_x() && npc.y == rc.combat.target_y()) {
                    has_entity = true;
                    break;
                }
            }
            if (!has_entity || rc.combat.blink_phase() % 2 == 0) {
                int target_dist = chebyshev_dist(rc.player.x, rc.player.y, rc.combat.target_x(), rc.combat.target_y());
                Color ret_color = (weapon_range > 0 && target_dist <= weapon_range)
                    ? Color::Green : Color::Red;
                ctx.put(rx, ry, '+', ret_color);
            }
            // else: let the underlying NPC/item glyph show through
        }
    }

    // Draw look mode cursor — read cell BEFORE overwriting with reticule
    if (rc.input.looking()) {
        int lsx = rc.input.look_x() - rc.camera_x;
        int lsy = rc.input.look_y() - rc.camera_y;
        if (lsx >= 0 && lsx < rc.map_rect.w && lsy >= 0 && lsy < rc.map_rect.h) {
            // Cache the actual glyph/color at this position
            int screen_x = rc.map_rect.x + lsx;
            int screen_y = rc.map_rect.y + lsy;
            {
                char buf[5] = {};
                Color fg = Color::White;
                rc.renderer->read_cell(screen_x, screen_y, buf, fg);
                rc.input.cache_look_cell(buf, fg);
            }

            if (rc.input.look_blink() % 2 == 0) {
                ctx.put(lsx, lsy, 'X', Color::Yellow);
            }
        }
    }
}

} // namespace astra
