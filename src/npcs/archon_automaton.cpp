#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_automaton(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Mechanical;
    npc.npc_role    = NpcRole::ArchonAutomaton;
    npc.role        = "Archon Automaton";
    npc.name        = "Archon Automaton";
    npc.hp          = 28;
    npc.max_hp      = 28;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 80;               // heavy, slow
    npc.base_xp     = 70;
    npc.base_damage = 3;
    npc.dv          = 8;
    npc.av          = 6;
    npc.damage_dice = Dice::make(2, 6);
    npc.damage_type = DamageType::Plasma;
    // Archon chassis: hardened, plasma-hot. Brittle vs. kinetic.
    npc.type_affinity = {-2, 4, 0, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
