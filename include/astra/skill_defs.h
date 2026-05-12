#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "astra/tilemap.h"

namespace astra {

struct Player; // forward declare for player_has_skill

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
    Sidestep = 102,
    SureFooted = 103,
    AdrenalineRush = 104,
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
    ScoutsEye = 907,
    Cartographer = 908,
    // Archaeology
    Cat_Archaeology = 10,
    RuinReader = 1000,
    ArtifactIdentification = 1001,
    Excavation = 1002,
    CulturalAttunement = 1003,
    PrecursorLinguist = 1004,
    BeaconSense = 1005,

    // Cooking
    Cat_Cooking         = 11,
    AdvancedFireMaking  = 1100,

    // Hacking
    Cat_Hacking         = 12,    // parent category for hacking sub-skills
    Intrusion           = 1200,  // White ICE LoS ticks Trace +1 not +2
    IceBreaking         = 1201,  // icebreaker_lite +1 dmg
    DaemonMastery       = 1202,  // +1 cyberdeck program slot
    GhostProtocol       = 1203,  // first program each Grid run is heatless
    DeepGridNavigator   = 1204,  // 50% passive gateway crack
    NeuralFortitude     = 1205,  // halve Black ICE adjacent dmg + half bleed-through
    CodeCraft           = 1206,  // unlock T3 program tinker recipes
    ConsciousnessAnchor = 1207,  // (capstone) Your.Anchor + lore-archive DataNode
    Programming1        = 1214,  // 3-fragment ceiling, starter pick
    Programming2        = 1215,  // 4-fragment ceiling, +2 random fragments
    Programming3        = 1216,  // 5-fragment ceiling, +2 random fragments

    // Synthetic IDs used by ability_bar to represent a binding to a
    // cyberdeck program slot. NOT real skills — never appear in
    // player.learned_skills, never have an Ability catalog entry.
    // Render and use_slot dispatch handles them specially.
    CyberdeckSlot1      = 2001,
    CyberdeckSlot2      = 2002,
    CyberdeckSlot3      = 2003,
    CyberdeckSlot4      = 2004,
    CyberdeckSlot5      = 2005,
    CyberdeckSlot6      = 2006,
};

inline bool is_cyberdeck_slot_skill(SkillId id) {
    auto v = static_cast<uint32_t>(id);
    return v >= 2001 && v <= 2006;
}

inline int cyberdeck_slot_index_from_skill(SkillId id) {
    return static_cast<int>(id) - static_cast<int>(SkillId::CyberdeckSlot1);
}

inline SkillId cyberdeck_slot_skill_id(int slot_index) {
    return static_cast<SkillId>(static_cast<int>(SkillId::CyberdeckSlot1) + slot_index);
}

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

struct SkillDetail {
    std::string header;
    std::string body;
    std::string cost_line;
    std::string requirement_line;
};

SkillDetail skill_detail(SkillId id);

// Returns the full skill catalog (static, built once).
const std::vector<SkillCategory>& skill_catalog();

// Look up a skill definition by ID. Returns nullptr if not found.
const SkillDef* find_skill(SkillId id);

// Check if the player has learned a specific skill.
bool player_has_skill(const Player& player, SkillId id);

// Check if the player has the terrain lore skill matching the given overworld tile.
SkillId terrain_lore_for(Tile terrain);

// Returns the max program-fragment chain length for this player.
// 0 if no Programming skill; 3/4/5 by tier.
int max_program_fragments(const Player& player);

} // namespace astra
