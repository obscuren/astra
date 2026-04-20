#pragma once

#include "astra/renderer.h"
#include "astra/ui.h"

#include <deque>
#include <string>

namespace astra {

class Game; // forward declare for command execution

class DevConsole {
public:
    DevConsole() = default;

    bool is_open() const { return open_; }
    void toggle();
    void log(const std::string& msg);
    void clear() { output_.clear(); }

    // Returns true if input was consumed
    bool handle_input(int key, Game& game);
    void execute_command(const std::string& cmd, Game& game);
    void draw(Renderer* renderer, int screen_w, int screen_h);

private:
    bool open_ = false;
    std::string input_;
    size_t cursor_ = 0;
    std::deque<std::string> output_;
    static constexpr size_t max_output_ = 50;
    int scroll_ = 0;
    std::deque<std::string> history_;
    int history_idx_ = -1;
    static constexpr size_t max_history_ = 50;
};

} // namespace astra
