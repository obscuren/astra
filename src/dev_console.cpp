#include "astra/dev_console.h"
#include "astra/game.h"

namespace astra {

void DevConsole::toggle() {
    open_ = !open_;
    input_.clear();
}

void DevConsole::log(const std::string& msg) {
    output_.push_back(msg);
    if (output_.size() > max_output_) {
        output_.pop_front();
    }
}

bool DevConsole::handle_input(int key, Game& game) {
    if (!open_) return false;

    switch (key) {
        case '`': case 27:
            open_ = false;
            return true;
        case '\n': case '\r':
            if (!input_.empty()) {
                history_.push_back(input_);
                if (history_.size() > max_history_)
                    history_.pop_front();
                log("> " + input_);
                game.execute_console_command(input_);
                input_.clear();
                scroll_ = 0;
                history_idx_ = -1;
            }
            return true;
        case 127: case 8:
            if (!input_.empty()) input_.pop_back();
            return true;
        case KEY_UP: {
            int sz = static_cast<int>(history_.size());
            if (sz > 0) {
                if (history_idx_ < 0) history_idx_ = sz;
                if (history_idx_ > 0) {
                    --history_idx_;
                    input_ = history_[history_idx_];
                }
            }
            return true;
        }
        case KEY_DOWN: {
            int sz = static_cast<int>(history_.size());
            if (history_idx_ >= 0) {
                ++history_idx_;
                if (history_idx_ >= sz) {
                    history_idx_ = -1;
                    input_.clear();
                } else {
                    input_ = history_[history_idx_];
                }
            }
            return true;
        }
        case KEY_PAGE_UP:
            scroll_++;
            return true;
        case KEY_PAGE_DOWN:
            if (scroll_ > 0) scroll_--;
            return true;
        default:
            if (key >= 32 && key < 127) {
                input_ += static_cast<char>(key);
            }
            return true;
    }
}

void DevConsole::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_) return;

    int con_h = std::min(20, screen_h / 2);
    if (con_h < 10) con_h = 10;
    Rect bounds{0, screen_h - con_h, screen_w, con_h};
    Panel console(renderer, bounds, "Console");
    console.set_footer("[`] Close  [Enter] Execute  [Up/Down] History  [PgUp/PgDn] Scroll");
    console.draw();
    DrawContext ctx = console.content();

    int content_h = ctx.height();
    int input_row = content_h - 1;

    std::string prompt = "> " + input_ + "_";
    ctx.text(0, input_row, prompt, Color::Cyan);

    int out_rows = input_row;
    int total = static_cast<int>(output_.size());

    int max_scroll = total - out_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_ > max_scroll) scroll_ = max_scroll;

    int end = total - scroll_;
    int start = end - out_rows;
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < end && row < out_rows; ++i, ++row) {
        const auto& line = output_[i];
        Color c = Color::DarkGray;
        if (line.size() >= 2 && line[0] == '>') c = Color::White;
        ctx.text(0, row, line, c);
    }
}

} // namespace astra
