#include "astra/galaxy_sim.h"
#include "astra/lore_generator.h"
#include "astra/narrative_templates.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace astra {

// ── Helpers ────────────────────────────────────────────────────────────────

static float randf(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}

static int randi(std::mt19937& rng, int lo, int hi) {
    if (lo >= hi) return lo;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static const char* philosophy_str(Philosophy p) {
    static const char* names[] = {"expansionist","contemplative","predatory","symbiotic","transcendent"};
    return names[static_cast<int>(p)];
}

// ── System generation (simple spatial model) ──────────────────────────────

std::vector<SimSystem> GalaxySim::generate_systems(std::mt19937& rng) {
    // Generate ~300 systems in a disk for the simulation
    // (These are abstract; mapped to real StarChart systems in Phase 2)
    std::vector<SimSystem> systems;
    int count = 300;
    for (int i = 0; i < count; ++i) {
        SimSystem s;
        s.id = static_cast<uint32_t>(i + 1);
        // Disk distribution
        float angle = randf(rng, 0, 6.283f);
        float r = std::sqrt(randf(rng, 0, 1.0f)) * 200.0f; // radius up to 200
        s.gx = r * std::cos(angle);
        s.gy = r * std::sin(angle);
        s.resource_richness = randf(rng, 0.1f, 1.0f);
        systems.push_back(s);
    }
    return systems;
}

float GalaxySim::distance_sq(const SimSystem& a, const SimSystem& b) {
    float dx = a.gx - b.gx;
    float dy = a.gy - b.gy;
    return dx * dx + dy * dy;
}

uint32_t GalaxySim::find_nearest_unclaimed(const CivState& civ,
                                            const std::vector<SimSystem>& systems) {
    // Find the homeworld position
    float hx = 0, hy = 0;
    for (const auto& s : systems) {
        if (s.id == civ.homeworld) { hx = s.gx; hy = s.gy; break; }
    }

    float best_dist = 1e18f;
    uint32_t best_id = 0;
    for (const auto& s : systems) {
        if (s.owner_civ >= 0) continue;
        float dx = s.gx - hx, dy = s.gy - hy;
        float d = dx * dx + dy * dy;
        if (d < best_dist) {
            best_dist = d;
            best_id = s.id;
        }
    }
    return best_id;
}

bool GalaxySim::territories_adjacent(const CivState& a, const CivState& b,
                                      const std::vector<SimSystem>& systems,
                                      float range) {
    float range_sq = range * range;
    for (uint32_t sa : a.territory) {
        for (uint32_t sb : b.territory) {
            // Find both systems
            const SimSystem* sysA = nullptr;
            const SimSystem* sysB = nullptr;
            for (const auto& s : systems) {
                if (s.id == sa) sysA = &s;
                if (s.id == sb) sysB = &s;
                if (sysA && sysB) break;
            }
            if (sysA && sysB && distance_sq(*sysA, *sysB) < range_sq)
                return true;
        }
    }
    return false;
}

// ── Spawn a civilization ──────────────────────────────────────────────────

CivState GalaxySim::spawn_civilization(std::mt19937& rng, int civ_id,
                                        std::vector<SimSystem>& systems,
                                        Philosophy /*hint*/) {
    CivState civ;
    civ.id = civ_id;

    // ── Allocate 100 trait points randomly with bias ──
    // Give each trait a random base (1-5), then distribute remaining points
    int* trait_arr[] = {
        &civ.traits.aggression, &civ.traits.curiosity, &civ.traits.industriousness,
        &civ.traits.cohesion, &civ.traits.spirituality, &civ.traits.adaptability,
        &civ.traits.diplomacy, &civ.traits.creativity, &civ.traits.technology
    };
    int remaining = CivTraits::total_points;
    for (int i = 0; i < CivTraits::trait_count; ++i) {
        *trait_arr[i] = randi(rng, 2, 8);
        remaining -= *trait_arr[i];
    }
    // Distribute remaining points randomly
    while (remaining > 0) {
        int idx = randi(rng, 0, CivTraits::trait_count - 1);
        int add = std::min(remaining, randi(rng, 1, 5));
        *trait_arr[idx] += add;
        remaining -= add;
    }
    // If we went negative from initial allocation, steal from highest
    while (remaining < 0) {
        int highest_idx = 0;
        for (int i = 1; i < CivTraits::trait_count; ++i) {
            if (*trait_arr[i] > *trait_arr[highest_idx]) highest_idx = i;
        }
        int steal = std::min(-remaining, *trait_arr[highest_idx] - 1);
        *trait_arr[highest_idx] -= steal;
        remaining += steal;
        if (steal == 0) break; // safety
    }

    // ── Derive philosophy from dominant traits ──
    struct { int val; Philosophy phil; } mapping[] = {
        {civ.traits.aggression + civ.traits.industriousness, Philosophy::Expansionist},
        {civ.traits.curiosity + civ.traits.creativity, Philosophy::Contemplative},
        {civ.traits.aggression * 2, Philosophy::Predatory},
        {civ.traits.diplomacy + civ.traits.cohesion + civ.traits.adaptability, Philosophy::Symbiotic},
        {civ.traits.spirituality + civ.traits.curiosity, Philosophy::Transcendent},
    };
    int best = 0;
    for (int i = 1; i < 5; ++i) {
        if (mapping[i].val > mapping[best].val) best = i;
    }
    civ.philosophy = mapping[best].phil;

    // Pick a random unclaimed system as homeworld
    std::vector<int> unclaimed;
    for (int i = 0; i < static_cast<int>(systems.size()); ++i) {
        if (systems[i].owner_civ < 0) unclaimed.push_back(i);
    }
    if (unclaimed.empty()) {
        civ.alive = false;
        return civ;
    }
    int idx = unclaimed[rng() % unclaimed.size()];
    civ.homeworld = systems[idx].id;
    civ.territory.insert(systems[idx].id);
    systems[idx].owner_civ = civ_id;

    // Initial state (influenced by traits)
    civ.population = randf(rng, 8.0f, 15.0f);
    civ.resources = randf(rng, 40.0f, 80.0f) + civ.traits.industriousness * 2.0f;
    civ.knowledge = randf(rng, 3.0f, 8.0f) + civ.traits.technology * 0.5f;
    civ.stability = randf(rng, 50.0f, 70.0f) + civ.traits.cohesion * 1.0f;
    civ.military = civ.traits.aggression * 2.0f;
    civ.sgra_awareness = civ.traits.spirituality * 0.5f;
    civ.weapon_tech = 0;
    civ.faction_tension = 0.0f;

    return civ;
}

// ── Per-tick civilization update ───────────────────────────────────────────

void GalaxySim::tick_civilization(std::mt19937& rng, CivState& civ,
                                  std::vector<SimSystem>& systems,
                                  std::vector<SimEvent>& events,
                                  int tick,
                                  const std::vector<CivState>& all_civs) {
    if (!civ.alive) return;

    // Decay cooldowns
    if (civ.cd_breakthrough > 0) --civ.cd_breakthrough;
    if (civ.cd_expansion > 0) --civ.cd_expansion;
    if (civ.cd_conflict > 0) --civ.cd_conflict;
    if (civ.cd_renaissance > 0) --civ.cd_renaissance;
    if (civ.cd_megastructure > 0) --civ.cd_megastructure;
    if (civ.cd_plague > 0) --civ.cd_plague;
    if (civ.cd_mining > 0) --civ.cd_mining;
    if (civ.cd_artifact > 0) --civ.cd_artifact;
    if (civ.cd_weapon > 0) --civ.cd_weapon;

    float territory_size = static_cast<float>(civ.territory.size());
    float capacity = territory_size * 120.0f;

    // ── Trait-derived modifiers (each trait 0-30ish, normalized to multipliers) ──
    const auto& t = civ.traits;
    float pop_growth_mult = 0.7f + t.industriousness * 0.03f + t.adaptability * 0.02f;
    float consumption_mult = 0.8f + t.aggression * 0.02f + t.industriousness * 0.01f;
    float research_mult = 0.5f + t.curiosity * 0.05f + t.technology * 0.05f + t.creativity * 0.03f;
    float military_growth = t.aggression * 0.04f + t.technology * 0.02f;
    float stability_drift = (t.cohesion - 10.0f) * 0.01f + t.diplomacy * 0.005f - t.aggression * 0.005f;
    float sgra_mult = 0.5f + t.spirituality * 0.05f + t.curiosity * 0.02f;

    // ── 1. Growth ──
    // Resource consumption scales sub-linearly with population (efficiency gains)
    float pop_need = std::sqrt(civ.population) * 0.3f * consumption_mult;
    float resource_income = 0.0f;
    for (uint32_t sid : civ.territory) {
        for (const auto& s : systems) {
            if (s.id == sid) {
                // Each system produces more with higher knowledge (tech improves extraction)
                float extraction = s.resource_richness * (2.0f + civ.knowledge * 0.005f);
                resource_income += extraction;
                break;
            }
        }
    }
    civ.resources += resource_income - pop_need;
    civ.resources = clampf(civ.resources, 0.0f, 5000.0f);

    // Population growth: slow and steady, capped by carrying capacity
    float growth_rate = pop_growth_mult * 0.2f;
    float resource_factor = clampf(civ.resources / (pop_need * 3.0f + 1.0f), 0.0f, 2.0f);
    float stability_factor = civ.stability / 100.0f;
    float growth = growth_rate * resource_factor * stability_factor;
    // Soft cap: growth slows as population approaches capacity
    if (civ.population > capacity * 0.8f) growth *= 0.3f;
    // Starvation: gradual decline, not instant death
    if (civ.resources <= 0.0f) {
        growth = -0.1f * (1.0f + civ.population * 0.001f); // slow bleed
        civ.resources = 0.0f;
    }
    civ.population += growth;
    civ.population = clampf(civ.population, 0.0f, 5000.0f);

    // Knowledge grows steadily — civilizations always learn
    civ.knowledge += 0.05f * research_mult * (0.5f + stability_factor * 0.5f);
    civ.military += military_growth * 0.05f;
    civ.sgra_awareness += 0.003f * sgra_mult * (civ.knowledge / 200.0f);
    civ.sgra_awareness = clampf(civ.sgra_awareness, 0.0f, 100.0f);

    // ── 2. Stability ──
    civ.age++;

    float target_stability = 55.0f + stability_drift * 50.0f;
    // Stability recovers slowly toward target
    civ.stability += (target_stability - civ.stability) * 0.02f;
    // Penalties for bad conditions
    if (civ.resources <= 0.0f) civ.stability -= 0.5f; // famine
    if (territory_size > civ.stability * 0.5f) civ.stability -= 0.2f; // overextension
    if (civ.knowledge > 500.0f && civ.stability < 40.0f) civ.stability -= 0.2f; // existential crisis
    if (civ.faction_count > 1) civ.stability -= 0.1f * civ.faction_count;
    // Recovery bonus from adaptability trait
    if (civ.stability < 30.0f) civ.stability += civ.traits.adaptability * 0.02f;

    // ── Entropy: civilizations face increasing pressure over time ──
    // After 500M years, stability ceiling slowly drops (institutional decay)
    // After 1B years, resource efficiency declines (depletion)
    // After 2B years, existential fatigue sets in
    // ── Entropy: nothing lasts forever ──
    // Instead of deterministic decay, use random catastrophic "crisis rolls"
    // that become more likely with age. Creates varied lifespans.
    float age_bya = civ.age / 1000.0f;
    float resilience = (civ.traits.cohesion + civ.traits.adaptability) * 0.5f; // 0-30ish

    // Crisis roll: chance of a major destabilizing event increases with age
    // At 0.3 Bya: ~0.1% per tick. At 1 Bya: ~1% per tick. At 3 Bya: ~5% per tick.
    float crisis_chance = age_bya * age_bya * 0.5f; // quadratic growth
    crisis_chance = std::max(0.0f, crisis_chance - resilience * 0.02f); // resilience reduces chance

    if (crisis_chance > 0.0f && randf(rng, 0.0f, 100.0f) < crisis_chance) {
        // Random crisis — severity varies
        float severity = randf(rng, 0.3f, 1.0f);
        civ.stability -= severity * 15.0f;
        civ.population *= (1.0f - severity * 0.15f);
        civ.resources *= (1.0f - severity * 0.2f);

        // Pick a crisis type for flavor
        int crisis_type = randi(rng, 0, 4);
        switch (crisis_type) {
        case 0: // Societal decay
            civ.faction_tension += 20.0f * severity;
            break;
        case 1: // Resource collapse
            civ.resources *= (1.0f - severity * 0.3f);
            break;
        case 2: // Military coup / internal conflict
            civ.military *= (1.0f - severity * 0.2f);
            civ.stability -= severity * 10.0f;
            break;
        case 3: // Knowledge stagnation (dark age)
            civ.knowledge *= (1.0f - severity * 0.1f);
            break;
        case 4: // Population crisis
            civ.population *= (1.0f - severity * 0.2f);
            break;
        }
    }

    // Gentle background resource depletion over very long timescales
    if (age_bya > 1.0f) {
        civ.resources -= (age_bya - 1.0f) * 0.02f * territory_size;
    }

    civ.stability = clampf(civ.stability, 0.0f, 100.0f);

    // Faction tension builds when stability is low
    if (civ.stability < 40.0f) {
        civ.faction_tension += (40.0f - civ.stability) * 0.02f;
    } else {
        civ.faction_tension = std::max(0.0f, civ.faction_tension - 0.1f);
    }

    // ── Helper to emit events ──
    auto emit = [&](LoreEventType type, const std::string& desc, uint32_t sys_id = 0, int other = -1) {
        SimEvent ev;
        ev.civ_id = civ.id;
        ev.tick = tick;
        ev.type = type;
        ev.description = desc;
        ev.system_id = sys_id ? sys_id : civ.homeworld;
        ev.other_civ_id = other;
        ev.pop_at_event = civ.population;
        ev.res_at_event = civ.resources;
        ev.knowledge_at_event = civ.knowledge;
        ev.military_at_event = civ.military;
        events.push_back(ev);
    };

    // ── 3. Event triggers ──

    // Scientific breakthrough (knowledge thresholds)
    if (civ.cd_breakthrough == 0) {
        float thresholds[] = {50, 100, 200, 350, 500, 700, 900};
        for (float t : thresholds) {
            if (civ.knowledge >= t && civ.knowledge < t + 1.0f) {
                civ.cd_breakthrough = 80 + randi(rng, 0, 40);
                civ.knowledge += 10.0f;
                civ.stability += 5.0f;
                emit(LoreEventType::ScientificBreakthrough, "BREAKTHROUGH");
                break;
            }
        }
    }

    // Weapon development (knowledge-gated, gives military jumps)
    if (civ.cd_weapon == 0 && civ.knowledge > (civ.weapon_tech + 1) * 150.0f) {
        civ.weapon_tech++;
        civ.military += 50.0f * civ.weapon_tech;
        civ.cd_weapon = 150 + randi(rng, 0, 100);
        emit(LoreEventType::ArtifactCreation, "WEAPON_BREAKTHROUGH");
    }

    // Expansion (population pressure)
    if (civ.cd_expansion == 0 && civ.population > capacity * 0.8f) {
        uint32_t target = find_nearest_unclaimed(civ, systems);
        if (target != 0) {
            civ.territory.insert(target);
            for (auto& s : systems) {
                if (s.id == target) { s.owner_civ = civ.id; break; }
            }
            civ.resources -= 20.0f;
            civ.cd_expansion = 30 + randi(rng, 0, 20);
            emit(LoreEventType::ColonyFounded, "COLONY", target);
        } else {
            // No room — pressure builds
            civ.stability -= 3.0f;
        }
    }

    // Terraforming (high knowledge + resources, on a owned system)
    if (civ.knowledge > 200.0f && civ.resources > 200.0f && rng() % 500 == 0) {
        // Pick a random owned system
        std::vector<uint32_t> owned(civ.territory.begin(), civ.territory.end());
        if (!owned.empty()) {
            uint32_t target = owned[rng() % owned.size()];
            for (auto& s : systems) {
                if (s.id == target && !s.terraformed) {
                    s.terraformed = true;
                    s.terraformed_by = civ.id;
                    s.resource_richness = std::min(1.0f, s.resource_richness + 0.3f);
                    civ.resources -= 100.0f;
                    emit(LoreEventType::Terraforming, "TERRAFORM", target);
                    break;
                }
            }
        }
    }

    // Trade route (multiple territories, good stability)
    if (civ.territory.size() > 3 && civ.stability > 60.0f && rng() % 300 == 0) {
        civ.resources += 30.0f;
        civ.stability += 3.0f;
        emit(LoreEventType::TradeRoute, "TRADE");
    }

    // Hyperspace route (knowledge threshold, once per level)
    if (civ.knowledge > 80.0f && rng() % 200 == 0 && civ.territory.size() > 2) {
        emit(LoreEventType::HyperspaceRoute, "HYPERSPACE");
        civ.resources += 15.0f;
    }

    // Megastructure (high stability + high resources + high knowledge)
    if (civ.cd_megastructure == 0 && civ.stability > 75.0f &&
        civ.resources > 400.0f && civ.knowledge > 300.0f) {
        civ.cd_megastructure = 200 + randi(rng, 0, 100);
        civ.resources -= 200.0f;
        civ.knowledge += 20.0f;
        civ.sgra_awareness += 5.0f;
        // Pick a system for it
        std::vector<uint32_t> owned(civ.territory.begin(), civ.territory.end());
        uint32_t loc = owned.empty() ? civ.homeworld : owned[rng() % owned.size()];
        for (auto& s : systems) {
            if (s.id == loc) { s.has_megastructure = true; s.megastructure_builder = civ.id; break; }
        }
        emit(LoreEventType::MegastructureBuilt, "MEGASTRUCTURE", loc);
    }

    // Cultural renaissance (high stability + decent knowledge)
    if (civ.cd_renaissance == 0 && civ.stability > 75.0f && civ.knowledge > 100.0f) {
        civ.cd_renaissance = 150 + randi(rng, 0, 100);
        civ.stability += 10.0f;
        civ.knowledge += 15.0f;
        emit(LoreEventType::CulturalRenaissance, "RENAISSANCE");
    }

    // Alien biology discovery (rare, needs expansion)
    if (civ.knowledge > 80.0f && civ.territory.size() > 3 && rng() % 2000 == 0) {
        emit(LoreEventType::AlienBiology, "ALIEN_BIO");
    }

    // Sgr A* detection (one-time)
    if (!civ.discovered_sgra && civ.knowledge > 250.0f) {
        civ.discovered_sgra = true;
        civ.sgra_awareness += 20.0f;
        emit(LoreEventType::SgrADetection, "SGRA_DETECT");
    }

    // Convergence discovery (one-time, after sgra detection)
    if (!civ.discovered_convergence && civ.sgra_awareness > 50.0f) {
        civ.discovered_convergence = true;
        civ.sgra_awareness += 30.0f;
        emit(LoreEventType::ConvergenceDiscovery, "CONVERGENCE");
    }

    // Predecessor ruin discovery (one-time, if there are ruins in territory)
    if (!civ.found_predecessor_ruins) {
        for (uint32_t sid : civ.territory) {
            for (const auto& s : systems) {
                if (s.id == sid && !s.ruin_layers.empty()) {
                    civ.found_predecessor_ruins = true;
                    civ.knowledge += 30.0f;
                    emit(LoreEventType::RuinDiscovery, "RUINS", sid);
                    // Follow up with reverse engineering
                    civ.knowledge += 20.0f;
                    emit(LoreEventType::ReverseEngineering, "REVERSE_ENG", sid);
                    break;
                }
            }
            if (civ.found_predecessor_ruins) break;
        }
    }

    // Artifact creation (knowledge-gated, random)
    if (civ.cd_artifact == 0 && civ.knowledge > 200.0f && rng() % 400 == 0) {
        civ.cd_artifact = 200 + randi(rng, 0, 100);
        emit(LoreEventType::ArtifactCreation, "ARTIFACT");
    }

    // Orbital construction (military + resources)
    if (civ.military > 50.0f && civ.resources > 100.0f && rng() % 300 == 0) {
        civ.military += 20.0f;
        civ.resources -= 50.0f;
        std::vector<uint32_t> owned(civ.territory.begin(), civ.territory.end());
        uint32_t loc = owned.empty() ? civ.homeworld : owned[rng() % owned.size()];
        emit(LoreEventType::OrbitalConstruction, "ORBITAL", loc);
    }

    // ── 4. Crisis events ──

    // Internal war (low stability)
    if (civ.cd_conflict == 0 && civ.stability < 20.0f) {
        civ.cd_conflict = 200 + randi(rng, 0, 100);
        civ.population *= 0.8f;
        civ.military *= 0.7f;
        civ.stability -= 10.0f;
        civ.resources *= 0.7f;
        emit(LoreEventType::CivilWar, "CIVIL_WAR");
    }

    // Faction schism (high faction tension)
    if (civ.faction_tension > 80.0f && civ.faction_count < 4) {
        civ.faction_count++;
        civ.faction_tension = 0.0f;
        civ.stability -= 15.0f;
        civ.military *= 0.8f; // forces split
        emit(LoreEventType::FactionSchism, "SCHISM");
    }

    // Resource war (internal, when resources are critically low)
    if (civ.cd_conflict == 0 && civ.resources < civ.population * 0.2f && civ.population > 50.0f) {
        civ.cd_conflict = 150;
        civ.population *= 0.85f;
        civ.stability -= 8.0f;
        // Redistributes some resources from conflict
        civ.resources += 30.0f;
        emit(LoreEventType::ResourceWar, "RESOURCE_WAR");
    }

    // Plague (random, worse when population is dense)
    if (civ.cd_plague == 0 && rng() % 1000 < static_cast<int>(civ.population / capacity * 5.0f)) {
        civ.cd_plague = 300 + randi(rng, 0, 200);
        float severity = randf(rng, 0.1f, 0.4f);
        civ.population *= (1.0f - severity);
        civ.stability -= severity * 30.0f;
        std::vector<uint32_t> owned(civ.territory.begin(), civ.territory.end());
        uint32_t loc = owned.empty() ? civ.homeworld : owned[rng() % owned.size()];
        for (auto& s : systems) {
            if (s.id == loc) s.plague_origin = true;
        }
        emit(LoreEventType::Plague, "PLAGUE", loc);
    }

    // Mining disaster (random)
    if (civ.cd_mining == 0 && civ.territory.size() > 2 && rng() % 500 == 0) {
        civ.cd_mining = 150;
        civ.resources -= 20.0f;
        civ.population -= 5.0f;
        emit(LoreEventType::MiningDisaster, "MINING_DISASTER");
    }

    // Weapon test (military + knowledge, scars a planet — rare)
    if (civ.military > 100.0f && civ.knowledge > 150.0f && rng() % 2000 == 0) {
        std::vector<uint32_t> owned(civ.territory.begin(), civ.territory.end());
        if (!owned.empty()) {
            uint32_t loc = owned[rng() % owned.size()];
            for (auto& s : systems) {
                if (s.id == loc) {
                    s.weapon_test_site = true;
                    s.scars.push_back({civ.id, LoreEventType::WeaponTestSite, "Weapon test scarred surface"});
                    break;
                }
            }
            emit(LoreEventType::WeaponTestSite, "WEAPON_TEST", loc);
        }
    }

    // Sacred site (high sgra_awareness + high stability)
    if (civ.sgra_awareness > 30.0f && civ.stability > 60.0f && rng() % 500 == 0) {
        emit(LoreEventType::SacredSite, "SACRED");
    }

    // ── 5. Sgr A* endgame ──
    if (civ.sgra_awareness > 85.0f) {
        if (civ.traits.spirituality > 15 || rng() % 50 == 0) {
            // Transcendence attempt
            civ.transcended = true;
            civ.alive = false;
            // Release all territory
            for (uint32_t sid : civ.territory) {
                for (auto& s : systems) {
                    if (s.id == sid) {
                        s.owner_civ = -1;
                        s.ruin_layers.push_back(civ.id);
                    }
                }
            }
            emit(LoreEventType::Transcendence, "TRANSCEND");
            // Beacon at homeworld
            for (auto& s : systems) {
                if (s.id == civ.homeworld) { s.beacon = true; break; }
            }
            return;
        }
    }

    // ── 6. Collapse check ──
    if (civ.population < 3.0f) {
        civ.alive = false;
        // Release territory, leave ruins
        for (uint32_t sid : civ.territory) {
            for (auto& s : systems) {
                if (s.id == sid) {
                    s.owner_civ = -1;
                    s.ruin_layers.push_back(civ.id);
                }
            }
        }
        emit(LoreEventType::CollapseUnknown, "COLLAPSE");
        // Legacy events
        if (rng() % 2 == 0) emit(LoreEventType::VaultSealed, "VAULT");
        if (rng() % 3 == 0) emit(LoreEventType::GuardianCreated, "GUARDIAN");
    }
}

// ── Inter-civilization interactions ────────────────────────────────────────

void GalaxySim::check_interactions(std::mt19937& rng,
                                    std::vector<CivState>& civs,
                                    std::vector<SimSystem>& systems,
                                    std::vector<SimEvent>& events,
                                    int tick) {
    for (size_t i = 0; i < civs.size(); ++i) {
        if (!civs[i].alive) continue;
        for (size_t j = i + 1; j < civs.size(); ++j) {
            if (!civs[j].alive) continue;
            if (!territories_adjacent(civs[i], civs[j], systems)) continue;

            auto& a = civs[i];
            auto& b = civs[j];

            auto emit = [&](int cid, LoreEventType type, const std::string& desc,
                           uint32_t sys_id = 0, int other = -1) {
                SimEvent ev;
                ev.civ_id = cid;
                ev.tick = tick;
                ev.type = type;
                ev.description = desc;
                ev.system_id = sys_id;
                ev.other_civ_id = other;
                ev.pop_at_event = (cid == a.id) ? a.population : b.population;
                ev.res_at_event = (cid == a.id) ? a.resources : b.resources;
                ev.knowledge_at_event = (cid == a.id) ? a.knowledge : b.knowledge;
                ev.military_at_event = (cid == a.id) ? a.military : b.military;
                events.push_back(ev);
            };

            // First contact (one-time between any pair)
            // Using a simple tick-based check: first time they're adjacent
            bool first_contact = rng() % 100 < 5; // simplification
            if (first_contact) {
                emit(a.id, LoreEventType::FirstContact, "FIRST_CONTACT", 0, b.id);
                emit(b.id, LoreEventType::FirstContact, "FIRST_CONTACT", 0, a.id);
            }

            // Interaction based on philosophies
            bool a_aggressive = a.traits.aggression > 15;
            bool b_aggressive = b.traits.aggression > 15;
            bool a_peaceful = a.traits.diplomacy > 12 && a.traits.aggression < 10;
            bool b_peaceful = b.traits.diplomacy > 12 && b.traits.aggression < 10;

            if (a_peaceful && b_peaceful && rng() % 200 == 0) {
                // Trade alliance
                a.resources += 10.0f;
                b.resources += 10.0f;
                a.knowledge += 5.0f;
                b.knowledge += 5.0f;
                emit(a.id, LoreEventType::TradeRoute, "TRADE_ALLIANCE", 0, b.id);
                emit(b.id, LoreEventType::TradeRoute, "TRADE_ALLIANCE", 0, a.id);
            }
            else if (a_aggressive && a.military > b.military * 1.5f && rng() % 100 < 10) {
                // A conquers B's border system
                for (uint32_t sid : b.territory) {
                    for (const auto& s : systems) {
                        if (s.id == sid) {
                            // Check if near A's territory
                            bool near_a = false;
                            for (uint32_t sa : a.territory) {
                                for (const auto& sa_sys : systems) {
                                    if (sa_sys.id == sa && distance_sq(s, sa_sys) < 900.0f) {
                                        near_a = true; break;
                                    }
                                }
                                if (near_a) break;
                            }
                            if (near_a) {
                                b.territory.erase(sid);
                                a.territory.insert(sid);
                                for (auto& sys : systems) {
                                    if (sys.id == sid) {
                                        sys.owner_civ = a.id;
                                        sys.battle_site = true;
                                        sys.scars.push_back({a.id, LoreEventType::SystemBattle,
                                            "Conquered in war"});
                                    }
                                }
                                a.military *= 0.9f;
                                b.military *= 0.7f;
                                b.population *= 0.9f;
                                b.stability -= 10.0f;
                                emit(a.id, LoreEventType::SystemBattle, "CONQUEST", sid, b.id);
                                emit(b.id, LoreEventType::SystemBattle, "DEFEAT", sid, a.id);
                                goto done_interaction;
                            }
                        }
                    }
                }
            }
            else if (a_aggressive && b_aggressive && rng() % 150 < 5) {
                // Border skirmish — both lose
                a.military *= 0.9f;
                b.military *= 0.9f;
                a.stability -= 3.0f;
                b.stability -= 3.0f;
                emit(a.id, LoreEventType::BorderConflict, "BORDER_CLASH", 0, b.id);
                emit(b.id, LoreEventType::BorderConflict, "BORDER_CLASH", 0, a.id);
            }
            else if ((a.philosophy == Philosophy::Transcendent || b.philosophy == Philosophy::Transcendent)
                     && rng() % 300 == 0) {
                // Knowledge sharing
                float shared = std::min(a.knowledge, b.knowledge) * 0.1f;
                a.knowledge += shared;
                b.knowledge += shared;
                emit(a.id, LoreEventType::ScientificBreakthrough, "KNOWLEDGE_SHARE", 0, b.id);
                emit(b.id, LoreEventType::ScientificBreakthrough, "KNOWLEDGE_SHARE", 0, a.id);
            }
            else if (b_aggressive && b.military > a.military * 1.5f && rng() % 100 < 10) {
                // B conquers A (mirror of above)
                for (uint32_t sid : a.territory) {
                    bool near_b = false;
                    for (const auto& s : systems) {
                        if (s.id == sid) {
                            for (uint32_t sb : b.territory) {
                                for (const auto& sb_sys : systems) {
                                    if (sb_sys.id == sb && distance_sq(s, sb_sys) < 900.0f) {
                                        near_b = true; break;
                                    }
                                }
                                if (near_b) break;
                            }
                            if (near_b) {
                                a.territory.erase(sid);
                                b.territory.insert(sid);
                                for (auto& sys : systems) {
                                    if (sys.id == sid) {
                                        sys.owner_civ = b.id;
                                        sys.battle_site = true;
                                    }
                                }
                                b.military *= 0.9f;
                                a.military *= 0.7f;
                                a.population *= 0.9f;
                                a.stability -= 10.0f;
                                emit(b.id, LoreEventType::SystemBattle, "CONQUEST", sid, a.id);
                                emit(a.id, LoreEventType::SystemBattle, "DEFEAT", sid, b.id);
                                goto done_interaction;
                            }
                        }
                    }
                }
            }
            done_interaction:;
        }
    }
}

// ── Main simulation loop ──────────────────────────────────────────────────

WorldLore GalaxySim::run(unsigned game_seed) {
    std::mt19937 rng(game_seed ^ 0x53494D55u); // "SIMU"

    auto systems = generate_systems(rng);
    std::vector<CivState> civs;
    std::vector<SimEvent> events;

    // Timeline: 8000 ticks = 8 billion years at 1M years/tick
    int total_ticks = 8000;

    // Schedule civilization emergences
    int civ_count = randi(rng, 8, 15);
    std::vector<int> emergence_ticks;
    emergence_ticks.push_back(randi(rng, 0, 200)); // first civ emerges early
    for (int i = 1; i < civ_count; ++i) {
        int last = emergence_ticks.back();
        emergence_ticks.push_back(last + randi(rng, 200, 800));
    }
    // Ensure all emerge before tick 7500 (leave room for human epoch)
    for (auto& t : emergence_ticks) {
        if (t > 7500) t = randi(rng, 5000, 7500);
    }

    // Philosophy assignments
    Philosophy philosophies[] = {
        Philosophy::Expansionist, Philosophy::Contemplative, Philosophy::Predatory,
        Philosophy::Symbiotic, Philosophy::Transcendent
    };

    // ── Simulation loop ──
    int next_civ = 0;
    for (int tick = 0; tick < total_ticks; ++tick) {
        // Spawn new civilizations at their scheduled tick
        while (next_civ < civ_count &&
               next_civ < static_cast<int>(emergence_ticks.size()) &&
               emergence_ticks[next_civ] <= tick) {
            Philosophy phil = philosophies[rng() % 5];
            auto civ = spawn_civilization(rng, next_civ, systems, phil);
            if (civ.alive) {
                SimEvent ev;
                ev.civ_id = next_civ;
                ev.tick = tick;
                ev.type = LoreEventType::Emergence;
                ev.description = "EMERGENCE";
                ev.system_id = civ.homeworld;
                events.push_back(ev);
            }
            civs.push_back(std::move(civ));
            ++next_civ;
        }

        // Update each active civilization
        for (auto& civ : civs) {
            tick_civilization(rng, civ, systems, events, tick, civs);
        }

        // Check inter-civilization interactions
        if (tick % 10 == 0) { // every 10M years
            check_interactions(rng, civs, systems, events, tick);
        }
    }

    // Build WorldLore from simulation results
    return build_lore(game_seed, rng, civs, events, systems);
}

// ── Convert simulation results into WorldLore ─────────────────────────────

WorldLore GalaxySim::build_lore(unsigned seed, std::mt19937& rng,
                                 const std::vector<CivState>& civs,
                                 const std::vector<SimEvent>& events,
                                 const std::vector<SimSystem>& systems) {
    WorldLore lore;
    lore.seed = seed;

    // Create a Civilization record for each simulated civ
    for (const auto& cs : civs) {
        Civilization civ;
        civ.philosophy = cs.philosophy;
        civ.architecture = static_cast<Architecture>(rng() % 5);
        civ.tech_style = static_cast<TechStyle>(rng() % 5);
        civ.homeworld_system_id = cs.homeworld;

        // Store trait summary for display
        {
            const auto& tr = cs.traits;
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "AGG:%d CUR:%d IND:%d COH:%d SPI:%d ADP:%d DIP:%d CRE:%d TEC:%d",
                tr.aggression, tr.curiosity, tr.industriousness, tr.cohesion,
                tr.spirituality, tr.adaptability, tr.diplomacy, tr.creativity, tr.technology);
            civ.trait_summary = buf;
        }

        // Determine collapse cause from how they died
        if (cs.transcended) {
            civ.collapse_cause = CollapseCause::Transcendence;
            civ.sgra_relation = SgrARelation::ReachedAndVanished;
        } else if (!cs.alive) {
            // Determine from events
            bool had_war = false, had_plague = false;
            for (const auto& ev : events) {
                if (ev.civ_id == cs.id) {
                    if (ev.type == LoreEventType::CivilWar || ev.type == LoreEventType::SystemBattle) had_war = true;
                    if (ev.type == LoreEventType::Plague) had_plague = true;
                }
            }
            if (had_war && !had_plague) civ.collapse_cause = CollapseCause::War;
            else if (had_plague) civ.collapse_cause = CollapseCause::Plague;
            else if (cs.resources < 10.0f) civ.collapse_cause = CollapseCause::ResourceExhaustion;
            else if (cs.sgra_awareness > 60.0f) civ.collapse_cause = CollapseCause::SgrAObsession;
            else civ.collapse_cause = CollapseCause::Unknown;

            if (cs.sgra_awareness > 60.0f) civ.sgra_relation = SgrARelation::BuiltToward;
            else if (cs.discovered_sgra) civ.sgra_relation = SgrARelation::TriedAndFailed;
            else civ.sgra_relation = SgrARelation::Unaware;
        } else {
            civ.collapse_cause = CollapseCause::Unknown;
            civ.sgra_relation = SgrARelation::Unaware;
        }

        // Predecessor relation (based on whether they found ruins)
        if (cs.id == 0) {
            civ.predecessor_relation = PredecessorRelation::Ignored;
        } else {
            civ.predecessor_relation = static_cast<PredecessorRelation>(rng() % 4);
        }

        // Find epoch timing from events
        float start_bya = 0, end_bya = 0;
        for (const auto& ev : events) {
            if (ev.civ_id == cs.id) {
                float t = static_cast<float>(8000 - ev.tick) / 1000.0f; // convert tick to Bya
                if (start_bya == 0 || t > start_bya) start_bya = t;
                if (end_bya == 0 || t < end_bya) end_bya = t;
            }
        }
        civ.epoch_start_bya = start_bya;
        civ.epoch_end_bya = end_bya;

        // Assign phoneme pool (cycle through to avoid repeats)
        civ.phoneme_pool = static_cast<PhonemePool>(cs.id % phoneme_pool_count);

        // Generate names
        NameGenerator namer(civ.phoneme_pool);
        civ.name = namer.civilization(rng);
        // Extract short name
        {
            auto first_space = civ.name.find(' ');
            if (first_space != std::string::npos) {
                auto second_space = civ.name.find(' ', first_space + 1);
                if (second_space != std::string::npos)
                    civ.short_name = civ.name.substr(first_space + 1, second_space - first_space - 1);
                else
                    civ.short_name = civ.name.substr(first_space + 1);
            } else {
                civ.short_name = civ.name;
            }
        }

        // Convert sim events to lore events with rich descriptions
        for (const auto& ev : events) {
            if (ev.civ_id != cs.id) continue;
            float time_bya = static_cast<float>(8000 - ev.tick) / 1000.0f;

            // Generate rich description from sim event
            std::string desc = generate_event_description(rng, ev, cs, civs, namer);

            civ.events.push_back({ev.type, time_bya, desc, ev.system_id});
        }

        // Generate figures from notable events
        LoreGenerator::generate_figures(rng, civ, namer);
        LoreGenerator::generate_artifacts(rng, civ, namer);
        NarrativeGenerator::generate(rng, civ, namer, lore.civilizations);

        lore.civilizations.push_back(std::move(civ));
    }

    // Human epoch
    lore.humanity = LoreGenerator::generate_human_epoch(rng, lore.civilizations);

    // Race origins
    LoreGenerator::generate_race_origins(rng, lore);

    // Count beacons
    lore.total_beacons = 0;
    lore.active_beacons = 0;
    for (const auto& s : systems) {
        if (s.beacon) {
            lore.total_beacons++;
            if (rng() % 3 != 0) lore.active_beacons++;
        }
        if (s.has_megastructure) lore.total_beacons++;
    }

    lore.generated = true;
    return lore;
}

// ── Rich event description generation ─────────────────────────────────────

std::string GalaxySim::generate_event_description(
    std::mt19937& rng, const SimEvent& event, const CivState& civ,
    const std::vector<CivState>& all_civs, const NameGenerator& namer) {

    std::string place = namer.place(rng);
    std::string civ_name = ""; // will be set from lore later, use place as proxy
    int pop = static_cast<int>(event.pop_at_event);
    int mil = static_cast<int>(event.military_at_event);
    std::string other_name = "";
    if (event.other_civ_id >= 0 && event.other_civ_id < static_cast<int>(all_civs.size())) {
        // Use a placeholder — real names assigned in build_lore
        other_name = "a neighboring species";
    }

    auto pick = [&](int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); };

    switch (event.type) {
    case LoreEventType::Emergence:
        switch (pick(3)) {
            case 0: return "Consciousness stirs on " + place + " — a new species awakens";
            case 1: return "The first sparks of intelligence ignite on " + place;
            default: return "Life on " + place + " achieves sentience and begins to question the stars";
        }
    case LoreEventType::ColonyFounded:
        switch (pick(3)) {
            case 0: return "Colony established on " + place + " — population " + std::to_string(pop * 100) + " thousand";
            case 1: return "Generation ships arrive at " + place + " after centuries in the void";
            default: return place + " claimed and colonized — its rich resources fuel further expansion";
        }
    case LoreEventType::Terraforming:
        switch (pick(3)) {
            case 0: return place + " terraformed into a garden world — oceans bloom on barren rock";
            case 1: return "Centuries-long terraforming transforms " + place + " into a habitable paradise";
            default: return "Atmosphere of " + place + " reshaped — first children born under open sky";
        }
    case LoreEventType::ScientificBreakthrough:
        if (event.description == "KNOWLEDGE_SHARE") {
            return "Joint research with " + other_name + " yields breakthrough at " + place;
        }
        switch (pick(4)) {
            case 0: return "Researchers on " + place + " achieve controlled singularity containment";
            case 1: return "Physicists on " + place + " prove faster-than-light communication possible";
            case 2: return place + " Observatory detects structured signals from deep within Sgr A*";
            default: return "Fundamental breakthrough at " + place + " research station reshapes physics";
        }
    case LoreEventType::ArtifactCreation:
        if (event.description == "WEAPON_BREAKTHROUGH") {
            switch (pick(3)) {
                case 0: return "Weapon engineers on " + place + " achieve a generational leap — a devastating new armament";
                case 1: return "The " + place + " weapons program produces a weapon of terrible power";
                default: return "Military breakthrough: new weapon class renders all prior armaments obsolete";
            }
        }
        switch (pick(3)) {
            case 0: return "Master artificers forge a legendary artifact at " + place;
            case 1: return "A navigation device that senses the beacon network across light-years is completed";
            default: return "An artifact of unknown purpose is constructed — it hums when pointed at the core";
        }
    case LoreEventType::MegastructureBuilt:
        switch (pick(4)) {
            case 0: return "A void-gate large enough to swallow moons constructed at " + place;
            case 1: return "Orbital ring encircles " + place + " — the largest structure ever built";
            case 2: return "Beacon array spanning three systems begins broadcasting toward the core from " + place;
            default: return "Dyson lattice construction begins around " + place + "'s star";
        }
    case LoreEventType::CulturalRenaissance:
        switch (pick(3)) {
            case 0: return place + " becomes the cultural heart — artists, philosophers, dreamers gather";
            case 1: return "The " + place + " Renaissance — a flowering of art, science, and exploration";
            default: return "New philosophical movement on " + place + " redefines the species' relationship with the void";
        }
    case LoreEventType::CivilWar:
        switch (pick(3)) {
            case 0: return "Outer colonies declare independence — civil war erupts (pop: " + std::to_string(pop * 100) + "k)";
            case 1: return "Ideological schism splits the civilization into hostile factions";
            default: return "Dispute over precursor artifacts escalates into open warfare at " + place;
        }
    case LoreEventType::FactionSchism:
        switch (pick(3)) {
            case 0: return "The " + place + " Secession — an entire sector declares independence";
            case 1: return "Rival factions establish competing governments — a divided species";
            default: return "Religious movement on " + place + " rejects orthodoxy, founding a breakaway sect";
        }
    case LoreEventType::ResourceWar:
        switch (pick(3)) {
            case 0: return "Rare element deposits trigger internal conflict at " + place;
            case 1: return "Control of hyperspace fuel becomes the defining struggle of the era";
            default: return "Wars over precursor technology caches devastate " + place + " system";
        }
    case LoreEventType::SystemBattle:
        if (event.description == "CONQUEST") {
            return "Battle of " + place + " — system conquered from " + other_name + " (military: " + std::to_string(mil) + ")";
        } else {
            return "Defeat at " + place + " — system lost to " + other_name;
        }
    case LoreEventType::BorderConflict:
        return "Border skirmish with " + other_name + " near " + place;
    case LoreEventType::FirstContact:
        return "First contact with " + other_name + " near " + place + " — cautious exchanges begin";
    case LoreEventType::TradeRoute:
        if (event.other_civ_id >= 0) {
            return "Trade alliance formed with " + other_name + " — technology and resources flow";
        }
        return "Merchant guilds establish the " + place + " Exchange";
    case LoreEventType::HyperspaceRoute:
        return "Hyperspace conduit links " + place + " to the network";
    case LoreEventType::Plague:
        switch (pick(3)) {
            case 0: return "Pathogen from " + place + "'s biosphere devastates neighboring colonies";
            case 1: return "Quarantine at " + place + " — millions perish before cure found";
            default: return "Biological contamination from breached vault on " + place + " triggers pandemic";
        }
    case LoreEventType::MiningDisaster:
        switch (pick(2)) {
            case 0: return "Deep core mining on " + place + " triggers catastrophic collapse";
            default: return "Excavation on " + place + " breaches a precursor containment chamber";
        }
    case LoreEventType::WeaponTestSite:
        return "Devastating weapon tested on " + place + "'s far side — crater visible from orbit";
    case LoreEventType::SacredSite:
        return place + " declared sacred ground after mystic experiences convergence vision";
    case LoreEventType::AlienBiology:
        return "Expedition to " + place + " discovers alien organisms — silicon-based, possibly sentient";
    case LoreEventType::SgrADetection:
        switch (pick(3)) {
            case 0: return "Deep-space sensors detect gravitational waves from galactic center";
            case 1: return "Precursor beacon fragment reveals coordinates pointing to Sgr A*";
            default: return "Astronomers observe impossible light patterns from Sgr A*";
        }
    case LoreEventType::ConvergenceDiscovery:
        return "The mathematical proof is undeniable: all paths lead to the center";
    case LoreEventType::RuinDiscovery:
        return "Ancient ruins discovered on " + place + " — impossibly old structures from a dead species";
    case LoreEventType::ReverseEngineering:
        return "Precursor technology reverse-engineered — a leap of centuries in a single discovery";
    case LoreEventType::OrbitalConstruction:
        switch (pick(2)) {
            case 0: return "Orbital shipyards above " + place + " begin fleet construction";
            default: return "Defense platforms deployed in orbit of " + place;
        }
    case LoreEventType::Transcendence:
        return "In a single galactic heartbeat, the species abandons physical form and streams toward Sgr A*";
    case LoreEventType::CollapseUnknown:
        return "Civilization vanishes. No records survive. Only ruins remain.";
    case LoreEventType::VaultSealed:
        return "Knowledge vaults sealed across " + std::to_string(2 + rng() % 8) + " systems — a message to whoever comes next";
    case LoreEventType::GuardianCreated:
        return "Autonomous guardians activated — single directive: protect the path to the center";
    default:
        return "Event at " + place;
    }
}

} // namespace astra
