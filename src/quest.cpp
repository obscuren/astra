#include "astra/quest.h"
#include "astra/player.h"
#include "astra/character.h"

#include <algorithm>

namespace astra {

bool Quest::all_objectives_complete() const {
    for (const auto& obj : objectives) {
        if (!obj.complete()) return false;
    }
    return !objectives.empty();
}

// ── QuestManager ────────────────────────────────────────────────────

void QuestManager::accept_quest(Quest quest, int world_tick) {
    quest.status = QuestStatus::Active;
    quest.accepted_tick = world_tick;
    active_.push_back(std::move(quest));
}

void QuestManager::complete_quest(const std::string& quest_id, Player& player) {
    for (auto it = active_.begin(); it != active_.end(); ++it) {
        if (it->id == quest_id) {
            it->status = QuestStatus::Completed;
            // Apply rewards
            player.xp += it->reward.xp;
            player.money += it->reward.credits;
            player.skill_points += it->reward.skill_points;
            if (!it->reward.faction_name.empty()) {
                for (auto& fs : player.reputation) {
                    if (fs.faction_name == it->reward.faction_name) {
                        fs.reputation += it->reward.reputation_change;
                        break;
                    }
                }
            }
            completed_.push_back(std::move(*it));
            active_.erase(it);
            return;
        }
    }
}

void QuestManager::fail_quest(const std::string& quest_id) {
    for (auto it = active_.begin(); it != active_.end(); ++it) {
        if (it->id == quest_id) {
            it->status = QuestStatus::Failed;
            completed_.push_back(std::move(*it));
            active_.erase(it);
            return;
        }
    }
}

bool QuestManager::has_active_quest(const std::string& id) const {
    for (const auto& q : active_) {
        if (q.id == id) return true;
    }
    return false;
}

Quest* QuestManager::find_active(const std::string& id) {
    for (auto& q : active_) {
        if (q.id == id) return &q;
    }
    return nullptr;
}

std::string QuestManager::check_completions() const {
    for (const auto& q : active_) {
        if (q.all_objectives_complete()) return q.id;
    }
    return "";
}

// ── Progress Tracking ───────────────────────────────────────────────

void QuestManager::on_npc_killed(const std::string& npc_role) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::KillNpc && obj.target_id == npc_role) {
                if (obj.current_count < obj.target_count) {
                    ++obj.current_count;
                }
            }
        }
    }
}

void QuestManager::on_item_picked_up(const std::string& item_name) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::CollectItem && obj.target_id == item_name) {
                if (obj.current_count < obj.target_count) {
                    ++obj.current_count;
                }
            }
        }
    }
}

void QuestManager::on_location_entered(const std::string& location_name) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::GoToLocation && obj.target_id == location_name) {
                obj.current_count = obj.target_count;
            }
        }
    }
}

void QuestManager::on_npc_talked(const std::string& npc_name) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::TalkToNpc && obj.target_id == npc_name) {
                obj.current_count = obj.target_count;
            }
        }
    }
}

// ── Random Quest Generation ─────────────────────────────────────────

Quest QuestManager::generate_kill_quest(std::mt19937& rng) {
    static const char* targets[] = {"Xytomorph", "Young Xytomorph"};
    static const char* adjectives[] = {"Dangerous", "Lurking", "Vicious", "Aggressive"};

    int target_idx = std::uniform_int_distribution<int>(0, 1)(rng);
    int adj_idx = std::uniform_int_distribution<int>(0, 3)(rng);
    int count = std::uniform_int_distribution<int>(2, 5)(rng);

    Quest q;
    q.id = "random_kill_" + std::to_string(rng());
    q.title = std::string(adjectives[adj_idx]) + " " + targets[target_idx] + "s";
    q.description = "Eliminate " + std::to_string(count) + " " + targets[target_idx] +
                    "s in the nearby caves.";
    q.is_story = false;

    QuestObjective obj;
    obj.type = ObjectiveType::KillNpc;
    obj.description = "Kill " + std::to_string(count) + " " + targets[target_idx] + "s";
    obj.target_id = targets[target_idx];
    obj.target_count = count;
    q.objectives.push_back(std::move(obj));

    q.reward.xp = count * 20;
    q.reward.credits = count * 10;

    return q;
}

Quest QuestManager::generate_fetch_quest(std::mt19937& rng) {
    static const char* items[] = {"Power Core", "Circuit Board", "Nano-Fiber", "Alloy Ingot"};
    int item_idx = std::uniform_int_distribution<int>(0, 3)(rng);
    int count = std::uniform_int_distribution<int>(1, 3)(rng);

    Quest q;
    q.id = "random_fetch_" + std::to_string(rng());
    q.title = "Supply Run: " + std::string(items[item_idx]);
    q.description = "Collect " + std::to_string(count) + " " + items[item_idx] +
                    " and bring them back.";
    q.is_story = false;

    QuestObjective obj;
    obj.type = ObjectiveType::CollectItem;
    obj.description = "Collect " + std::to_string(count) + " " + items[item_idx];
    obj.target_id = items[item_idx];
    obj.target_count = count;
    q.objectives.push_back(std::move(obj));

    q.reward.xp = count * 15;
    q.reward.credits = count * 20;

    return q;
}

} // namespace astra
