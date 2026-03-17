#pragma once

#include "astra/action.h"
#include "astra/fov.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/save_file.h"
#include "astra/tile_props.h"
#include "astra/tilemap.h"
#include "astra/ui.h"
#include "astra/visibility_map.h"
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace astra {

enum class GameState {
    MainMenu,
    Playing,
    GameOver,
    LoadMenu,
    HallOfFame,
};

enum class PanelTab : uint8_t {
    Messages,
    Inventory,
    Equipment,
    Ship,
};

static constexpr int panel_tab_count = 4;

class Game {
public:
    explicit Game(std::unique_ptr<Renderer> renderer);

    void run();

private:
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
    void try_move(int dx, int dy);
    void try_interact(int dx, int dy);
    void advance_world(int cost);
    void process_npc_turn(Npc& npc);
    void attack_npc(Npc& npc);
    void begin_targeting();
    void handle_targeting_input(int key);
    void shoot_target();
    void remove_dead_npcs();
    void check_player_death();
    void open_npc_dialog(Npc& npc);
    void advance_dialog(int selected);
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
    void render_effects_bar();
    void render_abilities_bar();

    // Layout
    void compute_layout();
    void compute_camera();

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
    static constexpr int menu_item_count_ = 4;

    // Gameplay
    unsigned seed_ = 0;
    std::mt19937 rng_;
    Player player_;
    std::vector<Npc> npcs_;
    TileMap map_;
    VisibilityMap visibility_;
    int camera_x_ = 0;
    int camera_y_ = 0;
    int current_region_ = -1;
    int world_tick_ = 0;

    // Tabs
    int active_tab_ = 0;
    bool panel_visible_ = true;

    // Input modes
    bool awaiting_interact_ = false;
    bool targeting_ = false;
    int target_x_ = 0, target_y_ = 0;
    int blink_phase_ = 0;
    Npc* target_npc_ = nullptr;

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
};

} // namespace astra
