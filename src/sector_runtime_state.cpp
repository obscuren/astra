#include "astra/sector_runtime_state.h"

namespace astra {

void apply_mutations(GridSector& sector, const SectorRuntimeState& state) {
    for (const auto& m : state.mutations) {
        if (m.x < sector.w && m.y < sector.h) {
            sector.set(m.x, m.y, m.new_tile);
        }
    }
    // killed_ice is consumed by the ICE-spawning logic at session-init time
    // (Cut 2 wires it up); not by this overlay.
}

} // namespace astra
