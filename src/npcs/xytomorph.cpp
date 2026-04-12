#include "astra/npc_defs.h"
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
    return npc;
}

} // namespace astra
