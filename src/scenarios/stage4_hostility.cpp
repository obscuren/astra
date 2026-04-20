#include "astra/scenarios.h"

#include "astra/event_bus.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/player.h"
#include "astra/scenario_effects.h"
#include "astra/star_chart.h"
#include "astra/world_manager.h"

#include <string>
#include <vector>

namespace astra {

namespace {
constexpr const char* kStage4Active = "stage4_active";
constexpr const char* kTransmissionSeen = "stage4_transmission_seen";

int ambush_count_for_level(int level) {
    if (level < 5) return 1;
    if (level < 10) return 2;
    return 3;
}
} // namespace

void register_stage4_hostility_scenario(Game& game) {
    game.event_bus().subscribe(EventKind::SystemEntered,
        [](Game& g, const Event& ev) {
            const auto& payload = std::get<SystemEnteredEvent>(ev);
            auto& world = g.world();

            if (!world.world_flag(kStage4Active)) return;
            if (world.ambushed_systems().count(payload.system_id)) return;

            // First warp after stage 3 completion fires the transmission.
            if (!world.world_flag(kTransmissionSeen)) {
                open_transmission(g,
                    "INCOMING TRANSMISSION - STELLARI CONCLAVE",
                    {
                        "Commander. You have interfered with a sacred cycle.",
                        "Cease immediately and return to Sgr A* trajectory.",
                        "",
                        "This is your only warning.",
                    });
                set_world_flag(g, kTransmissionSeen, true);
            }

            // Ambushes only in Conclave-controlled space.
            const auto& nav = world.navigation();
            const StarSystem* sys = nullptr;
            for (const auto& s : nav.systems) {
                if (s.id == payload.system_id) { sys = &s; break; }
            }
            if (!sys) return;
            if (sys->controlling_faction != Faction_StellariConclave) return;

            // Queue an ambush in the system's overworld / inbound location.
            int level = g.player().level;
            int count = ambush_count_for_level(level);
            std::vector<std::string> roles(count, "Conclave Sentry");

            // Body index 0 is the first body; we use the overworld key
            // (is_station=false) so the ambush meets the player on the
            // primary planet.
            inject_location_encounter(g,
                payload.system_id, 0, false,
                roles,
                "stage4_ambush");

            world.ambushed_systems().insert(payload.system_id);
        });
}

void register_all_scenarios(Game& game) {
    register_stage4_hostility_scenario(game);
}

} // namespace astra
