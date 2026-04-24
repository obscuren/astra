#pragma once

#include "astra/lore_types.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/poi_budget.h"
#include "astra/poi_placement.h"
#include "astra/quest.h"
#include "astra/star_chart.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"
#include "astra/world_manager.h"

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astra {

// Current save-file schema version. Pre-release: saves with any other
// version are rejected on load; no backward-compatibility or migration code.
inline constexpr uint32_t SAVE_FILE_VERSION = 45;   // v45: persisted ability bar slots

struct SaveSlot {
    std::string filename;    // stem, e.g. "save_12345"
    std::string location;
    int player_level = 0;
    int world_tick = 0;
    int kills = 0;
    int xp = 0;
    int money = 0;
    uint32_t timestamp = 0;
    bool dead = false;
    bool valid = false;
    std::string death_message;
};

struct MapState {
    uint32_t map_id = 0;
    TileMap tilemap;
    VisibilityMap visibility;
    std::vector<Npc> npcs;
    std::vector<GroundItem> ground_items;

    // v23: POI budget, hidden POIs, anchor hints carried on the overworld map.
    PoiBudget poi_budget;
    std::vector<HiddenPoi> hidden_pois;
    std::vector<std::pair<uint64_t, PoiAnchorHint>> anchor_hints;

    // v24: location cache persistence — LocationKey fields + player position.
    // maps[0] is the active map (key fields zeroed), maps[1+] are cached.
    uint32_t loc_system_id = 0;
    int loc_body_index = 0;
    int loc_moon_index = 0;
    bool loc_is_station = false;
    int loc_ow_x = 0;
    int loc_ow_y = 0;
    int loc_depth = 0;
    int player_x = 0;
    int player_y = 0;
};

struct SaveData {
    uint32_t version = SAVE_FILE_VERSION;   // v39: Archive dungeon migration
    uint32_t seed = 0;
    int world_tick = 0;
    bool dead = false;
    Player player;
    std::vector<MapState> maps;
    uint32_t current_map_id = 0;
    int current_region = -1;
    uint8_t active_widgets = 1; // bitfield — Messages on by default
    uint8_t focused_widget = 0;
    bool panel_visible = true;
    std::deque<std::string> messages;
    std::string death_message;
    std::vector<Item> stash;
    NavigationData navigation;
    uint8_t surface_mode = 0;  // 0=Dungeon, 1=DetailMap, 2=Overworld
    int overworld_x = 0;
    int overworld_y = 0;
    int zone_x = 1;
    int zone_y = 1;
    bool lost = false;
    int lost_moves = 0;
    int local_tick = 0;
    int local_ticks_per_day = 200;

    // v13: quest state
    std::vector<Quest> locked_quests;
    std::vector<Quest> available_quests;
    std::vector<Quest> active_quests;
    std::vector<Quest> completed_quests;
    std::map<LocationKey, QuestLocationMeta> quest_locations;
    std::set<LocationKey> pending_quest_cleanup;

    // v33: stellar_signal arc ids
    std::array<uint32_t, 3> stellar_signal_echo_ids = {0, 0, 0};
    uint32_t stellar_signal_beacon_id = 0;

    // v34: scenario world flags and per-run ambushed systems set
    std::unordered_map<std::string, bool> world_flags;
    std::unordered_set<uint32_t> ambushed_systems;

    // v37: persisted dungeon recipes keyed by surface-root LocationKey
    std::map<LocationKey, DungeonRecipe> dungeon_recipes;

    // v20: world lore
    WorldLore lore;

    // Overworld return position — set when the player boards their ship
    // from a planet overworld, cleared when they disembark.
    bool overworld_return_valid = false;
    int overworld_return_x = 0;
    int overworld_return_y = 0;
    LocationKey overworld_return_body_key{};
};

std::filesystem::path save_directory();
std::vector<SaveSlot> list_saves();
bool write_save(const std::string& name, const SaveData& data);
bool read_save(const std::string& name, SaveData& data);
bool delete_save(const std::string& name);

} // namespace astra
