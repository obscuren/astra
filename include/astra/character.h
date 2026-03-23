#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class PlayerClass : uint8_t {
    Smuggler,   // AGI/LUC focus, stealth and trade bonuses
    Engineer,   // INT/TOU focus, tinkering and repair
    Marine,     // STR/TOU focus, combat and armor
    Navigator,  // INT/WIL focus, star chart and piloting
    Scavenger,  // AGI/INT focus, loot and exploration
};

const char* class_name(PlayerClass c);

struct PrimaryAttributes {
    int strength = 10;     // STR — melee damage, carry weight
    int agility = 10;      // AGI — dodge, move speed, ranged accuracy
    int toughness = 10;    // TOU — max HP, resist physical
    int intelligence = 10; // INT — tinkering, hacking, XP gain
    int willpower = 10;    // WIL — mental resist, energy regen
    int luck = 10;         // LUC — crit chance, loot quality
};

struct Resistances {
    int acid = 0;       // AR
    int electrical = 0; // ER
    int cold = 0;       // CR
    int heat = 0;       // HR
};

struct FactionStanding {
    std::string faction_name;
    int reputation = 0;
};

} // namespace astra
