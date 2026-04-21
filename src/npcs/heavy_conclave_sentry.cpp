#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

// Heavy-tier hostile archetype for the Stellari Conclave faction.
// Stronger variant of the standard Conclave Sentry — more HP, damage,
// and armor. Used as a tougher gate-keeper after the player betrays the
// Conclave at higher threat levels.
Npc build_heavy_conclave_sentry(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Stellari;
    npc.npc_role    = NpcRole::HeavyConclaveSentry;
    npc.role        = "Heavy Conclave Sentry";
    npc.name        = "Heavy Conclave Sentry";
    npc.hp          = 55;
    npc.max_hp      = 55;
    npc.faction     = Faction_StellariConclave;
    npc.quickness   = 105;
    npc.base_xp     = 90;
    npc.base_damage = 5;
    npc.dv          = 11;
    npc.av          = 7;
    npc.damage_dice = Dice::make(1, 10);
    npc.damage_type = DamageType::Plasma;
    // Stellari resonance tech — strong vs. plasma, weak to kinetic slugs.
    npc.type_affinity = {-1, 3, 0, 0, 0};
    return npc;
}

} // namespace astra
