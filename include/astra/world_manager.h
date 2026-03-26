#pragma once

#include "astra/item.h"
#include "astra/npc.h"
#include "astra/star_chart.h"
#include "astra/tilemap.h"
#include "astra/time_of_day.h"
#include "astra/visibility_map.h"

#include <cstdint>
#include <map>
#include <random>
#include <tuple>

namespace astra {

using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int>;

struct LocationState {
    TileMap map;
    VisibilityMap visibility;
    std::vector<Npc> npcs;
    std::vector<GroundItem> ground_items;
    int player_x = 0;
    int player_y = 0;
};

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

    int& world_tick() { return world_tick_; }
    int world_tick() const { return world_tick_; }

    DayClock& day_clock() { return day_clock_; }
    const DayClock& day_clock() const { return day_clock_; }

    int& current_region() { return current_region_; }
    int current_region() const { return current_region_; }

    unsigned& seed() { return seed_; }
    unsigned seed() const { return seed_; }
    std::mt19937& rng() { return rng_; }

    NavigationData& navigation() { return navigation_; }
    const NavigationData& navigation() const { return navigation_; }

    std::map<LocationKey, LocationState>& location_cache() { return location_cache_; }
    const std::map<LocationKey, LocationState>& location_cache() const { return location_cache_; }
    static inline const LocationKey ship_key = {0, -2, -1, false, -1, -1, 0};

private:
    TileMap map_;
    VisibilityMap visibility_;
    std::vector<Npc> npcs_;
    std::vector<GroundItem> ground_items_;
    std::vector<Item> stash_;
    SurfaceMode surface_mode_ = SurfaceMode::Dungeon;
    int overworld_x_ = 0;
    int overworld_y_ = 0;
    int world_tick_ = 0;
    DayClock day_clock_;
    int current_region_ = -1;
    unsigned seed_ = 0;
    std::mt19937 rng_;
    NavigationData navigation_;
    std::map<LocationKey, LocationState> location_cache_;
};

} // namespace astra
