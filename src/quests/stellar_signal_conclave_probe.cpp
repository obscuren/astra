#include "astra/quest.h"
#include "astra/game.h"
#include "astra/scenario_effects.h"

#include <string>

namespace astra {

static const char* QUEST_ID_CONCLAVE_PROBE = "story_stellar_signal_conclave_probe";

class StellarSignalConclaveProbeQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_CONCLAVE_PROBE;
        q.title = "Into Conclave Space";
        q.description =
            "Nova warned you the Stellari Conclave would not ignore what you "
            "learned at the beacon. Warp into any system under Conclave "
            "control — confirm her warning is real.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::GoToLocation, "Warp to any Stellari Conclave system",
             1, 0, ""},
        };
        q.reward.xp = 200;
        q.journal_on_accept =
            "Nova's last words were a warning, not a briefing: the Conclave "
            "doesn't want this loop broken. Best way to prove she's right is "
            "to step into their territory and watch them notice.";
        q.journal_on_complete =
            "They noticed. Transmission came in the moment the drive cooled. "
            "Stellari Conclave — the voice on the other end didn't even "
            "pretend to ask.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_beacon"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::TitleOnly; }
    // Default offer_mode (NpcOffer) — Nova hands this quest out on the next
    // conversation after the beacon debrief, instead of it auto-accepting.

    void on_completed(Game& game) override {
        open_transmission(game,
            "INCOMING TRANSMISSION - STELLARI CONCLAVE",
            {
                "Commander. You have interfered with a sacred cycle.",
                "Cease immediately and return to Sgr A* trajectory.",
                "",
                "This is your only warning.",
            });
    }
};

void register_stellar_signal_conclave_probe(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalConclaveProbeQuest>());
}

} // namespace astra
