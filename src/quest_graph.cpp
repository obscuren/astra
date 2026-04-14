#include "astra/quest_graph.h"
#include "astra/quest.h"

#include <algorithm>
#include <memory>

namespace astra {

void QuestGraph::build() {
    build_from(story_quest_catalog());
}

void QuestGraph::build_from(const std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    dependents_.clear();
    prerequisites_.clear();
    arc_by_id_.clear();
    arc_members_.clear();
    ids_.clear();

    // First pass: ids, arcs, prerequisite edges
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        const std::string& id = q.id;
        ids_.insert(id);
        std::string arc = sq->arc_id();
        if (!arc.empty()) {
            arc_by_id_[id] = arc;
            arc_members_[arc].push_back(id);
        }
        auto prereqs = sq->prerequisite_ids();
        prerequisites_[id] = prereqs;
        for (const auto& p : prereqs) {
            dependents_[p].push_back(id);
        }
    }

    // Topological sort within each arc (Kahn's)
    for (auto& [arc, members] : arc_members_) {
        std::unordered_map<std::string, int> indeg;
        for (const auto& m : members) {
            indeg[m] = 0;
        }
        for (const auto& m : members) {
            for (const auto& p : prerequisites_[m]) {
                if (indeg.count(p)) indeg[m]++;
            }
        }
        std::vector<std::string> sorted;
        std::vector<std::string> ready;
        for (auto& [id, d] : indeg) {
            if (d == 0) ready.push_back(id);
        }
        std::sort(ready.begin(), ready.end());
        while (!ready.empty()) {
            auto cur = ready.front();
            ready.erase(ready.begin());
            sorted.push_back(cur);
            for (const auto& dep : dependents_[cur]) {
                if (!indeg.count(dep)) continue;
                if (--indeg[dep] == 0) ready.push_back(dep);
            }
            std::sort(ready.begin(), ready.end());
        }
        if (sorted.size() == members.size()) {
            members = std::move(sorted);
        }
        // If cycle: leave members unsorted; validator will report.
    }
}

const std::vector<std::string>& QuestGraph::dependents_of(const std::string& id) const {
    auto it = dependents_.find(id);
    return it == dependents_.end() ? empty_ : it->second;
}

const std::vector<std::string>& QuestGraph::prerequisites_of(const std::string& id) const {
    auto it = prerequisites_.find(id);
    return it == prerequisites_.end() ? empty_ : it->second;
}

std::string QuestGraph::arc_of(const std::string& id) const {
    auto it = arc_by_id_.find(id);
    return it == arc_by_id_.end() ? std::string() : it->second;
}

const std::vector<std::string>& QuestGraph::arc_members(const std::string& arc_id) const {
    auto it = arc_members_.find(arc_id);
    return it == arc_members_.end() ? empty_ : it->second;
}

bool QuestGraph::contains(const std::string& id) const {
    return ids_.count(id) > 0;
}

std::unordered_set<std::string> QuestGraph::descendants_of(const std::string& id) const {
    std::unordered_set<std::string> out;
    std::vector<std::string> stack{id};
    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();
        for (const auto& dep : dependents_of(cur)) {
            if (out.insert(dep).second) stack.push_back(dep);
        }
    }
    return out;
}

static std::unique_ptr<QuestGraph> g_graph;

const QuestGraph& quest_graph() {
    if (!g_graph) {
        g_graph = std::make_unique<QuestGraph>();
        g_graph->build();
    }
    return *g_graph;
}

void rebuild_quest_graph() {
    if (!g_graph) g_graph = std::make_unique<QuestGraph>();
    g_graph->build();
}

} // namespace astra
