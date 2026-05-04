#pragma once

#include "astra/skill_defs.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

// Forward-declared so ClassTemplate can carry starting equipment without
// dragging the full item.h dependency stack into character.h.
enum class EquipSlot : uint8_t;

enum class PlayerClass : uint8_t {
    DevCommander, // developer mode only — all-rounder for testing
    Voidwalker,   // melee tank — heavy armor, blade mastery
    Gunslinger,   // ranged agility — quick-draw pistols
    Technomancer, // tinkering/intel — engineer and hacker
    Operative,    // stealth/social — short blades and persuasion
    Marauder,     // survivalist — high toughness, luck-driven crits
    Gridrunner,   // netrunner — frail body, lethal in cyberspace
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
    int kinetic = 0;    // KR
    int acid = 0;       // AR
    int electrical = 0; // ER
    int cold = 0;       // CR
    int heat = 0;       // HR
};

struct FactionStanding {
    std::string faction_name;
    int reputation = 0;
};

enum class ReputationTier : int8_t {
    Hated    = -2,   // rep <= -300
    Disliked = -1,   // rep -299 to -60
    Neutral  =  0,   // rep -59 to 59
    Liked    =  1,   // rep 60 to 299
    Trusted  =  2,   // rep >= 300
};

ReputationTier reputation_tier(int reputation);
const char* reputation_tier_name(ReputationTier tier);
int reputation_price_pct(int reputation);  // buy modifier: +30/+15/0/-10/-20

// One pre-rolled starting item. If `equip_to` is set, the item is built and
// placed into that equipment slot directly; otherwise it is pushed into the
// player's inventory. `count` >1 stacks the item (caller must ensure the
// item def is stackable — non-stackable items at count >1 will be silently
// clamped to 1).
struct StartingItem {
    int                       def_id;
    int                       count = 1;
    std::optional<EquipSlot>  equip_to;  // unset → goes into inventory
};

// Class template — defines starting stats and gear for each PlayerClass
struct ClassTemplate {
    PlayerClass player_class;
    const char* description;
    char        card_glyph = '@';   // shown on the class-pick card in character creation
    PrimaryAttributes attributes;
    Resistances resistances;
    int bonus_hp = 0;            // added to base max_hp
    int bonus_carry_weight = 0;  // added to base max_carry_weight
    std::vector<SkillId> starting_skills;  // pre-learned skills (including category unlocks)
    int starting_sp = 0;         // bonus starting skill points
    int starting_money = 0;      // starting credits
    std::vector<StartingItem> starting_items; // gear granted on character spawn
};

const ClassTemplate& class_template(PlayerClass c);

// All gameplay classes (excludes DevCommander)
const std::vector<PlayerClass>& gameplay_classes();

} // namespace astra
