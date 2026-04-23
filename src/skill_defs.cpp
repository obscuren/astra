#include "astra/skill_defs.h"
#include "astra/player.h"
#include "astra/renderer.h"

namespace astra {

bool player_has_skill(const Player& player, SkillId id) {
    for (auto sid : player.learned_skills)
        if (sid == id) return true;
    return false;
}

static std::string acrobatics_category_description() {
    std::string s = "Mastery of agile movement and evasion in any environment.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("1 DV", Color::Cyan);
    s += " while this category is learned.";
    return s;
}

static std::string tinkering_category_description() {
    std::string s = "The art of repairing, modifying, and disassembling technology and equipment.\n\n";
    s += colored("Passive:", Color::White);
    s += " Grants the ";
    s += colored("Salvage", Color::Yellow);
    s += " ability: ";
    s += colored("40%", Color::Cyan);
    s += " chance on mechanical kills to recover 1-2 ";
    s += colored("Spare Parts", Color::Yellow);
    s += " and ";
    s += colored("30%", Color::Cyan);
    s += " chance of ";
    s += colored("Circuitry", Color::Cyan);
    s += ".";
    return s;
}

const std::vector<SkillCategory>& skill_catalog() {
    static const std::vector<SkillCategory> catalog = {
        {SkillId::Cat_Acrobatics, "Acrobatics",
         acrobatics_category_description(), 50, {
            {SkillId::Swiftness, "Swiftness",
             "You get +5 DV when attacked with ranged weapons.",
             true, 50, 0, nullptr},
            {SkillId::Sidestep, "Sidestep",
             "You get +2 DV while at least one hostile is adjacent. "
             "Stacks with Swiftness against adjacent ranged foes.",
             true, 75, 13, "Agility"},
            {SkillId::SureFooted, "Sure-Footed",
             "Your movement is lithe and efficient. Dungeon move "
             "actions cost 10% less time.",
             true, 75, 15, "Agility"},
            {SkillId::Tumble, "Tumble",
             "Dash up to 3 tiles in any direction, ignoring anything "
             "in between. Telegraphed. 25-tick cooldown.",
             false, 100, 17, "Agility"},
            {SkillId::AdrenalineRush, "Adrenaline Rush",
             "Self-cast surge: +2 DV and +25% quickness for 3 ticks. "
             "40-tick cooldown.",
             false, 150, 14, "Willpower"},
        }},
        {SkillId::Cat_ShortBlade, "Short Blade",
         "Proficiency with knives, daggers, and other short-edged weapons. Fast and precise.", 50, {
            {SkillId::ShortBladeExpertise, "Short Blade Expertise",
             "You get +1 hit with short blades. The action cost of attacking "
             "with a short blade in your primary hand is reduced by 25%.",
             true, 50, 0, nullptr},
            {SkillId::Jab, "Jab",
             "Immediately jab with your off-hand short blade for a quick strike.",
             false, 100, 15, "Agility"},
        }},
        {SkillId::Cat_LongBlade, "Long Blade",
         "Expertise with swords and long-edged weapons. Powerful strikes and parrying.", 75, {
            {SkillId::LongBladeExpertise, "Long Blade Expertise",
             "You get +1 hit with long blades. Increased parry chance when "
             "wielding a long blade.",
             true, 50, 0, nullptr},
            {SkillId::Cleave, "Cleave",
             "Strike all adjacent enemies in an arc with your long blade.",
             false, 150, 19, "Strength"},
        }},
        {SkillId::Cat_Pistol, "Pistol",
         "Training with sidearms and handguns. Quick draws and accurate fire at close range.", 50, {
            {SkillId::SteadyHand, "Steady Hand",
             "Your accuracy with pistols is improved by +1. Reduced sway "
             "when firing at range.",
             true, 50, 0, nullptr},
            {SkillId::Quickdraw, "Quickdraw",
             "Draw and fire your pistol in a single action at reduced cost.",
             false, 100, 15, "Agility"},
        }},
        {SkillId::Cat_Rifle, "Rifle",
         "Mastery of long-range firearms. Increased accuracy and effective range.", 75, {
            {SkillId::Marksman, "Marksman",
             "Your effective range with rifles is increased by +2. Better "
             "accuracy at long range.",
             true, 50, 0, nullptr},
            {SkillId::SuppressingFire, "Suppressing Fire",
             "Lay down a barrage that pins enemies in a cone, reducing their "
             "movement and accuracy.",
             false, 150, 17, "Willpower"},
        }},
        {SkillId::Cat_Tinkering, "Tinkering",
         tinkering_category_description(), 100, {
            {SkillId::BasicRepair, "Basic Repair",
             "You can repair damaged equipment using scrap materials. "
             "Restores durability over time.",
             true, 100, 15, "Intelligence"},
            {SkillId::Disassemble, "Disassemble",
             "Break down items into component parts. Higher intelligence "
             "yields better salvage.",
             false, 100, 17, "Intelligence"},
            {SkillId::Synthesize, "Synthesize",
             "Combine learned blueprints and crafting materials to create "
             "entirely new items. Requires advanced molecular understanding.",
             false, 150, 19, "Intelligence"},
        }},
        {SkillId::Cat_Endurance, "Endurance",
         "Physical resilience and mental fortitude. Shrug off damage and resist effects.", 50, {
            {SkillId::ThickSkin, "Thick Skin",
             "Your natural armor value is increased by +1. You shrug off "
             "minor wounds more easily.",
             true, 50, 15, "Toughness"},
            {SkillId::IronWill, "Iron Will",
             "Your mental resistance is strengthened. +5 to all resistance "
             "checks against psionic effects.",
             true, 100, 17, "Willpower"},
        }},
        {SkillId::Cat_Persuasion, "Persuasion",
         "Social manipulation and negotiation. Better prices and the power to intimidate.", 100, {
            {SkillId::Haggle, "Haggle",
             "Merchants offer better prices. Buy values reduced by 10%, "
             "sell values increased by 10%.",
             true, 50, 0, nullptr},
            {SkillId::Intimidate, "Intimidate",
             "Frighten a hostile creature, causing it to flee for several turns. "
             "Effectiveness scales with level.",
             false, 100, 15, "Willpower"},
        }},
        {SkillId::Cat_Wayfinding, "Wayfinding",
         "Knowledge of navigation, terrain, and survival in the wild. "
         "Reduces the chance of getting lost and improves overland travel.", 75, {
            {SkillId::CampMaking, "Camp Making",
             "Set up a makeshift camp with a fire for resting and cooking.",
             false, 50, 12, "Intelligence"},
            {SkillId::CompassSense, "Compass Sense",
             "Your natural sense of direction is sharpened. Grace period before "
             "regaining bearings is halved and recovery rate doubled.",
             true, 50, 13, "Intelligence"},
            {SkillId::LorePlains, "Terrain Lore: Plains",
             "Familiarity with open terrain. 50% less likely to get lost on "
             "plains and desert. Travel time halved.",
             true, 50, 12, "Intelligence"},
            {SkillId::LoreForest, "Terrain Lore: Forest",
             "You read forest trails instinctively. 50% less likely to get lost "
             "in forests and fungal growth. Travel time halved.",
             true, 50, 13, "Intelligence"},
            {SkillId::LoreWetlands, "Terrain Lore: Wetlands",
             "Navigating bogs and waterways comes naturally. 50% less likely to "
             "get lost in swamps, rivers, and lakeside. Travel time halved.",
             true, 75, 14, "Intelligence"},
            {SkillId::LoreMountains, "Terrain Lore: Mountains",
             "Highland pathfinding expertise. 50% less likely to get lost in "
             "mountains and craters. Travel time halved.",
             true, 75, 15, "Intelligence"},
            {SkillId::LoreTundra, "Terrain Lore: Tundra",
             "Survival knowledge for extreme environments. 50% less likely to "
             "get lost on ice fields and volcanic terrain. Travel time halved.",
             true, 75, 15, "Intelligence"},
            {SkillId::ScoutsEye, "Scout's Eye",
             "Your keen awareness reveals nearby creatures on the minimap. "
             "Hostile and friendly NPCs appear as colored markers.",
             true, 75, 13, "Intelligence"},
            {SkillId::Cartographer, "Cartographer",
             "Your trained eye spots points of interest and valuables. "
             "Items and landmarks appear on the minimap.",
             true, 100, 14, "Intelligence"},
        }},
        {SkillId::Cat_Archaeology, "Archaeology",
         "The study of ancient civilizations and their remains. "
         "Identify ruins, decipher inscriptions, and uncover lost knowledge.", 75, {
            {SkillId::RuinReader, "Ruin Reader",
             "Lore fragments found in ruins reveal their full text instead of "
             "partial translations.",
             true, 50, 12, "Intelligence"},
            {SkillId::ArtifactIdentification, "Artifact Identification",
             "Ancient items are automatically identified on pickup.",
             true, 75, 13, "Intelligence"},
            {SkillId::Excavation, "Excavation",
             "Search ruin tiles for hidden caches. Chance to discover lore "
             "fragments and sealed chambers.",
             false, 50, 12, "Intelligence"},
            {SkillId::CulturalAttunement, "Cultural Attunement",
             "Bonus when using artifacts from civilizations you have studied.",
             true, 75, 14, "Intelligence"},
            {SkillId::PrecursorLinguist, "Precursor Linguist",
             "Read ancient inscriptions. Unlocks sealed doors and reveals "
             "vault locations.",
             true, 100, 15, "Intelligence"},
            {SkillId::BeaconSense, "Beacon Sense",
             "Sgr A* beacon nodes glow on the star chart before visiting. "
             "Feel the ancient signal toward the center.",
             true, 100, 16, "Intelligence"},
        }},
        {SkillId::Cat_Cooking, "Cooking",
         "The art of turning raw ingredients into nourishing meals. Unlocks the "
         "kitchen tab for preparing recipes at a cooking fire.", 50, {
            {SkillId::AdvancedFireMaking, "Advanced Fire Making",
             "Requires Camp Making. Reduces the Camp Making cooldown by 40%.",
             true, 75, 0, nullptr},
        }},
    };
    return catalog;
}

const SkillDef* find_skill(SkillId id) {
    for (const auto& cat : skill_catalog()) {
        for (const auto& sk : cat.skills) {
            if (sk.id == id) return &sk;
        }
    }
    return nullptr;
}

SkillId terrain_lore_for(Tile terrain) {
    switch (terrain) {
        case Tile::OW_Plains: case Tile::OW_Desert:
            return SkillId::LorePlains;
        case Tile::OW_Forest: case Tile::OW_Fungal:
            return SkillId::LoreForest;
        case Tile::OW_Swamp: case Tile::OW_River: case Tile::OW_Lake:
            return SkillId::LoreWetlands;
        case Tile::OW_Mountains: case Tile::OW_Crater:
            return SkillId::LoreMountains;
        case Tile::OW_IceField: case Tile::OW_LavaFlow:
            return SkillId::LoreTundra;
        default:
            return static_cast<SkillId>(0); // no matching lore
    }
}

} // namespace astra
