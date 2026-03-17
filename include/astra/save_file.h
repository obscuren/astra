#pragma once

#include "astra/npc.h"
#include "astra/player.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace astra {

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
};

struct SaveData {
    uint32_t version = 1;
    uint32_t seed = 0;
    int world_tick = 0;
    bool dead = false;
    Player player;
    std::vector<MapState> maps;
    uint32_t current_map_id = 0;
    int current_region = -1;
    int active_tab = 0;
    bool panel_visible = true;
    std::deque<std::string> messages;
    std::string death_message;
};

std::filesystem::path save_directory();
std::vector<SaveSlot> list_saves();
bool write_save(const std::string& name, const SaveData& data);
bool read_save(const std::string& name, SaveData& data);
bool delete_save(const std::string& name);

} // namespace astra
