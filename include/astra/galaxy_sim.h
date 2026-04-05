#pragma once

#include "astra/lore_types.h"
#include "astra/name_generator.h"

#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astra {

// ── Simulated civilization state ──

struct CivState {
    int id = 0;                     // index into WorldLore::civilizations
    bool alive = true;
    bool transcended = false;

    // Core state
    float population = 10.0f;       // abstract units (0-1000+)
    float resources = 50.0f;        // materials, fuel, food
    float knowledge = 5.0f;         // science/tech level
    float stability = 70.0f;        // internal cohesion (0-100)
    float military = 0.0f;          // defense/offense capability
    float sgra_awareness = 0.0f;    // convergence understanding (0-100)

    // Weapon tech level — breakthroughs give decisive advantages
    int weapon_tech = 0;            // 0-5, each level is a generational leap

    // Territory
    std::unordered_set<uint32_t> territory; // claimed system IDs
    uint32_t homeworld = 0;

    // Faction tracking (for internal wars)
    float faction_tension = 0.0f;   // builds up, triggers schisms at threshold
    int faction_count = 1;          // 1 = unified, 2+ = internal factions

    // Cooldowns (ticks until event can fire again)
    int cd_breakthrough = 0;
    int cd_expansion = 0;
    int cd_conflict = 0;
    int cd_renaissance = 0;
    int cd_megastructure = 0;
    int cd_plague = 0;
    int cd_mining = 0;
    int cd_artifact = 0;
    int cd_weapon = 0;

    // One-time flags
    bool discovered_sgra = false;
    bool discovered_convergence = false;
    bool found_predecessor_ruins = false;

    // Philosophy (copied from Civilization for easy access)
    Philosophy philosophy = Philosophy::Expansionist;
};

// ── Sim event (structured data, turned into LoreEvent + narrative later) ──

struct SimEvent {
    int civ_id;                     // which civilization
    int tick;                       // when (in sim ticks)
    LoreEventType type;
    std::string description;        // generated from state context
    uint32_t system_id = 0;         // where it happened
    int other_civ_id = -1;          // for inter-civ events
    // State snapshot at time of event (for narrative context)
    float pop_at_event = 0;
    float res_at_event = 0;
    float knowledge_at_event = 0;
    float military_at_event = 0;
};

// ── System info for the simulation ──

// Planet-scale scars left by simulation events
struct PlanetScar {
    int civ_id;                     // who caused it
    LoreEventType cause;            // what happened
    std::string description;        // brief description for world gen
};

struct SimSystem {
    uint32_t id;
    float gx, gy;                   // galactic coordinates
    float resource_richness;         // 0.0-1.0, how much resources it provides
    int owner_civ = -1;             // -1 = unclaimed
    std::vector<int> ruin_layers;   // civ IDs of dead civs that once owned this
    std::vector<PlanetScar> scars;  // physical marks left on this system
    bool terraformed = false;       // was it terraformed by a civ?
    int terraformed_by = -1;        // which civ terraformed it
    bool has_megastructure = false;  // orbital megastructure present
    int megastructure_builder = -1;
    bool beacon = false;            // Sgr A* beacon placed here
    bool battle_site = false;       // major battle fought here
    bool weapon_test_site = false;  // weapon tested here (scarred surface)
    bool plague_origin = false;     // plague originated here
    int lore_tier = 0;              // 0-3, computed after simulation
};

// ── The galaxy simulation ──

class GalaxySim {
public:
    // Run the full simulation and produce a WorldLore.
    static WorldLore run(unsigned game_seed);

private:
    // Simulation phases
    static std::vector<SimSystem> generate_systems(std::mt19937& rng);
    static CivState spawn_civilization(std::mt19937& rng, int civ_id,
                                        std::vector<SimSystem>& systems,
                                        Philosophy philosophy);

    static void tick_civilization(std::mt19937& rng, CivState& civ,
                                  std::vector<SimSystem>& systems,
                                  std::vector<SimEvent>& events,
                                  int tick,
                                  const std::vector<CivState>& all_civs);

    static void check_interactions(std::mt19937& rng,
                                    std::vector<CivState>& civs,
                                    std::vector<SimSystem>& systems,
                                    std::vector<SimEvent>& events,
                                    int tick);

    // Post-simulation: convert SimEvents + CivStates into WorldLore
    static WorldLore build_lore(unsigned seed,
                                 std::mt19937& rng,
                                 const std::vector<CivState>& civs,
                                 const std::vector<SimEvent>& events,
                                 const std::vector<SimSystem>& systems);

    // Helpers
    static uint32_t find_nearest_unclaimed(const CivState& civ,
                                            const std::vector<SimSystem>& systems);
    static float distance_sq(const SimSystem& a, const SimSystem& b);
    static bool territories_adjacent(const CivState& a, const CivState& b,
                                      const std::vector<SimSystem>& systems,
                                      float range = 30.0f);
    static std::string generate_event_description(std::mt19937& rng,
                                                    const SimEvent& event,
                                                    const CivState& civ,
                                                    const std::vector<CivState>& all_civs,
                                                    const NameGenerator& namer);
};

} // namespace astra
