#include "astra/quest.h"
#include "astra/game.h"

namespace astra {

class HaulerDQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_what_they_found";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "What They Found";
        q.description = "An Astronomer wants you to quietly inspect the site "
                        "and bring back any precursor artifacts.";
        q.giver_npc = "Astronomer";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::CollectItem,
            "Recover a precursor artifact", 1, 0, "Precursor Relic"});
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report to the Astronomer", 1, 0, "Astronomer"});
        q.reward.xp = 400;
        q.reward.credits = 150;
        return q;
    }

    std::string arc_id() const override { return "hauler_arc"; }
    std::string arc_title() const override { return "The Hauler Arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_hauler_track_signal"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Hidden; }
    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Astronomer"; }
};

void register_hauler_d(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerDQuest>());
}

} // namespace astra
