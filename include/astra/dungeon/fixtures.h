#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Layer 6:
//   6.i   Stairs (strategy-dispatched).
//   6.iii Required fixtures (style-driven, runs before quest fixtures).
//   6.ii  Quest fixtures from spec.fixtures.
void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, const DungeonLevelSpec& spec,
                    LevelContext& ctx, std::mt19937& rng);

} // namespace astra::dungeon
