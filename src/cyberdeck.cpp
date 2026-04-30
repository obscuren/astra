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

void cyberdeck_add_heat(CyberdeckData& cd, int amount) {
    cd.heat_current += amount;
    if (cd.heat_current < 0) cd.heat_current = 0;
}

bool cyberdeck_decay_heat(CyberdeckData& cd) {
    cd.heat_current -= cd.stats.cooling_rate;
    if (cd.heat_current < 0) cd.heat_current = 0;
    return cd.heat_current == 0;
}

bool cyberdeck_overheated(const CyberdeckData& cd) {
    return cd.heat_current > cd.stats.heat_cap;
}

void cyberdeck_force_reboot(CyberdeckData& cd) {
    cd.ram_current = 0;
    cd.heat_current = 0;
}

} // namespace astra
