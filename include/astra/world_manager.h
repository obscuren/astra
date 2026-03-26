#pragma once

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

private:
    TileMap map_;
    VisibilityMap visibility_;
};

} // namespace astra
