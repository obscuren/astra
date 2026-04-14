#include "astra/quest.h"
#include "astra/quest_graph.h"

#include <unordered_map>
#include <unordered_set>

namespace astra {

namespace {

enum class Color : uint8_t { White, Gray, Black };

bool has_cycle_dfs(const std::string& id,
                   std::unordered_map<std::string, Color>& color,
                   const QuestGraph& g) {
    color[id] = Color::Gray;
    for (const auto& dep : g.dependents_of(id)) {
        auto c = color[dep];
        if (c == Color::Gray) return true;
        if (c == Color::White && has_cycle_dfs(dep, color, g)) return true;
    }
    color[id] = Color::Black;
    return false;
}

} // namespace

std::vector<std::string> validate_quest_catalog(
    const std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    std::vector<std::string> errors;

    QuestGraph g;
    g.build_from(catalog);

    // Collect ids and check duplicates
    std::unordered_set<std::string> ids;
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        if (!ids.insert(q.id).second) {
            errors.push_back("Duplicate quest id: " + q.id);
        }
    }

    // Check prerequisites and offer_giver_role
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        for (const auto& p : sq->prerequisite_ids()) {
            if (!ids.count(p)) {
                errors.push_back("Quest '" + q.id + "' has unknown prerequisite '" + p + "'");
            }
        }
        if (sq->offer_mode() == OfferMode::NpcOffer
            && sq->offer_giver_role().empty()
            && !sq->prerequisite_ids().empty()) {
            errors.push_back("Quest '" + q.id + "' uses NpcOffer but declares no offer_giver_role");
        }
    }

    // Cycle detection
    std::unordered_map<std::string, Color> color;
    for (const auto& id : ids) color[id] = Color::White;
    for (const auto& id : ids) {
        if (color[id] == Color::White) {
            if (has_cycle_dfs(id, color, g)) {
                errors.push_back("Cycle detected involving quest '" + id + "'");
                break;
            }
        }
    }

    return errors;
}

} // namespace astra
