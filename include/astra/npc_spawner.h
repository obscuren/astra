#pragma once

#include "astra/npc.h"
#include "astra/station_type.h"
#include "astra/tilemap.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

struct Player;

// Spawn NPCs in hub station rooms based on room flavor.
// Player is used for faction reputation (affects shop stock tiers).
void spawn_hub_npcs(TileMap& map, std::vector<Npc>& npcs,
                    int player_x, int player_y, std::mt19937& rng,
                    const Player* player = nullptr);

// Spawn NPCs in settlement detail maps (near fixtures).
void spawn_settlement_npcs(TileMap& map, std::vector<Npc>& npcs,
                           int player_x, int player_y, std::mt19937& rng,
                           const Player* player = nullptr);

// Spawn NPCs in outpost detail maps (near fixtures).
void spawn_outpost_npcs(TileMap& map, std::vector<Npc>& npcs,
                        int player_x, int player_y, std::mt19937& rng,
                        const Player* player = nullptr);

// Spawn NPCs in scav station rooms based on room flavor.
void spawn_scav_npcs(TileMap& map, std::vector<Npc>& npcs,
                     int player_x, int player_y, std::mt19937& rng,
                     const Player* player = nullptr);

// Spawn NPCs in pirate station rooms based on room flavor.
void spawn_pirate_npcs(TileMap& map, std::vector<Npc>& npcs,
                       int player_x, int player_y, std::mt19937& rng,
                       const StationContext& ctx,
                       const Player* player = nullptr);

// Find a random walkable position within bounds, avoiding occupied tiles.
// Returns true and sets out_x/out_y on success.
bool find_walkable_in_bounds(const TileMap& map, const Rect& bounds,
                              int& out_x, int& out_y,
                              const std::vector<std::pair<int,int>>& occupied,
                              std::mt19937& rng);

// Spawn NPCs in settlements with civ-style-aware roles and scaling.
void spawn_settlement_npcs_v2(TileMap& map, std::vector<Npc>& npcs,
                               int player_x, int player_y,
                               std::mt19937& rng, const Player* player,
                               int size_category,
                               const std::string& style_name,
                               Biome biome);

} // namespace astra
