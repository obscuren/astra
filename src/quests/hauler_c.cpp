#include "astra/quest.h"
#include "astra/game.h"
#include "astra/star_chart.h"
#include "astra/world_manager.h"

namespace astra {

class HaulerCQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_pirate_outpost";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "The Pirate Outpost";
        q.description =
            "The Phobos relay pointed to a pirate base dug into the rust "
            "flats of Mars. They're excavating something ancient under the "
            "dunes. Land on Mars, clear the outpost, then report to the "
            "Station Commander on The Heavens Above.";
        q.giver_npc = "Station Commander";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::GoToLocation,
            "Travel to Mars", 1, 0, "Mars"});
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

    void on_accepted(Game& game) override {
        auto& nav = game.world().navigation();
        for (auto& sys : nav.systems) {
            if (sys.id == 1) { generate_system_bodies(sys); break; }
        }
        // Sol = 1, Mars = body 3. Overworld (moon -1) hosts the dig.
        LocationKey k{1, 3, -1, false, -1, -1, 0};
        auto& ql = game.world().quest_locations();
        auto it = ql.find(k);
        QuestLocationMeta meta = (it != ql.end()) ? it->second : QuestLocationMeta{};
        meta.quest_id = quest_id;
        meta.quest_title = "The Pirate Outpost";
        meta.target_system_id = 1;
        meta.target_body_index = 3;
        meta.target_moon_index = -1;
        meta.poi_type = Tile::OW_Outpost;
        meta.difficulty_override = 4;
        meta.npc_roles = {
            "Pirate", "Pirate", "Pirate", "Pirate", "Pirate",
            "Pirate", "Pirate",
        };
        ql[k] = std::move(meta);
    }

    void on_completed(Game& game) override {
        auto& ql = game.world().quest_locations();
        for (auto it = ql.begin(); it != ql.end(); ) {
            // Don't wipe the meta if a sibling (What They Found) is still
            // using the same Mars dig site for its artifact.
            if (it->second.quest_id == quest_id) it = ql.erase(it);
            else ++it;
        }
    }
};

void register_hauler_c(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerCQuest>());
}

} // namespace astra
