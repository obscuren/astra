#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_sentry_drone(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;
    npc.npc_role    = NpcRole::ArchonSentryDrone;
    npc.role        = "Archon Sentry Drone";
    npc.name        = "Archon Sentry Drone";
    npc.hp          = 18;
    npc.max_hp      = 18;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 100;
    npc.base_xp     = 45;
    npc.base_damage = 3;
    npc.dv          = 10;
    npc.av          = 5;
    npc.damage_dice = Dice::make(1, 8);
    npc.damage_type = DamageType::Plasma;
    npc.ai                 = NpcAi::Turret;
    npc.attack_range       = 6;
    npc.ranged_damage_dice = Dice::make(1, 8);
    npc.ranged_damage_type = DamageType::Plasma;
    npc.type_affinity = {-1, 3, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
