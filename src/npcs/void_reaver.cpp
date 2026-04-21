#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_void_reaver(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;
    npc.npc_role    = NpcRole::VoidReaver;
    npc.role        = "Void Reaver";
    npc.name        = "Void Reaver";
    npc.hp          = 20;
    npc.max_hp      = 20;
    npc.faction     = Faction_VoidReavers;
    npc.quickness   = 120;
    npc.base_xp     = 40;
    npc.base_damage = 3;
    npc.dv          = 10;
    npc.av          = 2;
    npc.damage_dice = Dice::make(1, 8);
    npc.damage_type = DamageType::Kinetic;
    // +1 kinetic (hardened armor vs. bullets).
    npc.type_affinity = {1, 0, 0, 0, 0};
    npc.flags |= static_cast<uint64_t>(CreatureFlag::Biological);
    return npc;
}

} // namespace astra
