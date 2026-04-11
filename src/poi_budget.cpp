#include "astra/poi_budget.h"

#include "astra/celestial_body.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <random>
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

namespace {

bool is_habitable(const MapProperties& p) {
    return p.body_type == BodyType::Terrestrial &&
           (p.body_atmosphere == Atmosphere::Standard ||
            p.body_atmosphere == Atmosphere::Dense);
}

bool is_marginal(const MapProperties& p) {
    return p.body_type == BodyType::Terrestrial &&
           (p.body_atmosphere == Atmosphere::Thin ||
            p.body_atmosphere == Atmosphere::Toxic ||
            p.body_atmosphere == Atmosphere::Reducing);
}

bool is_airless(const MapProperties& p) {
    return (p.body_type == BodyType::Rocky ||
            p.body_type == BodyType::DwarfPlanet) ||
           (p.body_type == BodyType::Terrestrial &&
            p.body_atmosphere == Atmosphere::None);
}

bool is_asteroid(const MapProperties& p) {
    return p.body_type == BodyType::AsteroidBelt;
}

std::string pick_ruin_civ(const MapProperties& p, std::mt19937& rng) {
    // If lore gives us a primary civ, 70% weight on it, 30% on "unknown".
    if (p.lore_primary_civ_index >= 0) {
        if (std::uniform_int_distribution<int>(0, 99)(rng) < 70) {
            return "precursor_" + std::to_string(p.lore_primary_civ_index);
        }
    }
    return "unknown";
}

} // namespace

PoiBudget roll_poi_budget(const MapProperties& props, std::mt19937& rng) {
    PoiBudget b;

    const bool habitable = is_habitable(props);
    const bool marginal = is_marginal(props);
    const bool airless = is_airless(props);
    const bool asteroid = is_asteroid(props);
    std::uniform_int_distribution<int> pct(0, 99);

    // --- Settlements ---
    if (habitable) {
        b.settlements = 3;
        if (props.lore_tier >= 2) b.settlements += 2;
    } else if (marginal) {
        if (pct(rng) < 40) b.settlements = 1;
    }

    // --- Outposts ---
    {
        int chance = 30;
        if (props.lore_tier >= 2) chance = 70;
        if (props.lore_plague_origin) chance = 80;
        if (pct(rng) < chance) {
            b.outposts = std::uniform_int_distribution<int>(1, 2)(rng);
            if (props.lore_tier >= 2)
                b.outposts += std::uniform_int_distribution<int>(1, 2)(rng);
        }
    }

    // --- Caves ---
    if (props.body_has_dungeon) {
        b.caves.natural = std::uniform_int_distribution<int>(2, 5)(rng);
        if (asteroid) b.caves.natural = std::min(b.caves.natural, 2);
        if (props.lore_tier >= 2) b.caves.mine = 1;
        if (props.lore_tier >= 3) b.caves.excavation = 1;
    }

    // --- Ruins ---
    {
        int count = 0;
        if (props.body_danger_level >= 3)
            count = std::uniform_int_distribution<int>(1, 4)(rng);
        else if (pct(rng) < 30)
            count = std::uniform_int_distribution<int>(1, 2)(rng);
        if (props.lore_tier >= 3)
            count += std::uniform_int_distribution<int>(4, 6)(rng);
        else if (props.lore_tier >= 2)
            count += std::uniform_int_distribution<int>(2, 4)(rng);
        else if (props.lore_tier >= 1)
            count += std::uniform_int_distribution<int>(1, 2)(rng);

        // Hidden ratio: clamp(lore_tier * 0.25, 0, 0.6).
        float hidden_ratio = std::min(0.6f, props.lore_tier * 0.25f);

        for (int i = 0; i < count; ++i) {
            RuinRequest r;
            r.civ = pick_ruin_civ(props, rng);
            r.formation = (pct(rng) < 15) ? RuinFormation::Connected : RuinFormation::Solo;
            float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
            r.hidden = (roll < hidden_ratio);
            b.ruins.push_back(std::move(r));
        }
    }

    // --- Crashed ships ---
    {
        int base_chance = 20 + props.body_danger_level * 10;
        if (props.lore_battle_site) base_chance = 100;
        if (pct(rng) < base_chance) {
            int count = std::uniform_int_distribution<int>(1, 3)(rng);
            if (props.lore_battle_site)
                count += std::uniform_int_distribution<int>(2, 4)(rng);
            for (int i = 0; i < count; ++i) {
                ShipRequest s;
                if (asteroid) {
                    s.klass = ShipClass::EscapePod;
                } else {
                    int r = pct(rng);
                    if (r < 30) s.klass = ShipClass::EscapePod;
                    else if (r < 80) s.klass = ShipClass::Freighter;
                    else s.klass = ShipClass::Corvette;
                }
                b.ships.push_back(s);
            }
        }
    }

    return b;
}

PoiBudget reconstruct_poi_budget_from_map(const TileMap& overworld) {
    PoiBudget b;
    for (int y = 0; y < overworld.height(); ++y) {
        for (int x = 0; x < overworld.width(); ++x) {
            Tile t = overworld.get(x, y);
            switch (t) {
                case Tile::OW_Settlement:  ++b.settlements; break;
                case Tile::OW_Outpost:     ++b.outposts; break;
                case Tile::OW_Ruins: {
                    RuinRequest r;
                    r.civ = "unknown";
                    r.hidden = false;
                    b.ruins.push_back(r);
                    break;
                }
                case Tile::OW_CrashedShip: {
                    ShipRequest s;
                    s.klass = ShipClass::Freighter;
                    b.ships.push_back(s);
                    break;
                }
                case Tile::OW_CaveEntrance: ++b.caves.natural; break;
                case Tile::OW_Beacon:       ++b.beacons; break;
                case Tile::OW_Megastructure:++b.megastructures; break;
                default: break;
            }
        }
    }
    return b;
}

} // namespace astra
