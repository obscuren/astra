#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_rust_hound(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::RustHound;
    npc.role        = "Rust Hound";
    npc.name        = "Rust Hound";
    npc.hp          = 8;
    npc.max_hp      = 8;
    npc.faction     = Faction_Feral;   // feral scavenger drone; hostile to everyone
    npc.quickness   = 140;              // fast
    npc.base_xp     = 20;
    npc.base_damage = 1;
    npc.dv          = 11;
    npc.av          = 1;
    npc.damage_dice = Dice::make(1, 4);
    npc.damage_type = DamageType::Kinetic;
    // Hardened chassis: +2 kinetic resistance, weak to electrical.
    npc.type_affinity = {2, 0, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
