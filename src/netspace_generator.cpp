#include "astra/netspace_generator.h"

#include "astra/grammars/gen_door_netspace.h"
#include "astra/grammars/gen_vending_netspace.h"
#include "astra/grammars/gen_camera_netspace.h"
#include "astra/grammars/gen_atm_netspace.h"
#include "astra/grammars/gen_turret_netspace.h"
#include "astra/grammars/gen_elevator_netspace.h"
#include "astra/grammars/gen_corpse_netspace.h"
#include "astra/grammars/gen_combat_netspace.h"

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
        n.set(x, 0,         NetTile::WallSolid);
        n.set(x, n.h - 1,   NetTile::WallSolid);
    }
    for (int y = 0; y < n.h; ++y) {
        n.set(0,         y, NetTile::WallSolid);
        n.set(n.w - 1,   y, NetTile::WallSolid);
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

Netspace gen_for_target(const TargetDescriptor& desc) {
    switch (desc.kind) {
        case NetspaceTargetKind::Door:
            return gen_door_netspace(desc);
        case NetspaceTargetKind::VendingMachine:
            return gen_vending_netspace(desc);
        case NetspaceTargetKind::Camera:
            return gen_camera_netspace(desc);
        // Phase 4+ lights up the remaining kinds.
        case NetspaceTargetKind::Atm:
            return gen_atm_netspace(desc);
        case NetspaceTargetKind::Turret:
            return gen_turret_netspace(desc);
        case NetspaceTargetKind::Elevator:
            return gen_elevator_netspace(desc);
        case NetspaceTargetKind::Corpse:
            return gen_corpse_netspace(desc);
        case NetspaceTargetKind::CombatArena:
            return gen_combat_netspace(desc);
        case NetspaceTargetKind::TrafficLight:
        case NetspaceTargetKind::NpcHead:
        case NetspaceTargetKind::Mainframe:
        case NetspaceTargetKind::BlackwallTear:
        case NetspaceTargetKind::Empty:
        default:
            return gen_empty_netspace(desc);
    }
}

}  // namespace astra
