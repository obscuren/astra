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

    // Single pass: collect ids, check duplicates
    std::vector<std::string> ordered_ids;
    ordered_ids.reserve(catalog.size());
    std::unordered_set<std::string> ids;
    for (const auto& sq : catalog) {
        std::string id = sq->create_quest().id;
        if (!ids.insert(id).second) {
            errors.push_back("Duplicate quest id: " + id);
        }
        ordered_ids.push_back(std::move(id));
    }

    // Prereq + offer checks (no second create_quest())
    for (size_t i = 0; i < catalog.size(); ++i) {
        const auto& sq = catalog[i];
        const auto& id = ordered_ids[i];
        for (const auto& p : sq->prerequisite_ids()) {
            if (!ids.count(p)) {
                errors.push_back("Quest '" + id + "' has unknown prerequisite '" + p + "'");
            }
        }
        if (sq->offer_mode() == OfferMode::NpcOffer
            && sq->offer_giver_role().empty()
            && !sq->prerequisite_ids().empty()) {
            errors.push_back("Quest '" + id + "' uses NpcOffer but declares no offer_giver_role");
        }
    }

    // Cycle detection in deterministic catalog order
    std::unordered_map<std::string, Color> color;
    for (const auto& id : ordered_ids) color[id] = Color::White;
    for (const auto& id : ordered_ids) {
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
