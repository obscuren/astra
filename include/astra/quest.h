#pragma once

#include "astra/item.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace astra {

struct Player;
struct Npc;
struct NavigationData;
class WorldManager;
class Game;

enum class QuestStatus : uint8_t {
    Locked,     // Prerequisites not yet satisfied (story quests only)
    Available,  // Unlocked, awaiting NPC acceptance (or about to auto-accept)
    Active,
    Completed,
    Failed,
};

enum class RevealPolicy : uint8_t {
    Hidden,     // Show "??? — ???" with "Locked" hint only
    TitleOnly,  // Show title, hide description
    Full,       // Show title + description
};

enum class OfferMode : uint8_t {
    Auto,       // Becomes Active immediately on unlock
    NpcOffer,   // Enters Available; offered by a named NPC role via dialog
};

enum class ObjectiveType : uint8_t {
    KillNpc,
    GoToLocation,
    CollectItem,
    TalkToNpc,
    DeliverItem,
    InstallShipComponent,
};

struct QuestObjective {
    ObjectiveType type;
    std::string description;
    int target_count = 1;
    int current_count = 0;
    std::string target_id;        // NPC role, item name, location name, etc.

    bool complete() const { return current_count >= target_count; }
};

struct FactionReward {
    std::string faction_name;
    int reputation_change = 0;
};

struct QuestReward {
    int xp = 0;
    int credits = 0;
    int skill_points = 0;
    std::vector<Item> items;
    std::vector<FactionReward> factions;
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

    // Target location for map markers (set by generator, 0/-1 = none)
    uint32_t target_system_id = 0;
    int target_body_index = -1;

    // Chain / DAG (story quests only; standalone quests leave these defaults)
    std::string arc_id;
    std::vector<std::string> prerequisite_ids;
    RevealPolicy reveal = RevealPolicy::Full;

    bool all_objectives_complete() const;
    // True if all objectives except a trailing TalkToNpc are complete (ready for turn-in)
    bool ready_for_turnin() const;
};

class QuestManager {
public:
    QuestManager() = default;

    // Initialize locked/available from story catalog.
    // Caller is responsible for calling this once on new-game.
    // Quests already present in active_/completed_ are skipped.
    void init_from_catalog(Game& game);

    // Quest lifecycle
    void accept_quest(Quest quest, int world_tick, Player& player);
    void complete_quest(const std::string& quest_id, Game& game, int world_tick);
    void fail_quest(const std::string& quest_id, Game& game);

    // Progress tracking (called by game systems)
    void on_npc_killed(const std::string& npc_role);
    void on_item_picked_up(const std::string& item_name);
    void on_location_entered(const std::string& location_name);
    void on_npc_talked(const std::string& npc_name);
    void on_ship_component_installed(const std::string& slot_name);

    // Query
    const std::vector<Quest>& active_quests() const { return active_; }
    const std::vector<Quest>& completed_quests() const { return completed_; }
    bool has_active_quest(const std::string& id) const;
    Quest* find_active(const std::string& id);

    // Check if any quest just completed all objectives (returns quest id, empty if none)
    std::string check_completions() const;

    // Update journal entries for all active quests with current objective progress
    void update_quest_journals(Player& player);

    // New pool queries
    const std::vector<Quest>& locked_quests() const { return locked_; }
    const std::vector<Quest>& available_quests() const { return available_; }
    std::vector<const Quest*> available_for_role(const std::string& role) const;

    // Accept from available pool. Returns false if not found.
    bool accept_available(const std::string& quest_id, Game& game, int world_tick);

    // Restore from save (replaces internal state without triggering rewards)
    void restore(std::vector<Quest> active, std::vector<Quest> completed);

    // 4-arg restore (saves v27+).
    void restore(std::vector<Quest> locked,
                 std::vector<Quest> available,
                 std::vector<Quest> active,
                 std::vector<Quest> completed);

    // After restore, fold in any catalog quests that aren't in any pool yet
    // (handles catalog growth across save versions).
    void reconcile_with_catalog(Game& game);

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
    std::vector<Quest> locked_;
    std::vector<Quest> available_;
    std::vector<Quest> active_;
    std::vector<Quest> completed_;
};

// ── Story Quests ─────────────────────────────────────────────────────

class StoryQuest {
public:
    virtual ~StoryQuest() = default;

    virtual Quest create_quest() = 0;

    // Arc / DAG declarations (defaults = standalone, no arc)
    virtual std::string arc_id() const { return ""; }
    virtual std::string arc_title() const { return ""; }
    virtual std::vector<std::string> prerequisite_ids() const { return {}; }
    virtual RevealPolicy reveal_policy() const { return RevealPolicy::Full; }

    // Offer semantics
    virtual OfferMode offer_mode() const { return OfferMode::NpcOffer; }
    virtual std::string offer_giver_role() const { return ""; }

    // Lifecycle hooks
    virtual void on_unlocked(Game& game) {}
    virtual void on_accepted(Game& game) {}
    virtual void on_completed(Game& game) {}
    virtual void on_failed(Game& game) {}
};

// Registry of all story quests
const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog();
StoryQuest* find_story_quest(const std::string& id);

// Returns a list of validation errors. Empty vector = catalog is valid.
// Pass an explicit catalog reference to avoid recursion through story_quest_catalog().
std::vector<std::string> validate_quest_catalog(
    const std::vector<std::unique_ptr<StoryQuest>>& catalog);

} // namespace astra
