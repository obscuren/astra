#include "astra/world_manager.h"

namespace astra {

bool WorldManager::world_flag(const std::string& name) const {
    auto it = world_flags_.find(name);
    return it != world_flags_.end() && it->second;
}

void WorldManager::set_world_flag(const std::string& name, bool value) {
    world_flags_[name] = value;
}

const DungeonRecipe* WorldManager::find_dungeon_recipe(const LocationKey& root) const {
    auto it = dungeon_recipes_.find(root);
    return (it == dungeon_recipes_.end()) ? nullptr : &it->second;
}

} // namespace astra
