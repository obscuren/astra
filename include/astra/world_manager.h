#pragma once

#include "astra/item.h"
#include "astra/lore_influence_map.h"
#include "astra/lore_types.h"
#include "astra/npc.h"
#include "astra/star_chart.h"
#include "astra/tilemap.h"
#include "astra/time_of_day.h"
#include "astra/visibility_map.h"

#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <tuple>

namespace astra {

// LocationKey: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}
using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int>;

static constexpr int zones_per_tile = 3; // 3x3 zone grid per overworld tile

struct LocationState {
    TileMap map;
    VisibilityMap visibility;
    std::vector<Npc> npcs;
    std::vector<GroundItem> ground_items;
    int player_x = 0;
    int player_y = 0;
};

struct QuestFixturePlacement {
    std::string fixture_id;   // registry key
    int x = -1;               // -1 = unresolved; resolver picks + writes back
    int y = -1;
};

struct QuestLocationMeta {
    std::string quest_id;
    std::string quest_title;               // display name for markers
    int difficulty_override = -1;          // -1 = use default
    std::vector<std::string> npc_roles;    // specific NPCs to spawn
    std::vector<std::string> quest_items;  // items to place on ground
    std::vector<QuestFixturePlacement> fixtures;  // quest-driven fixtures
    Tile poi_type = Tile::Empty;           // overworld stamp to place
    bool remove_on_completion = false;     // clean up after quest done
    uint32_t target_system_id = 0;         // star chart marker: system
    int target_body_index = -1;            // star chart marker: body
};

enum class SurfaceMode : uint8_t {
    Dungeon,
    DetailMap,
    Overworld,
};

// Records where the player was standing on a planet overworld when they
// triggered Board Ship from the Ship tab. When the player later disembarks
// (without warping to a different body first), they're restored to this
// exact tile. Cleared when the player warps elsewhere or returns.
struct OverworldReturnPos {
    bool valid = false;
    int x = 0;
    int y = 0;
    LocationKey body_key{};
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
    int& zone_x() { return zone_x_; }
    int& zone_y() { return zone_y_; }
    int zone_x() const { return zone_x_; }
    int zone_y() const { return zone_y_; }

    int& world_tick() { return world_tick_; }
    int world_tick() const { return world_tick_; }

    DayClock& day_clock() { return day_clock_; }
    const DayClock& day_clock() const { return day_clock_; }

    int& current_region() { return current_region_; }
    int current_region() const { return current_region_; }

    unsigned& seed() { return seed_; }
    unsigned seed() const { return seed_; }
    std::mt19937& rng() { return rng_; }

    WorldLore& lore() { return lore_; }
    const WorldLore& lore() const { return lore_; }

    NavigationData& navigation() { return navigation_; }
    const NavigationData& navigation() const { return navigation_; }

    OverworldReturnPos& overworld_return() { return overworld_return_; }
    const OverworldReturnPos& overworld_return() const { return overworld_return_; }

    const LoreInfluenceMap& lore_influence() const { return lore_influence_; }
    void set_lore_influence(LoreInfluenceMap m) { lore_influence_ = std::move(m); }

    std::map<LocationKey, LocationState>& location_cache() { return location_cache_; }
    const std::map<LocationKey, LocationState>& location_cache() const { return location_cache_; }
    static inline const LocationKey ship_key = {0, -2, -1, false, -1, -1, 0};
    static inline const LocationKey maintenance_key = {0, -3, -1, false, -1, -1, 0};

    // Quest-triggered world modification
    std::map<LocationKey, QuestLocationMeta>& quest_locations() { return quest_locations_; }
    const std::map<LocationKey, QuestLocationMeta>& quest_locations() const { return quest_locations_; }

    std::set<LocationKey>& pending_quest_cleanup() { return pending_quest_cleanup_; }
    const std::set<LocationKey>& pending_quest_cleanup() const { return pending_quest_cleanup_; }

    // Collect system IDs that have active quest targets
    std::set<uint32_t> quest_target_system_ids() const {
        std::set<uint32_t> ids;
        for (const auto& [key, meta] : quest_locations_) {
            if (meta.target_system_id != 0)
                ids.insert(meta.target_system_id);
        }
        return ids;
    }

    // Check if a specific body in a system is a quest target
    bool is_quest_target_body(uint32_t system_id, int body_index) const {
        for (const auto& [key, meta] : quest_locations_) {
            if (meta.target_system_id == system_id && meta.target_body_index == body_index)
                return true;
        }
        return false;
    }

    // Get quest title for a target body (empty if not a quest target)
    std::string quest_title_for_body(uint32_t system_id, int body_index) const {
        for (const auto& [key, meta] : quest_locations_) {
            if (meta.target_system_id == system_id && meta.target_body_index == body_index)
                return meta.quest_title;
        }
        return "";
    }

private:
    TileMap map_;
    VisibilityMap visibility_;
    std::vector<Npc> npcs_;
    std::vector<GroundItem> ground_items_;
    std::vector<Item> stash_;
    SurfaceMode surface_mode_ = SurfaceMode::Dungeon;
    int overworld_x_ = 0;
    int overworld_y_ = 0;
    int zone_x_ = 1;  // 0-2 within 3x3 grid, default center
    int zone_y_ = 1;
    int world_tick_ = 0;
    DayClock day_clock_;
    int current_region_ = -1;
    unsigned seed_ = 0;
    std::mt19937 rng_;
    NavigationData navigation_;
    OverworldReturnPos overworld_return_;
    WorldLore lore_;
    std::map<LocationKey, LocationState> location_cache_;
    std::map<LocationKey, QuestLocationMeta> quest_locations_;
    std::set<LocationKey> pending_quest_cleanup_;
    LoreInfluenceMap lore_influence_;
};

} // namespace astra
