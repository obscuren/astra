#include "astra/energy.h"

#include <algorithm>

namespace astra {

int transfer_energy(EnergyStore& src, EnergyStore& dst, int requested,
                    int efficiency_bonus_per_n) {
    if (requested <= 0) return 0;
    int drain = std::min({requested, src.current, deficit(dst)});
    if (drain <= 0) return 0;
    src.current -= drain;
    int bonus = (efficiency_bonus_per_n > 0)
                  ? drain / efficiency_bonus_per_n
                  : 0;
    int deposited = std::min(drain + bonus, dst.capacity - dst.current);
    dst.current += deposited;
    return deposited;
}

} // namespace astra
