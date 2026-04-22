#include "astra/dungeon/conclave_archive.h"

namespace astra {

std::vector<DungeonLevelSpec> build_conclave_archive_levels() {
    std::vector<DungeonLevelSpec> out;
    out.reserve(3);

    // L1 — Outer Ruin (Conclave-held, battle-scarred).
    {
        DungeonLevelSpec l1;
        l1.style_id    = dungeon::StyleId::PrecursorRuin;
        l1.civ_name    = "Precursor";
        l1.decay_level = 3;
        l1.enemy_tier  = 1;
        l1.overlays    = { dungeon::OverlayKind::BattleScarred };
        l1.npc_roles   = {
            "Conclave Sentry", "Conclave Sentry", "Conclave Sentry",
            "Conclave Sentry", "Conclave Sentry", "Conclave Sentry",
            "Conclave Sentry", "Conclave Sentry",
            "Heavy Conclave Sentry", "Heavy Conclave Sentry",
            "Conclave Sentry Drone", "Conclave Sentry Drone",
            "Conclave Sentry Drone", "Conclave Sentry Drone",
        };
        out.push_back(std::move(l1));
    }

    // L2 — Inner Sanctum (reasserted Precursor defenders; fighting-stopped-here).
    {
        DungeonLevelSpec l2;
        l2.style_id    = dungeon::StyleId::PrecursorRuin;
        l2.civ_name    = "Precursor";
        l2.decay_level = 2;
        l2.enemy_tier  = 2;
        l2.overlays    = { dungeon::OverlayKind::BattleScarred };
        l2.npc_roles   = {
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Archon Remnant",
            "Archon Automaton", "Archon Automaton",
            "Archon Sentry Drone", "Archon Sentry Drone",
            "Archon Sentry Drone", "Archon Sentry Drone",
            "Archon Sentry Drone",
        };
        out.push_back(std::move(l2));
    }

    // L3 — Crystal Vault (pristine; boss + nova resonance crystal on plinth).
    {
        DungeonLevelSpec l3;
        l3.style_id      = dungeon::StyleId::PrecursorRuin;
        l3.civ_name      = "Precursor";
        l3.decay_level   = 0;
        l3.enemy_tier    = 3;
        l3.is_boss_level = true;
        l3.overlays      = {};   // pristine
        l3.fixtures      = {
            PlannedFixture{ "nova_resonance_crystal", "required_plinth" },
        };
        l3.npc_roles     = {
            "Archon Automaton", "Archon Automaton", "Archon Automaton",
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Archon Remnant",
            "Archon Sentry Drone", "Archon Sentry Drone",
            "Archon Sentry Drone",
            "Archon Sentinel",
        };
        out.push_back(std::move(l3));
    }

    return out;
}

} // namespace astra
