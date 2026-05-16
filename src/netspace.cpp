#include "astra/netspace.h"

namespace astra {

void recompute_breakwall_lookup(Netspace& ns) {
    ns.breakwall_lookup.clear();
    for (size_t i = 0; i < ns.breakwalls.size(); ++i) {
        for (const auto& tile : ns.breakwalls[i].tiles) {
            ns.breakwall_lookup[tile] = i;
        }
    }
}

void restamp_breakwall_group(Netspace& ns, const BreakwallGroup& g) {
    const NetTile t = (g.current_density == 0) ? NetTile::Floor : NetTile::Breakwall;
    for (const auto& [x, y] : g.tiles) {
        ns.set(x, y, t);
    }
}

}  // namespace astra
