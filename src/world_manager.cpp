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
    lan_metadatas_.try_emplace(key);
    // Per-target netspaces replace the multi-region node graph that
    // switch_active_lan used to populate; LAN metadata stays as a region
    // name carrier only.
}

void WorldManager::lan_full_reset() {
    LanMetadata& meta = lan_metadatas_[current_lan_key_];
    meta = LanMetadata{};
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

Hackable* WorldManager::find_hackable_by_ip(uint32_t ip) {
    auto& m = map();
    for (int i = 0; i < m.fixture_count(); ++i) {
        FixtureData& fd = m.fixture_mut(i);
        if (!fd.cyber) continue;
        if (fd.cyber->ip != ip) continue;
        return &*fd.cyber;
    }
    for (auto& npc : npcs()) {
        if (!npc.alive() || !npc.cyber) continue;
        if (npc.cyber->ip != ip) continue;
        return &*npc.cyber;
    }
    return nullptr;
}

Npc& WorldManager::add_npc(Npc&& npc) {
    if (npc.uid <= 0) {
        npc.uid = allocate_npc_uid();
    } else if (npc.uid >= next_npc_uid_) {
        // UID was loaded from save — keep it but advance the counter past it
        next_npc_uid_ = npc.uid + 1;
    }
    npcs_.push_back(std::move(npc));
    return npcs_.back();
}

Npc* WorldManager::npc_by_uid(int32_t uid) {
    if (uid <= 0) return nullptr;
    for (auto& n : npcs_) {
        if (n.uid == uid) return &n;
    }
    return nullptr;
}

const Npc* WorldManager::npc_by_uid(int32_t uid) const {
    if (uid <= 0) return nullptr;
    for (const auto& n : npcs_) {
        if (n.uid == uid) return &n;
    }
    return nullptr;
}

} // namespace astra
