#pragma once

#include "astra/npc.h"
#include "astra/station_type.h"

#include <random>

namespace astra {

// Each NPC archetype has its own builder.
// These are called by create_npc() — add new ones as the world grows.
Npc build_station_keeper(Race race, std::mt19937& rng, const StationContext& ctx = {});
Npc build_merchant(Race race, std::mt19937& rng, int faction_rep = 0);
Npc build_drifter(Race race, std::mt19937& rng);
Npc build_xytomorph(std::mt19937& rng);

// Unique named NPC — no race/rng params needed.
Npc build_nova();

// Hub station NPC builders
Npc build_food_merchant(Race race, std::mt19937& rng, int faction_rep = 0);
Npc build_medic(Race race, std::mt19937& rng);
Npc build_commander(Race race, std::mt19937& rng, const StationContext& ctx = {});
Npc build_arms_dealer(Race race, std::mt19937& rng, int faction_rep = 0);
Npc build_astronomer(Race race, std::mt19937& rng);
Npc build_engineer(Race race, std::mt19937& rng);

// Random civilian NPC — race-based glyph, generic dialog
Npc build_civilian(Race race, std::mt19937& rng);
Npc build_random_civilian(std::mt19937& rng); // picks random friendly race

// Hub station variants — station-specific dialog
Npc build_hub_civilian(Race race, std::mt19937& rng);
Npc build_random_hub_civilian(std::mt19937& rng);
Npc build_hub_drifter(Race race, std::mt19937& rng);

// Settlement NPC builders
Npc build_scavenger(Race race, std::mt19937& rng);
Npc build_prospector(Race race, std::mt19937& rng);
Npc build_archon_remnant(std::mt19937& rng);
Npc build_void_reaver(std::mt19937& rng);

// Scav station NPC builders
Npc build_scav_keeper(Race race, std::mt19937& rng, const StationContext& ctx = {});
Npc build_scav_junk_dealer(Race race, std::mt19937& rng);

// Pirate station NPC builders
Npc build_pirate_captain(const StationContext& ctx);
Npc build_pirate_grunt();
Npc build_black_market_vendor(const StationContext& ctx);

} // namespace astra
