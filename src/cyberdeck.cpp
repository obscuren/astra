#include "astra/cyberdeck.h"

namespace astra {

CyberdeckStats cyberdeck_stats_tier1() {
    return CyberdeckStats{};
}

CyberdeckStats cyberdeck_stats_tier2() {
    CyberdeckStats s;
    s.ram_max      = 8;
    s.cpu          = 2;
    s.slots        = 4;
    s.stealth      = 1;
    s.cooling_rate = 1;
    s.heat_cap     = 12;
    return s;
}

} // namespace astra
