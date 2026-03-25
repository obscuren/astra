#pragma once

#include "astra/skill_defs.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class PlayerClass : uint8_t {
    DevCommander, // developer mode only — all-rounder for testing
    Voidwalker,   // melee tank — heavy armor, blade mastery
    Gunslinger,   // ranged agility — quick-draw pistols
    Technomancer, // tinkering/intel — engineer and hacker
    Operative,    // stealth/social — short blades and persuasion
    Marauder,     // survivalist — high toughness, luck-driven crits
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

// Class template — defines starting stats for each PlayerClass
struct ClassTemplate {
    PlayerClass player_class;
    const char* description;
    PrimaryAttributes attributes;
    Resistances resistances;
    int bonus_hp = 0;            // added to base max_hp
    int bonus_carry_weight = 0;  // added to base max_carry_weight
    std::vector<SkillId> starting_skills;  // pre-learned skills (including category unlocks)
    int starting_sp = 0;         // bonus starting skill points
    int starting_money = 0;      // starting credits
};

const ClassTemplate& class_template(PlayerClass c);

// All gameplay classes (excludes DevCommander)
const std::vector<PlayerClass>& gameplay_classes();

} // namespace astra
