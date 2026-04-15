#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_remnant(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::ArchonRemnant;
    npc.role        = "Archon Remnant";
    npc.name        = "Archon Remnant";
    npc.hp          = 15;
    npc.max_hp      = 15;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 100;
    npc.base_xp     = 30;
    npc.base_damage = 2;
    npc.dv          = 9;
    npc.av          = 3;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Plasma;
    // TypeAffinity field order: {kinetic, plasma, electrical, cryo, acid}
    // +3 plasma (hardened), -2 kinetic (brittle chassis).
    npc.type_affinity = {-2, 3, 0, 0, 0};
    return npc;
}

} // namespace astra
