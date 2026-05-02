#include "astra/grid_persistence.h"

#include "astra/game.h"
#include "astra/grid_session.h"
#include "astra/hacking_system.h"
#include "astra/lan.h"
#include "astra/world_manager.h"

#include <cstdint>

namespace astra {

SectorRuntimeState* active_runtime_state(Game& game) {
    auto* sess = game.hacking().session();
    if (!sess) return nullptr;

    auto& meta = game.world().lan_metadata();
    if (!meta.lan_root.valid()) return nullptr;

    if (sess->current_node == meta.lan_root) {
        return &meta.lan_sector_state;
    }
    // Anything else routed through this LAN's subnets goes into
    // subnet_states keyed by node id. Deep-Grid / regional darknet
    // don't have a LAN-side bucket — bail out for those.
    auto& net = game.world().grid_network();
    const auto* node = net.find(sess->current_node);
    if (!node) return nullptr;
    if (node->kind != GridNodeKind::Subnet) return nullptr;

    return &meta.subnet_states[sess->current_node.value];
}

void record_sector_mutation(Game& game, int x, int y, GridTile new_tile) {
    if (auto* state = active_runtime_state(game)) {
        SectorMutation m;
        m.x        = static_cast<uint8_t>(x);
        m.y        = static_cast<uint8_t>(y);
        m.new_tile = new_tile;
        state->mutations.push_back(m);
    }
}

void record_killed_ice(Game& game, int x, int y) {
    if (auto* state = active_runtime_state(game)) {
        state->killed_ice.emplace_back(static_cast<uint8_t>(x),
                                       static_cast<uint8_t>(y));
    }
}

} // namespace astra
