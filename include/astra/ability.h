#pragma once

#include "astra/effect.h"
#include "astra/item.h"
#include "astra/skill_defs.h"

#include <memory>
#include <string>
#include <vector>

namespace astra {

class Game;
struct Npc;
struct Player;

class Ability {
public:
    virtual ~Ability() = default;

    // Execute the ability. Target may be nullptr for self/AoE abilities.
    virtual bool execute(Game& game, Npc* target) = 0;

    // Cooldown ticks to apply after a successful use. Defaults to
    // the static cooldown_ticks field; overrides can scale based on
    // player state (e.g., reduced by a skill).
    virtual int effective_cooldown(const Player& player) const;

    SkillId skill_id;
    std::string name;
    std::string description;
    int cooldown_ticks = 3;
    EffectId cooldown_effect = EffectId::CooldownJab;
    bool needs_adjacent_target = false;
    WeaponClass required_weapon = WeaponClass::None;
    int action_cost = 50;
};

// Returns all ability instances (shared, don't modify)
const std::vector<std::unique_ptr<Ability>>& ability_catalog();

// Find ability by skill ID (nullptr if not found)
Ability* find_ability(SkillId id);

// Get the player's equipped ability slots (up to 5)
std::vector<SkillId> get_ability_bar(const Player& player);

// Try to use ability in slot 0-4. Returns true if used.
bool use_ability(int slot, Game& game);

} // namespace astra
