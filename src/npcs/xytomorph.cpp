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
    npc.invulnerable = false;
    // Hostile — no interactions, no personal name
    return npc;
}

} // namespace astra
