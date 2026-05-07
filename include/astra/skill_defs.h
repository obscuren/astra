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
    Cat_Hacking         = 12,    // gate for jacking into the Grid
    Intrusion           = 1200,  // White ICE LoS ticks Trace +1 not +2
    IceBreaking         = 1201,  // icebreaker_lite +1 dmg
    DaemonMastery       = 1202,  // +1 cyberdeck program slot
    GhostProtocol       = 1203,  // first program each Grid run is heatless
    DeepGridNavigator   = 1204,  // 50% passive gateway crack
    NeuralFortitude     = 1205,  // halve Black ICE adjacent dmg + half bleed-through
    CodeCraft           = 1206,  // unlock T3 program tinker recipes
    ConsciousnessAnchor = 1207,  // (capstone) Your.Anchor + lore-archive DataNode
    // Plan 7 — device-shell skill nodes.
    ColdHands           = 1208,  // -10% Detection on privileged shell commands
    RootKit             = 1209,  // -10% hashcat duration
    // Plan 8 — ImplantReader
    ImplantReader       = 1210,  // reveals NPC implant status in look widget
    // Plan 8 — Tether perks: project Mark on no-Crystal targets
    TetherL1            = 1211,  // Cat_Hacking — Tether L1: project Mark (LoS, short range 1 tile)
    TetherL2            = 1212,  // Cat_Hacking — Tether L2: long range (8 tiles)
    TetherL3            = 1213,  // Cat_Hacking — Tether L3: AoE 3 tiles
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

} // namespace astra
