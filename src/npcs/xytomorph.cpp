#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_xytomorph(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race = Race::Xytomorph;
    npc.npc_role = NpcRole::Xytomorph;
    npc.role = "Xytomorph";
    npc.hp = 12;
    npc.max_hp = 12;
    npc.faction = Faction_XytomorphHive;
    npc.quickness = 150;
    npc.base_xp = 25;
    npc.base_damage = 2;
    npc.dv = 8; npc.av = 4;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Acid;
    npc.type_affinity = {2, -2, 0, 0, 3};
    return npc;
}

} // namespace astra
