#pragma once

#include "astra/renderer.h"
#include <deque>
#include <string>
#include <string_view>

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

} // namespace astra
