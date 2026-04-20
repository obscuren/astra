#include "astra/quest.h"
#include "astra/journal.h"
#include "astra/player.h"
#include "astra/character.h"
#include "astra/quest_graph.h"
#include "astra/star_chart.h"
#include "astra/time_of_day.h"
#include "astra/game.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace astra {

bool Quest::all_objectives_complete() const {
    for (const auto& obj : objectives) {
        if (!obj.complete()) return false;
    }
    return !objectives.empty();
}

bool Quest::ready_for_turnin() const {
    if (objectives.empty()) return false;
    for (size_t i = 0; i < objectives.size(); ++i) {
        const auto& obj = objectives[i];
        // Skip the final TalkToNpc objective — that's the turn-in itself
        if (obj.type == ObjectiveType::TalkToNpc && i == objectives.size() - 1)
            continue;
        if (!obj.complete()) return false;
    }
    return true;
}

// ── QuestManager ────────────────────────────────────────────────────

static std::string make_timestamp(int world_tick) {
    return "Cycle " + std::to_string(global_cycle(world_tick))
         + ", Day " + std::to_string(day_in_cycle(world_tick));
}

// Choose the journal-entry key for a quest: arc-level for story arcs,
// per-quest for standalone. All stages of the same arc share one entry.
static std::string journal_key_for(const Quest& quest) {
    return quest.arc_id.empty() ? quest.id : ("arc:" + quest.arc_id);
}

// Render the personal body for an arc journal entry — a narrative diary
// composed of each stage's authored journal_on_accept / journal_on_complete
// text, not a task checklist (the Quests tab is for tasks).
static std::string render_arc_body(const QuestManager& qm,
                                   const std::string& arc_id) {
    const auto& graph = quest_graph();
    const auto& members = graph.arc_members(arc_id);
    std::string body;
    for (const auto& id : members) {
        auto look = qm.find_quest(id);
        if (!look.quest) continue;
        const Quest& q = *look.quest;

        // Skip stages the player hasn't reached yet.
        if (look.status == QuestStatus::Locked ||
            look.status == QuestStatus::Available) continue;

        // Accept prose
        if (!q.journal_on_accept.empty()) {
            if (!body.empty()) body += "\n";
            body += q.journal_on_accept;
            if (body.back() != '\n') body += "\n";
        }
        // Complete prose — only when the stage is actually done
        if (look.status == QuestStatus::Completed &&
            !q.journal_on_complete.empty()) {
            if (!body.empty() && body.back() != '\n') body += "\n";
            body += q.journal_on_complete;
            if (body.back() != '\n') body += "\n";
        }
    }
    return body;
}

void QuestManager::accept_quest(Quest quest, int world_tick, Player& player) {
    quest.status = QuestStatus::Active;
    quest.accepted_tick = world_tick;

    // Push to active_ first so render_arc_body sees this stage as Active.
    active_.push_back(std::move(quest));
    const Quest& pushed = active_.back();

    const std::string key = journal_key_for(pushed);

    // Update or create the journal entry
    JournalEntry* existing = find_journal_entry(player.journal, key);
    if (existing) {
        // Arc-level update: refresh title (uses arc_title if set), body, timestamp
        StoryQuest* sq = find_story_quest(pushed.id);
        std::string arc_title = sq ? sq->arc_title() : std::string{};
        existing->title = arc_title.empty() ? ("Quest: " + pushed.title)
                                            : ("Arc: " + arc_title);
        existing->timestamp = make_timestamp(world_tick);
        existing->personal = render_arc_body(*this, pushed.arc_id);
        if (!pushed.giver_npc.empty()) {
            existing->personal += "Given by: " + pushed.giver_npc + "\n";
        }
    } else {
        JournalEntry entry;
        entry.category = JournalCategory::Quest;
        entry.quest_id = key;
        entry.world_tick = world_tick;
        entry.timestamp = make_timestamp(world_tick);
        if (pushed.arc_id.empty()) {
            entry.title = "Quest: " + pushed.title;
            entry.technical = pushed.description;
            entry.personal = pushed.journal_on_accept.empty()
                               ? pushed.description
                               : pushed.journal_on_accept;
        } else {
            StoryQuest* sq = find_story_quest(pushed.id);
            std::string arc_title = sq ? sq->arc_title() : pushed.arc_id;
            entry.title = "Arc: " + arc_title;
            entry.technical = pushed.description;   // seed with first stage desc
            entry.personal = render_arc_body(*this, pushed.arc_id);
            if (!pushed.giver_npc.empty()) {
                entry.personal += "Given by: " + pushed.giver_npc + "\n";
            }
        }
        player.journal.push_back(std::move(entry));
    }
}

static void cleanup_quest_fixtures(Game& game, const std::string& quest_id) {
    auto& locs = game.world().quest_locations();
    auto& pending = game.world().pending_quest_cleanup();

    const auto& nav = game.world().navigation();
    LocationKey cur = {nav.current_system_id,
                       nav.current_body_index,
                       nav.current_moon_index,
                       false,
                       game.world().overworld_x(),
                       game.world().overworld_y(),
                       0};

    for (auto it = locs.begin(); it != locs.end();) {
        auto& meta = it->second;
        if (meta.quest_id != quest_id || !meta.remove_on_completion) {
            ++it;
            continue;
        }

        if (it->first == cur) {
            // Current map is loaded — remove fixtures directly.
            for (const auto& p : meta.fixtures) {
                if (p.x >= 0 && p.y >= 0) {
                    game.world().map().remove_fixture(p.x, p.y);
                }
            }
        } else {
            // Map not loaded; mark for drain on next entry.
            pending.insert(it->first);
        }
        it = locs.erase(it);
    }
}

void QuestManager::complete_quest(const std::string& quest_id, Game& game, int world_tick) {
    for (auto it = active_.begin(); it != active_.end(); ++it) {
        if (it->id == quest_id) {
            it->status = QuestStatus::Completed;
            // Apply rewards
            game.player().xp += it->reward.xp;
            game.player().money += it->reward.credits;
            game.player().skill_points += it->reward.skill_points;
            for (auto& reward_item : it->reward.items) {
                game.player().inventory.items.push_back(std::move(reward_item));
            }
            for (const auto& fr : it->reward.factions) {
                if (fr.faction_name.empty()) continue;
                for (auto& fs : game.player().reputation) {
                    if (fs.faction_name == fr.faction_name) {
                        fs.reputation += fr.reputation_change;
                        break;
                    }
                }
            }

            // Move quest to completed_ BEFORE rebuilding the journal body so
            // render_arc_body sees this stage's status as Completed.
            const std::string key = journal_key_for(*it);
            const std::string arc_id = it->arc_id;
            const std::string stage_title = it->title;
            QuestReward snapshot_reward = it->reward;
            completed_.push_back(std::move(*it));
            active_.erase(it);

            JournalEntry* je = find_journal_entry(game.player().journal, key);
            if (je) {
                if (!arc_id.empty()) {
                    // Arc entry: title stays "Arc: …", body rebuilt from all stages.
                    je->personal = render_arc_body(*this, arc_id);
                    std::string rewards;
                    if (snapshot_reward.xp > 0) rewards += std::to_string(snapshot_reward.xp) + " XP  ";
                    if (snapshot_reward.credits > 0) rewards += std::to_string(snapshot_reward.credits) + "$  ";
                    if (snapshot_reward.skill_points > 0) rewards += std::to_string(snapshot_reward.skill_points) + " SP  ";
                    for (const auto& ri : snapshot_reward.items) rewards += ri.label() + "  ";
                    if (!rewards.empty())
                        je->personal += "Stage complete: " + stage_title + "  (+" + rewards + ")\n";
                } else {
                    je->title = "Quest Complete: " + stage_title;
                    const Quest& done = game.quests().completed_quests().back();
                    std::string body = done.journal_on_accept.empty()
                                         ? done.description
                                         : done.journal_on_accept;
                    if (!done.journal_on_complete.empty()) {
                        if (!body.empty() && body.back() != '\n') body += "\n";
                        body += "\n" + done.journal_on_complete;
                    }
                    je->personal = body;
                }
            }

            cleanup_quest_fixtures(game, quest_id);

            // Fire StoryQuest hook
            StoryQuest* sq = find_story_quest(quest_id);
            if (sq) sq->on_completed(game);

            // Cascade unlock dependents
            const auto& graph = quest_graph();
            for (const auto& dep_id : graph.dependents_of(quest_id)) {
                auto dep_it = std::find_if(locked_.begin(), locked_.end(),
                    [&](const Quest& q){ return q.id == dep_id; });
                if (dep_it == locked_.end()) continue;

                // Are all of this dep's prereqs now Completed?
                bool ready = true;
                for (const auto& p : graph.prerequisites_of(dep_id)) {
                    bool p_done = false;
                    for (const auto& c : completed_) {
                        if (c.id == p && c.status == QuestStatus::Completed) {
                            p_done = true; break;
                        }
                    }
                    if (!p_done) { ready = false; break; }
                }
                if (!ready) continue;

                Quest unlocked = std::move(*dep_it);
                locked_.erase(dep_it);
                StoryQuest* dep_sq = find_story_quest(dep_id);
                if (dep_sq) dep_sq->on_unlocked(game);

                if (dep_sq && dep_sq->offer_mode() == OfferMode::Auto) {
                    std::string dep_id_copy = unlocked.id;
                    accept_quest(std::move(unlocked), world_tick, game.player());
                    dep_sq->on_accepted(game);
                    pending_announcements_.push_back(std::move(dep_id_copy));
                } else {
                    unlocked.status = QuestStatus::Available;
                    available_.push_back(std::move(unlocked));
                }
            }
            return;
        }
    }
}

void QuestManager::fail_quest(const std::string& quest_id, Game& game) {
    const auto& graph = quest_graph();
    std::unordered_set<std::string> to_fail = graph.descendants_of(quest_id);
    to_fail.insert(quest_id);

    auto move_failed = [&](std::vector<Quest>& pool, const std::string& id) -> bool {
        for (auto it = pool.begin(); it != pool.end(); ++it) {
            if (it->id == id) {
                it->status = QuestStatus::Failed;
                StoryQuest* sq = find_story_quest(id);
                if (sq) sq->on_failed(game);
                completed_.push_back(std::move(*it));
                pool.erase(it);
                return true;
            }
        }
        return false;
    };

    for (const auto& id : to_fail) {
        if (move_failed(active_, id)) {
            cleanup_quest_fixtures(game, id);
            continue;
        }
        if (move_failed(available_, id)) {
            cleanup_quest_fixtures(game, id);
            continue;
        }
        move_failed(locked_, id);  // ignore return; quest may already be completed/unknown
        cleanup_quest_fixtures(game, id);
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

bool QuestManager::has_pending_announcement() const {
    return !pending_announcements_.empty();
}

std::string QuestManager::pop_pending_announcement() {
    if (pending_announcements_.empty()) return {};
    std::string id = std::move(pending_announcements_.front());
    pending_announcements_.pop_front();
    return id;
}

QuestManager::Lookup QuestManager::find_quest(const std::string& id) const {
    for (const auto& q : active_)    if (q.id == id) return {&q, QuestStatus::Active};
    for (const auto& q : available_) if (q.id == id) return {&q, QuestStatus::Available};
    for (const auto& q : locked_)    if (q.id == id) return {&q, QuestStatus::Locked};
    for (const auto& q : completed_) if (q.id == id) return {&q, q.status};
    return {nullptr, QuestStatus::Unknown};
}

QuestStatus QuestManager::status_of(const std::string& id) const {
    return find_quest(id).status;
}

void QuestManager::restore(std::vector<Quest> active, std::vector<Quest> completed) {
    active_ = std::move(active);
    completed_ = std::move(completed);
}

std::string QuestManager::check_completions() const {
    for (const auto& q : active_) {
        if (q.all_objectives_complete()) return q.id;
    }
    return "";
}

void QuestManager::update_quest_journals(Player& player) {
    // Arc entries refresh when a stage transitions (accept/complete); the
    // narrative text is seeded from authored journal_on_accept /
    // journal_on_complete rather than tick-by-tick objective progress, so
    // there's nothing to rebuild here for arcs. Standalone quests likewise
    // author their own narrative via journal_on_accept — task progress
    // lives in the Quests tab. This function becomes a no-op; kept in the
    // interface so callers don't break.
    (void)player;
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
        for (size_t i = 0; i < q.objectives.size(); ++i) {
            auto& obj = q.objectives[i];
            if (obj.type == ObjectiveType::TalkToNpc && obj.target_id == npc_name) {
                // Only complete if all prior objectives are done
                bool prior_done = true;
                for (size_t j = 0; j < i; ++j) {
                    if (!q.objectives[j].complete()) { prior_done = false; break; }
                }
                if (prior_done) {
                    obj.current_count = obj.target_count;
                }
            }
        }
    }
}

void QuestManager::on_ship_component_installed(const std::string& slot_name) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::InstallShipComponent &&
                obj.target_id == slot_name) {
                obj.current_count = obj.target_count;
            }
        }
    }
}

void QuestManager::on_fixture_interacted(const std::string& fixture_id) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::InteractFixture &&
                obj.target_id == fixture_id &&
                obj.current_count < obj.target_count) {
                ++obj.current_count;
            }
        }
    }
}

// ── Random Quest Generation ─────────────────────────────────────────

// Pick a random landable body from a system (generates bodies if needed)
// Returns {body_name, body_index} — empty name / -1 if none found
static std::pair<std::string, int> pick_body(StarSystem& sys, std::mt19937& rng) {
    generate_system_bodies(sys);
    std::vector<int> landable;
    for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
        if (sys.bodies[i].landable) landable.push_back(i);
    }
    if (landable.empty()) return {"", -1};
    int idx = landable[std::uniform_int_distribution<int>(0, static_cast<int>(landable.size()) - 1)(rng)];
    return {sys.bodies[idx].name, idx};
}

// Find the current system
static StarSystem* current_system(NavigationData& nav) {
    for (auto& s : nav.systems) {
        if (s.id == nav.current_system_id) return &s;
    }
    return nullptr;
}

// Collect nearby discovered systems sorted by distance, up to max_dist
static std::vector<std::pair<float, StarSystem*>> nearby_systems(
    NavigationData& nav, float max_dist) {
    auto* cur = current_system(nav);
    if (!cur) return {};
    std::vector<std::pair<float, StarSystem*>> result;
    for (auto& s : nav.systems) {
        if (s.id == cur->id || !s.discovered) continue;
        float dx = s.gx - cur->gx;
        float dy = s.gy - cur->gy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d <= max_dist) result.push_back({d, &s});
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return result;
}

// Reward multiplier based on distance (1.0 = local, up to 3.0 for far systems)
static float distance_reward_mult(float dist) {
    if (dist <= 5.0f) return 1.0f;
    if (dist <= 20.0f) return 1.5f;
    if (dist <= 50.0f) return 2.0f;
    return 3.0f;
}

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

Quest QuestManager::generate_deliver_quest(const std::string& npc_name, std::mt19937& rng) {
    static const char* cargo[] = {"Sealed Crate", "Data Chip", "Medical Supplies", "Fuel Cell"};
    static const char* urgency[] = {"Urgent", "Priority", "Routine", "Classified"};

    int cargo_idx = std::uniform_int_distribution<int>(0, 3)(rng);
    int urg_idx = std::uniform_int_distribution<int>(0, 3)(rng);

    Quest q;
    q.id = "random_deliver_" + std::to_string(rng());
    q.title = std::string(urgency[urg_idx]) + " Delivery";
    q.description = "Deliver a " + std::string(cargo[cargo_idx]) +
                    " to " + npc_name + ".";
    q.is_story = false;

    QuestObjective obj;
    obj.type = ObjectiveType::TalkToNpc;
    obj.description = "Deliver " + std::string(cargo[cargo_idx]) + " to " + npc_name;
    obj.target_id = npc_name;
    obj.target_count = 1;
    q.objectives.push_back(std::move(obj));

    q.reward.xp = 30;
    q.reward.credits = 40;

    return q;
}

Quest QuestManager::generate_scout_quest(const std::string& body_name, std::mt19937& rng) {
    static const char* reasons[] = {
        "Survey the area for mineral deposits.",
        "Scout for signs of hostile activity.",
        "Map the terrain for future expeditions.",
        "Search for salvageable wreckage.",
    };

    int reason_idx = std::uniform_int_distribution<int>(0, 3)(rng);

    Quest q;
    q.id = "random_scout_" + std::to_string(rng());
    q.title = "Recon: " + body_name;
    q.description = std::string(reasons[reason_idx]) +
                    " Travel to " + body_name + " and report back.";
    q.is_story = false;

    QuestObjective obj;
    obj.type = ObjectiveType::GoToLocation;
    obj.description = "Travel to " + body_name;
    obj.target_id = body_name;
    obj.target_count = 1;
    q.objectives.push_back(std::move(obj));

    q.reward.xp = 50;
    q.reward.credits = 30;

    return q;
}

Quest QuestManager::generate_quest_for_role(const std::string& role,
                                             const std::string& npc_name,
                                             NavigationData& nav,
                                             std::mt19937& rng) {
    // Difficulty tiers by distance:
    //   Easy  (local system)        — dist 0,       reward x1.0
    //   Medium (nearby systems)     — dist ≤ 20,    reward x1.5
    //   Hard   (distant systems)    — dist ≤ 50,    reward x2.0
    //   Extreme (far systems)       — dist > 50,    reward x3.0

    // Pick difficulty: easy 40%, medium 35%, hard 20%, extreme 5%
    int roll = std::uniform_int_distribution<int>(1, 100)(rng);
    float max_dist;
    if (roll <= 40)       max_dist = 0.0f;    // local only
    else if (roll <= 75)  max_dist = 20.0f;   // nearby
    else if (roll <= 95)  max_dist = 50.0f;   // distant
    else                  max_dist = 200.0f;  // far

    // Find a target system (local or remote)
    auto* cur_sys = current_system(nav);
    StarSystem* target_sys = cur_sys;
    float dist = 0.0f;

    if (max_dist > 0.0f && cur_sys) {
        auto candidates = nearby_systems(nav, max_dist);
        if (!candidates.empty()) {
            // Pick a random candidate, weighted toward closer ones
            int idx = std::uniform_int_distribution<int>(0, static_cast<int>(candidates.size()) - 1)(rng);
            target_sys = candidates[idx].second;
            dist = candidates[idx].first;
        }
    }

    // Pick a body from the target system
    std::string body_name;
    int body_index = -1;
    std::string system_name;
    if (target_sys) {
        auto [bn, bi] = pick_body(*target_sys, rng);
        body_name = bn;
        body_index = bi;
        system_name = target_sys->name;
    }

    float mult = distance_reward_mult(dist);
    bool is_remote = (target_sys && cur_sys && target_sys->id != cur_sys->id);
    std::string location_hint = is_remote
        ? " in the " + system_name + " system"
        : "";

    // Generate quest by role
    Quest q;
    if (role == "Station Keeper") {
        int pick = std::uniform_int_distribution<int>(0, 1)(rng);
        if (pick == 0 && !body_name.empty()) {
            q = generate_scout_quest(body_name, rng);
            if (is_remote)
                q.description = q.description.substr(0, q.description.find(" Travel to"))
                    + " Travel to " + body_name + location_hint + " and report back.";
        } else {
            q = generate_deliver_quest("Merchant", rng);
        }
    } else if (role == "Merchant") {
        int pick = std::uniform_int_distribution<int>(0, 1)(rng);
        if (pick == 0)
            q = generate_fetch_quest(rng);
        else
            q = generate_deliver_quest(npc_name, rng);
    } else if (role == "Drifter") {
        int pick = std::uniform_int_distribution<int>(0, 2)(rng);
        if (pick == 0) {
            q = generate_kill_quest(rng);
            if (!body_name.empty()) {
                q.description += location_hint.empty()
                    ? " Look near " + body_name + "."
                    : " Head to " + body_name + location_hint + ".";
            }
        } else if (!body_name.empty()) {
            q = generate_scout_quest(body_name, rng);
            if (is_remote)
                q.description = q.description.substr(0, q.description.find(" Travel to"))
                    + " Travel to " + body_name + location_hint + " and report back.";
        } else {
            q = generate_kill_quest(rng);
        }
    } else {
        q = generate_kill_quest(rng);
    }

    // Set target location for map markers (scout and kill quests with locations)
    if (target_sys && body_index >= 0 && !body_name.empty()) {
        q.target_system_id = target_sys->id;
        q.target_body_index = body_index;
    }

    // Scale rewards by distance
    q.reward.xp = static_cast<int>(q.reward.xp * mult);
    q.reward.credits = static_cast<int>(q.reward.credits * mult);

    // Add danger bonus from target system
    if (target_sys && target_sys->danger_level > 3) {
        q.reward.xp += (target_sys->danger_level - 3) * 10;
        q.reward.credits += (target_sys->danger_level - 3) * 5;
    }

    return q;
}

void QuestManager::init_from_catalog(Game& game) {
    locked_.clear();
    available_.clear();
    // active_/completed_ preserved; caller responsible.

    const auto& catalog = story_quest_catalog();
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        // Copy chain/reveal metadata from StoryQuest into the Quest struct
        q.arc_id = sq->arc_id();
        q.prerequisite_ids = sq->prerequisite_ids();
        q.reveal = sq->reveal_policy();
        // Skip if already in active_/completed_
        bool already = false;
        for (const auto& a : active_)    if (a.id == q.id) { already = true; break; }
        if (!already) for (const auto& c : completed_) if (c.id == q.id) { already = true; break; }
        if (already) continue;

        if (!sq->prerequisite_ids().empty()) {
            q.status = QuestStatus::Locked;
            locked_.push_back(std::move(q));
        } else if (sq->offer_mode() == OfferMode::Auto) {
            sq->on_unlocked(game);
            q.status = QuestStatus::Active;
            q.accepted_tick = 0;
            std::string id_copy = q.id;
            active_.push_back(std::move(q));
            sq->on_accepted(game);
            pending_announcements_.push_back(std::move(id_copy));
        } else {
            q.status = QuestStatus::Available;
            available_.push_back(std::move(q));
        }
    }
}

std::vector<const Quest*> QuestManager::available_for_role(const std::string& role) const {
    std::vector<const Quest*> out;
    if (role.empty()) return out;
    for (const auto& q : available_) {
        StoryQuest* sq = find_story_quest(q.id);
        if (!sq) continue;
        const std::string giver = sq->offer_giver_role();
        if (giver.empty()) continue;  // Tutorial-only or special quests
        if (giver == role) out.push_back(&q);
    }
    return out;
}

bool QuestManager::accept_available(const std::string& quest_id, Game& game, int world_tick) {
    for (auto it = available_.begin(); it != available_.end(); ++it) {
        if (it->id != quest_id) continue;
        Quest q = std::move(*it);
        available_.erase(it);
        StoryQuest* sq = find_story_quest(quest_id);
        accept_quest(std::move(q), world_tick, game.player());
        if (sq) sq->on_accepted(game);
        return true;
    }
    return false;
}

void QuestManager::restore(std::vector<Quest> locked,
                           std::vector<Quest> available,
                           std::vector<Quest> active,
                           std::vector<Quest> completed) {
    locked_ = std::move(locked);
    available_ = std::move(available);
    active_ = std::move(active);
    completed_ = std::move(completed);
}

void QuestManager::reconcile_with_catalog(Game& game) {
    const auto& catalog = story_quest_catalog();
    std::unordered_set<std::string> seen;
    for (const auto& q : locked_)    seen.insert(q.id);
    for (const auto& q : available_) seen.insert(q.id);
    for (const auto& q : active_)    seen.insert(q.id);
    for (const auto& q : completed_) seen.insert(q.id);

    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        q.arc_id = sq->arc_id();
        q.prerequisite_ids = sq->prerequisite_ids();
        q.reveal = sq->reveal_policy();
        if (seen.count(q.id)) continue;

        bool prereqs_ok = true;
        for (const auto& p : sq->prerequisite_ids()) {
            bool done = false;
            for (const auto& c : completed_) {
                if (c.id == p && c.status == QuestStatus::Completed) { done = true; break; }
            }
            if (!done) { prereqs_ok = false; break; }
        }

        if (!prereqs_ok) {
            q.status = QuestStatus::Locked;
            locked_.push_back(std::move(q));
        } else if (sq->offer_mode() == OfferMode::Auto) {
            sq->on_unlocked(game);
            q.status = QuestStatus::Active;
            active_.push_back(std::move(q));
            sq->on_accepted(game);
        } else {
            q.status = QuestStatus::Available;
            available_.push_back(std::move(q));
        }
    }

    // Backfill chain/reveal fields on existing pool entries for older saves
    auto patch = [](std::vector<Quest>& pool) {
        for (auto& q : pool) {
            StoryQuest* sq = find_story_quest(q.id);
            if (!sq) continue;
            if (q.arc_id.empty()) q.arc_id = sq->arc_id();
            if (q.prerequisite_ids.empty()) q.prerequisite_ids = sq->prerequisite_ids();
            q.reveal = sq->reveal_policy();
        }
    };
    patch(locked_);
    patch(available_);
    patch(active_);
    patch(completed_);
}

} // namespace astra
