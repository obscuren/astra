#include "astra/game.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <thread>

namespace astra {

static const char* title_art[] = {
    R"(        .            *                .          |  )",
    R"(   *         .              .                .  -o- )",
    R"(        _        _                   *           |  )",
    R"(  .    / \   ___| |_ _ __ __ _           .         )",
    R"(      / _ \ / __| __| '__/ _` |   .                )",
    R"( *   / ___ \\__ \ |_| | | (_| |        *           )",
    R"(    /_/   \_\___/\__|_|  \__,_|  .                 )",
    R"(         .          *       .          .            )",
    R"(   .            .                 *                 )",
};
static constexpr int title_art_lines = 9;

static const char* menu_items[] = {
    "New Game",
    "Quit",
};

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

void Game::run() {
    renderer_->init();
    running_ = true;
    compute_layout();

    while (running_) {
        // Recompute layout if screen size changed
        int w = renderer_->get_width();
        int h = renderer_->get_height();
        if (w != screen_w_ || h != screen_h_) {
            compute_layout();
        }

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

void Game::compute_layout() {
    screen_w_ = renderer_->get_width();
    screen_h_ = renderer_->get_height();

    pane_w_ = screen_w_ / 5;
    if (pane_w_ < 14) pane_w_ = 14;

    pane_x_ = 0;       // pane on the left
    map_view_x_ = pane_w_ + 1; // after pane + separator
    map_view_y_ = 1;   // row 0 = status bar
    map_view_w_ = screen_w_ - pane_w_ - 1; // -1 for separator
    map_view_h_ = screen_h_ - 1 - log_h_ - 1; // -1 status bar, -1 log separator

    log_y_ = map_view_y_ + map_view_h_;
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
                new_game();
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
        case 'w': case 'k': case KEY_UP:    try_move( 0, -1); break;
        case 's': case 'j': case KEY_DOWN:  try_move( 0,  1); break;
        case 'a': case 'h': case KEY_LEFT:  try_move(-1,  0); break;
        case 'd': case 'l': case KEY_RIGHT: try_move( 1,  0); break;
    }
}

// --- Logic ---

void Game::new_game() {
    compute_layout();

    map_ = TileMap(map_view_w_, map_view_h_);
    map_.generate(static_cast<unsigned>(std::time(nullptr)));

    player_ = Player{};
    map_.find_open_spot(player_.x, player_.y);

    visibility_ = VisibilityMap(map_.width(), map_.height());
    recompute_fov();

    messages_.clear();
    log("Welcome aboard, commander. Your journey to Sgr A* begins.");

    state_ = GameState::Playing;
}

void Game::try_move(int dx, int dy) {
    int nx = player_.x + dx;
    int ny = player_.y + dy;
    if (map_.passable(nx, ny)) {
        player_.x = nx;
        player_.y = ny;
        recompute_fov();
    }
}

void Game::recompute_fov() {
    compute_fov(map_, visibility_, player_.x, player_.y, player_.view_radius);

    // If any tile of a lit region is visible, reveal the entire region
    std::vector<bool> reveal(map_.region_count(), false);
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            if (visibility_.get(x, y) == Visibility::Visible) {
                int rid = map_.region_id(x, y);
                if (rid >= 0 && map_.region(rid).lit) {
                    reveal[rid] = true;
                }
            }
        }
    }

    // Mark all tiles of revealed lit regions as visible
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            int rid = map_.region_id(x, y);
            if (rid >= 0 && reveal[rid]) {
                visibility_.set_visible(x, y);
            }
        }
    }
}

void Game::update() {
    // Tick-based — world updates happen in response to player actions,
    // not continuously. Future: enemy AI, environmental effects, etc.
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
    int art_start_y = screen_h_ / 2 - title_art_lines - 2;

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

    center_string(screen_h_ - 2, "wasd/hjkl to navigate, enter to select");
}

void Game::render_play() {
    // Status bar (row 0, full width)
    std::string status = " HP: " + std::to_string(player_.hp) + "/"
                       + std::to_string(player_.max_hp)
                       + "  |  Depth: " + std::to_string(player_.depth)
                       + "  |  q: menu";
    if (static_cast<int>(status.size()) < screen_w_) {
        status.append(screen_w_ - status.size(), ' ');
    }
    renderer_->draw_string(0, 0, status);

    render_map();
    render_pane();
    render_log();
}

void Game::render_map() {
    // Draw tile map into the map viewport area, respecting visibility
    for (int y = 0; y < map_view_h_ && y < map_.height(); ++y) {
        for (int x = 0; x < map_view_w_ && x < map_.width(); ++x) {
            Visibility v = visibility_.get(x, y);
            if (v == Visibility::Unexplored) continue;

            char g = tile_glyph(map_.get(x, y));
            if (g == ' ') continue;

            if (v == Visibility::Visible) {
                Tile t = map_.get(x, y);
                Color c = (t == Tile::Wall) ? Color::White : Color::Default;
                renderer_->draw_char(map_view_x_ + x, map_view_y_ + y, g, c);
            } else {
                // Explored — dimmed blue
                renderer_->draw_char(map_view_x_ + x, map_view_y_ + y, g, Color::Blue);
            }
        }
    }

    // Draw player
    renderer_->draw_char(map_view_x_ + player_.x, map_view_y_ + player_.y, '@', Color::Yellow);
}

void Game::render_pane() {
    int sep_x = pane_w_;

    // Vertical separator
    for (int y = 0; y < screen_h_; ++y) {
        renderer_->draw_char(sep_x, y, '|');
    }

    int y = 1;
    auto pane_line = [&](const std::string& text) {
        renderer_->draw_string(pane_x_ + 1, y++, text);
    };

    pane_line("-- Status --");
    y++;
    pane_line("HP: " + std::to_string(player_.hp) + "/" + std::to_string(player_.max_hp));
    pane_line("Depth: " + std::to_string(player_.depth));
    y++;
    pane_line("-- Position --");
    y++;
    pane_line("X: " + std::to_string(player_.x));
    pane_line("Y: " + std::to_string(player_.y));
}

void Game::render_log() {
    // Separator line above log
    for (int x = map_view_x_; x < screen_w_; ++x) {
        renderer_->draw_char(x, log_y_ - 1, '-');
    }

    // Render most recent messages
    int visible = log_h_;
    int start = static_cast<int>(messages_.size()) > visible
        ? static_cast<int>(messages_.size()) - visible : 0;
    int line = 0;
    for (int i = start; i < static_cast<int>(messages_.size()); ++i) {
        std::string msg = messages_[i];
        if (static_cast<int>(msg.size()) > map_view_w_ - 2) {
            msg.resize(map_view_w_ - 2);
        }
        renderer_->draw_string(map_view_x_ + 1, log_y_ + line, msg);
        ++line;
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
    int x = (screen_w_ - static_cast<int>(text.size())) / 2;
    if (x < 0) x = 0;
    renderer_->draw_string(x, y, text);
}

} // namespace astra
