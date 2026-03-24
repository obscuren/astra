#include "astra/skill_defs.h"

namespace astra {

const std::vector<SkillCategory>& skill_catalog() {
    static const std::vector<SkillCategory> catalog = {
        {SkillId::Cat_Acrobatics, "Acrobatics",
         "Mastery of agile movement and evasion. Improves dodge and mobility in combat.", 50, {
            {SkillId::Swiftness, "Swiftness",
             "You get +5 bonus to DV when attacked with missile weapons.",
             true, 50, 0, nullptr},
            {SkillId::Tumble, "Tumble",
             "Dodge away when hit, reducing damage and repositioning.",
             false, 100, 17, "Agility"},
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
         "The art of repairing, modifying, and disassembling technology and equipment.", 100, {
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

} // namespace astra
