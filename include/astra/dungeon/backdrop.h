#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
}

namespace astra::dungeon {

// Layer 1: fills every cell with an impassable, opaque tile and sets
// biome to Dungeon so any stray Empty renders as underground block char.
void apply_backdrop(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, std::mt19937& rng);

} // namespace astra::dungeon
