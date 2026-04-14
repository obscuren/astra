#include "astra/quest_fixture.h"

#include <unordered_map>

namespace astra {

namespace {
std::unordered_map<std::string, QuestFixtureDef>& registry() {
    static std::unordered_map<std::string, QuestFixtureDef> r;
    return r;
}
}

void register_quest_fixture(QuestFixtureDef def) {
    std::string key = def.id;
    registry()[std::move(key)] = std::move(def);
}

const QuestFixtureDef* find_quest_fixture(const std::string& id) {
    auto& r = registry();
    auto it = r.find(id);
    return it == r.end() ? nullptr : &it->second;
}

void clear_quest_fixtures() {
    registry().clear();
}

} // namespace astra
