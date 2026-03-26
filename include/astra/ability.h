#pragma once

#include "astra/item.h"
#include "astra/skill_defs.h"
#include "astra/effect.h"

#include <vector>

namespace astra {

class Game; // forward declare

struct AbilityDef {
    SkillId skill_id;
    const char* name;
    const char* description;
    int cooldown_ticks;
    EffectId cooldown_effect;
    bool needs_adjacent_target;
    WeaponClass required_weapon;
    int action_cost;           // ticks consumed (50 = normal move)
};

// Returns all defined abilities
const std::vector<AbilityDef>& ability_catalog();

// Find ability by skill ID (nullptr if not an active ability)
const AbilityDef* find_ability(SkillId id);

// Get the player's equipped abilities (up to 5 slots)
// Returns skill IDs of learned active abilities
std::vector<SkillId> get_ability_bar(const struct Player& player);

// Try to use an ability. Returns true if used.
bool use_ability(int slot, Game& game);

} // namespace astra
