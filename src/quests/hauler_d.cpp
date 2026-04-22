#include "astra/quest.h"
#include "astra/game.h"
#include "astra/star_chart.h"
#include "astra/world_manager.h"

namespace astra {

class HaulerDQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_what_they_found";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "What They Found";
        q.description =
            "The Astronomer suspects the pirates on Mars are squatting on a "
            "Precursor dig. While you're clearing their outpost, slip a "
            "relic off the site and bring it back to her — quietly, and not "
            "through the Commander.";
        q.giver_npc = "Astronomer";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::GoToLocation,
            "Travel to the dig site on Mars", 1, 0, "Mars"});
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

    void on_accepted(Game& game) override {
        auto& nav = game.world().navigation();
        for (auto& sys : nav.systems) {
            if (sys.id == 1) { generate_system_bodies(sys); break; }
        }
        // Same Mars dig site as The Pirate Outpost. Merge so both quests
        // share POI stamp, difficulty, and pirate spawns while this one
        // layers on the relic drop.
        LocationKey k{1, 3, -1, false, -1, -1, 0};
        auto& ql = game.world().quest_locations();
        auto it = ql.find(k);
        QuestLocationMeta meta = (it != ql.end()) ? it->second : QuestLocationMeta{};
        meta.target_system_id = 1;
        meta.target_body_index = 3;
        meta.target_moon_index = -1;
        if (meta.poi_type == Tile::Empty) meta.poi_type = Tile::OW_Outpost;
        meta.quest_items.push_back("Precursor Relic");
        // Don't clobber a sibling's quest_id; only claim ownership if
        // nothing else is holding this key.
        if (meta.quest_id.empty()) {
            meta.quest_id = quest_id;
            meta.quest_title = "What They Found";
        }
        ql[k] = std::move(meta);
    }
};

void register_hauler_d(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerDQuest>());
}

} // namespace astra
