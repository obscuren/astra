#pragma once

#include "astra/renderer.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace astra {

class Game;

enum class TelegraphShape : uint8_t {
    Line,      // straight line from origin in one of 8 dirs, up to `range`
    Ray,       // like Line but stops at first wall/enemy
    Cone,      // widening arc from origin in a direction
    Burst,     // radius around a target tile
    Adjacent,  // 8 neighbors of origin (fixed)
};

struct TelegraphSpec {
    TelegraphShape shape = TelegraphShape::Line;
    int range = 3;
    int width = 1;
    bool diagonals = true;
    bool stop_at_wall = true;
    bool stop_at_enemy = false;
    bool require_walkable_dest = false;
};

struct TelegraphTile {
    int x;
    int y;
    bool blocked = false;
};

struct TelegraphResult {
    std::vector<TelegraphTile> path;
    int dest_x = -1;
    int dest_y = -1;
};

class Telegraph {
public:
    using ConfirmFn = std::function<void(const TelegraphResult&)>;

    void begin(const TelegraphSpec& spec, int origin_x, int origin_y,
               ConfirmFn on_confirm);
    void cancel();

    bool active() const { return active_; }
    const TelegraphResult& preview() const { return preview_; }

    bool handle_input(int key, Game& game);

    // Renders the preview overlay. `camera_x/y` is the world-space camera,
    // `screen_ox/oy` is the absolute screen position of the map viewport's
    // top-left corner, and `screen_w/h` is the viewport size in cells.
    void render(Renderer* renderer, int camera_x, int camera_y,
                int screen_w, int screen_h,
                int screen_ox, int screen_oy) const;

private:
    void recompute(const Game& game);

    bool active_ = false;
    TelegraphSpec spec_;
    int origin_x_ = 0;
    int origin_y_ = 0;
    int dir_dx_ = 0;
    int dir_dy_ = 0;
    int length_ = 1;
    // TODO(burst): used when Burst/Cone shapes are implemented
    int cursor_x_ = 0;
    int cursor_y_ = 0;
    TelegraphResult preview_;
    ConfirmFn on_confirm_;
};

} // namespace astra
