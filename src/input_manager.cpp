#include "astra/input_manager.h"
#include "astra/game.h"

#include <algorithm>

namespace astra {

void InputManager::begin_look(int px, int py) {
    looking_ = true;
    look_x_ = px;
    look_y_ = py;
    look_blink_ = 0;
}

void InputManager::begin_look_at(int mx, int my) {
    looking_ = true;
    look_x_ = mx;
    look_y_ = my;
    look_blink_ = 0;
}

void InputManager::handle_look_input(int key, int map_w, int map_h) {
    switch (key) {
        case 'k': case KEY_UP:    look_y_--; break;
        case 'j': case KEY_DOWN:  look_y_++; break;
        case 'h': case KEY_LEFT:  look_x_--; break;
        case 'l': case KEY_RIGHT: look_x_++; break;
        case 27:
            looking_ = false;
            return;
        default:
            return;
    }
    if (look_x_ < 0) look_x_ = 0;
    if (look_y_ < 0) look_y_ = 0;
    if (look_x_ >= map_w) look_x_ = map_w - 1;
    if (look_y_ >= map_h) look_y_ = map_h - 1;
}

// ── Dev console ─────────────────────────────────────────────────────

void InputManager::toggle_console() {
    console_open_ = !console_open_;
    console_input_.clear();
}

void InputManager::console_log(const std::string& msg) {
    console_output_.push_back(msg);
    if (console_output_.size() > max_console_lines_) {
        console_output_.pop_front();
    }
}

void InputManager::handle_console_input(int key, Game& game) {
    switch (key) {
        case '`': case 27:
            console_open_ = false;
            return;
        case '\n': case '\r':
            if (!console_input_.empty()) {
                console_history_.push_back(console_input_);
                if (console_history_.size() > max_console_history_)
                    console_history_.pop_front();
                console_log("> " + console_input_);
                game.execute_console_command(console_input_);
                console_input_.clear();
                console_scroll_ = 0;
                console_history_idx_ = -1;
            }
            return;
        case 127: case 8:
            if (!console_input_.empty()) console_input_.pop_back();
            return;
        case KEY_UP: {
            int hist_size = static_cast<int>(console_history_.size());
            if (hist_size > 0) {
                if (console_history_idx_ < 0) console_history_idx_ = hist_size;
                if (console_history_idx_ > 0) {
                    --console_history_idx_;
                    console_input_ = console_history_[console_history_idx_];
                }
            }
            return;
        }
        case KEY_DOWN: {
            int hist_size = static_cast<int>(console_history_.size());
            if (console_history_idx_ >= 0) {
                ++console_history_idx_;
                if (console_history_idx_ >= hist_size) {
                    console_history_idx_ = -1;
                    console_input_.clear();
                } else {
                    console_input_ = console_history_[console_history_idx_];
                }
            }
            return;
        }
        case KEY_PAGE_UP:
            console_scroll_++;
            return;
        case KEY_PAGE_DOWN:
            if (console_scroll_ > 0) console_scroll_--;
            return;
        default:
            if (key >= 32 && key < 127) {
                console_input_ += static_cast<char>(key);
            }
            return;
    }
}

void InputManager::render_console(Renderer* renderer, int screen_w, int screen_h) {
    if (!console_open_) return;

    int con_h = std::min(20, screen_h / 2);
    if (con_h < 10) con_h = 10;
    Rect bounds{0, screen_h - con_h, screen_w, con_h};
    Panel console(renderer, bounds, "Console");
    console.set_footer("[`] Close  [Enter] Execute  [Up/Down] History  [PgUp/PgDn] Scroll");
    console.draw();
    DrawContext ctx = console.content();

    int content_h = ctx.height();
    int input_row = content_h - 1;

    std::string prompt = "> " + console_input_ + "_";
    ctx.text(0, input_row, prompt, Color::Cyan);

    int out_rows = input_row;
    int total = static_cast<int>(console_output_.size());

    int max_scroll = total - out_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (console_scroll_ > max_scroll) console_scroll_ = max_scroll;

    int end = total - console_scroll_;
    int start = end - out_rows;
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < end && row < out_rows; ++i, ++row) {
        const auto& line = console_output_[i];
        Color c = Color::DarkGray;
        if (line.size() >= 2 && line[0] == '>') c = Color::White;
        ctx.text(0, row, line, c);
    }
}

} // namespace astra
