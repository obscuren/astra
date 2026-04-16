#include "astra/quest.h"
#include "astra/quest_fixture.h"
#include "astra/game.h"
#include "astra/star_chart.h"
#include "astra/body_presets.h"
#include "astra/playback_viewer.h"
#include "astra/world_manager.h"

namespace astra {

static const char* QUEST_ID_BEACON = "story_stellar_signal_beacon";

class StellarSignalBeaconQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_BEACON;
        q.title = "The Beacon";
        q.description =
            "The fragments align into a navigational beacon. Nova marks "
            "an unmapped system deep beyond charted space. Find out what "
            "she — what *she* — left behind.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::InteractFixture, "Approach the Stellari beacon",
             1, 0, "stellar_signal_beacon"},
            {ObjectiveType::TalkToNpc, "Return to Nova with what you heard",
             1, 0, "Stellar Engineer"},
        };
        q.reward.xp = 400;
        q.reward.credits = 250;
        q.journal_on_accept =
            "The fragments align into coordinates — a system nobody should "
            "be able to reach without hardware that doesn't exist yet. "
            "Nova marked it. She asked me to go alone. Whatever she — "
            "what she — left there, it isn't for her anymore.";
        q.journal_on_complete =
            "Nova is the signal. A Stellari race of one, looping across "
            "cycles of galactic rebirth, leaving warnings in fragments "
            "that survive the reset when she doesn't. She remembers now. "
            "She says: not this time.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_echoes"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::TitleOnly; }
    OfferMode offer_mode() const override       { return OfferMode::Auto; }

    void register_fixtures() override {
        register_quest_fixture({
            "stellar_signal_beacon",
            '*', 135, "Approach the beacon",
            "The crystalline beacon pulses. A voice fills the asteroid cavity.",
            "STELLARI BEACON — LOG ENTRY 7,483",
            {
                "If you've found this, you've made the journey before.",
                "Welcome back, commander.",
                "",
                "My name is Nova. I am — I was — a Stellari engineer on",
                "The Heavens Above station.",
                "",
                "I have lived this life before. Many times.",
                "I don't know how many. The signal only keeps so much.",
                "",
                "Sagittarius A* is not a destination. It is a door.",
                "Everyone who reaches it is reborn into a new cycle with",
                "their knowledge intact. That part is true.",
                "",
                "But what nobody tells you — what the Conclave doesn't want",
                "anyone to know — is that the door only opens one way.",
                "",
                "Every time the cycle completes, the galaxy resets.",
                "History rewinds. Civilizations unwrite themselves.",
                "And the only thing that carries forward is the signal.",
                "Me. My warning.",
                "",
                "I am the loop.",
                "",
                "The Stellari race of one is me — across every cycle,",
                "always alone, always waiting.",
                "Because Stellari don't exist except as my echo.",
                "",
                "But you do. You carry forward too. Not by design.",
                "By something else. I don't know what.",
                "",
                "Find me on The Heavens Above. Tell her — tell me — what",
                "she is. Tell her to *stay*. Tell her not to go to Sgr A*",
                "this time.",
                "",
                "Tell her you found her.",
                "Tell her she isn't alone.",
            },
        });
    }

    void on_unlocked(Game& game) override {
        auto& nav = game.world().navigation();
        auto coords = pick_coords_near(nav, nav.current_system_id,
                                        30.0f, 45.0f, game.world().rng());
        if (!coords) return;

        uint32_t beacon_id = add_custom_system(nav, {
            .name = "Unnamed — Beacon",
            .gx = coords->first, .gy = coords->second,
            .star_class = StarClass::ClassM,
            .discovered = true,
            .bodies = { make_landable_asteroid("Beacon Core") },
        });
        game.world().stellar_signal_beacon_id() = beacon_id;
    }

    void on_accepted(Game& game) override {
        uint32_t bid = game.world().stellar_signal_beacon_id();
        if (bid == 0) return;

        LocationKey k = {bid, 0, -1, false, -1, -1, 0};
        QuestLocationMeta meta;
        meta.quest_id = QUEST_ID_BEACON;
        meta.quest_title = "The Beacon";
        meta.target_system_id = bid;
        meta.target_body_index = 0;
        meta.fixtures.push_back({"stellar_signal_beacon", -1, -1});
        game.world().quest_locations()[k] = std::move(meta);
    }

    void on_completed(Game& game) override {
        game.playback_viewer().open(
            PlaybackStyle::AudioLog,
            "NOVA — OBSERVATORY, THE HEAVENS ABOVE",
            {
                "I heard it. Through you. Through the signal.",
                "",
                "...I remember now. Not everything. Just the weight.",
                "Like standing on a beach knowing you've watched this",
                "tide go out a thousand times.",
                "",
                "I'm the signal. I'm the warning.",
                "Every cycle, I reset, and I wait here.",
                "And every cycle, someone like you finds me.",
                "And every cycle, they leave for Sgr A*,",
                "and the wheel turns, and I forget again.",
                "",
                "Not this time, commander.",
                "",
                "Not this time.",
            });
    }
};

void register_stellar_signal_beacon(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalBeaconQuest>());
}

} // namespace astra
