#pragma once

#include "crawler/renderer.h"
#include <memory>

namespace crawler {

class Game {
public:
    explicit Game(std::unique_ptr<Renderer> renderer);

    void run();

private:
    void handle_input(int key);
    void update();
    void render();

    std::unique_ptr<Renderer> renderer_;
    bool running_ = false;

    int player_x_ = 40;
    int player_y_ = 12;
};

} // namespace crawler
