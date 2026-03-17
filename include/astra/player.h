#pragma once

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
};

} // namespace astra
