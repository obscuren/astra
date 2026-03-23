#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class SkillId : uint32_t {
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
    // Endurance
    ThickSkin = 700,
    IronWill = 701,
    // Persuasion
    Haggle = 800,
    Intimidate = 801,
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
    std::string name;
    std::vector<SkillDef> skills;
};

// Returns the full skill catalog (static, built once).
const std::vector<SkillCategory>& skill_catalog();

// Look up a skill definition by ID. Returns nullptr if not found.
const SkillDef* find_skill(SkillId id);

} // namespace astra
