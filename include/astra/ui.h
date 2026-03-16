#pragma once

#include "astra/renderer.h"
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace astra {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    Rect inset(int n) const;
    Rect inset(int horiz, int vert) const;
    Rect row(int index) const;
    Rect rows(int start, int count) const;
    Rect split_left(int width) const;
    Rect split_right(int width) const;
    Rect split_top(int height) const;
    Rect split_bottom(int height) const;
    bool contains(int px, int py) const;
    bool empty() const;
};

class DrawContext {
public:
    DrawContext(Renderer* r, Rect bounds);

    // Core — all coords local to bounds, clipped
    void put(int x, int y, char ch);
    void put(int x, int y, char ch, Color fg);
    void text(int x, int y, std::string_view s, Color fg = Color::Default);

    // Lines
    void hline(int y, char ch = '-');
    void vline(int x, char ch = '|');
    void border(char h = '-', char v = '|', char corner = '+');
    void fill(char ch = ' ');

    // Aligned text
    void text_left(int y, std::string_view s, Color fg = Color::Default);
    void text_center(int y, std::string_view s, Color fg = Color::Default);
    void text_right(int y, std::string_view s, Color fg = Color::Default);

    // Label-value pattern — returns x after last char written
    int label_value(int x, int y,
                    std::string_view label, Color label_color,
                    std::string_view value, Color value_color);

    // Progress bar [====----] — returns x after closing bracket
    int bar(int x, int y, int bar_width, int value, int max_value,
            Color fill_color, Color empty_color = Color::DarkGray,
            char fill_ch = '=', char empty_ch = '-');

    // Sub-region
    DrawContext sub(Rect local_rect) const;

    const Rect& bounds() const;
    int width() const;
    int height() const;

private:
    Renderer* renderer_;
    Rect bounds_;
};

struct TextList {
    static void draw(DrawContext& ctx, const std::deque<std::string>& lines,
                     int scroll_offset = -1, Color fg = Color::Default);
};

class Window {
public:
    // Create a window at a specific rect
    Window(Renderer* renderer, Rect bounds, std::string_view title = "");

    // Create a centered window given screen dimensions
    Window(Renderer* renderer, int screen_w, int screen_h,
           int width, int height, std::string_view title = "");

    // Draw the window frame (border, title, ornament, footer)
    void draw();

    // Get the content area DrawContext (inside border, below title ornament)
    DrawContext content() const;

    // Set footer key hints
    void set_footer(std::string_view text, Color color = Color::DarkGray);

    const Rect& bounds() const;

private:
    void draw_ornament(DrawContext& ctx, int y);

    Renderer* renderer_;
    Rect bounds_;
    std::string title_;
    std::string footer_;
    Color footer_color_ = Color::DarkGray;
};

struct DialogOption {
    std::string label;
    int hotkey = -1;  // ASCII key that selects this option, or -1 for none
};

// Result of Dialog::handle_input()
enum class DialogResult {
    None,       // No action taken
    Closed,     // User pressed close key
    Selected,   // User confirmed an option (check selected())
};

class Dialog {
public:
    Dialog(std::string_view title, std::string_view body = "");

    // Build options
    void add_option(std::string_view label, int hotkey = -1);

    // Input — returns what happened
    DialogResult handle_input(int key);

    // Render into a centered window
    void draw(Renderer* renderer, int screen_w, int screen_h);

    int selected() const { return selection_; }
    bool is_open() const { return open_; }
    void open() { open_ = true; selection_ = 0; }
    void close() { open_ = false; }

private:
    std::string title_;
    std::string body_;
    std::vector<DialogOption> options_;
    int selection_ = 0;
    bool open_ = false;
};

} // namespace astra
