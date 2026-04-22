#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

namespace astra {
class TileMap;
struct CivConfig;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Main orchestrator. Walks all six layers.
//
// RNG discipline: each layer receives its own sub-seeded mt19937
// derived from ctx.seed via named XOR mixing, so adding/removing
// an overlay won't reshuffle decoration placement.
//
//   backdrop      seed ^ 0xBDBDBDBDu
//   layout        seed ^ 0x1A1A1A1Au
//   connectivity  seed ^ 0xC0FFEE00u
//   overlays      seed ^ 0x0FEB0FEBu
//   decoration    seed ^ 0xDEC02011u
//   fixtures      seed ^ 0xF12F12F1u
void run(TileMap& map, const DungeonStyle& style, const CivConfig& civ,
         const DungeonLevelSpec& spec, LevelContext& ctx);

} // namespace astra::dungeon
