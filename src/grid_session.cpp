#include "astra/grid_session.h"
#include "astra/grid_combat.h"

namespace astra {

bool GridLootBuffer::empty() const {
    return credits == 0
        && code_fragments_t1 == 0
        && code_fragments_t2 == 0
        && programs_acquired.empty()
        && lore_unlocked.empty();
}

Anchor* GridSession::anchor_for_npc(int npc_id) {
    for (auto& a : anchors_) {
        if (a.npc_id == npc_id) return &a;
    }
    return nullptr;
}

Anchor* GridSession::anchor_at(int x, int y) {
    for (auto& a : anchors_) {
        if (a.x == x && a.y == y && !a.severed()) return &a;
    }
    return nullptr;
}

Anchor* GridSession::add_anchor_for_npc(int npc_id, int sx, int sy,
                                        int npc_threat_tier, bool bound) {
    Anchor a;
    a.id = next_anchor_id_++;
    a.x = sx;
    a.y = sy;
    a.max_hp = anchor_max_hp(npc_threat_tier);
    a.hp = a.max_hp;
    a.npc_id = npc_id;
    a.bound = bound;
    a.identified = false;
    a.xp_granted = false;
    anchors_.push_back(a);
    return &anchors_.back();
}

} // namespace astra
