#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_conclave_sentry_drone(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Mechanical;
    npc.npc_role    = NpcRole::ConclaveSentryDrone;
    npc.role        = "Conclave Sentry Drone";
    npc.name        = "Conclave Sentry Drone";
    npc.hp          = 16;
    npc.max_hp      = 16;
    npc.faction     = Faction_StellariConclave;
    npc.quickness   = 100;
    npc.base_xp     = 40;
    npc.base_damage = 3;
    npc.dv          = 10;
    npc.av          = 4;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Plasma;
    npc.ai                 = NpcAi::Turret;
    npc.attack_range       = 6;
    npc.ranged_damage_dice = Dice::make(1, 6);
    npc.ranged_damage_type = DamageType::Plasma;
    npc.type_affinity = {1, 2, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
