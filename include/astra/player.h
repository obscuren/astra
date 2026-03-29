#pragma once

#include "astra/character.h"
#include "astra/effect.h"
#include "astra/item.h"
#include "astra/journal.h"
#include "astra/race.h"
#include "astra/ship.h"
#include "astra/skill_defs.h"
#include "astra/tinkering.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class HungerState : uint8_t {
    Satiated,
    Normal,
    Hungry,
    Starving,
};

inline const char* hunger_name(HungerState h) {
    switch (h) {
        case HungerState::Satiated: return "Satiated";
        case HungerState::Normal:   return "";
        case HungerState::Hungry:   return "Hungry";
        case HungerState::Starving: return "Starving";
    }
    return "";
}

struct Player {
    // Identity
    std::string name = "Commander";
    Race race = Race::Human;
    PlayerClass player_class = PlayerClass::DevCommander;

    // Position
    int x = 0;
    int y = 0;
    int depth = 1;
    int view_radius = 8;
    int light_radius = 6;

    // Vitals
    int hp = 10;
    int max_hp = 10;
    int temperature = 20;
    HungerState hunger = HungerState::Satiated;
    int money = 0;

    // Primary attributes
    PrimaryAttributes attributes;
    int attribute_points = 0;

    // Base secondary stats
    int quickness = 100;
    int move_speed = 100;
    int attack_value = 1;
    int defense_value = 5;
    int dodge_value = 3;

    // Resistances
    Resistances resistances;

    // Progression
    int level = 1;
    int xp = 0;
    int max_xp = 100;
    int energy = 0;
    int kills = 0;
    int regen_counter = 0;

    // Effects
    EffectList effects;

    // Equipment & inventory
    Equipment equipment;
    Inventory inventory;

    // Skills
    int skill_points = 0;
    std::vector<SkillId> learned_skills;

    // Reputation
    std::vector<FactionStanding> reputation;

    // Starship
    Starship ship;

    // Tinkering
    std::vector<BlueprintSignature> learned_blueprints;

    // Journal
    std::vector<JournalEntry> journal;

    // Derived stats — attribute modifier + equipment + active effects
    int effective_attack() const {
        auto eq = equipment.total_modifiers();
        auto ef = effect_modifiers(effects);
        return attack_value + (attributes.strength - 10) / 2 + eq.attack + ef.attack;
    }
    int effective_defense() const {
        auto eq = equipment.total_modifiers();
        auto ef = effect_modifiers(effects);
        return defense_value + (attributes.toughness - 10) / 3 + eq.defense + ef.defense;
    }
    int effective_dodge() const {
        return dodge_value + (attributes.agility - 10) / 3 + effect_dodge_mod(effects);
    }
    int effective_max_hp() const {
        auto eq = equipment.total_modifiers();
        auto ef = effect_modifiers(effects);
        return max_hp + (attributes.toughness - 10) * 2 + eq.max_hp + ef.max_hp;
    }
};

inline int regen_interval(HungerState h) {
    switch (h) {
        case HungerState::Satiated:  return 15;
        case HungerState::Normal:    return 20;
        case HungerState::Hungry:    return 40;
        case HungerState::Starving:  return 0;
    }
    return 0;
}

// Look up player's reputation with a faction (0 if not found)
int reputation_for(const Player& player, const std::string& faction);

} // namespace astra
