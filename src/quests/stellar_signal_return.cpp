#include "astra/quest.h"
#include "astra/game.h"
#include "astra/world_manager.h"

#include <memory>
#include <string>

namespace astra {

static const char* QUEST_ID_RETURN = "story_stellar_signal_return";

class StellarSignalReturnQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_RETURN;
        q.title = "Return to the Heavens Above";
        q.description =
            "You have confirmed Nova's rumor to be true. Return to the "
            "Heavens Above to speak to her about the next steps.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::GoToLocation,
             "Return to The Heavens Above",
             1, 0, "The Heavens Above"},
        };
        q.reward.xp = 100;
        q.journal_on_accept =
            "The Conclave noticed. Their warning came in the moment the "
            "drive cooled — Nova was right about all of it. Heading back "
            "to The Heavens Above to hear what she wants to do next.";
        q.journal_on_complete =
            "Landed at The Heavens Above. The station's not what I left "
            "— ARIA's frantic on the comms, and the Conclave is already "
            "here.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_conclave_probe"};
    }
    RevealPolicy reveal_policy() const override   { return RevealPolicy::Full; }
    OfferMode    offer_mode()    const override   { return OfferMode::Auto; }

    void on_accepted(Game& game) override {
        // Sol = 1, Jupiter body index = 5 (THA's host).
        LocationKey k{1, 5, -1, false, -1, -1, 0};
        QuestLocationMeta meta;
        meta.quest_id = QUEST_ID_RETURN;
        meta.quest_title = "Return to the Heavens Above";
        meta.target_system_id = 1;
        meta.target_body_index = 5;
        game.world().quest_locations()[k] = std::move(meta);
    }
};

void register_stellar_signal_return(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalReturnQuest>());
}

} // namespace astra
