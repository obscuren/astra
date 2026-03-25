#include "astra/npc_defs.h"

namespace astra {

Npc build_xytomorph(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race = Race::Xytomorph;
    npc.glyph = 'X';
    npc.color = Color::Red;
    npc.role = "Xytomorph";
    npc.hp = 12;
    npc.max_hp = 12;
    npc.disposition = Disposition::Hostile;
    npc.quickness = 150;
    npc.base_xp = 25;
    npc.base_damage = 2;
    return npc;
}

} // namespace astra
