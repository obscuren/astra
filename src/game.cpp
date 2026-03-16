#include "astra/game.h"

#include <chrono>
#include <thread>

namespace astra {

static const char* title_art[] = {
    R"(    _        _             )",
    R"(   / \   ___| |_ _ __ __ _ )",
    R"(  / _ \ / __| __| '__/ _` |)",
    R"( / ___ \\__ \ |_| | | (_| |)",
    R"(/_/   \_\___/\__|_|  \__,_|)",
};
static constexpr int title_art_lines = 5;

static const char* menu_items[] = {
    "New Game",
    "Quit",
};

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

void Game::run() {
    renderer_->init();
    running_ = true;

    // Compute HUD layout based on terminal size
    int h = renderer_->get_height();
    log_top_ = h - log_height_;
    map_bottom_ = log_top_ - 1;

    while (running_) {
        int key = renderer_->poll_input();
        if (key != -1) {
            handle_input(key);
        }

        update();
        render();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    renderer_->shutdown();
}

// --- Input ---

void Game::handle_input(int key) {
    switch (state_) {
        case GameState::MainMenu: handle_menu_input(key); break;
        case GameState::Playing:  handle_play_input(key);  break;
    }
}

void Game::handle_menu_input(int key) {
    switch (key) {
        case 'w': case 'k': case KEY_UP:
            menu_selection_ = (menu_selection_ - 1 + menu_item_count_) % menu_item_count_;
            break;
        case 's': case 'j': case KEY_DOWN:
            menu_selection_ = (menu_selection_ + 1) % menu_item_count_;
            break;
        case '\n': case '\r': case ' ':
            if (menu_selection_ == 0) {
                // New Game
                int w = renderer_->get_width();
                int map_h = map_bottom_ - map_top_ + 1;
                player_x_ = w / 2;
                player_y_ = map_top_ + map_h / 2;
                messages_.clear();
                log("Welcome to the dungeon. Move with wasd/hjkl.");
                state_ = GameState::Playing;
            } else if (menu_selection_ == 1) {
                running_ = false;
            }
            break;
        case 'q':
            running_ = false;
            break;
    }
}

void Game::handle_play_input(int key) {
    switch (key) {
        case 'q': state_ = GameState::MainMenu; break;
        case 'w': case 'k': case KEY_UP:    --player_y_; break;
        case 's': case 'j': case KEY_DOWN:  ++player_y_; break;
        case 'a': case 'h': case KEY_LEFT:  --player_x_; break;
        case 'd': case 'l': case KEY_RIGHT: ++player_x_; break;
    }
}

// --- Update ---

void Game::update() {
    if (state_ != GameState::Playing) return;

    int w = renderer_->get_width();
    if (player_x_ < 0) player_x_ = 0;
    if (player_y_ < map_top_) player_y_ = map_top_;
    if (player_x_ >= w) player_x_ = w - 1;
    if (player_y_ > map_bottom_) player_y_ = map_bottom_;
}

// --- Rendering ---

void Game::render() {
    renderer_->clear();

    switch (state_) {
        case GameState::MainMenu: render_menu(); break;
        case GameState::Playing:  render_play(); break;
    }

    renderer_->present();
}

void Game::render_menu() {
    int h = renderer_->get_height();
    int art_start_y = h / 2 - title_art_lines - 2;

    for (int i = 0; i < title_art_lines; ++i) {
        center_string(art_start_y + i, title_art[i]);
    }

    int menu_y = art_start_y + title_art_lines + 2;
    for (int i = 0; i < menu_item_count_; ++i) {
        std::string label;
        if (i == menu_selection_) {
            label = "> " + std::string(menu_items[i]) + " <";
        } else {
            label = "  " + std::string(menu_items[i]) + "  ";
        }
        center_string(menu_y + i, label);
    }

    center_string(h - 2, "wasd/hjkl to navigate, enter to select");
}

void Game::render_play() {
    // Draw player
    renderer_->draw_char(player_x_, player_y_, '@');

    // Draw HUD
    render_hud();
}

void Game::render_hud() {
    int w = renderer_->get_width();

    // Status bar (top row)
    std::string status = " HP: 10/10  |  Depth: 1  |  q: menu";
    // Pad to full width
    if (static_cast<int>(status.size()) < w) {
        status.append(w - status.size(), ' ');
    }
    renderer_->draw_string(0, 0, status);

    // Separator between map and log
    for (int x = 0; x < w; ++x) {
        renderer_->draw_char(x, log_top_ - 1, '-');
    }

    // Message log
    int log_line = 0;
    int start = messages_.size() > static_cast<size_t>(log_height_)
        ? messages_.size() - log_height_ : 0;
    for (size_t i = start; i < messages_.size(); ++i) {
        renderer_->draw_string(1, log_top_ + log_line, messages_[i]);
        ++log_line;
    }
}

// --- Helpers ---

void Game::log(const std::string& msg) {
    messages_.push_back(msg);
    if (messages_.size() > max_messages_) {
        messages_.pop_front();
    }
}

void Game::center_string(int y, const std::string& text) {
    int w = renderer_->get_width();
    int x = (w - static_cast<int>(text.size())) / 2;
    if (x < 0) x = 0;
    renderer_->draw_string(x, y, text);
}

} // namespace astra
