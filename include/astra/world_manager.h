#pragma once

#include "astra/dungeon_recipe.h"
#include "astra/item.h"
#include "astra/lan.h"
#include "astra/location_key.h"
#include "astra/lore_influence_map.h"
#include "astra/lore_types.h"
#include "astra/npc.h"
#include "astra/star_chart.h"
#include "astra/tilemap.h"
#include "astra/time_of_day.h"
#include "astra/ground_effect.h"
#include "astra/noise_event.h"
#include "astra/trap.h"
#include "astra/visibility_map.h"

#include <array>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace astra {

static constexpr int zones_per_tile = 3; // 3x3 zone grid per overworld tile

struct LocationState {
    TileMap map;
    VisibilityMap visibility;
    std::vector<Npc> npcs;
    std::vector<GroundItem> ground_items;
    std::vector<Trap> traps;
    std::vector<NoiseEvent> noise_events;
    std::vector<GroundEffect> ground_effects;   // NEW
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
    int target_moon_index = -1;            // star chart marker: moon (-1 = body)
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

    // Stable UID allocation — assigns the next monotonic UID (never reused).
    int32_t allocate_npc_uid() { return next_npc_uid_++; }
    int32_t next_npc_uid() const { return next_npc_uid_; }
    void    set_next_npc_uid(int32_t v) { next_npc_uid_ = v; }

    // Add an NPC to the world, assigning it a stable UID if not already set.
    // Returns a reference to the stored NPC (valid until npcs_ is modified).
    Npc& add_npc(Npc&& npc);

    // Look up a live NPC by stable UID. Returns nullptr if not found.
    Npc*       npc_by_uid(int32_t uid);
    const Npc* npc_by_uid(int32_t uid) const;

    std::vector<GroundItem>& ground_items() { return ground_items_; }
    const std::vector<GroundItem>& ground_items() const { return ground_items_; }

    std::vector<Trap>& traps() { return traps_; }
    const std::vector<Trap>& traps() const { return traps_; }

    std::vector<NoiseEvent>& noise_events() { return noise_events_; }
    const std::vector<NoiseEvent>& noise_events() const { return noise_events_; }

    std::vector<GroundEffect>& ground_effects() { return ground_effects_; }
    const std::vector<GroundEffect>& ground_effects() const { return ground_effects_; }

    std::vector<Item>& stash() { return stash_; }
    const std::vector<Item>& stash() const { return stash_; }
    static constexpr int max_stash_size = 20;

    SurfaceMode surface_mode() const { return surface_mode_; }
    void set_surface_mode(SurfaceMode m) { surface_mode_ = m; }
    bool on_overworld() const { return surface_mode_ == SurfaceMode::Overworld; }
    bool on_detail_map() const { return surface_mode_ == SurfaceMode::DetailMap; }
    bool is_outdoor() const { return on_overworld() || on_detail_map(); }

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

    // Plan 5 Cut 3: galaxy generation index. Starts at 0 for a fresh save.
    // RebirthSequence::apply() bumps this on every Sgr A* crossing so newly
    // registered WarpAnchorRecords carry the new id and old ones can be flagged
    // un-warpable by ConsciousnessSave::mark_past_galaxy_unwarpable.
    uint16_t galaxy_id() const { return galaxy_id_; }
    void     set_galaxy_id(uint16_t id) { galaxy_id_ = id; }

    WorldLore& lore() { return lore_; }
    const WorldLore& lore() const { return lore_; }

    NavigationData& navigation() { return navigation_; }
    const NavigationData& navigation() const { return navigation_; }

    // Plan 5.5: per-map LAN persistence. `lan_metadata()` returns the active
    // map's metadata. The active map is identified by `current_lan_key_`,
    // which `Game::on_map_loaded()` sets via `switch_active_lan(key)` on every
    // map transition. Mutations recorded in the active LanMetadata (cracked
    // firewalls, looted DataNodes, decrypted EncryptedFiles, killed ICE)
    // survive cross-map round-trips because each map has its own bucket in
    // `lan_metadatas_`.
    //
    // Mutating accessor: lazy-creates an entry for `current_lan_key_` on first
    // access. Const accessor: returns a const ref to the matching entry, or to
    // a shared empty fallback if no entry exists yet.
    LanMetadata&       lan_metadata();
    const LanMetadata& lan_metadata() const;

    // Direct access to the per-key map (used by save/load).
    using LanMetadataMap = std::unordered_map<LocationKey, LanMetadata, LocationKeyHash>;
    LanMetadataMap&       lan_metadatas()       { return lan_metadatas_; }
    const LanMetadataMap& lan_metadatas() const { return lan_metadatas_; }

    const LocationKey& current_lan_key() const { return current_lan_key_; }
    void set_current_lan_key(const LocationKey& k) { current_lan_key_ = k; }

    // Switch the active LAN to `key`. If no entry exists for `key` yet, a
    // fresh LanMetadata is constructed and `register_hackables_in_lan` is
    // run to populate it from the active map. If an entry already exists,
    // the call simply re-points the active key — the prior LAN's nodes /
    // mutations stay intact in `grid_network_` and `lan_metadatas_`.
    //
    // Called by `Game::on_map_loaded()` on every map transition.
    void switch_active_lan(const LocationKey& key);

    OverworldReturnPos& overworld_return() { return overworld_return_; }
    const OverworldReturnPos& overworld_return() const { return overworld_return_; }

    // Plan 5.5: derive the LocationKey for the active map, mirroring the
    // logic used by `Game::save_current_location()`. Single source of truth
    // for "which map is the player on". Used by `Game::on_map_loaded()` to
    // identify the active LAN bucket in `lan_metadatas_`.
    LocationKey current_location_key() const;

    const LoreInfluenceMap& lore_influence() const { return lore_influence_; }
    void set_lore_influence(LoreInfluenceMap m) { lore_influence_ = std::move(m); }

    std::map<LocationKey, LocationState>& location_cache() { return location_cache_; }
    const std::map<LocationKey, LocationState>& location_cache() const { return location_cache_; }
    static inline const LocationKey ship_key = {0, -2, -1, false, -1, -1, 0};
    static inline const LocationKey maintenance_key = {0, -3, -1, false, -1, -1, 0};

    // Quest-triggered world modification
    std::map<LocationKey, QuestLocationMeta>& quest_locations() { return quest_locations_; }
    const std::map<LocationKey, QuestLocationMeta>& quest_locations() const { return quest_locations_; }

    // Dungeon recipe registry — controls procedural generation for keyed locations
    std::map<LocationKey, DungeonRecipe>& dungeon_recipes() { return dungeon_recipes_; }
    const std::map<LocationKey, DungeonRecipe>& dungeon_recipes() const { return dungeon_recipes_; }
    const DungeonRecipe* find_dungeon_recipe(const LocationKey& root) const;

    std::set<LocationKey>& pending_quest_cleanup() { return pending_quest_cleanup_; }
    const std::set<LocationKey>& pending_quest_cleanup() const { return pending_quest_cleanup_; }

    // Stellar Signal arc state
    std::array<uint32_t, 3>& stellar_signal_echo_ids() { return stellar_signal_echo_ids_; }
    const std::array<uint32_t, 3>& stellar_signal_echo_ids() const { return stellar_signal_echo_ids_; }
    uint32_t& stellar_signal_beacon_id() { return stellar_signal_beacon_id_; }
    uint32_t stellar_signal_beacon_id() const { return stellar_signal_beacon_id_; }

    // Scenario world flags — string-keyed boolean state flipped by scenarios.
    // Persisted in the save file. Examples: "stage4_active".
    bool world_flag(const std::string& name) const;
    void set_world_flag(const std::string& name, bool value);
    const std::unordered_map<std::string, bool>& world_flags() const { return world_flags_; }
    std::unordered_map<std::string, bool>& world_flags() { return world_flags_; }

    // Systems the player has already been ambushed in during current Stage 4 run.
    // Used so each system spawns its Conclave ambush at most once.
    const std::unordered_set<uint32_t>& ambushed_systems() const { return ambushed_systems_; }
    std::unordered_set<uint32_t>& ambushed_systems() { return ambushed_systems_; }

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

    // Dev path (testing only). Wipes the active LAN's persisted runtime
    // state (cracked firewalls, looted nodes, decrypted files, killed ICE),
    // drops every Subnet + LanRoot node belonging to this LAN, and re-runs
    // register_hackables_in_lan to rebuild. Documented as destructive in
    // the :spawn help text.
    //
    // Plan 5.5: only touches the LAN identified by `current_lan_key_`;
    // sibling maps' LANs are untouched.
    void lan_full_reset();

    // Find a Hackable on the active map (fixtures + alive NPCs) by its packed IP.
    // Returns nullptr if no match. Used by `ping <ip>` and `jack <ip>`.
    const Hackable* find_hackable_by_ip(uint32_t ip) const;
    Hackable*       find_hackable_by_ip(uint32_t ip);

private:
    int32_t next_npc_uid_ = 1;   // 0 / negative reserved as "invalid"; 1 is first valid UID
    TileMap map_;
    VisibilityMap visibility_;
    std::vector<Npc> npcs_;
    std::vector<GroundItem> ground_items_;
    std::vector<Trap> traps_;
    std::vector<NoiseEvent> noise_events_;
    std::vector<GroundEffect> ground_effects_;
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
    uint16_t galaxy_id_ = 0;
    NavigationData navigation_;
    // Plan 5.5: per-map LAN persistence. `lan_metadatas_` is keyed by the
    // same LocationKey used by `location_cache_`. `current_lan_key_` selects
    // the active map's bucket; `lan_metadata()` returns it (lazy-creating
    // on first mutating access). `empty_lan_` is an immutable read-only
    // fallback for the rare const-access call before any LAN has been set
    // up (e.g. during save/load bootstrap).
    LanMetadataMap lan_metadatas_;
    LocationKey current_lan_key_{};
    LanMetadata empty_lan_{};
    OverworldReturnPos overworld_return_;
    WorldLore lore_;
    std::map<LocationKey, LocationState> location_cache_;
    std::map<LocationKey, QuestLocationMeta> quest_locations_;
    std::map<LocationKey, DungeonRecipe> dungeon_recipes_;
    std::set<LocationKey> pending_quest_cleanup_;
    LoreInfluenceMap lore_influence_;
    // Arc-specific state
    std::array<uint32_t, 3> stellar_signal_echo_ids_ = {0, 0, 0};
    uint32_t stellar_signal_beacon_id_ = 0;
    // Scenario state
    std::unordered_map<std::string, bool> world_flags_;
    std::unordered_set<uint32_t> ambushed_systems_;
};

} // namespace astra
