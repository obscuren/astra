#include "astra/telegraph.h"

#include "astra/game.h"
#include "astra/renderer.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <cstdlib>

namespace astra {

static bool tile_blocks_line(const Game& game, int x, int y, const TelegraphSpec& spec) {
    // TODO(phase 3): respect FoV — currently a telegraph line can reveal
    // tiles the player hasn't seen. Phase 3 (Tumble) will need this.
    const auto& map = game.world().map();
    if (x < 0 || y < 0 || x >= map.width() || y >= map.height()) return true;
    if (spec.stop_at_wall && !map.passable(x, y)) return true;
    if (spec.stop_at_enemy) {
        for (const auto& npc : game.world().npcs()) {
            if (npc.alive() && npc.x == x && npc.y == y) return true;
        }
    }
    return false;
}

static void compute_line(const Game& game, int ox, int oy, int dx, int dy,
                         int length, const TelegraphSpec& spec,
                         TelegraphResult& out) {
    // Caller invariant: dx, dy ∈ {-1, 0, 1}. The function strides in
    // Chebyshev steps so larger magnitudes would skip tiles.
    out.path.clear();
    out.dest_x = ox;
    out.dest_y = oy;
    for (int i = 1; i <= length; ++i) {
        int tx = ox + dx * i;
        int ty = oy + dy * i;
        bool blocked = tile_blocks_line(game, tx, ty, spec);
        out.path.push_back({tx, ty, blocked});
        if (blocked) break;
        out.dest_x = tx;
        out.dest_y = ty;
    }
}

void Telegraph::begin(const TelegraphSpec& spec, int origin_x, int origin_y,
                      ConfirmFn on_confirm) {
    spec_ = spec;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    on_confirm_ = std::move(on_confirm);
    dir_dx_ = 0;
    dir_dy_ = 0;
    length_ = 1;
    cursor_x_ = origin_x;
    cursor_y_ = origin_y;
    preview_ = {};
    preview_.dest_x = origin_x;
    preview_.dest_y = origin_y;
    active_ = true;
}

void Telegraph::cancel() {
    active_ = false;
    on_confirm_ = nullptr;
    preview_ = {};
}

void Telegraph::recompute(const Game& game) {
    preview_.path.clear();
    preview_.dest_x = origin_x_;
    preview_.dest_y = origin_y_;

    if (spec_.shape == TelegraphShape::Line ||
        spec_.shape == TelegraphShape::Ray) {
        if (dir_dx_ == 0 && dir_dy_ == 0) return;
        compute_line(game, origin_x_, origin_y_, dir_dx_, dir_dy_, length_, spec_, preview_);
    } else if (spec_.shape == TelegraphShape::Burst) {
        // Free-cursor placement reticule: a (2*width+1)-side square centred
        // on the cursor. The cursor itself is constrained to Chebyshev range
        // from the origin via handle_input clamping. The dest is the cursor
        // tile; surrounding tiles are previewed so the player sees the burst
        // footprint.
        preview_.dest_x = cursor_x_;
        preview_.dest_y = cursor_y_;
        preview_.blocked_los = false;
        const auto& map = game.world().map();

        // Wall block check: walk Bresenham origin → dest; any impassable
        // *intermediate* tile blocks the throw (you can't toss past a wall).
        // Endpoints don't count: origin is the player, dest is checked by
        // require_walkable_dest separately.
        if (cursor_x_ != origin_x_ || cursor_y_ != origin_y_) {
            int x = origin_x_, y = origin_y_;
            int x1 = cursor_x_, y1 = cursor_y_;
            int adx = std::abs(x1 - x), sx_step = x < x1 ? 1 : -1;
            int ady = -std::abs(y1 - y), sy_step = y < y1 ? 1 : -1;
            int err = adx + ady;
            for (;;) {
                int e2 = 2 * err;
                if (e2 >= ady) { err += ady; x += sx_step; }
                if (e2 <= adx) { err += adx; y += sy_step; }
                if (x == x1 && y == y1) break;
                if (x < 0 || y < 0 || x >= map.width() || y >= map.height()) {
                    preview_.blocked_los = true; break;
                }
                if (!map.passable(x, y)) { preview_.blocked_los = true; break; }
            }
        }

        int w = std::max(0, spec_.width);
        for (int dy = -w; dy <= w; ++dy) {
            for (int dx = -w; dx <= w; ++dx) {
                int tx = cursor_x_ + dx;
                int ty = cursor_y_ + dy;
                bool blocked = preview_.blocked_los;
                if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) {
                    blocked = true;
                }
                preview_.path.push_back({tx, ty, blocked});
            }
        }
    }
}

bool Telegraph::handle_input(int key, Game& game) {
    if (!active_) return false;

    auto set_dir = [&](int ndx, int ndy) {
        bool same_axis = (ndx == dir_dx_ && ndy == dir_dy_) ||
                         (ndx == -dir_dx_ && ndy == -dir_dy_);
        if (!same_axis || (dir_dx_ == 0 && dir_dy_ == 0)) {
            dir_dx_ = ndx;
            dir_dy_ = ndy;
            length_ = 1;
            return;
        }
        if (ndx == dir_dx_ && ndy == dir_dy_) {
            if (length_ < spec_.range) ++length_;
        } else {
            if (length_ > 1) --length_;
        }
    };

    auto move_cursor = [&](int ndx, int ndy) {
        int nx = cursor_x_ + ndx;
        int ny = cursor_y_ + ndy;
        // Weighted Chebyshev clamp — horizontal cells count half because
        // terminal glyphs are ~2:1 (taller than wide). Without this, L/R
        // visually reaches half as far as U/D for the same range value.
        int adx = std::abs(nx - origin_x_);
        int ady = std::abs(ny - origin_y_);
        int reach = std::max((adx + 1) / 2, ady);
        if (reach > spec_.range) return;
        cursor_x_ = nx;
        cursor_y_ = ny;
    };

    auto handle_dir = [&](int ndx, int ndy) {
        if (spec_.shape == TelegraphShape::Burst) {
            move_cursor(ndx, ndy);
        } else {
            set_dir(ndx, ndy);
        }
    };

    switch (key) {
        case 'k': case KEY_UP:    handle_dir( 0, -1); break;
        case 'j': case KEY_DOWN:  handle_dir( 0,  1); break;
        case 'h': case KEY_LEFT:  handle_dir(-1,  0); break;
        case 'l': case KEY_RIGHT: handle_dir( 1,  0); break;
        case 'y': handle_dir(-1, -1); break;
        case 'u': handle_dir( 1, -1); break;
        case 'b': handle_dir(-1,  1); break;
        case 'n': handle_dir( 1,  1); break;
        case '\n': case '\r': {
            recompute(game);
            bool ok = false;
            if (spec_.shape == TelegraphShape::Burst) {
                // For placement-style telegraphs, validity = the centre tile
                // is in-map, (optionally) walkable, and there is no wall
                // blocking the throw line from origin to dest.
                const auto& map = game.world().map();
                int dx = preview_.dest_x;
                int dy = preview_.dest_y;
                ok = (dx >= 0 && dy >= 0 &&
                      dx < map.width() && dy < map.height());
                if (ok && spec_.require_walkable_dest) {
                    ok = map.passable(dx, dy);
                }
                if (ok && preview_.blocked_los) {
                    game.log("Can't throw through walls.");
                    return true;
                }
            } else {
                ok = !preview_.path.empty() && !preview_.path.back().blocked;
                if (ok && spec_.require_walkable_dest) {
                    ok = game.world().map().passable(preview_.dest_x, preview_.dest_y);
                }
            }
            if (!ok) {
                game.log("Invalid destination.");
                return true;
            }
            auto cb = std::move(on_confirm_);
            active_ = false;
            if (cb) cb(preview_);
            return true;
        }
        case '\033':
            cancel();
            game.log("Cancelled.");
            return true;
        default:
            return false;
    }

    recompute(game);
    return true;
}

void Telegraph::render(Renderer* r, int camera_x, int camera_y,
                       int screen_w, int screen_h,
                       int screen_ox, int screen_oy) const {
    if (!active_ || !r) return;
    const bool is_burst = (spec_.shape == TelegraphShape::Burst);

    // Tether: stylized line from origin to the edge of the footprint.
    // Body glyphs reflect arrival direction; the tile adjacent to the
    // footprint gets a touchdown marker.
    if (is_burst &&
        (preview_.dest_x != origin_x_ || preview_.dest_y != origin_y_)) {
        const int w = std::max(0, spec_.width);
        auto in_footprint = [&](int x, int y) {
            return std::abs(x - preview_.dest_x) <= w &&
                   std::abs(y - preview_.dest_y) <= w;
        };

        struct Step { int x; int y; int prev_dx; int prev_dy; };
        std::vector<Step> tether;
        int x = origin_x_, y = origin_y_;
        const int x1 = preview_.dest_x, y1 = preview_.dest_y;
        const int dx = std::abs(x1 - x), sx_step = x < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y), sy_step = y < y1 ? 1 : -1;
        int err = dx + dy;
        while (!(x == x1 && y == y1)) {
            int e2 = 2 * err;
            int step_dx = 0, step_dy = 0;
            if (e2 >= dy) { err += dy; x += sx_step; step_dx = sx_step; }
            if (e2 <= dx) { err += dx; y += sy_step; step_dy = sy_step; }
            if (in_footprint(x, y)) break;
            tether.push_back({x, y, step_dx, step_dy});
        }

        for (size_t i = 0; i < tether.size(); ++i) {
            const auto& s = tether[i];
            const char* glyph;
            if (i + 1 == tether.size()) {
                glyph = "'";          // touchdown — adjacent to footprint
            } else if (s.prev_dx != 0 && s.prev_dy != 0) {
                glyph = (s.prev_dx == s.prev_dy) ? "\\" : "/";
            } else if (s.prev_dx != 0) {
                glyph = "-";
            } else {
                glyph = "|";
            }
            int sx = s.x - camera_x;
            int sy = s.y - camera_y;
            if (sx >= 0 && sy >= 0 && sx < screen_w && sy < screen_h) {
                r->draw_glyph(screen_ox + sx, screen_oy + sy, glyph, Color::DarkGray);
            }
        }
    }

    for (const auto& t : preview_.path) {
        int sx = t.x - camera_x;
        int sy = t.y - camera_y;
        if (sx < 0 || sy < 0 || sx >= screen_w || sy >= screen_h) continue;
        Color col = t.blocked ? Color::Red : Color::Cyan;
        if (is_burst) {
            // Whole footprint (including dest) renders as ░ — no X marker.
            r->draw_glyph(screen_ox + sx, screen_oy + sy, "\xe2\x96\x91", col);
        } else {
            // Line / Ray shapes: keep + along path, X at dest.
            bool is_dest = (!t.blocked &&
                            t.x == preview_.dest_x && t.y == preview_.dest_y);
            char glyph = is_dest ? 'X' : '+';
            r->draw_char(screen_ox + sx, screen_oy + sy, glyph, col);
        }
    }
}

} // namespace astra
