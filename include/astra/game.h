#pragma once

#include "astra/renderer.h"
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

    // Rendering
    void render();
    void render_menu();
    void render_play();
    void render_hud();

    // Helpers
    void log(const std::string& msg);
    void center_string(int y, const std::string& text);

    std::unique_ptr<Renderer> renderer_;
    GameState state_ = GameState::MainMenu;
    bool running_ = false;

    // Menu
    int menu_selection_ = 0;
    static constexpr int menu_item_count_ = 2;

    // Player
    int player_x_ = 0;
    int player_y_ = 0;

    // HUD layout — computed on start
    int map_top_ = 1;
    int map_bottom_ = 0;
    int log_top_ = 0;
    int log_height_ = 5;

    // Message log
    std::deque<std::string> messages_;
    static constexpr int max_messages_ = 50;
};

} // namespace astra
