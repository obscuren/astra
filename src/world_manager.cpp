#include "astra/world_manager.h"
#include "astra/lan.h"

namespace astra {

bool WorldManager::world_flag(const std::string& name) const {
    auto it = world_flags_.find(name);
    return it != world_flags_.end() && it->second;
}

void WorldManager::set_world_flag(const std::string& name, bool value) {
    world_flags_[name] = value;
}

const DungeonRecipe* WorldManager::find_dungeon_recipe(const LocationKey& root) const {
    auto it = dungeon_recipes_.find(root);
    return (it == dungeon_recipes_.end()) ? nullptr : &it->second;
}

void WorldManager::on_hackable_removed(GridNodeId subnet_id) {
    if (!subnet_id.valid()) return;

    auto& net = grid_network_;
    auto& edges = net.edges_mut();

    // Remove edges touching this subnet
    edges.erase(std::remove_if(edges.begin(), edges.end(),
        [&](const GridEdge& e) { return e.from == subnet_id || e.to == subnet_id; }),
        edges.end());

    if (auto* n = net.find_mut(subnet_id)) {
        // Tombstone the node so any latent reference (e.g. a Hackable
        // still holding jack_in_node_id from before death) returns a
        // visible "removed" marker rather than crashing.
        n->label = "[removed]";
        n->security_tier = 0;
    }

    // Drop any persistence keyed by this subnet (LAN sector state stays;
    // only the per-subnet sub-sector overlay is purged).
    auto it = lan_metadata_.subnet_states.find(subnet_id.value);
    if (it != lan_metadata_.subnet_states.end()) {
        lan_metadata_.subnet_states.erase(it);
    }

    if (lan_metadata_.nodes_total > 0) lan_metadata_.nodes_total -= 1;
}

void WorldManager::lan_full_reset() {
    auto& net = grid_network_;
    LanMetadata& meta = lan_metadata_;

    if (meta.lan_root.valid()) {
        // Drop every Subnet node reachable from lan_root (one hop only —
        // Subnets don't chain in this model).
        std::vector<GridNodeId> to_drop;
        for (const auto& e : net.edges()) {
            if (e.from == meta.lan_root) {
                if (auto* n = net.find(e.to)) {
                    if (n->kind == GridNodeKind::Subnet) to_drop.push_back(e.to);
                }
            }
        }
        for (GridNodeId id : to_drop) on_hackable_removed(id);

        // Drop the lan_root's own edges.
        auto& edges = net.edges_mut();
        edges.erase(std::remove_if(edges.begin(), edges.end(),
            [&](const GridEdge& e) {
                return e.from == meta.lan_root || e.to == meta.lan_root;
            }),
            edges.end());

        // Tombstone lan_root.
        if (auto* n = net.find_mut(meta.lan_root)) {
            n->label = "[removed]";
            n->security_tier = 0;
        }
    }

    // Wipe persistence + reset meta to defaults.
    meta = LanMetadata{};
    register_hackables_in_lan(*this, net, meta);
}

} // namespace astra
