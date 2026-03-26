#pragma once

#include "astra/item.h"
#include "astra/npc.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

namespace astra {

class WorldManager {
public:
    WorldManager() = default;

    TileMap& map() { return map_; }
    const TileMap& map() const { return map_; }

    VisibilityMap& visibility() { return visibility_; }
    const VisibilityMap& visibility() const { return visibility_; }

    std::vector<Npc>& npcs() { return npcs_; }
    const std::vector<Npc>& npcs() const { return npcs_; }

    std::vector<GroundItem>& ground_items() { return ground_items_; }
    const std::vector<GroundItem>& ground_items() const { return ground_items_; }

    std::vector<Item>& stash() { return stash_; }
    const std::vector<Item>& stash() const { return stash_; }
    static constexpr int max_stash_size = 20;

private:
    TileMap map_;
    VisibilityMap visibility_;
    std::vector<Npc> npcs_;
    std::vector<GroundItem> ground_items_;
    std::vector<Item> stash_;
};

} // namespace astra
