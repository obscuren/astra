#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_sentry_drone(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::SentryDrone;
    npc.role        = "Sentry Drone";
    npc.name        = "Sentry Drone";
    npc.hp          = 14;
    npc.max_hp      = 14;
    npc.faction     = Faction_Feral;   // use whatever rust_hound uses
    npc.quickness   = 100;
    npc.base_xp     = 35;
    npc.base_damage = 2;
    npc.dv          = 10;
    npc.av          = 4;
    npc.damage_dice = Dice::make(1, 6);
    // TODO: ranged attack — ships as melee placeholder until ranged combat lands.
    npc.damage_type = DamageType::Plasma;
    npc.type_affinity = {1, 2, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
