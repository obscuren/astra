#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
}

namespace astra::dungeon {

// Layer 2: dispatches on style.layout. Post-condition:
// map.region_count() >= 1, entries in LevelContext are populated.
void apply_layout(TileMap& map, const DungeonStyle& style,
                  const CivConfig& civ, LevelContext& ctx,
                  std::mt19937& rng);

} // namespace astra::dungeon
