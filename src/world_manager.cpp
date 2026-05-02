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

LocationKey WorldManager::current_location_key() const {
    if (map_.location_name() == "Maintenance Tunnels") {
        return maintenance_key;
    }
    if (navigation_.on_ship) {
        return ship_key;
    }
    if (navigation_.at_station) {
        return LocationKey{navigation_.current_system_id, -1, -1, true, -1, -1, 0};
    }
    if (on_overworld()) {
        return LocationKey{navigation_.current_system_id,
                           navigation_.current_body_index,
                           navigation_.current_moon_index,
                           false, -1, -1, 0};
    }
    if (on_detail_map()) {
        return LocationKey{navigation_.current_system_id,
                           navigation_.current_body_index,
                           navigation_.current_moon_index,
                           false, overworld_x_, overworld_y_, 0};
    }
    // Dungeon — anchor to the detail tile the player entered from at the
    // current dungeon depth.
    return LocationKey{navigation_.current_system_id,
                       navigation_.current_body_index,
                       navigation_.current_moon_index,
                       false, overworld_x_, overworld_y_,
                       navigation_.current_depth};
}

LanMetadata& WorldManager::lan_metadata() {
    return lan_metadatas_[current_lan_key_];
}

const LanMetadata& WorldManager::lan_metadata() const {
    auto it = lan_metadatas_.find(current_lan_key_);
    if (it == lan_metadatas_.end()) return empty_lan_;
    return it->second;
}

void WorldManager::switch_active_lan(const LocationKey& key) {
    current_lan_key_ = key;
    auto [it, inserted] = lan_metadatas_.try_emplace(key);
    if (inserted) {
        // First visit to this map — register the active map's hackables.
        register_hackables_in_lan(*this, grid_network_, it->second);
    }
    // Otherwise: prior LAN is intact. Its Subnet nodes still live in
    // `grid_network_` and the cached map's Hackables still hold their
    // `jack_in_node_id` pointers from the prior visit, so re-jacking
    // resolves to the same persisted sectors.
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

    // Drop any persistence keyed by this subnet from the active LAN.
    // NPC death + fixture removal only happens on the live (active) map,
    // so the owning LAN is always the active one. Cached maps' fixtures
    // and NPCs are inert and don't trigger this code path.
    auto& active = lan_metadatas_[current_lan_key_];
    auto it = active.subnet_states.find(subnet_id.value);
    if (it != active.subnet_states.end()) {
        active.subnet_states.erase(it);
    }
    if (active.nodes_total > 0) active.nodes_total -= 1;
}

void WorldManager::lan_full_reset() {
    auto& net = grid_network_;
    LanMetadata& meta = lan_metadatas_[current_lan_key_];

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

    // Wipe persistence + reset meta to defaults — but ONLY the active LAN.
    // Sibling maps' LANs in `lan_metadatas_` are left alone.
    meta = LanMetadata{};
    register_hackables_in_lan(*this, net, meta);
}

const Hackable* WorldManager::find_hackable_by_ip(uint32_t ip) const {
    const auto& m = map();
    for (int i = 0; i < m.fixture_count(); ++i) {
        const FixtureData& fd = m.fixture(i);
        if (!fd.cyber) continue;
        if (fd.cyber->ip != ip) continue;
        return &*fd.cyber;
    }
    for (const auto& npc : npcs()) {
        if (!npc.alive() || !npc.cyber) continue;
        if (npc.cyber->ip != ip) continue;
        return &*npc.cyber;
    }
    return nullptr;
}

} // namespace astra
