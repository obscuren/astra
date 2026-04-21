#pragma once

#include "astra/dungeon_recipe.h"

#include <cstdint>
#include <utility>

namespace astra {

class TileMap;

// Generate one dungeon level into `map` according to recipe.levels[depth - 1].
// Places one StairsUp at `entered_from` (or a seeded default if {-1,-1}),
// one DungeonHatch "stairs-down" (unless the level is_boss_level), and any
// planned fixtures declared in the level spec.
//
// NPC spawning is NOT done here — the caller populates world_.npcs().
void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed,
                            std::pair<int, int> entered_from);

// Locate the (x,y) of the (single) StairsUp fixture; returns {-1,-1} if none.
std::pair<int, int> find_stairs_up(const TileMap& map);

// Locate the (x,y) of the (single) DungeonHatch fixture — the "stairs-down"
// in the dungeon level context. Returns {-1,-1} if none.
std::pair<int, int> find_stairs_down(const TileMap& map);

// Deterministic per-level seed from world seed + LocationKey (depth mixed in).
uint32_t dungeon_level_seed(uint32_t world_seed, const LocationKey& level_key);

} // namespace astra
