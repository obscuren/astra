#pragma once

#include "astra/action.h"
#include "astra/fov.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/save_file.h"
#include "astra/star_chart.h"
#include "astra/star_chart_viewer.h"
#include "astra/trade_window.h"
#include "astra/tile_props.h"
#include "astra/time_of_day.h"
#include "astra/map_properties.h"
#include "astra/tilemap.h"
#include "astra/ui.h"
#include "astra/visibility_map.h"
#include <deque>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <vector>

namespace astra {

enum class GameState {
    MainMenu,
    Playing,
    GameOver,
    LoadMenu,
    HallOfFame,
};

enum class SurfaceMode : uint8_t {
    Dungeon,
    DetailMap,
    Overworld,
};

enum class PanelTab : uint8_t {
    Messages,
    Inventory,
    Equipment,
    Ship,
    Wait,
};

static constexpr int panel_tab_count = 5;

class Game {
public:
    explicit Game(std::unique_ptr<Renderer> renderer);

    void run();

private:
    using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int>;

    // Input
    void handle_input(int key);
    void handle_menu_input(int key);
    void handle_play_input(int key);
    void handle_gameover_input(int key);
    void handle_load_input(int key);
    void handle_hall_input(int key);

    // Logic
    void update();
    void new_game();
    void travel_to_destination(const ChartAction& action);
    void save_current_location();
    void restore_location(const LocationKey& key);
    void enter_ship();
    void enter_detail_map();
    void exit_detail_to_overworld();
    void enter_dungeon_from_detail();
    void exit_dungeon_to_detail();
    void transition_detail_edge(int dx, int dy);
    MapProperties build_detail_props(int ow_x, int ow_y);

    // Legacy — kept for dungeon-from-overworld POI flow
    void enter_overworld_tile();
    void exit_to_overworld();
    void try_move(int dx, int dy);
    void try_interact(int dx, int dy);
    void advance_world(int cost);
    void process_npc_turn(Npc& npc);
    void attack_npc(Npc& npc);
    void begin_targeting();
    void handle_targeting_input(int key);
    void shoot_target();
    void pickup_ground_item();
    void drop_item(int index);
    void use_item(int index);
    void equip_item(int index);
    void unequip_slot(int index);
    void reload_weapon();
    void remove_dead_npcs();
    void check_player_death();
    void open_npc_dialog(Npc& npc);
    void advance_dialog(int selected);
    void interact_fixture(int fixture_id);
    void recompute_fov();
    void check_region_change();
    void save_game();
    bool load_game(const std::string& filename);

    // Rendering
    void render();
    void render_menu();
    void render_play();
    void render_gameover();
    void render_load_menu();
    void render_hall_of_fame();
    void render_stats_bar();
    void render_bars();
    void render_tabs();
    void render_map();
    void render_side_panel();
    void render_item_inspect();
    void render_effects_bar();
    void render_abilities_bar();

    // Layout
    void compute_layout();
    void compute_camera();

    // Dev tools
    void dev_warp_random();
    void dev_warp_stamp_test();

    // Helpers
    void log(const std::string& msg);
    Color hp_color() const;
    Color hunger_color() const;
    bool tile_occupied(int x, int y) const;

    std::unique_ptr<Renderer> renderer_;
    GameState state_ = GameState::MainMenu;
    bool running_ = false;

    // Menu
    int menu_selection_ = 0;
#ifdef ASTRA_DEV_MODE
    static constexpr int menu_item_count_ = 5;
#else
    static constexpr int menu_item_count_ = 4;
#endif

    // Dev mode
    bool dev_mode_ = false;
    Tile dev_warp_stamp_test_poi_ = Tile::Empty;

    // Gameplay
    unsigned seed_ = 0;
    std::mt19937 rng_;
    Player player_;
    std::vector<Npc> npcs_;
    std::vector<GroundItem> ground_items_;
    std::vector<Item> stash_;
    static constexpr int max_stash_size_ = 20;
    TileMap map_;
    VisibilityMap visibility_;
    NavigationData navigation_;
    StarChartViewer star_chart_viewer_;
    TradeWindow trade_window_;
    int camera_x_ = 0;
    int camera_y_ = 0;
    int current_region_ = -1;
    int world_tick_ = 0;
    DayClock day_clock_;

    // Tabs
    int active_tab_ = 0;
    bool panel_visible_ = true;

    // Input modes
    bool awaiting_interact_ = false;
    bool targeting_ = false;
    int target_x_ = 0, target_y_ = 0;
    int blink_phase_ = 0;
    Npc* target_npc_ = nullptr;
    int inventory_cursor_ = 0;
    int wait_cursor_ = 0;
    bool inspecting_item_ = false;
    Item inspected_item_;

    // Dialogs / interaction state
    Dialog npc_dialog_{""};
    Dialog pause_menu_;
    Npc* interacting_npc_ = nullptr;
    const std::vector<DialogNode>* dialog_tree_ = nullptr; // active tree (talk or quest)
    int dialog_node_ = -1;

    // Tracks which top-level options map to which trait action
    enum class InteractOption : uint8_t { Talk, Shop, Quest, Farewell };
    std::vector<InteractOption> interact_options_;

    // UI layout (computed from screen size)
    int screen_w_ = 0;
    int screen_h_ = 0;
    Rect screen_rect_;
    Rect stats_bar_rect_;
    Rect hp_bar_rect_;
    Rect xp_bar_rect_;
    Rect tabs_rect_;
    Rect map_rect_;
    Rect separator_rect_;
    Rect side_panel_rect_;
    Rect bottom_sep_rect_;
    Rect effects_rect_;
    Rect abilities_rect_;

    // Save/load
    std::vector<SaveSlot> save_slots_;
    int load_selection_ = 0;
    bool confirm_delete_ = false;

    // Death
    std::string death_message_;

    // Message log
    std::deque<std::string> messages_;
    static constexpr size_t max_messages_ = 200;

    // Location cache — preserves visited locations
    // Key: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}
    //   station:    {system_id, -1, -1, true, -1, -1, 0}
    //   overworld:  {system_id, body_index, moon_index, false, -1, -1, 0}
    //   detail map: {system_id, body_index, moon_index, false, ow_x, ow_y, 0}
    //   dungeon:    {system_id, body_index, moon_index, false, ow_x, ow_y, 1}
    struct LocationState {
        TileMap map;
        VisibilityMap visibility;
        std::vector<Npc> npcs;
        std::vector<GroundItem> ground_items;
        int player_x = 0;
        int player_y = 0;
    };
    static inline const LocationKey ship_key_ = {0, -2, -1, false, -1, -1, 0};
    std::map<LocationKey, LocationState> location_cache_;

    // Surface mode
    SurfaceMode surface_mode_ = SurfaceMode::Dungeon;
    bool on_overworld() const { return surface_mode_ == SurfaceMode::Overworld; }
    bool on_detail_map() const { return surface_mode_ == SurfaceMode::DetailMap; }
    int overworld_x_ = 0;
    int overworld_y_ = 0;
};

} // namespace astra
