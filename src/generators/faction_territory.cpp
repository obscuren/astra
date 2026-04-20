#include "astra/faction_territory.h"

#include "astra/faction.h"
#include "astra/star_chart.h"

#include <random>

namespace astra {

namespace {

constexpr float kCapitalMinDist   = 40.0f;  // galaxy units between capitals
constexpr float kInfluenceRadius  = 35.0f;  // owned if dist <= this
constexpr float kNoiseRate        = 0.10f;  // fraction of owned systems that enclave

struct Capital {
    size_t system_index = 0;
    FactionTerritory faction = FactionTerritory::Unclaimed;
};

// Faction capital budget. Total = 7. Sol is a hard-pinned Terran capital
// before procedural placement runs.
struct FactionBudget {
    FactionTerritory faction;
    int count;
};

constexpr FactionBudget kBudget[] = {
    {FactionTerritory::StellariConclave, 3},
    {FactionTerritory::TerranFederation, 2},
    {FactionTerritory::KrethMiningGuild, 1},
    {FactionTerritory::VeldraniAccord,   1},
};

float dist_sq(const StarSystem& a, const StarSystem& b) {
    float dx = a.gx - b.gx, dy = a.gy - b.gy;
    return dx * dx + dy * dy;
}

int find_sol(const NavigationData& nav) {
    for (size_t i = 0; i < nav.systems.size(); ++i) {
        if (nav.systems[i].id == 1) return static_cast<int>(i);
    }
    return -1;
}

bool far_enough_from_all(const StarSystem& candidate,
                         const std::vector<Capital>& existing,
                         const std::vector<StarSystem>& systems,
                         float min_dist) {
    float min_sq = min_dist * min_dist;
    for (const auto& cap : existing) {
        if (dist_sq(candidate, systems[cap.system_index]) < min_sq) {
            return false;
        }
    }
    return true;
}

std::vector<Capital> place_capitals(const NavigationData& nav,
                                    std::mt19937& rng) {
    std::vector<Capital> capitals;

    int sol = find_sol(nav);
    int terran_remaining = 0;
    for (const auto& b : kBudget) {
        if (b.faction == FactionTerritory::TerranFederation) {
            terran_remaining = b.count - 1;
            break;
        }
    }
    if (sol >= 0) {
        capitals.push_back({static_cast<size_t>(sol),
                            FactionTerritory::TerranFederation});
    }

    auto try_place = [&](FactionTerritory f, float min_dist) -> bool {
        std::uniform_int_distribution<size_t> pick(0, nav.systems.size() - 1);
        for (int attempt = 0; attempt < 500; ++attempt) {
            size_t idx = pick(rng);
            if (nav.systems[idx].id == 1) continue;
            bool already = false;
            for (const auto& c : capitals) {
                if (c.system_index == idx) { already = true; break; }
            }
            if (already) continue;
            if (!far_enough_from_all(nav.systems[idx], capitals,
                                     nav.systems, min_dist)) continue;
            capitals.push_back({idx, f});
            return true;
        }
        return false;
    };

    float min_dist = kCapitalMinDist;
    for (int relax = 0; relax < 4; ++relax) {
        bool all_placed = true;

        for (int i = 0; i < terran_remaining; ++i) {
            if (!try_place(FactionTerritory::TerranFederation, min_dist)) {
                all_placed = false;
            }
        }
        terran_remaining = 0;

        for (const auto& b : kBudget) {
            if (b.faction == FactionTerritory::TerranFederation) continue;
            for (int i = 0; i < b.count; ++i) {
                if (!try_place(b.faction, min_dist)) {
                    all_placed = false;
                }
            }
        }
        if (all_placed) break;
        min_dist *= 0.9f;
    }

    return capitals;
}

} // namespace

void assign_system_factions(NavigationData& nav, uint32_t seed) {
    for (auto& s : nav.systems) s.controlling_faction.clear();
    nav.faction_map.cells.clear();

    if (nav.systems.empty()) return;

    std::mt19937 rng(seed ^ 0xF4C710Du);

    auto capitals = place_capitals(nav, rng);
    if (capitals.empty()) return;

    // ── Assign every system to the nearest capital within influence. ──
    const float inf_sq = kInfluenceRadius * kInfluenceRadius;
    for (size_t i = 0; i < nav.systems.size(); ++i) {
        StarSystem& s = nav.systems[i];

        // If this system is itself a capital, set its faction directly.
        bool is_capital = false;
        for (const auto& cap : capitals) {
            if (cap.system_index == i) {
                s.controlling_faction = faction_name_from_enum(cap.faction);
                is_capital = true;
                break;
            }
        }
        if (is_capital) continue;

        // Nearest-capital search.
        FactionTerritory best = FactionTerritory::Unclaimed;
        float best_sq = inf_sq;
        for (const auto& cap : capitals) {
            float d2 = dist_sq(s, nav.systems[cap.system_index]);
            if (d2 <= best_sq) {
                best_sq = d2;
                best = cap.faction;
            }
        }
        s.controlling_faction = faction_name_from_enum(best);
    }

    // ── Noise pass: enclaves. ──
    // Deterministic per-system by seeding from (seed ^ system_id).
    for (auto& s : nav.systems) {
        if (s.controlling_faction.empty()) continue;  // no enclaves in wilderness

        std::mt19937 r(seed ^ (s.id * 2654435761u));
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(r) >= kNoiseRate) continue;

        // 80% swap to different territorial faction, 20% to Unclaimed.
        if (roll(r) < 0.8f) {
            FactionTerritory current = faction_enum_from_name(s.controlling_faction);
            std::uniform_int_distribution<int> pick(1, 4);  // 1..4 = non-Unclaimed
            FactionTerritory chosen;
            do {
                chosen = static_cast<FactionTerritory>(pick(r));
            } while (chosen == current);
            s.controlling_faction = faction_name_from_enum(chosen);
        } else {
            s.controlling_faction = "";
        }
    }

    // TODO next task: FactionMap precompute.
}

std::string faction_at_coord(const NavigationData&, float, float) {
    return "";
}

FactionTerritory faction_enum_from_name(const std::string& faction) {
    if (faction == Faction_StellariConclave) return FactionTerritory::StellariConclave;
    if (faction == Faction_TerranFederation) return FactionTerritory::TerranFederation;
    if (faction == Faction_KrethMiningGuild) return FactionTerritory::KrethMiningGuild;
    if (faction == Faction_VeldraniAccord)   return FactionTerritory::VeldraniAccord;
    return FactionTerritory::Unclaimed;
}

const char* faction_name_from_enum(FactionTerritory t) {
    switch (t) {
        case FactionTerritory::StellariConclave: return Faction_StellariConclave;
        case FactionTerritory::TerranFederation: return Faction_TerranFederation;
        case FactionTerritory::KrethMiningGuild: return Faction_KrethMiningGuild;
        case FactionTerritory::VeldraniAccord:   return Faction_VeldraniAccord;
        case FactionTerritory::Unclaimed:        return "";
    }
    return "";
}

} // namespace astra
