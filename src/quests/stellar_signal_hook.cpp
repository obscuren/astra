#include "astra/quest.h"
#include "astra/game.h"
#include "astra/star_chart.h"
#include "astra/body_presets.h"
#include "astra/faction.h"
#include "astra/world_manager.h"

namespace astra {

static const char* QUEST_ID_HOOK = "story_stellar_signal_hook";

class StellarSignalHookQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_HOOK;
        q.title = "Static in the Dark";
        q.description =
            "Nova, the Stellar Engineer on The Heavens Above, is hearing "
            "something in the galactic background — a modulated signal that "
            "shouldn't exist. She can't leave the station to investigate, "
            "and she says the signal is calling her by name.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives.push_back({
            ObjectiveType::TalkToNpc,
            "Hear Nova out at the Observatory",
            1, 0,
            "Stellar Engineer",
        });
        q.reward.xp = 50;
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_getting_airborne"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode offer_mode() const override       { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Stellar Engineer"; }

    void on_accepted(Game& game) override {
        auto& nav = game.world().navigation();
        auto& rng = game.world().rng();

        // Echo 1 — The Fire-Worn (red dwarf + scar planet)
        auto c1 = pick_coords_near(nav, nav.current_system_id, 4.0f, 9.0f, rng);
        if (!c1) return;
        uint32_t echo1_id = add_custom_system(nav, {
            .name = "The Fire-Worn",
            .gx = c1->first, .gy = c1->second,
            .star_class = StarClass::ClassM,
            .discovered = true,
            .bodies = { make_scar_planet("The Fire-Worn Prime",
                                         Biome::ScarredGlassed) },
        });

        // Echo 2 — The Quiet Shell (derelict station, no bodies)
        auto c2 = pick_coords_near(nav, nav.current_system_id, 6.0f, 12.0f, rng);
        if (!c2) return;
        CustomSystemSpec spec2;
        spec2.name = "The Quiet Shell";
        spec2.gx = c2->first; spec2.gy = c2->second;
        spec2.star_class = StarClass::ClassG;
        spec2.discovered = true;
        spec2.has_station = true;
        spec2.station.type = StationType::Abandoned;
        spec2.station.specialty = StationSpecialty::Generic;
        spec2.station.name = "The Quiet Shell";
        uint32_t echo2_id = add_custom_system(nav, std::move(spec2));

        // Echo 3 — The Edge (neutron + crystalline asteroid)
        auto c3 = pick_coords_near(nav, nav.current_system_id, 10.0f, 18.0f, rng);
        if (!c3) return;
        uint32_t echo3_id = add_custom_system(nav, {
            .name = "The Edge",
            .gx = c3->first, .gy = c3->second,
            .star_class = StarClass::Neutron,
            .discovered = true,
            .bodies = { make_landable_asteroid("Edge Crystal") },
        });

        game.world().stellar_signal_echo_ids() = {echo1_id, echo2_id, echo3_id};
    }
};

void register_stellar_signal_hook(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalHookQuest>());
}

} // namespace astra
