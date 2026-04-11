#include "astra/poi_budget.h"

#include "astra/map_properties.h"

#include <sstream>

namespace astra {

int PoiBudget::visible_ruin_count() const {
    int n = 0;
    for (const auto& r : ruins) if (!r.hidden) ++n;
    return n;
}

int PoiBudget::hidden_ruin_count() const {
    int n = 0;
    for (const auto& r : ruins) if (r.hidden) ++n;
    return n;
}

std::string format_poi_budget(const PoiBudget& budget) {
    std::ostringstream os;
    os << "Settlements:    " << budget.settlements << "\n";
    os << "Outposts:       " << budget.outposts << "\n";
    os << "Ruins:          " << budget.ruins.size();
    if (!budget.ruins.empty()) {
        os << " (" << budget.visible_ruin_count() << " visible, "
           << budget.hidden_ruin_count() << " uncharted)";
    }
    os << "\n";
    os << "Caves:          " << budget.total_caves()
       << " (natural x" << budget.caves.natural
       << ", mine x" << budget.caves.mine
       << ", excavation x" << budget.caves.excavation << ")\n";
    os << "Crashed Ships:  " << budget.ships.size() << "\n";
    if (budget.beacons > 0)
        os << "Beacons:        " << budget.beacons << "\n";
    if (budget.megastructures > 0)
        os << "Megastructures: " << budget.megastructures << "\n";
    return os.str();
}

// Placeholder — filled in by Task 3.
PoiBudget roll_poi_budget(const MapProperties& /*props*/, std::mt19937& /*rng*/) {
    return PoiBudget{};
}

// Placeholder — filled in by Task 17.
PoiBudget reconstruct_poi_budget_from_map(const TileMap& /*overworld*/) {
    return PoiBudget{};
}

} // namespace astra
