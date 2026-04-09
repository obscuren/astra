#include "astra/map_renderer.h"
#include "astra/animation.h"
#include "astra/combat_system.h"
#include "astra/input_manager.h"
#include "astra/world_manager.h"
#include "astra/player.h"
#include "astra/overworld_stamps.h"
#include "astra/world_context.h"
#include "astra/render_descriptor.h"

namespace astra {

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
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
        case Tile::OW_Swamp:       return static_cast<Color>(22);
        case Tile::OW_CaveEntrance:return Color::Magenta;
        case Tile::OW_Ruins:       return static_cast<Color>(178); // warm gold
        case Tile::OW_Settlement:  return Color::Yellow;
        case Tile::OW_CrashedShip: return Color::Cyan;
        case Tile::OW_Outpost:     return Color::Green;
        case Tile::OW_Beacon:      return Color::Cyan;
        case Tile::OW_Megastructure: return Color::Yellow;
        case Tile::OW_AlienTerrain: return Color::BrightMagenta;
        case Tile::OW_ScorchedEarth: return static_cast<Color>(208);
        case Tile::OW_GlassedCrater: return static_cast<Color>(136);
        case Tile::OW_Landing:     return static_cast<Color>(14); // bright cyan
        default:                   return Color::White;
    }
}

void render_map(const MapRenderContext& rc) {
    WorldContext wctx(rc.renderer, rc.map_rect);
    UIContext ctx(rc.renderer, rc.map_rect);  // kept for non-tile rendering

    for (int sy = 0; sy < rc.map_rect.h; ++sy) {
        for (int sx = 0; sx < rc.map_rect.w; ++sx) {
            int mx = rc.camera_x + sx;
            int my = rc.camera_y + sy;

            // Starfield backdrop — space stations only
            if (rc.world.map().biome() == Biome::Station && rc.world.map().get(mx, my) == Tile::Empty) {
                RenderDescriptor desc;
                desc.category = RenderCategory::Tile;
                desc.type_id  = static_cast<uint16_t>(Tile::Empty);
                desc.seed     = position_seed(mx, my);
                desc.flags    = RF_Starfield;
                desc.biome    = Biome::Station;
                wctx.put(sx, sy, desc);
            }

            // Tiles respect FOV
            Visibility v = rc.world.visibility().get(mx, my);
            if (v == Visibility::Unexplored) continue;

            Tile tile_at = rc.world.map().get(mx, my);
            char g = tile_glyph(tile_at);
            if (g == ' ' && tile_at != Tile::Fixture) continue;

            // Overworld: no FOV dimming, use overworld colors + UTF-8 glyphs
            if (rc.world.map().map_type() == MapType::Overworld) {
                // OW_Ruins: encode neighbor mask for baroque pipe rendering
                if (tile_at == Tile::OW_Ruins) {
                    uint8_t nb = 0;
                    auto& map = rc.world.map();
                    if (my > 0 && map.get(mx, my - 1) == Tile::OW_Ruins) nb |= 0x01;  // N
                    if (my + 1 < map.height() && map.get(mx, my + 1) == Tile::OW_Ruins) nb |= 0x02;  // S
                    if (mx + 1 < map.width() && map.get(mx + 1, my) == Tile::OW_Ruins) nb |= 0x04;  // E
                    if (mx > 0 && map.get(mx - 1, my) == Tile::OW_Ruins) nb |= 0x08;  // W

                    RenderDescriptor desc;
                    desc.category = RenderCategory::Tile;
                    desc.type_id = static_cast<uint16_t>(tile_at);
                    desc.seed = nb;
                    desc.biome = rc.world.map().biome();
                    desc.flags = RF_None;
                    wctx.put(sx, sy, desc);
                    continue;  // skip normal overworld rendering for this tile
                }

                uint8_t gov = rc.world.map().glyph_override(mx, my);

                // Stamp glyph overrides (non-quest) stay on old UIContext path
                // because stamp_glyph() returns custom per-cell glyphs
                bool has_stamp = (gov != 0 && gov != SG_QuestMarker);
                const char* stamp_og = has_stamp ? stamp_glyph(gov) : nullptr;

                // Animation overrides — semantic path via WorldContext
                bool has_anim = false;
                if (rc.animations) {
                    if (auto anim = rc.animations->query(mx, my)) {
                        has_anim = true;
                        wctx.put_animation(sx, sy, anim->type, anim->frame_index);
                    }
                }

                if (!has_anim) {
                    if (stamp_og) {
                        // Stamp glyph override — old path
                        Color c = overworld_tile_color(tile_at, rc.world.map().biome());
                        ctx.put(sx, sy, stamp_og, c);
                    } else {
                        // Descriptor-based rendering for overworld tiles
                        RenderDescriptor desc;
                        desc.category = RenderCategory::Tile;
                        desc.type_id  = static_cast<uint16_t>(tile_at);
                        desc.seed     = position_seed(mx, my);
                        desc.flags    = RF_None;
                        desc.biome    = rc.world.map().biome();
                        if (tile_at == Tile::OW_AlienTerrain &&
                            rc.world.map().alien_biome() != Biome::Station)
                            desc.biome = rc.world.map().alien_biome();
                        if (gov == SG_QuestMarker) desc.flags |= RF_Interactable;
                        wctx.put(sx, sy, desc);
                    }
                }
                continue;
            }

            Biome biome = rc.world.map().biome();

            // Fixtures — descriptor-based rendering via theme
            if (tile_at == Tile::Fixture) {
                int fid = rc.world.map().fixture_id(mx, my);
                if (fid >= 0 && fid < rc.world.map().fixture_count()) {
                    const auto& f = rc.world.map().fixture(fid);

                    if (v == Visibility::Visible) {
                        // Animation override — semantic path via WorldContext
                        if (rc.animations) {
                            if (auto anim = rc.animations->query(mx, my)) {
                                wctx.put_animation(sx, sy, anim->type, anim->frame_index);
                                continue;
                            }
                        }

                        RenderDescriptor desc;
                        desc.category = RenderCategory::Fixture;
                        desc.type_id = static_cast<uint16_t>(f.type);
                        desc.seed = position_seed(mx, my);
                        desc.biome = biome;
                        desc.flags = RF_Lit;
                        if (f.open) desc.flags |= RF_Open;
                        wctx.put(sx, sy, desc);
                    } else {
                        // Remembered fixture
                        RenderDescriptor desc;
                        desc.category = RenderCategory::Fixture;
                        desc.type_id = static_cast<uint16_t>(f.type);
                        desc.seed = position_seed(mx, my);
                        desc.biome = biome;
                        desc.flags = RF_Remembered;
                        if (f.open) desc.flags |= RF_Open;
                        wctx.put(sx, sy, desc);
                    }
                } else {
                    ctx.put(sx, sy, '?', Color::Red);  // invalid fixture
                }
                continue;
            }

            // Non-fixture dungeon/station tiles — animation override via WorldContext
            if (rc.animations) {
                if (auto anim = rc.animations->query(mx, my)) {
                    wctx.put_animation(sx, sy, anim->type, anim->frame_index);
                    continue;
                }
            }

            {
                RenderDescriptor desc;
                desc.category = RenderCategory::Tile;
                desc.type_id  = static_cast<uint16_t>(tile_at);
                desc.biome    = biome;

                if (tile_at == Tile::StructuralWall) {
                    uint8_t mat = rc.world.map().glyph_override(mx, my);
                    desc.seed = encode_wall_seed(mat, mx, my);
                } else {
                    uint8_t s = position_seed(mx, my) & 0x7F;  // bit 7 reserved for ruin tint
                    if (tile_at == Tile::Wall &&
                        rc.world.map().has_custom_flag(mx, my, 0x02)) {
                        s |= 0x80;  // CF_RUIN_TINT — top bit signals ruin tinting
                        // Encode civ index in bits 4-6
                        uint8_t flags = rc.world.map().get_custom_flags(mx, my);
                        int civ = (flags & 0x1C) >> 2;  // CF_CIV_MASK >> CF_CIV_SHIFT
                        s = (s & 0x80) | (static_cast<uint8_t>(civ & 0x07) << 4);
                        // Encode wall neighbor mask in bits 0-3 (N=1, S=2, E=4, W=8)
                        auto is_ruin_wall = [&](int x, int y) {
                            if (x < 0 || x >= rc.world.map().width() ||
                                y < 0 || y >= rc.world.map().height()) return false;
                            Tile t = rc.world.map().get(x, y);
                            return (t == Tile::Wall || t == Tile::StructuralWall) &&
                                   rc.world.map().has_custom_flag(x, y, 0x02);
                        };
                        bool n = is_ruin_wall(mx, my - 1);
                        bool so = is_ruin_wall(mx, my + 1);
                        bool e = is_ruin_wall(mx + 1, my);
                        bool w = is_ruin_wall(mx - 1, my);

                        // Detect wall primary axis for pipe rendering.
                        // A thick wall's interior tiles see neighbors on all sides,
                        // but cross-axis neighbors are just wall thickness, not junctions.
                        //
                        // Check for open (non-wall) space to determine which direction
                        // this wall faces. "Open" on E or W = vertical wall. Open on
                        // N or S = horizontal wall. Open on none = interior.
                        bool open_n = !n;
                        bool open_s = !so;
                        bool open_e = !e;
                        bool open_w = !w;

                        uint8_t neighbors = 0;
                        if (n && so && e && w) {
                            // Interior: all 4 walls → solid fill
                            neighbors = 0x0F;
                        } else if ((open_e || open_w) && !(open_n || open_s)) {
                            // Vertical wall edge: has N+S but open on E or W side
                            // Only report N/S connections, suppress E/W
                            if (n) neighbors |= 0x01;
                            if (so) neighbors |= 0x02;
                        } else if ((open_n || open_s) && !(open_e || open_w)) {
                            // Horizontal wall edge: has E+W but open on N or S side
                            // Only report E/W connections, suppress N/S
                            if (e) neighbors |= 0x04;
                            if (w) neighbors |= 0x08;
                        } else {
                            // Corner or true junction — report all actual neighbors
                            if (n) neighbors |= 0x01;
                            if (so) neighbors |= 0x02;
                            if (e) neighbors |= 0x04;
                            if (w) neighbors |= 0x08;
                        }
                        s = (s & 0xF0) | neighbors;
                    }
                    desc.seed = s;
                }

                if (v == Visibility::Visible) {
                    desc.flags = RF_Lit;
                } else {
                    desc.flags = RF_Remembered;
                }

                wctx.put(sx, sy, desc);
            }
        }
    }

    // Draw visible ground items
    for (const auto& gi : rc.world.ground_items()) {
        if (rc.world.visibility().get(gi.x, gi.y) == Visibility::Visible) {
            RenderDescriptor desc;
            desc.category = RenderCategory::Item;
            desc.type_id = gi.item.item_def_id;
            desc.rarity = gi.item.rarity;
            wctx.put(gi.x - rc.camera_x, gi.y - rc.camera_y, desc);
        }
    }

    // Draw visible NPCs (only effect animations override NPC glyph)
    for (const auto& npc : rc.world.npcs()) {
        if (npc.alive() && rc.world.visibility().get(npc.x, npc.y) == Visibility::Visible) {
            if (rc.animations) {
                if (auto anim = rc.animations->query_effect(npc.x, npc.y)) {
                    wctx.put_animation(npc.x - rc.camera_x, npc.y - rc.camera_y, anim->type, anim->frame_index);
                    continue;
                }
            }
            RenderDescriptor desc;
            desc.category = RenderCategory::Npc;
            desc.type_id = static_cast<uint16_t>(npc.npc_role);
            desc.seed = static_cast<uint8_t>(npc.race);
            wctx.put(npc.x - rc.camera_x, npc.y - rc.camera_y, desc);
        }
    }

    // Draw player (only effect animations override player glyph)
    {
        bool anim_override = false;
        if (rc.animations) {
            if (auto anim = rc.animations->query_effect(rc.player.x, rc.player.y)) {
                wctx.put_animation(rc.player.x - rc.camera_x, rc.player.y - rc.camera_y, anim->type, anim->frame_index);
                anim_override = true;
            }
        }
        if (!anim_override) {
            RenderDescriptor desc;
            desc.category = RenderCategory::Player;
            wctx.put(rc.player.x - rc.camera_x, rc.player.y - rc.camera_y, desc);
        }
    }

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

            // [X] cursor — brackets always visible, X blinks
            if (lsx > 0)
                ctx.put(lsx - 1, lsy, '[', Color::White);
            if (rc.input.look_blink() % 2 == 0)
                ctx.put(lsx, lsy, 'X', Color::Yellow);
            if (lsx + 1 < rc.map_rect.w)
                ctx.put(lsx + 1, lsy, ']', Color::White);
        }
    }
}

} // namespace astra
