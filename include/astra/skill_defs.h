#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "astra/tilemap.h"

namespace astra {

enum class SkillId : uint32_t {
    // Category unlocks (1-99)
    Cat_Acrobatics = 1,
    Cat_ShortBlade = 2,
    Cat_LongBlade = 3,
    Cat_Pistol = 4,
    Cat_Rifle = 5,
    Cat_Tinkering = 6,
    Cat_Endurance = 7,
    Cat_Persuasion = 8,

    // Acrobatics
    Swiftness = 100,
    Tumble = 101,
    // Short Blade
    ShortBladeExpertise = 200,
    Jab = 201,
    // Long Blade
    LongBladeExpertise = 300,
    Cleave = 301,
    // Pistol
    SteadyHand = 400,
    Quickdraw = 401,
    // Rifle
    Marksman = 500,
    SuppressingFire = 501,
    // Tinkering
    BasicRepair = 600,
    Disassemble = 601,
    Synthesize = 602,
    // Endurance
    ThickSkin = 700,
    IronWill = 701,
    // Persuasion
    Haggle = 800,
    Intimidate = 801,
    // Wayfinding
    Cat_Wayfinding = 9,
    CampMaking = 900,
    CompassSense = 901,
    LorePlains = 902,
    LoreForest = 903,
    LoreWetlands = 904,
    LoreMountains = 905,
    LoreTundra = 906,
};

struct SkillDef {
    SkillId id;
    std::string name;
    std::string description;
    bool passive = true;
    int sp_cost = 50;
    int attribute_req = 0;
    const char* attribute_name = nullptr;
};

struct SkillCategory {
    SkillId unlock_id;            // purchasing this unlocks the category
    std::string name;
    std::string description;
    int sp_cost = 50;             // cost to unlock the category
    std::vector<SkillDef> skills;
};

// Returns the full skill catalog (static, built once).
const std::vector<SkillCategory>& skill_catalog();

// Look up a skill definition by ID. Returns nullptr if not found.
const SkillDef* find_skill(SkillId id);

// Check if the player has the terrain lore skill matching the given overworld tile.
// Requires player.learned_skills to be checked via player_has_skill().
SkillId terrain_lore_for(Tile terrain);

} // namespace astra
