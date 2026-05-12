#include "astra/netspace_generator.h"

namespace astra {

namespace {

constexpr int kStubWidth  = 14;
constexpr int kStubHeight = 8;
constexpr int kJackInX    = 1;
constexpr int kJackInY    = 4;
constexpr int kExitX      = 12;
constexpr int kExitY      = 4;

}  // namespace

Netspace gen_empty_netspace(const TargetDescriptor& desc) {
    Netspace n;
    n.target = desc;
    n.w = kStubWidth;
    n.h = kStubHeight;
    n.tiles.assign(static_cast<size_t>(kStubWidth) * static_cast<size_t>(kStubHeight),
                   NetTile::Floor);
    n.title = "EMPTY :: jack out via exit tile";

    for (int x = 0; x < n.w; ++x) {
        n.set(x, 0,         NetTile::Wall);
        n.set(x, n.h - 1,   NetTile::Wall);
    }
    for (int y = 0; y < n.h; ++y) {
        n.set(0,         y, NetTile::Wall);
        n.set(n.w - 1,   y, NetTile::Wall);
    }

    n.jack_in_x = kJackInX;
    n.jack_in_y = kJackInY;
    n.exit_x    = kExitX;
    n.exit_y    = kExitY;
    n.set(kJackInX, kJackInY, NetTile::JackIn);
    n.set(kExitX,   kExitY,   NetTile::Exit);

    n.window_state = WindowState::Stable;
    return n;
}

}  // namespace astra
