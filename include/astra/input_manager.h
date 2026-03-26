#pragma once

#include "astra/renderer.h"
#include "astra/ui.h"

#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace astra {

class Game; // forward declare for console command execution

class InputManager {
public:
    // ── Look mode ───────────────────────────────────────────────
    bool looking() const { return looking_; }
    int look_x() const { return look_x_; }
    int look_y() const { return look_y_; }
    int look_blink() const { return look_blink_; }
    void tick_look_blink() { ++look_blink_; }

    void begin_look(int px, int py);
    void begin_look_at(int mx, int my);
    void cancel_look() { looking_ = false; }
    void handle_look_input(int key, int map_w, int map_h);

    // Cached glyph/color (read before cursor drawn in render_map)
    const char* cached_glyph() const { return look_cell_glyph_; }
    Color cached_color() const { return look_cell_color_; }
    void cache_look_cell(const char* glyph, Color fg) {
        std::strncpy(look_cell_glyph_, glyph, 4);
        look_cell_glyph_[4] = '\0';
        look_cell_color_ = fg;
    }

    // ── Dev console ──────────────────────────────────────────────
    bool console_open() const { return console_open_; }
    void toggle_console();
    void handle_console_input(int key, Game& game);
    void console_log(const std::string& msg);
    void clear_console() { console_output_.clear(); }
    void render_console(Renderer* renderer, int screen_w, int screen_h);

private:
    bool looking_ = false;
    int look_x_ = 0, look_y_ = 0;
    int look_blink_ = 0;
    char look_cell_glyph_[5] = {};
    Color look_cell_color_ = Color::White;

    // Console
    bool console_open_ = false;
    std::string console_input_;
    std::deque<std::string> console_output_;
    static constexpr size_t max_console_lines_ = 50;
    int console_scroll_ = 0;
    std::deque<std::string> console_history_;
    int console_history_idx_ = -1;
    static constexpr size_t max_console_history_ = 50;
};

} // namespace astra
