#pragma once

#include "astra/aura.h"
#include "astra/character.h"
#include "astra/dice.h"
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

    // Shield
    int shield_hp = 0;
    int shield_max_hp = 0;
    TypeAffinity shield_affinity;

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

    // Auras this entity currently emits. Rebuilt from equipment,
    // effects, and skills by rebuild_auras_from_sources; Manual
    // entries persist across rebuilds.
    std::vector<Aura> auras;

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

    // Tutorial — tracks which datapad tabs have shown their help overlay
    uint16_t tab_help_seen = 0;  // bitfield, one bit per CharTab

    // Derived stats — attribute modifier + equipment + active effects
    int effective_dv() const {
        auto eq = equipment.total_modifiers();
        auto ef = effect_modifiers(effects);
        return dodge_value + (attributes.agility - 10) / 2 + eq.dv + ef.dv;
    }

    int effective_av(DamageType type) const {
        int total_av = 0;
        auto add_slot = [&](const std::optional<Item>& slot) {
            if (!slot) return;
            if (slot->type != ItemType::Armor) return;
            total_av += slot->modifiers.av + slot->type_affinity.for_type(type);
        };
        add_slot(equipment.head);
        add_slot(equipment.body);
        add_slot(equipment.left_arm);
        add_slot(equipment.right_arm);
        add_slot(equipment.feet);
        auto ef = effect_modifiers(effects);
        total_av += ef.av;
        return total_av;
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
