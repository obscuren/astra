#include "astra/character.h"

namespace astra {

const char* class_name(PlayerClass c) {
    switch (c) {
        case PlayerClass::DevCommander: return "Dev Commander";
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

const ClassTemplate& class_template(PlayerClass c) {
    // Only DevCommander is defined for now — other classes coming later
    (void)c;
    return s_dev_commander;
}

} // namespace astra
