#include "astra/quest.h"
#include "astra/game.h"
#include "astra/faction.h"
#include "astra/star_chart.h"
#include "astra/world_manager.h"

namespace astra {

class HaulerBQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_track_signal";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "Track the Signal";
        q.description =
            "The cargo manifest decoded into a looped pirate transmission. "
            "Its carrier repeater is bouncing off Phobos, the inner moon of "
            "Mars. Land on Phobos, scan the relay for the signal's true "
            "terminus, then bring the fix back to the Station Keeper.";
        q.giver_npc = "Station Keeper";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::GoToLocation,
            "Land on Phobos and sweep the relay", 1, 0, "Phobos"});
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

    void on_accepted(Game& game) override {
        // Sol = 1, Mars = body 3, Phobos = moon 0.
        auto& nav = game.world().navigation();
        for (auto& sys : nav.systems) {
            if (sys.id == 1) { generate_system_bodies(sys); break; }
        }
        LocationKey k{1, 3, 0, false, -1, -1, 0};
        QuestLocationMeta meta;
        meta.quest_id = quest_id;
        meta.quest_title = "Track the Signal";
        meta.target_system_id = 1;
        meta.target_body_index = 3;
        meta.target_moon_index = 0;
        meta.poi_type = Tile::OW_Ruins;
        meta.remove_on_completion = true;
        game.world().quest_locations()[k] = std::move(meta);
    }

    void on_unlocked(Game& game) override {
        game.log("New intel from the Station Keeper. Check your journal.");
    }

    void on_completed(Game& game) override {
        auto& ql = game.world().quest_locations();
        for (auto it = ql.begin(); it != ql.end(); ) {
            if (it->second.quest_id == quest_id) it = ql.erase(it);
            else ++it;
        }
    }
};

void register_hauler_b(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerBQuest>());
}

} // namespace astra
