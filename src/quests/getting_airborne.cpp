#include "astra/quest.h"
#include "astra/game.h"

namespace astra {

static const char* quest_id = "story_getting_airborne";

class GettingAirborneQuest : public StoryQuest {
    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "Getting Airborne";
        q.description =
            "Your ship took heavy damage from pirates. The engine, hull plating, "
            "and navigation computer are destroyed. Find replacement parts to "
            "get back in the air.";
        q.is_story = true;

        q.objectives.push_back({ObjectiveType::InstallShipComponent,
            "Restore engine power", 1, 0, "Engine"});
        q.objectives.push_back({ObjectiveType::InstallShipComponent,
            "Repair hull breach", 1, 0, "Hull"});
        q.objectives.push_back({ObjectiveType::InstallShipComponent,
            "Bring navigation online", 1, 0, "Navi Computer"});

        q.reward.xp = 100;
        q.reward.credits = 50;
        q.reward.faction_name = "Stellari Conclave";
        q.reward.reputation_change = 5;
        return q;
    }

    void on_completed(Game& game) override {
        game.log("ARIA: \"All primary systems restored. We're flight-ready, commander. The galaxy awaits.\"");
    }
};

// Add to the story quest catalog — called from missing_hauler.cpp's build_catalog
void register_getting_airborne(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<GettingAirborneQuest>());
}

} // namespace astra
