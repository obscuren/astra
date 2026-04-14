#include "astra/quest.h"
#include "astra/game.h"
#include "astra/faction.h"

namespace astra {

class HaulerBQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_track_signal";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "Track the Signal";
        q.description = "The cargo manifest revealed a pirate transmission source. "
                        "Trace it to its origin.";
        q.giver_npc = "Station Keeper";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report back to the Station Keeper", 1, 0, "Station Keeper"});
        q.reward.xp = 250;
        q.reward.credits = 150;
        q.reward.factions.push_back({Faction_StellariConclave, 8});
        return q;
    }

    std::string arc_id() const override { return "hauler_arc"; }
    std::string arc_title() const override { return "The Hauler Arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_missing_hauler"};
    }
    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Station Keeper"; }

    void on_unlocked(Game& game) override {
        game.log("New intel from the Station Keeper. Check your journal.");
    }
};

void register_hauler_b(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerBQuest>());
}

} // namespace astra
