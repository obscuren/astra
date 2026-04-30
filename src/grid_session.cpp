#include "astra/grid_session.h"

namespace astra {

bool GridLootBuffer::empty() const {
    return credits == 0
        && code_fragments_t1 == 0
        && code_fragments_t2 == 0
        && programs_acquired.empty()
        && lore_unlocked.empty();
}

} // namespace astra
