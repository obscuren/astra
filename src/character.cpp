#include "astra/character.h"

namespace astra {

const char* class_name(PlayerClass c) {
    switch (c) {
        case PlayerClass::DevCommander: return "Dev Commander";
        case PlayerClass::Voidwalker:   return "Voidwalker";
        case PlayerClass::Gunslinger:   return "Gunslinger";
        case PlayerClass::Technomancer: return "Technomancer";
        case PlayerClass::Operative:    return "Operative";
        case PlayerClass::Marauder:     return "Marauder";
    }
    return "Unknown";
}

static const ClassTemplate s_dev_commander = {
    PlayerClass::DevCommander,
    "Developer testing class. High stats across the board with "
    "multiple skills unlocked and generous starting resources.",
    {14, 12, 16, 12, 12, 10},  // strong all-rounder, TOU/STR focus
    {5, 5, 5, 5},              // all resistances
    5, 20,                      // +5 HP, +20 carry
    {SkillId::Cat_ShortBlade, SkillId::ShortBladeExpertise,
     SkillId::Cat_Pistol, SkillId::SteadyHand,
     SkillId::Cat_Endurance, SkillId::ThickSkin,
     SkillId::Cat_Tinkering, SkillId::BasicRepair, SkillId::Disassemble, SkillId::Synthesize},
    200, 50,                    // 200 SP, 50 credits
};

static const ClassTemplate s_voidwalker = {
    PlayerClass::Voidwalker,
    "Melee-focused space marine. Heavy armor and blade mastery "
    "make the Voidwalker a frontline juggernaut.",
    {14, 10, 14, 8, 10, 10},
    {0, 0, 2, 3},                // cold +2, heat +3
    4, 15,                        // +4 HP, +15 carry
    {SkillId::Cat_LongBlade, SkillId::LongBladeExpertise,
     SkillId::Cat_Endurance, SkillId::ThickSkin},
    50, 20,
};

static const ClassTemplate s_gunslinger = {
    PlayerClass::Gunslinger,
    "Ranged specialist with lightning reflexes. Quick-draw pistols "
    "and acrobatic evasion keep enemies at a distance.",
    {8, 16, 10, 10, 10, 12},
    {0, 0, 0, 0},
    0, 5,                         // +0 HP, +5 carry
    {SkillId::Cat_Pistol, SkillId::SteadyHand, SkillId::Quickdraw,
     SkillId::Cat_Acrobatics},
    50, 25,
};

static const ClassTemplate s_technomancer = {
    PlayerClass::Technomancer,
    "Engineer and hacker who bends technology to their will. "
    "Weak in direct combat but unmatched at the workbench.",
    {8, 10, 8, 16, 14, 10},
    {0, 5, 0, 0},                // electrical +5
    0, 10,                        // +0 HP, +10 carry
    {SkillId::Cat_Tinkering, SkillId::BasicRepair, SkillId::Disassemble,
     SkillId::Cat_Rifle},
    100, 30,
};

static const ClassTemplate s_operative = {
    PlayerClass::Operative,
    "Stealth agent and smooth talker. Short blades in the dark, "
    "silver tongue in the light.",
    {10, 14, 10, 12, 10, 10},
    {0, 0, 0, 0},
    2, 5,                         // +2 HP, +5 carry
    {SkillId::Cat_ShortBlade, SkillId::ShortBladeExpertise, SkillId::Jab,
     SkillId::Cat_Persuasion, SkillId::Haggle},
    75, 40,
};

static const ClassTemplate s_marauder = {
    PlayerClass::Marauder,
    "Survivalist berserker forged in the void. Shrugs off damage "
    "and relies on instinct and sheer toughness.",
    {12, 10, 16, 8, 8, 12},
    {3, 0, 0, 0},                // acid +3
    6, 10,                        // +6 HP, +10 carry
    {SkillId::Cat_Endurance, SkillId::ThickSkin, SkillId::IronWill,
     SkillId::Cat_LongBlade},
    25, 15,
};

const ClassTemplate& class_template(PlayerClass c) {
    switch (c) {
        case PlayerClass::DevCommander: return s_dev_commander;
        case PlayerClass::Voidwalker:   return s_voidwalker;
        case PlayerClass::Gunslinger:   return s_gunslinger;
        case PlayerClass::Technomancer: return s_technomancer;
        case PlayerClass::Operative:    return s_operative;
        case PlayerClass::Marauder:     return s_marauder;
    }
    return s_voidwalker;
}

const std::vector<PlayerClass>& gameplay_classes() {
    static const std::vector<PlayerClass> classes = {
        PlayerClass::Voidwalker,
        PlayerClass::Gunslinger,
        PlayerClass::Technomancer,
        PlayerClass::Operative,
        PlayerClass::Marauder,
    };
    return classes;
}

} // namespace astra
