#pragma once

#include "astra/item.h"

#include <cstdint>

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
    int x = 0;
    int y = 0;
    int hp = 10;
    int max_hp = 10;
    int depth = 1;
    int view_radius = 8;
    int light_radius = 6;

    // Stats
    int temperature = 20;
    HungerState hunger = HungerState::Satiated;
    int money = 0;
    int quickness = 100;
    int move_speed = 100;
    int attack_value = 1;
    int defense_value = 5;
    int level = 1;
    int xp = 0;
    int max_xp = 100;
    int energy = 0;
    int kills = 0;
    int regen_counter = 0;
    bool invulnerable = false;

    // Equipment & inventory
    Equipment equipment;
    Inventory inventory;

    int effective_attack() const {
        return attack_value + equipment.total_modifiers().attack;
    }
    int effective_defense() const {
        return defense_value + equipment.total_modifiers().defense;
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

} // namespace astra
