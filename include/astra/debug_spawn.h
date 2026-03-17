#pragma once

#include "astra/npc.h"
#include "astra/tilemap.h"

#include <random>
#include <utility>
#include <vector>

namespace astra {

// Spawn debug/test NPCs for development. Call after the core station NPCs
// have been placed. Add new experimental spawns here.
void debug_spawn(TileMap& map,
                 std::vector<Npc>& npcs,
                 int player_x, int player_y,
                 std::vector<std::pair<int,int>>& occupied,
                 std::mt19937& rng);

} // namespace astra
