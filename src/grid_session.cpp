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

} // namespace astra
