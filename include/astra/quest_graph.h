#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

namespace astra {

class StoryQuest;

class QuestGraph {
public:
    // Build from the live story_quest_catalog().
    void build();

    // Build from an explicit catalog (used by validator to avoid recursion).
    void build_from(const std::vector<std::unique_ptr<StoryQuest>>& catalog);

    const std::vector<std::string>& dependents_of(const std::string& id) const;
    const std::vector<std::string>& prerequisites_of(const std::string& id) const;

    // Returns "" if unknown.
    std::string arc_of(const std::string& id) const;

    // Quests in an arc, topologically sorted (roots first). Empty if arc unknown.
    const std::vector<std::string>& arc_members(const std::string& arc_id) const;

    // True if the id is registered (catalog membership).
    bool contains(const std::string& id) const;

    // Transitive forward set (all dependents, dependents-of-dependents, ...).
    std::unordered_set<std::string> descendants_of(const std::string& id) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> dependents_;
    std::unordered_map<std::string, std::vector<std::string>> prerequisites_;
    std::unordered_map<std::string, std::string> arc_by_id_;
    std::unordered_map<std::string, std::vector<std::string>> arc_members_;
    std::unordered_set<std::string> ids_;
    std::vector<std::string> empty_;
};

// Global accessor (built lazily on first call).
const QuestGraph& quest_graph();
void rebuild_quest_graph();

} // namespace astra
