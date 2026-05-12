#include "astra/grid_session.h"
#include "astra/grid_combat.h"
#include "astra/grid_constants.h"

#include <algorithm>

namespace astra {

int GridSession::gain_trace(int amount) {
    if (amount > 0 && trace_resistance_pct > 0) {
        int pct = trace_resistance_pct;
        if (pct > 100) pct = 100;
        // Round to nearest to avoid silently zeroing tiny gains.
        amount = (amount * (100 - pct) + 50) / 100;
        if (amount < 0) amount = 0;
    }
    trace += amount;
    if (trace < 0) trace = 0;
    if (trace > kTraceMax) trace = kTraceMax;
    return trace;
}


bool GridLootBuffer::empty() const {
    return credits == 0
        && code_fragments_t1 == 0
        && code_fragments_t2 == 0
        && programs_acquired.empty()
        && lore_unlocked.empty();
}

Imprint* GridSession::imprint_for_npc(int npc_id) {
    for (auto& a : imprints_) {
        if (a.npc_id == npc_id) return &a;
    }
    return nullptr;
}

Imprint* GridSession::imprint_at(int x, int y) {
    for (auto& a : imprints_) {
        if (a.x == x && a.y == y && !a.severed()) return &a;
    }
    return nullptr;
}

Imprint* GridSession::add_imprint_for_npc(int npc_id, int sx, int sy,
                                        int npc_threat_tier, bool bound) {
    Imprint a;
    a.id = next_imprint_id_++;
    a.x = sx;
    a.y = sy;
    a.max_hp = anchor_max_hp(npc_threat_tier);
    a.hp = a.max_hp;
    a.npc_id = npc_id;
    a.bound = bound;
    a.identified = false;
    a.xp_granted = false;
    imprints_.push_back(a);
    return &imprints_.back();
}

} // namespace astra
