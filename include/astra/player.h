#pragma once

namespace astra {

struct Player {
    int x = 0;
    int y = 0;
    int hp = 10;
    int max_hp = 10;
    int depth = 1;
    int view_radius = 8;
};

} // namespace astra
