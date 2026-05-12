#include "astra/character.h"

#include "astra/item.h"
#include "astra/item_ids.h"

namespace astra {

const char* class_name(PlayerClass c) {
    switch (c) {
        case PlayerClass::DevCommander: return "Dev Commander";
        case PlayerClass::Voidwalker:   return "Voidwalker";
        case PlayerClass::Gunslinger:   return "Gunslinger";
        case PlayerClass::Technomancer: return "Technomancer";
        case PlayerClass::Operative:    return "Operative";
        case PlayerClass::Marauder:     return "Marauder";
        case PlayerClass::Gridrunner:   return "Gridrunner";
    }
    return "Unknown";
}

static const ClassTemplate s_dev_commander = {
    PlayerClass::DevCommander,
    "Developer testing class. High stats across the board with "
    "multiple skills unlocked and generous starting resources.",
    'D',
    {14, 12, 16, 12, 12, 10},  // strong all-rounder, TOU/STR focus
    {5, 5, 5, 5},              // all resistances
    5, 20,                      // +5 HP, +20 carry
    {SkillId::Cat_ShortBlade, SkillId::ShortBladeExpertise,
     SkillId::Cat_Pistol, SkillId::SteadyHand,
     SkillId::Cat_Endurance, SkillId::ThickSkin,
     SkillId::Cat_Tinkering, SkillId::BasicRepair, SkillId::Disassemble, SkillId::Synthesize},
    200, 50,                    // 200 SP, 50 credits
    {                            // starting items — full pantry + cookbooks for testing
        {ITEM_RAW_MEAT,                10},
        {ITEM_CARROT,                  10},
        {ITEM_FLOUR,                   10},
        {ITEM_HERBS,                   10},
        {ITEM_SYNTH_PROTEIN,           10},
        {ITEM_COOKBOOK_HEARTY_STEW,     1},
        {ITEM_COOKBOOK_PROTEIN_BAKE,    1},
        {ITEM_COOKBOOK_HEROS_FEAST,     1},
    },
};

static const ClassTemplate s_voidwalker = {
    PlayerClass::Voidwalker,
    "Melee-focused space marine. Heavy armor and blade mastery "
    "make the Voidwalker a frontline juggernaut.",
    'V',
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
    'G',
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
    'T',
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
    'O',
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
    'M',
    {12, 10, 16, 8, 8, 12},
    {3, 0, 0, 0},                // acid +3
    6, 10,                        // +6 HP, +10 carry
    {SkillId::Cat_Endurance, SkillId::ThickSkin, SkillId::IronWill,
     SkillId::Cat_LongBlade},
    25, 15,
};

static const ClassTemplate s_gridrunner = {
    PlayerClass::Gridrunner,
    "Cyberspace specialist. Frail in meatspace, lethal once jacked in. "
    "Slips past ICE, bleeds Trace slowly, and owns any LAN within reach.",
    'R',
    {7, 12, 8, 16, 14, 9},        // INT-heavy, low STR/TOU
    {0, 5, 2, 0},                 // electrical +5, cold +2
    -2, 8,                        // -2 HP (glass cannon), +8 carry (room for deck + cells)
    {SkillId::Cat_Hacking,        // parent category for hacking sub-skills
     SkillId::Programming1,       // unlocks the Cyberdeck Compiler (3-fragment ceiling)
     SkillId::Intrusion,          // White ICE Trace +1 not +2
     SkillId::GhostProtocol,      // first program each Grid run is heatless
     SkillId::Cat_ShortBlade},    // bare-bones meatspace defense
    100, 60,                      // 100 SP, 60 credits
    {
        {ITEM_RELAY_CORTEX_MK1,     1, std::nullopt, ImplantSlot::Head},  // jack-in gate
        {ITEM_PIDGIN_MK1,           1, EquipSlot::Utility1},              // T1 cyberdeck
        {ITEM_PROG_BREACH,          1},                                    // load into deck slot 1 manually
        {ITEM_PROG_ICEBREAKER_LITE, 1},
        {ITEM_PROG_GHOST_TRACE,     1},
        {ITEM_SMALL_ENERGY_CELL,    2},                                    // recharge the deck
        {ITEM_COMBAT_KNIFE,         1, EquipSlot::RightHand},             // last-ditch melee
        {ITEM_PADDED_VEST,          1, EquipSlot::Body},                   // basic body armor
    },
};

const ClassTemplate& class_template(PlayerClass c) {
    switch (c) {
        case PlayerClass::DevCommander: return s_dev_commander;
        case PlayerClass::Voidwalker:   return s_voidwalker;
        case PlayerClass::Gunslinger:   return s_gunslinger;
        case PlayerClass::Technomancer: return s_technomancer;
        case PlayerClass::Operative:    return s_operative;
        case PlayerClass::Marauder:     return s_marauder;
        case PlayerClass::Gridrunner:   return s_gridrunner;
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
        PlayerClass::Gridrunner,
    };
    return classes;
}

} // namespace astra
