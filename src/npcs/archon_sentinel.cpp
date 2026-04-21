#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_sentinel(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Mechanical;
    npc.npc_role    = NpcRole::ArchonSentinel;
    npc.role        = "Archon Sentinel";
    npc.name        = "Archon Sentinel";
    npc.hp          = 50;
    npc.max_hp      = 50;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 80;   // ponderous but brutal
    npc.base_xp     = 120;
    npc.base_damage = 5;
    npc.dv          = 10;
    npc.av          = 10;   // high — forces STR-penetration play
    npc.damage_dice = Dice::make(2, 8);
    npc.damage_type = DamageType::Plasma;
    // Resistant to both common types: +4 plasma (barely dents), +2 kinetic.
    npc.type_affinity = {2, 4, 0, 0, 0};
    npc.elite       = true;   // miniboss — triggers hp x2, dv+2, av+1 at scale
    return npc;
}

} // namespace astra
