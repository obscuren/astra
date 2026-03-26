#pragma once

#include "astra/tilemap.h"

namespace astra {

class WorldManager {
public:
    WorldManager() = default;

    // ── Map ─────────────────────────────────────────────────────
    TileMap& map() { return map_; }
    const TileMap& map() const { return map_; }

private:
    TileMap map_;
};

} // namespace astra
