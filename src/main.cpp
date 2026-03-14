#include "crawler/game.h"
#include "crawler/terminal_renderer.h"

#include <memory>

int main() {
    auto renderer = std::make_unique<crawler::TerminalRenderer>();
    crawler::Game game(std::move(renderer));
    game.run();
    return 0;
}
