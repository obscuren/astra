#pragma once

#include "astra/action.h"
#include "astra/animation.h"
#include "astra/map_editor.h"
#include "astra/minimap.h"
#include "astra/lore_viewer.h"
#include "astra/fov.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/save_file.h"
#include "astra/star_chart.h"
#include "astra/character_creation.h"
#include "astra/character_screen.h"
#include "astra/combat_system.h"
#include "astra/dev_console.h"
#include "astra/dialog_manager.h"
#include "astra/help_screen.h"
#include "astra/save_system.h"
#include "astra/input_manager.h"
#include "astra/quest.h"
#include "astra/repair_bench.h"
#include "astra/world_manager.h"
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

// SurfaceMode moved to world_manager.h

// Side-panel widgets — multiple can be active simultaneously (bitfield).
enum class Widget : uint8_t {
    Messages = 0,
    Wait     = 1,
    Minimap  = 2,
};
static constexpr int widget_count = 3;
static constexpr uint8_t widget_default = (1 << static_cast<uint8_t>(Widget::Messages));

inline bool widget_active(uint8_t mask, Widget w) {
    return (mask >> static_cast<uint8_t>(w)) & 1;
}
inline void widget_toggle(uint8_t& mask, Widget w) {
    mask ^= (1 << static_cast<uint8_t>(w));
}

// Configurable toggle keys for each widget (indexed by Widget enum value).
struct WidgetKeys {
    int keys[widget_count] = { KEY_F1, KEY_F2, KEY_F3 };
    const char* labels[widget_count] = { "F1", "F2", "F3" };
};

class Game {
public:
    explicit Game(std::unique_ptr<Renderer> renderer);

    void run();

    // Public accessors for subsystems (DevConsole, DialogManager, etc.)
    Player& player() { return player_; }
    std::vector<Npc>& npcs() { return world_.npcs(); }
    WorldManager& world() { return world_; }
    Renderer* renderer() { return renderer_.get(); }
    TradeWindow& trade_window() { return trade_window_; }
    StarChartViewer& star_chart_viewer() { return star_chart_viewer_; }
    void log(const std::string& msg);
    void enter_ship();
    void exit_ship_to_station();
    void enter_maintenance_tunnels();
    void exit_maintenance_tunnels();
    void exit_dungeon_to_detail();
    void advance_world(int cost);

    // Save system accessors
    const std::string& death_message() const { return death_message_; }
    void set_death_message(const std::string& msg) { death_message_ = msg; }
    uint8_t active_widgets() const { return active_widgets_; }
    void set_active_widgets(uint8_t w) { active_widgets_ = w; }
    int focused_widget() const { return focused_widget_; }
    void set_focused_widget(int w) { focused_widget_ = w; }
    bool panel_visible() const { return panel_visible_; }
    void set_panel_visible(bool v) { panel_visible_ = v; }
    std::deque<std::string>& messages() { return messages_; }
    bool tile_occupied(int x, int y) const;
    void set_dev_mode(bool v) { dev_mode_ = v; }
    bool lost() const { return lost_; }
    int lost_moves() const { return lost_moves_; }
    void set_lost(bool v, int moves = 0) { lost_ = v; lost_moves_ = moves; }
    DialogManager& dialog() { return dialog_; }
    CombatSystem& combat() { return combat_; }
    QuestManager& quests() { return quest_manager_; }
    AnimationManager& animations() { return animations_; }
    void auto_step();
    bool auto_walk_should_stop() const;
    std::pair<int,int> bfs_explore_goal() const;
    std::pair<int,int> bfs_step_toward(int gx, int gy) const;
    void open_repair_bench();
    void open_lore_viewer() { lore_viewer_.open(world_.lore()); }
    void rebuild_star_chart_viewer();
    void reset_interaction_state();
    void post_load();
    void apply_passive_skill_effects();
    void save_current_location();
    void restore_location(const LocationKey& key);
    void recompute_fov();
    void compute_camera();
    MapEditor& map_editor() { return map_editor_; }

    // Dev commands
    void dev_command_warp_random();
    void dev_command_warp_stamp(Tile poi);
    void dev_command_warp_to_system(uint32_t system_id);
    void dev_command_level_up();
    void dev_command_kill_hostiles();
    void dev_command_biome_test(Biome biome, int layer,
                                const std::string& poi_type = "",
                                const std::string& poi_style = "",
                                bool connected = false,
                                const std::string& civ_name = "");

private:
    // LocationKey moved to world_manager.h

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
    void new_game(const CreationResult& cr);
    void travel_to_destination(const ChartAction& action);
    void enter_detail_map();
    void exit_detail_to_overworld();
    void enter_dungeon_from_detail();
    void transition_detail_edge(int dx, int dy);
    MapProperties build_detail_props(int ow_x, int ow_y);

    // Legacy — kept for dungeon-from-overworld POI flow
    void enter_overworld_tile();
    void exit_to_overworld();
    void try_move(int dx, int dy);
    void try_interact(int dx, int dy);
    void use_action();
    void use_at(int tx, int ty);
    int count_adjacent_interactables() const;
    bool is_interactable(int tx, int ty) const;
    void process_npc_turn(Npc& npc);
    void attack_npc(Npc& npc);
    void begin_targeting();
    void handle_targeting_input(int key);
    void shoot_target();
    void render_look_popup();
    std::string look_tile_name(int mx, int my) const;
    std::string look_tile_desc(int mx, int my) const;
    void pickup_ground_item();
    void drop_item(int index);
    void use_item(int index);
    void equip_item(int index);
    void unequip_slot(int index);
    void reload_weapon();
    void remove_dead_npcs();
    void check_player_death();
    void check_level_up();
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
    void render_widget_bar();
    // render_map extracted to map_renderer.h
    void render_side_panel();
    void render_messages_widget(UIContext& ctx);
    void render_wait_widget(UIContext& ctx);
    void render_minimap_widget(UIContext& ctx);
    void render_effects_bar();
    void render_abilities_bar();
    void render_welcome_screen();
    void render_lost_popup();
    void render_pause_menu();
    void render_quit_confirm();

    // Layout
    void compute_layout();

    // Dev tools
    void dev_warp_random();
    void dev_warp_stamp_test();

    // Dev console command execution (public — called by InputManager)


    // Helpers
    Color hp_color() const;
    Color hunger_color() const;

    std::unique_ptr<Renderer> renderer_;
    GameState state_ = GameState::MainMenu;
    GameState prev_state_ = GameState::MainMenu; // for returning from Load/HallOfFame
    bool running_ = false;
    bool show_welcome_ = false;
    bool tutorial_pending_ = false;  // show tutorial choice dialog on next frame

    // Menu
    int menu_selection_ = 0;
#ifdef ASTRA_DEV_MODE
    static constexpr int menu_item_count_ = 6; // dev game, new, load, hall, editor, quit
#else
    static constexpr int menu_item_count_ = 4; // new, load, hall, quit
#endif

    // Dev mode
    bool dev_mode_ = false;
    Tile dev_warp_stamp_test_poi_ = Tile::Empty;

    DevConsole console_;
    HelpScreen help_screen_;

    // Gameplay
    Player player_;
    WorldManager world_;
    StarChartViewer star_chart_viewer_;
    TradeWindow trade_window_;
    CharacterScreen character_screen_;
    CharacterCreation character_creation_;
    CombatSystem combat_;
    QuestManager quest_manager_;
    RepairBench repair_bench_;
    AnimationManager animations_;
    MapEditor map_editor_;
    Minimap minimap_;
    LoreViewer lore_viewer_;
    int camera_x_ = 0;
    int camera_y_ = 0;

    SaveSystem save_system_;

    // Widgets
    uint8_t active_widgets_ = widget_default;
    int focused_widget_ = 0;  // index into Widget enum — receives +/- input
    int widget_order_[widget_count] = {1, 0, 0}; // enable-order counter per widget (Messages starts at 1)
    int widget_order_seq_ = 1; // next sequence number
    bool panel_visible_ = true;
    int message_scroll_ = 0;  // 0 = latest, >0 = scrolled back
    WidgetKeys widget_keys_;

    // Input subsystem (look mode)
    InputManager input_;

    // Input modes
    bool awaiting_interact_ = false;
    bool awaiting_autowalk_ = false;
    bool auto_walking_ = false;
    int auto_walk_dx_ = 0, auto_walk_dy_ = 0;
    bool auto_exploring_ = false;
    int auto_walk_hp_ = 0; // HP when auto-walk started, to detect damage
    int explore_goal_x_ = -1, explore_goal_y_ = -1; // committed BFS goal
    bool targeting_ = false;
    int target_x_ = 0, target_y_ = 0;
    int blink_phase_ = 0;
    Npc* target_npc_ = nullptr;
    int inventory_cursor_ = 0;
    int wait_cursor_ = 0;
    // Lost mechanic — getting lost on the overworld
    bool lost_ = false;
    bool lost_pending_ = false;  // popup shown, awaiting dismiss before entering detail
    int lost_moves_ = 0;  // moves since getting lost (ramps regain chance)
    MenuState lost_popup_;
    void check_get_lost();
    void enter_lost_detail();    // called when lost popup is dismissed
    void check_regain_bearings();
    int get_lost_chance(Tile terrain) const;    // % chance per overworld move
    int regain_chance() const;                  // % chance per detail move, ramps with lost_moves_

    // Dialogs
    DialogManager dialog_;
    MenuState pause_menu_;
    MenuState quit_confirm_;

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
    // LocationState, location_cache_, ship_key_ moved to world_
};

} // namespace astra
