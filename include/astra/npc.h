#pragma once

#include "astra/effect.h"
#include "astra/interaction.h"
#include "astra/race.h"

#include <cstdint>
#include <random>
#include <string>

namespace astra {

enum class Disposition : uint8_t {
    Friendly,
    Neutral,
    Hostile,
};

// NPC templates define the archetype; the factory fills in name/race.
enum class NpcRole : uint8_t {
    StationKeeper,
    Merchant,
    Drifter,
    Xytomorph,
    FoodMerchant,
    Medic,
    Commander,
    ArmsDealer,
    Astronomer,
    Engineer,
    Nova,
    Civilian,
    Scavenger,
    Prospector,
};

struct Npc {
    int x = 0;
    int y = 0;
    std::string name;           // personal name, e.g. "Krath"
    std::string role;           // title, e.g. "Station Keeper"
    Race race = Race::Human;
    int hp = 1;
    int max_hp = 1;
    Disposition disposition = Disposition::Neutral;
    std::string faction;        // faction name (empty = unaligned)
    EffectList effects;
    int quickness = 100;
    int energy = 0;
    int level = 1;
    bool elite = false;
    int base_xp = 0;
    int base_damage = 0;
    NpcRole npc_role = NpcRole::Civilian;
    InteractionData interactions;

    // When displaced by player swap, NPC tries to return here next tick
    int return_x = -1;
    int return_y = -1;

    bool alive() const { return hp > 0; }
    int xp_reward() const { return base_xp * level * (elite ? 3 : 1); }
    int attack_damage() const { return base_damage * level + (elite ? 1 : 0); }
    void scale_to_level(int lvl, bool is_elite);
    std::string display_name() const;
};

// Create a fully configured NPC. Race is used for name generation.
// For hostile types the race is implicit (e.g. Xytomorph).
Npc create_npc(NpcRole role, Race race, std::mt19937& rng);

// Generate a personal name appropriate for a given race.
std::string generate_name(Race race, std::mt19937& rng);

// Create an NPC from a role name string (e.g. "Xytomorph", "Station Keeper").
Npc create_npc_by_role(const std::string& role_name, std::mt19937& rng);

} // namespace astra
