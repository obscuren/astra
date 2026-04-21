#include "astra/dungeon/conclave_archive.h"

namespace astra {

std::vector<DungeonLevelSpec> build_conclave_archive_levels() {
    std::vector<DungeonLevelSpec> out;
    out.reserve(3);

    // L1 — Outer Ruin. Conclave foothold.
    {
        DungeonLevelSpec l1;
        l1.civ_name    = "Precursor";
        l1.decay_level = 3;
        l1.enemy_tier  = 1;
        l1.npc_roles   = {"Conclave Sentry", "Conclave Sentry",
                          "Conclave Sentry", "Conclave Sentry"};
        out.push_back(std::move(l1));
    }

    // L2 — Inner Sanctum. Precursor defenses waking.
    {
        DungeonLevelSpec l2;
        l2.civ_name    = "Precursor";
        l2.decay_level = 2;
        l2.enemy_tier  = 2;
        l2.npc_roles   = {"Heavy Conclave Sentry", "Heavy Conclave Sentry",
                          "Heavy Conclave Sentry",
                          "Archon Remnant", "Archon Remnant"};
        out.push_back(std::move(l2));
    }

    // L3 — Crystal Vault. Archon Sentinel boss + Nova's crystal.
    {
        DungeonLevelSpec l3;
        l3.civ_name      = "Precursor";
        l3.decay_level   = 0;    // pristine
        l3.enemy_tier    = 3;
        l3.is_boss_level = true;
        l3.npc_roles     = {"Heavy Conclave Sentry", "Heavy Conclave Sentry",
                            "Archon Sentinel"};
        l3.fixtures      = {
            PlannedFixture{ "nova_resonance_crystal", "back_chamber" },
        };
        out.push_back(std::move(l3));
    }

    return out;
}

} // namespace astra
