#include "crawler/game.h"

#include <chrono>
#include <thread>

namespace crawler {

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

void Game::run() {
    renderer_->init();
    running_ = true;

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

void Game::handle_input(int key) {
    switch (key) {
        case 'q': running_ = false; break;
        case 'w': case 'k': --player_y_; break;
        case 's': case 'j': ++player_y_; break;
        case 'a': case 'h': --player_x_; break;
        case 'd': case 'l': ++player_x_; break;
    }
}

void Game::update() {
    // Clamp player to screen bounds
    int w = renderer_->get_width();
    int h = renderer_->get_height();
    if (player_x_ < 0) player_x_ = 0;
    if (player_y_ < 0) player_y_ = 0;
    if (player_x_ >= w) player_x_ = w - 1;
    if (player_y_ >= h) player_y_ = h - 1;
}

void Game::render() {
    renderer_->clear();
    renderer_->draw_string(0, 0, "crawler v0.1 — q to quit, wasd/hjkl to move");
    renderer_->draw_char(player_x_, player_y_, '@');
    renderer_->present();
}

} // namespace crawler
