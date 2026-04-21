#include "astra/dungeon/conclave_archive.h"

namespace astra {

std::vector<DungeonLevelSpec> build_conclave_archive_levels() {
    std::vector<DungeonLevelSpec> out;
    out.reserve(3);

    // L1 — Outer Ruin. Conclave foothold, first Precursor drones waking.
    {
        DungeonLevelSpec l1;
        l1.civ_name    = "Precursor";
        l1.decay_level = 3;
        l1.enemy_tier  = 1;
        l1.npc_roles   = {
            "Conclave Sentry", "Conclave Sentry", "Conclave Sentry",
            "Conclave Sentry", "Conclave Sentry", "Conclave Sentry",
            "Conclave Sentry",
            "Sentry Drone", "Sentry Drone", "Sentry Drone",
            "Rust Hound",
        };
        out.push_back(std::move(l1));
    }

    // L2 — Inner Sanctum. Conclave presence thinning, Archon defenses
    // reasserting across the galleries.
    {
        DungeonLevelSpec l2;
        l2.civ_name    = "Precursor";
        l2.decay_level = 2;
        l2.enemy_tier  = 2;
        l2.npc_roles   = {
            "Heavy Conclave Sentry", "Heavy Conclave Sentry",
            "Heavy Conclave Sentry", "Heavy Conclave Sentry",
            "Heavy Conclave Sentry",
            "Sentry Drone", "Sentry Drone", "Sentry Drone",
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Rust Hound", "Rust Hound",
        };
        out.push_back(std::move(l2));
    }

    // L3 — Crystal Vault. Fully Precursor. Archon Sentinel boss with a
    // supporting detachment; the Conclave never held this level.
    {
        DungeonLevelSpec l3;
        l3.civ_name      = "Precursor";
        l3.decay_level   = 0;    // pristine
        l3.enemy_tier    = 3;
        l3.is_boss_level = true;
        l3.npc_roles     = {
            "Archon Automaton", "Archon Automaton",
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Sentry Drone", "Sentry Drone",
            "Archon Sentinel",
        };
        l3.fixtures      = {
            PlannedFixture{ "nova_resonance_crystal", "back_chamber" },
        };
        out.push_back(std::move(l3));
    }

    return out;
}

} // namespace astra
