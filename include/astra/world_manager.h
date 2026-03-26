#pragma once

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

private:
    TileMap map_;
    VisibilityMap visibility_;
    std::vector<Npc> npcs_;
};

} // namespace astra
