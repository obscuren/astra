#pragma once

#include "astra/fov.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/tilemap.h"
#include "astra/ui.h"
#include "astra/visibility_map.h"
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace astra {

enum class GameState {
    MainMenu,
    Playing,
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

    // Logic
    void update();
    void new_game();
    void try_move(int dx, int dy);
    void recompute_fov();

    // Rendering
    void render();
    void render_menu();
    void render_play();
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

    std::unique_ptr<Renderer> renderer_;
    GameState state_ = GameState::MainMenu;
    bool running_ = false;

    // Menu
    int menu_selection_ = 0;
    static constexpr int menu_item_count_ = 2;

    // Gameplay
    Player player_;
    std::vector<Npc> npcs_;
    TileMap map_;
    VisibilityMap visibility_;
    int camera_x_ = 0;
    int camera_y_ = 0;

    // Tabs
    int active_tab_ = 0;
    bool panel_visible_ = true;

    // Dialogs
    Dialog test_dialog_;
    Dialog pause_menu_;

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

    // Message log
    std::deque<std::string> messages_;
    static constexpr size_t max_messages_ = 200;
};

} // namespace astra
