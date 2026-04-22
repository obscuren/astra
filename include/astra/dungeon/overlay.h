#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <random>

namespace astra {
class TileMap;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Layer 4: applies spec.overlays, filtered against style.allowed_overlays.
// Non-allowed overlays are silently skipped.
void apply_overlays(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, std::mt19937& rng);

} // namespace astra::dungeon
