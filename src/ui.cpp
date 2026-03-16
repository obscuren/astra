#include "astra/ui.h"

#include <algorithm>

namespace astra {

// --- Rect ---

Rect Rect::inset(int n) const {
    return {x + n, y + n, std::max(0, w - 2 * n), std::max(0, h - 2 * n)};
}

Rect Rect::inset(int horiz, int vert) const {
    return {x + horiz, y + vert, std::max(0, w - 2 * horiz), std::max(0, h - 2 * vert)};
}

Rect Rect::row(int index) const {
    if (index < 0 || index >= h) return {x, y, w, 0};
    return {x, y + index, w, 1};
}

Rect Rect::rows(int start, int count) const {
    int s = std::max(0, start);
    int c = std::min(count, h - s);
    if (c <= 0) return {x, y + s, w, 0};
    return {x, y + s, w, c};
}

Rect Rect::split_left(int width) const {
    return {x, y, std::min(width, w), h};
}

Rect Rect::split_right(int width) const {
    int rw = std::min(width, w);
    return {x + w - rw, y, rw, h};
}

Rect Rect::split_top(int height) const {
    return {x, y, w, std::min(height, h)};
}

Rect Rect::split_bottom(int height) const {
    int rh = std::min(height, h);
    return {x, y + h - rh, w, rh};
}

bool Rect::contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
}

bool Rect::empty() const {
    return w <= 0 || h <= 0;
}

// --- DrawContext ---

DrawContext::DrawContext(Renderer* r, Rect bounds)
    : renderer_(r), bounds_(bounds) {}

void DrawContext::put(int x, int y, char ch) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_char(ax, ay, ch);
    }
}

void DrawContext::put(int x, int y, char ch, Color fg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_char(ax, ay, ch, fg);
    }
}

void DrawContext::text(int x, int y, std::string_view s, Color fg) {
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        put(x + i, y, s[i], fg);
    }
}

void DrawContext::hline(int y, char ch) {
    for (int x = 0; x < bounds_.w; ++x) {
        put(x, y, ch);
    }
}

void DrawContext::vline(int x, char ch) {
    for (int y = 0; y < bounds_.h; ++y) {
        put(x, y, ch);
    }
}

void DrawContext::border(char h, char v, char corner) {
    // Top and bottom
    for (int x = 1; x < bounds_.w - 1; ++x) {
        put(x, 0, h);
        put(x, bounds_.h - 1, h);
    }
    // Left and right
    for (int y = 1; y < bounds_.h - 1; ++y) {
        put(0, y, v);
        put(bounds_.w - 1, y, v);
    }
    // Corners
    put(0, 0, corner);
    put(bounds_.w - 1, 0, corner);
    put(0, bounds_.h - 1, corner);
    put(bounds_.w - 1, bounds_.h - 1, corner);
}

void DrawContext::fill(char ch) {
    for (int y = 0; y < bounds_.h; ++y) {
        for (int x = 0; x < bounds_.w; ++x) {
            put(x, y, ch);
        }
    }
}

void DrawContext::text_left(int y, std::string_view s, Color fg) {
    text(0, y, s, fg);
}

void DrawContext::text_center(int y, std::string_view s, Color fg) {
    int x = (bounds_.w - static_cast<int>(s.size())) / 2;
    if (x < 0) x = 0;
    text(x, y, s, fg);
}

void DrawContext::text_right(int y, std::string_view s, Color fg) {
    int x = bounds_.w - static_cast<int>(s.size());
    if (x < 0) x = 0;
    text(x, y, s, fg);
}

int DrawContext::label_value(int x, int y,
                             std::string_view label, Color label_color,
                             std::string_view value, Color value_color) {
    text(x, y, label, label_color);
    int vx = x + static_cast<int>(label.size());
    text(vx, y, value, value_color);
    return vx + static_cast<int>(value.size());
}

int DrawContext::bar(int x, int y, int bar_width, int value, int max_value,
                     Color fill_color, Color empty_color,
                     char fill_ch, char empty_ch) {
    int filled = (max_value > 0) ? (value * bar_width / max_value) : 0;
    if (filled < 0) filled = 0;
    if (filled > bar_width) filled = bar_width;

    put(x, y, '[');
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) {
            put(x + 1 + i, y, fill_ch, fill_color);
        } else {
            put(x + 1 + i, y, empty_ch, empty_color);
        }
    }
    put(x + 1 + bar_width, y, ']');
    return x + bar_width + 2;
}

DrawContext DrawContext::sub(Rect local_rect) const {
    // Translate local_rect into absolute coordinates, clipped to bounds
    int ax = bounds_.x + local_rect.x;
    int ay = bounds_.y + local_rect.y;
    int aw = local_rect.w;
    int ah = local_rect.h;

    // Clip to parent bounds
    if (ax < bounds_.x) { aw -= (bounds_.x - ax); ax = bounds_.x; }
    if (ay < bounds_.y) { ah -= (bounds_.y - ay); ay = bounds_.y; }
    if (ax + aw > bounds_.x + bounds_.w) aw = bounds_.x + bounds_.w - ax;
    if (ay + ah > bounds_.y + bounds_.h) ah = bounds_.y + bounds_.h - ay;
    if (aw < 0) aw = 0;
    if (ah < 0) ah = 0;

    return DrawContext(renderer_, {ax, ay, aw, ah});
}

const Rect& DrawContext::bounds() const { return bounds_; }
int DrawContext::width() const { return bounds_.w; }
int DrawContext::height() const { return bounds_.h; }

// --- TextList ---

void TextList::draw(DrawContext& ctx, const std::deque<std::string>& lines,
                    int scroll_offset, Color fg) {
    int h = ctx.height();
    if (h <= 0) return;

    int total = static_cast<int>(lines.size());

    // scroll_offset == -1 means auto-scroll to bottom
    int start;
    if (scroll_offset < 0) {
        start = std::max(0, total - h);
    } else {
        start = std::min(scroll_offset, std::max(0, total - h));
    }

    int line = 0;
    for (int i = start; i < total && line < h; ++i, ++line) {
        std::string_view s = lines[i];
        if (static_cast<int>(s.size()) > ctx.width()) {
            s = s.substr(0, ctx.width());
        }
        ctx.text(0, line, s, fg);
    }
}

} // namespace astra
