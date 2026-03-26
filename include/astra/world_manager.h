#pragma once

#include "astra/item.h"
#include "astra/npc.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

#include <cstdint>

namespace astra {

enum class SurfaceMode : uint8_t {
    Dungeon,
    DetailMap,
    Overworld,
};

class WorldManager {
public:
    WorldManager() = default;

    TileMap& map() { return map_; }
    const TileMap& map() const { return map_; }

    VisibilityMap& visibility() { return visibility_; }
    const VisibilityMap& visibility() const { return visibility_; }

    std::vector<Npc>& npcs() { return npcs_; }
    const std::vector<Npc>& npcs() const { return npcs_; }

    std::vector<GroundItem>& ground_items() { return ground_items_; }
    const std::vector<GroundItem>& ground_items() const { return ground_items_; }

    std::vector<Item>& stash() { return stash_; }
    const std::vector<Item>& stash() const { return stash_; }
    static constexpr int max_stash_size = 20;

    SurfaceMode surface_mode() const { return surface_mode_; }
    void set_surface_mode(SurfaceMode m) { surface_mode_ = m; }
    bool on_overworld() const { return surface_mode_ == SurfaceMode::Overworld; }
    bool on_detail_map() const { return surface_mode_ == SurfaceMode::DetailMap; }

    int& overworld_x() { return overworld_x_; }
    int& overworld_y() { return overworld_y_; }

private:
    TileMap map_;
    VisibilityMap visibility_;
    std::vector<Npc> npcs_;
    std::vector<GroundItem> ground_items_;
    std::vector<Item> stash_;
    SurfaceMode surface_mode_ = SurfaceMode::Dungeon;
    int overworld_x_ = 0;
    int overworld_y_ = 0;
};

} // namespace astra
