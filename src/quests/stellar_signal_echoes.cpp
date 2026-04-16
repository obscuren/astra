#include "astra/quest.h"
#include "astra/quest_fixture.h"
#include "astra/game.h"
#include "astra/world_manager.h"
#include "astra/playback_viewer.h"
#include "astra/faction.h"

namespace astra {

static const char* QUEST_ID_ECHOES = "story_stellar_signal_echoes";

class StellarSignalEchoesQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_ECHOES;
        q.title = "Three Echoes";
        q.description =
            "Nova has triangulated three systems where the signal is "
            "strongest. Plant a receiver drone at each Signal Node and "
            "bring the recordings back to her.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::InteractFixture, "Plant the drone at The Fire-Worn",
             1, 0, "stellar_signal_echo1"},
            {ObjectiveType::InteractFixture, "Plant the drone at The Quiet Shell",
             1, 0, "stellar_signal_echo2"},
            {ObjectiveType::InteractFixture, "Plant the drone at The Edge",
             1, 0, "stellar_signal_echo3"},
            {ObjectiveType::TalkToNpc, "Return the recordings to Nova",
             1, 0, "Stellar Engineer"},
        };
        q.reward.xp = 200;
        q.reward.credits = 100;
        q.reward.factions.push_back({Faction_StellariConclave, 10});
        q.journal_on_accept =
            "Three systems, three drones. Nova marked them on my chart — "
            "The Fire-Worn (red dwarf, scar planet), The Quiet Shell "
            "(derelict station), and The Edge (neutron remnant, crystalline "
            "fragment). Whatever the signal is, it's loudest in places "
            "nothing should be left.";
        q.journal_on_complete =
            "The three recordings are all in Nova's voice — but older, "
            "weathered, from a version of herself she has no memory of "
            "being. One of them called me 'the one with green eyes.' "
            "Nova thinks she may have lived this cycle before.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_hook"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode offer_mode() const override       { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Stellar Engineer"; }

    void register_fixtures() override {
        register_quest_fixture({
            "stellar_signal_echo1",
            '*', 135, "Plant receiver drone",
            "The drone sinks into the glassed surface. The signal resolves.",
            "FRAGMENT — UNKNOWN VOICE, EARLY CYCLE",
            {
                "...if you're hearing this, they've reached the third iteration.",
                "The feedback loop is holding.",
                "",
                "Don't trust the Conclave. They think it's a gift.",
                "It's a trap.",
            },
        });
        register_quest_fixture({
            "stellar_signal_echo2",
            '*', 135, "Plant receiver drone",
            "The drone clips to the comms array. A voice flickers through the static.",
            "FRAGMENT — UNKNOWN VOICE, MID CYCLE",
            {
                "...memory is the only thing that survives.",
                "Not bodies. Not bonds. Not names.",
                "Only the signal.",
                "",
                "Leave it where they can find it.",
                "He'll come back. They always come back.",
            },
        });
        register_quest_fixture({
            "stellar_signal_echo3",
            '*', 135, "Plant receiver drone",
            "The drone locks onto the crystal. The final fragment plays.",
            "FRAGMENT — UNKNOWN VOICE, LATE CYCLE",
            {
                "...find the one with green eyes.",
                "He always finds you.",
                "Don't forget him this time.",
                "",
                "And this time... try to stay.",
            },
        });
    }

    void on_accepted(Game& game) override {
        auto ids = game.world().stellar_signal_echo_ids();
        if (ids[0] == 0 || ids[1] == 0 || ids[2] == 0) return;

        // Echo 1: detail map of body 0 (The Fire-Worn Prime)
        LocationKey k1 = {ids[0], 0, -1, false, -1, -1, 0};
        QuestLocationMeta m1;
        m1.quest_id = QUEST_ID_ECHOES;
        m1.quest_title = "Three Echoes";
        m1.target_system_id = ids[0];
        m1.target_body_index = 0;
        m1.npc_roles = {"Archon Remnant", "Archon Remnant", "Archon Remnant"};
        m1.fixtures.push_back({"stellar_signal_echo1", -1, -1});
        game.world().quest_locations()[k1] = std::move(m1);

        // Echo 2: derelict station interior (is_station = true)
        LocationKey k2 = {ids[1], -1, -1, true, -1, -1, 0};
        QuestLocationMeta m2;
        m2.quest_id = QUEST_ID_ECHOES;
        m2.quest_title = "Three Echoes";
        m2.target_system_id = ids[1];
        m2.npc_roles = {"Void Reaver", "Void Reaver"};
        m2.fixtures.push_back({"stellar_signal_echo2", -1, -1});
        game.world().quest_locations()[k2] = std::move(m2);

        // Echo 3: detail map of body 0 (Edge Crystal)
        LocationKey k3 = {ids[2], 0, -1, false, -1, -1, 0};
        QuestLocationMeta m3;
        m3.quest_id = QUEST_ID_ECHOES;
        m3.quest_title = "Three Echoes";
        m3.target_system_id = ids[2];
        m3.target_body_index = 0;
        m3.npc_roles = {"Archon Sentinel"};
        m3.fixtures.push_back({"stellar_signal_echo3", -1, -1});
        game.world().quest_locations()[k3] = std::move(m3);
    }

    void on_completed(Game& game) override {
        game.playback_viewer().open(
            PlaybackStyle::AudioLog,
            "NOVA — OBSERVATORY, THE HEAVENS ABOVE",
            {
                "That's my voice.",
                "",
                "Older. Worn. But mine.",
                "",
                "I never recorded that. I've never even been off this station.",
                "And yet...",
                "",
                "She knows you, commander. She called you 'the one with",
                "green eyes.' How would she know that?",
                "",
                "...Unless I've done this before.",
            });
    }
};

void register_stellar_signal_echoes(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalEchoesQuest>());
}

} // namespace astra
