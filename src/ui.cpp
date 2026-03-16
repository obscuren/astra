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

// --- Window ---

Window::Window(Renderer* renderer, Rect bounds, std::string_view title)
    : renderer_(renderer), bounds_(bounds), title_(title) {}

Window::Window(Renderer* renderer, int screen_w, int screen_h,
               int width, int height, std::string_view title)
    : renderer_(renderer),
      bounds_{(screen_w - width) / 2, (screen_h - height) / 2, width, height},
      title_(title) {}

void Window::draw() {
    DrawContext ctx(renderer_, bounds_);

    // Clear window area
    ctx.fill(' ');

    // Border
    for (int x = 1; x < bounds_.w - 1; ++x) {
        ctx.put(x, 0, '-', Color::DarkGray);
        ctx.put(x, bounds_.h - 1, '-', Color::DarkGray);
    }
    for (int y = 1; y < bounds_.h - 1; ++y) {
        ctx.put(0, y, '|', Color::DarkGray);
        ctx.put(bounds_.w - 1, y, '|', Color::DarkGray);
    }
    ctx.put(0, 0, '+', Color::DarkGray);
    ctx.put(bounds_.w - 1, 0, '+', Color::DarkGray);
    ctx.put(0, bounds_.h - 1, '+', Color::DarkGray);
    ctx.put(bounds_.w - 1, bounds_.h - 1, '+', Color::DarkGray);

    // Title (centered, row 1)
    if (!title_.empty()) {
        ctx.text_center(1, title_, Color::White);

        // Decorative ornament below title (row 2)
        draw_ornament(ctx, 2);
    }

    // Footer
    if (!footer_.empty()) {
        // Separator above footer
        int sep_y = bounds_.h - 3;
        draw_ornament(ctx, sep_y);

        // Footer text centered
        ctx.text_center(bounds_.h - 2, footer_, footer_color_);
    }
}

void Window::draw_ornament(DrawContext& ctx, int y) {
    // Decorative separator: ------=+  ||  +=------
    int inner_w = bounds_.w - 2; // inside borders
    int center = inner_w / 2;

    // Center piece: " || "
    int cp_start = center - 1;
    ctx.put(1 + cp_start, y, '|', Color::Cyan);
    ctx.put(1 + cp_start + 1, y, '|', Color::Cyan);

    // Connectors: +=  and  =+
    int left_conn = cp_start - 2;
    if (left_conn >= 0) {
        ctx.put(1 + left_conn, y, '+', Color::DarkGray);
        ctx.put(1 + left_conn + 1, y, '=', Color::DarkGray);
    }
    int right_conn = cp_start + 2;
    if (right_conn + 1 < inner_w) {
        ctx.put(1 + right_conn, y, '=', Color::DarkGray);
        ctx.put(1 + right_conn + 1, y, '+', Color::DarkGray);
    }

    // Dashes extending outward
    for (int x = 1; x < 1 + left_conn; ++x) {
        ctx.put(x, y, '-', Color::DarkGray);
    }
    for (int x = 1 + right_conn + 2; x < bounds_.w - 1; ++x) {
        ctx.put(x, y, '-', Color::DarkGray);
    }
}

DrawContext Window::content() const {
    // Content area: inside border, below title+ornament, above footer+ornament
    int top = title_.empty() ? 1 : 3;
    int bottom = footer_.empty() ? 1 : 3;
    Rect content_rect = {
        bounds_.x + 2,            // 1 border + 1 padding
        bounds_.y + top,
        bounds_.w - 4,            // 2 border + 2 padding
        bounds_.h - top - bottom
    };
    return DrawContext(renderer_, content_rect);
}

void Window::set_footer(std::string_view text, Color color) {
    footer_ = text;
    footer_color_ = color;
}

const Rect& Window::bounds() const { return bounds_; }

// --- Dialog ---

Dialog::Dialog(std::string_view title, std::string_view body)
    : title_(title), body_(body) {}

void Dialog::add_option(std::string_view label, int hotkey) {
    options_.push_back({std::string(label), hotkey});
}

DialogResult Dialog::handle_input(int key) {
    if (!open_) return DialogResult::None;

    // Close on Escape
    if (key == '\033') {
        close();
        return DialogResult::Closed;
    }

    // Confirm selection
    if (key == '\n' || key == '\r') {
        close();
        return DialogResult::Selected;
    }

    // Arrow keys / vim keys to navigate
    if (key == KEY_UP || key == 'k') {
        if (!options_.empty()) {
            selection_ = (selection_ - 1 + static_cast<int>(options_.size())) % static_cast<int>(options_.size());
        }
        return DialogResult::None;
    }
    if (key == KEY_DOWN || key == 'j') {
        if (!options_.empty()) {
            selection_ = (selection_ + 1) % static_cast<int>(options_.size());
        }
        return DialogResult::None;
    }

    // Hotkeys
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        if (options_[i].hotkey != -1 && options_[i].hotkey == key) {
            selection_ = i;
            close();
            return DialogResult::Selected;
        }
    }

    return DialogResult::None;
}

void Dialog::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_) return;

    // Compute window size based on content
    int content_w = static_cast<int>(title_.size());
    if (static_cast<int>(body_.size()) > content_w) content_w = static_cast<int>(body_.size());
    for (const auto& opt : options_) {
        int opt_w = static_cast<int>(opt.label.size()) + 6; // "> [x] label"
        if (opt_w > content_w) content_w = opt_w;
    }
    content_w += 6; // padding

    // Height: body (if any) + blank + options (each with spacing) + padding
    int body_lines = body_.empty() ? 0 : 2; // body text + blank line
    int option_lines = static_cast<int>(options_.size()) * 2;
    int content_h = body_lines + option_lines + 1;

    // Window size = content + border(2) + title(2) + footer(3) + padding
    int win_w = std::min(content_w + 4, screen_w - 4);
    int win_h = std::min(content_h + 8, screen_h - 4);

    Window win(renderer, screen_w, screen_h, win_w, win_h, title_);
    win.set_footer("[Esc] Close   [Enter] Select");
    win.draw();

    DrawContext ctx = win.content();
    int y = 0;

    // Body text
    if (!body_.empty()) {
        // Simple word wrap
        std::string_view remaining = body_;
        int max_w = ctx.width();
        while (!remaining.empty() && y < ctx.height()) {
            if (static_cast<int>(remaining.size()) <= max_w) {
                ctx.text(0, y++, remaining, Color::Default);
                break;
            }
            // Find last space within max_w
            int cut = max_w;
            while (cut > 0 && remaining[cut] != ' ') --cut;
            if (cut == 0) cut = max_w; // no space found, hard break
            ctx.text(0, y++, remaining.substr(0, cut), Color::Default);
            remaining = remaining.substr(cut);
            if (!remaining.empty() && remaining[0] == ' ') remaining = remaining.substr(1);
        }
        y++; // blank line after body
    }

    // Options
    for (int i = 0; i < static_cast<int>(options_.size()) && y < ctx.height(); ++i) {
        bool active = (i == selection_);
        std::string prefix;
        if (options_[i].hotkey != -1) {
            prefix += '[';
            prefix += static_cast<char>(options_[i].hotkey);
            prefix += "] ";
        }

        if (active) {
            ctx.text(0, y, "> ", Color::Yellow);
            ctx.text(2, y, prefix, Color::DarkGray);
            ctx.text(2 + static_cast<int>(prefix.size()), y, options_[i].label, Color::Yellow);
        } else {
            ctx.text(0, y, "  ", Color::Default);
            ctx.text(2, y, prefix, Color::DarkGray);
            ctx.text(2 + static_cast<int>(prefix.size()), y, options_[i].label, Color::Cyan);
        }
        y += 2; // spacing between options
    }
}

} // namespace astra
