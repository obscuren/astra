#pragma once

#include "astra/fov.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"
#include <deque>
#include <memory>
#include <string>

namespace astra {

enum class GameState {
    MainMenu,
    Playing,
};

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
    void render_map();
    void render_pane();
    void render_log();

    // Layout
    void compute_layout();

    // Helpers
    void log(const std::string& msg);
    void center_string(int y, const std::string& text);

    std::unique_ptr<Renderer> renderer_;
    GameState state_ = GameState::MainMenu;
    bool running_ = false;

    // Menu
    int menu_selection_ = 0;
    static constexpr int menu_item_count_ = 2;

    // Gameplay
    Player player_;
    TileMap map_;
    VisibilityMap visibility_;

    // UI layout (computed from screen size)
    int screen_w_ = 0;
    int screen_h_ = 0;
    int map_view_x_ = 0;   // top-left of map viewport
    int map_view_y_ = 1;   // row 0 is status bar
    int map_view_w_ = 0;
    int map_view_h_ = 0;
    int pane_x_ = 0;       // info pane left edge
    int pane_w_ = 0;
    int log_y_ = 0;        // message log top row
    int log_h_ = 4;

    // Message log
    std::deque<std::string> messages_;
    static constexpr size_t max_messages_ = 50;
};

} // namespace astra
