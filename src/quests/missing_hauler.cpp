#include "astra/quest.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/celestial_body.h"

namespace astra {

class MissingHaulerQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_missing_hauler";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "The Missing Hauler";
        q.description = "A cargo hauler went dark near the asteroid belt. "
                         "Investigate the wreckage and recover the cargo manifest.";
        q.giver_npc = "Station Keeper";
        q.is_story = true;

        q.objectives.push_back({ObjectiveType::GoToLocation,
            "Find the missing hauler", 1, 0, "Derelict Hauler"});
        q.objectives.push_back({ObjectiveType::CollectItem,
            "Recover the cargo manifest", 1, 0, "Cargo Manifest"});
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report back to the Station Keeper", 1, 0, "Station Keeper"});

        q.reward.xp = 200;
        q.reward.credits = 100;
        q.reward.faction_name = Faction_StellariConclave;
        q.reward.reputation_change = 10;
        return q;
    }

    void on_accepted(Game& game) override {
        auto& world = game.world();
        auto& nav = world.navigation();

        // Find a suitable target: asteroid belt or rocky body in the current system
        auto& systems = nav.systems;
        int sys_idx = -1;
        for (int i = 0; i < static_cast<int>(systems.size()); ++i) {
            if (systems[i].id == nav.current_system_id) {
                sys_idx = i;
                break;
            }
        }
        if (sys_idx < 0) return;

        auto& sys = systems[sys_idx];
        generate_system_bodies(sys);

        // Pick target body: prefer asteroid belt, then rocky/dwarf planet
        int target_body = -1;
        for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
            if (sys.bodies[i].type == BodyType::AsteroidBelt) {
                target_body = i;
                break;
            }
        }
        if (target_body < 0) {
            for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
                auto t = sys.bodies[i].type;
                if (t == BodyType::Rocky || t == BodyType::DwarfPlanet) {
                    target_body = i;
                    break;
                }
            }
        }
        // Fallback: first body that exists
        if (target_body < 0 && !sys.bodies.empty()) {
            target_body = 0;
        }
        if (target_body < 0) return;

        // Make the body landable and mark it as having a dungeon
        sys.bodies[target_body].landable = true;
        sys.bodies[target_body].has_dungeon = true;

        // Register quest location for the dungeon on this body
        // Key: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}
        // We'll use a special key for the overworld of this body
        LocationKey ow_key = {sys.id, target_body, -1, false, -1, -1, 0};
        QuestLocationMeta meta;
        meta.quest_id = quest_id;
        meta.quest_title = "The Missing Hauler";
        meta.difficulty_override = 3;
        meta.npc_roles = {"Xytomorph", "Xytomorph", "Young Xytomorph"};
        meta.quest_items = {"Cargo Manifest"};
        meta.poi_type = Tile::OW_CaveEntrance;
        meta.remove_on_completion = true;
        meta.target_system_id = sys.id;
        meta.target_body_index = target_body;
        world.quest_locations()[ow_key] = std::move(meta);

        // Update the active quest with the actual location name
        std::string body_name = sys.bodies[target_body].name;
        Quest* active = game.quests().find_active(quest_id);
        if (active) {
            active->description = "A cargo hauler went dark near " + body_name +
                                  " in the " + sys.name + " system. "
                                  "Investigate the wreckage and recover the cargo manifest.";
            if (!active->objectives.empty()) {
                active->objectives[0].description = "Travel to " + body_name;
                active->objectives[0].target_id = body_name;
            }
        }

        // Log destination to player
        game.log("Coordinates uploaded: " + colored(body_name, Color::Cyan) +
                 " in " + colored(sys.name, Color::Yellow) + ". Check your star chart [m].");
    }

    void on_completed(Game& game) override {
        // Remove quest locations
        auto& ql = game.world().quest_locations();
        for (auto it = ql.begin(); it != ql.end(); ) {
            if (it->second.quest_id == quest_id)
                it = ql.erase(it);
            else
                ++it;
        }
    }
};

// ── Story Quest Registry ─────────────────────────────────────────────

// Forward declare registrations from other quest files
void register_getting_airborne(std::vector<std::unique_ptr<StoryQuest>>& catalog);

static std::vector<std::unique_ptr<StoryQuest>> build_catalog() {
    std::vector<std::unique_ptr<StoryQuest>> catalog;
    catalog.push_back(std::make_unique<MissingHaulerQuest>());
    register_getting_airborne(catalog);
    return catalog;
}

const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog() {
    static auto catalog = build_catalog();
    return catalog;
}

StoryQuest* find_story_quest(const std::string& id) {
    for (const auto& sq : story_quest_catalog()) {
        Quest q = sq->create_quest();
        if (q.id == id) return sq.get();
    }
    return nullptr;
}

} // namespace astra
