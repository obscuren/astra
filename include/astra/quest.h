#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace astra {

struct Player;
struct Npc;
struct Item;
struct NavigationData;
class WorldManager;
class Game;

enum class QuestStatus : uint8_t {
    Available,
    Active,
    Completed,
    Failed,
};

enum class ObjectiveType : uint8_t {
    KillNpc,
    GoToLocation,
    CollectItem,
    TalkToNpc,
    DeliverItem,
};

struct QuestObjective {
    ObjectiveType type;
    std::string description;
    int target_count = 1;
    int current_count = 0;
    std::string target_id;        // NPC role, item name, location name, etc.

    bool complete() const { return current_count >= target_count; }
};

struct QuestReward {
    int xp = 0;
    int credits = 0;
    int skill_points = 0;
    std::string item_name;
    std::string faction_name;
    int reputation_change = 0;
};

struct Quest {
    std::string id;
    std::string title;
    std::string description;
    std::string giver_npc;
    QuestStatus status = QuestStatus::Available;
    std::vector<QuestObjective> objectives;
    QuestReward reward;
    bool is_story = false;
    int accepted_tick = 0;

    bool all_objectives_complete() const;
    // True if all objectives except a trailing TalkToNpc are complete (ready for turn-in)
    bool ready_for_turnin() const;
};

class QuestManager {
public:
    QuestManager() = default;

    // Quest lifecycle
    void accept_quest(Quest quest, int world_tick);
    void complete_quest(const std::string& quest_id, Player& player);
    void fail_quest(const std::string& quest_id);

    // Progress tracking (called by game systems)
    void on_npc_killed(const std::string& npc_role);
    void on_item_picked_up(const std::string& item_name);
    void on_location_entered(const std::string& location_name);
    void on_npc_talked(const std::string& npc_name);

    // Query
    const std::vector<Quest>& active_quests() const { return active_; }
    const std::vector<Quest>& completed_quests() const { return completed_; }
    bool has_active_quest(const std::string& id) const;
    Quest* find_active(const std::string& id);

    // Check if any quest just completed all objectives (returns quest id, empty if none)
    std::string check_completions() const;

    // Random quest generation (simple — no world awareness)
    Quest generate_kill_quest(std::mt19937& rng);
    Quest generate_fetch_quest(std::mt19937& rng);
    Quest generate_deliver_quest(const std::string& npc_name, std::mt19937& rng);
    Quest generate_scout_quest(const std::string& body_name, std::mt19937& rng);

    // Generate a random quest appropriate for an NPC role, using world state
    // for real locations and distance-scaled difficulty/rewards
    Quest generate_quest_for_role(const std::string& role,
                                  const std::string& npc_name,
                                  NavigationData& nav,
                                  std::mt19937& rng);

private:
    std::vector<Quest> active_;
    std::vector<Quest> completed_;
};

// ── Story Quests ─────────────────────────────────────────────────────

class StoryQuest {
public:
    virtual ~StoryQuest() = default;
    virtual Quest create_quest() = 0;
    virtual void on_accepted(Game& game) {}
    virtual void on_completed(Game& game) {}
    virtual void on_failed(Game& game) {}
};

// Registry of all story quests
const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog();
StoryQuest* find_story_quest(const std::string& id);

} // namespace astra
