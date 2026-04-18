#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

// Mid-tier hostile archetype for the Stellari Conclave faction.
// Used by Stage 4 of the Stellar Signal arc, after the player betrays the
// Conclave and they flip hostile. Sits between Archon Remnant (hp 15) and
// Archon Sentinel (hp 50, elite miniboss).
Npc build_conclave_sentry(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Stellari;
    npc.npc_role    = NpcRole::ConclaveSentry;
    npc.role        = "Conclave Sentry";
    npc.name        = "Conclave Sentry";
    npc.hp          = 30;
    npc.max_hp      = 30;
    npc.faction     = Faction_StellariConclave;
    npc.quickness   = 110;
    npc.base_xp     = 50;
    npc.base_damage = 3;
    npc.dv          = 9;
    npc.av          = 4;
    npc.damage_dice = Dice::make(1, 8);
    npc.damage_type = DamageType::Plasma;
    // Stellari resonance tech — strong vs. plasma, weak to kinetic slugs.
    npc.type_affinity = {-1, 2, 0, 0, 0};
    return npc;
}

} // namespace astra
