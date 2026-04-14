#include "astra/quest.h"
#include "astra/game.h"

namespace astra {

class HaulerCQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_pirate_outpost";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "The Pirate Outpost";
        q.description = "Assault the pirate base. They're excavating something ancient.";
        q.giver_npc = "Station Commander";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::KillNpc,
            "Clear the outpost", 5, 0, "Pirate"});
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report to the Commander", 1, 0, "Station Commander"});
        q.reward.xp = 400;
        q.reward.credits = 200;
        return q;
    }

    std::string arc_id() const override { return "hauler_arc"; }
    std::string arc_title() const override { return "The Hauler Arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_hauler_track_signal"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::TitleOnly; }
    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Station Commander"; }
};

void register_hauler_c(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerCQuest>());
}

} // namespace astra
