#include "astra/ui.h"
#include "astra/item.h"

#include <algorithm>
#include <string>

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

void DrawContext::put(int x, int y, char ch, Color fg, Color bg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_char(ax, ay, ch, fg, bg);
    }
}

void DrawContext::put(int x, int y, const char* utf8, Color fg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_glyph(ax, ay, utf8, fg);
    }
}

void DrawContext::text(int x, int y, std::string_view s, Color fg) {
    int col = 0;
    int i = 0;
    int len = static_cast<int>(s.size());
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            // ASCII byte
            put(x + col, y, s[i], fg);
            ++i;
        } else {
            // UTF-8 lead byte — determine sequence length
            int seq_len = 1;
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;

            char buf[5] = {};
            for (int j = 0; j < seq_len && i + j < len; ++j) {
                buf[j] = s[i + j];
            }
            put(x + col, y, buf, fg);
            i += seq_len;
        }
        ++col;
    }
}

void DrawContext::text_rich(int x, int y, std::string_view s, Color default_fg) {
    Color cur = default_fg;
    int col = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < s.size()) {
            cur = static_cast<Color>(static_cast<uint8_t>(s[i + 1]));
            i += 2;
            continue;
        }
        if (ch == static_cast<unsigned char>(COLOR_END)) {
            cur = default_fg;
            ++i;
            continue;
        }
        if (ch < 0x80) {
            // ASCII byte
            put(x + col, y, static_cast<char>(ch), cur);
            ++i;
        } else {
            // UTF-8 multi-byte sequence
            int seq_len = 1;
            if ((ch & 0xE0) == 0xC0) seq_len = 2;
            else if ((ch & 0xF0) == 0xE0) seq_len = 3;
            else if ((ch & 0xF8) == 0xF0) seq_len = 4;
            char buf[5] = {};
            for (int j = 0; j < seq_len && i + j < s.size(); ++j)
                buf[j] = s[i + j];
            put(x + col, y, buf, cur);
            i += seq_len;
        }
        ++col;
    }
}

void DrawContext::text(int x, int y, std::string_view s, Color fg, Color bg) {
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        put(x + i, y, s[i], fg, bg);
    }
}

void DrawContext::hline(int y, char ch) {
    for (int x = 0; x < bounds_.w; ++x) {
        put(x, y, ch);
    }
}

void DrawContext::hline(int y, const char* utf8, Color fg) {
    for (int x = 0; x < bounds_.w; ++x) {
        put(x, y, utf8, fg);
    }
}

void DrawContext::vline(int x, char ch) {
    for (int y = 0; y < bounds_.h; ++y) {
        put(x, y, ch);
    }
}

void DrawContext::vline(int x, const char* utf8, Color fg) {
    for (int y = 0; y < bounds_.h; ++y) {
        put(x, y, utf8, fg);
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

void DrawContext::box(Color fg) {
    // Top and bottom
    for (int x = 1; x < bounds_.w - 1; ++x) {
        put(x, 0, BoxDraw::H, fg);
        put(x, bounds_.h - 1, BoxDraw::H, fg);
    }
    // Left and right
    for (int y = 1; y < bounds_.h - 1; ++y) {
        put(0, y, BoxDraw::V, fg);
        put(bounds_.w - 1, y, BoxDraw::V, fg);
    }
    // Corners
    put(0, 0, BoxDraw::TL, fg);
    put(bounds_.w - 1, 0, BoxDraw::TR, fg);
    put(0, bounds_.h - 1, BoxDraw::BL, fg);
    put(bounds_.w - 1, bounds_.h - 1, BoxDraw::BR, fg);
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
    (void)fill_ch; (void)empty_ch;
    static const char* FILL  = "\xe2\x96\xb0"; // ▰
    static const char* EMPTY = "\xe2\x96\xb1"; // ▱

    int filled = (max_value > 0) ? (value * bar_width / max_value) : 0;
    if (filled < 0) filled = 0;
    if (filled > bar_width) filled = bar_width;

    put(x, y, '[');
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) {
            put(x + 1 + i, y, FILL, fill_color);
        } else {
            put(x + 1 + i, y, EMPTY, empty_color);
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
    ctx.box(Color::DarkGray);

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
    // Decorative separator: ──────═╡  ║║  ╞═──────
    int inner_w = bounds_.w - 2; // inside borders
    int center = inner_w / 2;

    // Center piece: " ║║ "
    int cp_start = center - 1;
    ctx.put(1 + cp_start, y, BoxDraw::DV, Color::Cyan);
    ctx.put(1 + cp_start + 1, y, BoxDraw::DV, Color::Cyan);

    // Connectors: ╡═  and  ═╞
    int left_conn = cp_start - 2;
    if (left_conn >= 0) {
        ctx.put(1 + left_conn, y, BoxDraw::DR, Color::DarkGray);
        ctx.put(1 + left_conn + 1, y, BoxDraw::DH, Color::DarkGray);
    }
    int right_conn = cp_start + 2;
    if (right_conn + 1 < inner_w) {
        ctx.put(1 + right_conn, y, BoxDraw::DH, Color::DarkGray);
        ctx.put(1 + right_conn + 1, y, BoxDraw::DL, Color::DarkGray);
    }

    // ─ extending outward
    for (int x = 1; x < 1 + left_conn; ++x) {
        ctx.put(x, y, BoxDraw::H, Color::DarkGray);
    }
    for (int x = 1 + right_conn + 2; x < bounds_.w - 1; ++x) {
        ctx.put(x, y, BoxDraw::H, Color::DarkGray);
    }

    // Where ornament meets window border, use T-junctions
    ctx.put(0, y, BoxDraw::LT, Color::DarkGray);
    ctx.put(bounds_.w - 1, y, BoxDraw::RT, Color::DarkGray);
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

    // Fixed size: 60% of terminal
    int win_w = screen_w * 60 / 100;
    int win_h = screen_h * 60 / 100;
    if (win_w < 30) win_w = 30;
    if (win_h < 12) win_h = 12;
    if (win_w > screen_w - 4) win_w = screen_w - 4;
    if (win_h > screen_h - 4) win_h = screen_h - 4;

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

// --- Item name rendering ---

int draw_item_name(DrawContext& ctx, int x, int y, const Item& item, bool selected) {
    Color name_color = selected ? Color::White : rarity_color(item.rarity);
    ctx.text(x, y, item.name, name_color);
    x += static_cast<int>(item.name.size());

    if (item.stackable && item.stack_count > 1) {
        std::string stack = " x" + std::to_string(item.stack_count);
        ctx.text(x, y, stack, Color::White);
        x += static_cast<int>(stack.size());
    }
    return x;
}

// --- Panel ---

Panel::Panel(Renderer* renderer, Rect bounds, std::string_view title)
    : renderer_(renderer), bounds_(bounds), title_(title) {}

void Panel::set_footer(std::string_view text) { footer_ = text; }
const Rect& Panel::bounds() const { return bounds_; }

void Panel::draw() {
    DrawContext ctx(renderer_, bounds_);
    ctx.fill(' ');

    int w = bounds_.w;
    int h = bounds_.h;
    Color border = Color::White;

    // Top border: ▐▀▀▀▀▀▌
    ctx.put(0, 0, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < w - 1; ++x)
        ctx.put(x, 0, BoxDraw::UPPER_HALF, border);
    ctx.put(w - 1, 0, BoxDraw::LEFT_HALF, border);

    // Side borders: ▐ ... ▌
    for (int y = 1; y < h - 1; ++y) {
        ctx.put(0, y, BoxDraw::RIGHT_HALF, border);
        ctx.put(w - 1, y, BoxDraw::LEFT_HALF, border);
    }

    // Bottom border: ▐▄▄▄▄▄▌
    ctx.put(0, h - 1, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < w - 1; ++x)
        ctx.put(x, h - 1, BoxDraw::LOWER_HALF, border);
    ctx.put(w - 1, h - 1, BoxDraw::LEFT_HALF, border);

    // Title row (row 1) + separator (row 2)
    bool has_title = !title_.empty();
    if (has_title) {
        int tx = (w - static_cast<int>(title_.size())) / 2;
        ctx.text(tx, 1, title_, Color::White);

        for (int x = 1; x < w - 1; ++x)
            ctx.put(x, 2, BoxDraw::H, Color::DarkGray);
    }

    // Footer separator + text
    if (!footer_.empty()) {
        int sep_y = h - 3;
        for (int x = 1; x < w - 1; ++x)
            ctx.put(x, sep_y, BoxDraw::H, Color::DarkGray);

        int fx = (w - static_cast<int>(footer_.size())) / 2;
        if (fx < 1) fx = 1;
        int fy = h - 2;
        for (size_t i = 0; i < footer_.size(); ++i) {
            if (footer_[i] == '[') {
                ctx.put(fx++, fy, '[', Color::White);
                size_t end = footer_.find(']', i + 1);
                if (end != std::string::npos) {
                    for (size_t j = i + 1; j < end; ++j)
                        ctx.put(fx++, fy, footer_[j], Color::Yellow);
                    ctx.put(fx++, fy, ']', Color::White);
                    i = end;
                }
            } else {
                ctx.put(fx++, fy, footer_[i], Color::DarkGray);
            }
        }
    }
}

DrawContext Panel::content() const {
    bool has_title = !title_.empty();
    bool has_footer = !footer_.empty();
    int top = has_title ? 3 : 1;
    int bottom = has_footer ? 3 : 1;
    return DrawContext(renderer_, Rect{
        bounds_.x + 1,
        bounds_.y + top,
        bounds_.w - 2,
        bounds_.h - top - bottom
    });
}

// --- Item info ---

void draw_item_info(DrawContext& ctx, const Item& item) {
    int y = 0;

    ctx.put(0, y, item.glyph, item.color);
    ctx.text(2, y, rarity_name(item.rarity), rarity_color(item.rarity));
    y++;

    if (!item.description.empty()) {
        y++;
        std::string_view desc = item.description;
        int max_w = ctx.width();
        while (!desc.empty() && y < ctx.height()) {
            if (static_cast<int>(desc.size()) <= max_w) {
                ctx.text(0, y++, desc, Color::Default);
                break;
            }
            int cut = max_w;
            while (cut > 0 && desc[cut] != ' ') --cut;
            if (cut == 0) cut = max_w;
            ctx.text(0, y++, desc.substr(0, cut), Color::Default);
            desc = desc.substr(cut);
            if (!desc.empty() && desc[0] == ' ') desc = desc.substr(1);
        }
    }
    y++;

    const auto& m = item.modifiers;
    if (m.attack) {
        ctx.label_value(0, y, "Attack:    ", Color::DarkGray,
            (m.attack > 0 ? "+" : "") + std::to_string(m.attack), Color::Red);
        y++;
    }
    if (m.defense) {
        ctx.label_value(0, y, "Defense:   ", Color::DarkGray,
            (m.defense > 0 ? "+" : "") + std::to_string(m.defense), Color::Blue);
        y++;
    }
    if (m.max_hp) {
        ctx.label_value(0, y, "Max HP:    ", Color::DarkGray,
            (m.max_hp > 0 ? "+" : "") + std::to_string(m.max_hp), Color::Green);
        y++;
    }
    if (m.view_radius) {
        ctx.label_value(0, y, "Vision:    ", Color::DarkGray,
            (m.view_radius > 0 ? "+" : "") + std::to_string(m.view_radius), Color::Cyan);
        y++;
    }
    if (m.quickness) {
        ctx.label_value(0, y, "Quickness: ", Color::DarkGray,
            (m.quickness > 0 ? "+" : "") + std::to_string(m.quickness), Color::Yellow);
        y++;
    }

    if (item.ranged) {
        const auto& rd = *item.ranged;
        ctx.text(0, y, "Charge: ", Color::DarkGray);
        int bar_w = std::min(16, ctx.width() - 10);
        if (bar_w > 0) {
            ctx.bar(8, y, bar_w, rd.current_charge, rd.charge_capacity,
                    Color::Cyan, Color::DarkGray);
        }
        std::string charge_str = std::to_string(rd.current_charge) + "/"
                               + std::to_string(rd.charge_capacity);
        ctx.text(8 + bar_w + 1, y, charge_str, Color::Cyan);
        y++;
        ctx.label_value(0, y, "Range:     ", Color::DarkGray,
            std::to_string(rd.max_range), Color::White);
        y++;
    }

    if (item.max_durability > 0 && y < ctx.height()) {
        ctx.text(0, y, "Durabl: ", Color::DarkGray);
        int bar_w = std::min(16, ctx.width() - 10);
        if (bar_w > 0) {
            Color dur_color = (item.durability * 3 > item.max_durability) ? Color::Green : Color::Red;
            ctx.bar(8, y, bar_w, item.durability, item.max_durability,
                    dur_color, Color::DarkGray);
        }
        std::string dur_str = std::to_string(item.durability) + "/"
                            + std::to_string(item.max_durability);
        ctx.text(8 + bar_w + 1, y, dur_str, Color::Green);
        y++;
    }

    if (y < ctx.height()) {
        y++;
        std::string info = "Wt:" + std::to_string(item.weight);
        info += "  Buy:" + std::to_string(item.buy_value);
        info += "  Sell:" + std::to_string(item.sell_value);
        ctx.text(0, y, info, Color::DarkGray);
        y++;
    }

    // Enhancements
    if (!item.enhancements.empty()) {
        y++;
        ctx.text(0, y, "Enhancements:", Color::White);
        y++;
        for (int si = 0; si < static_cast<int>(item.enhancements.size()); ++si) {
            if (y >= ctx.height()) break;
            const auto& enh = item.enhancements[si];
            if (enh.filled) {
                std::string line = " [" + std::to_string(si + 1) + "] " + enh.material_name;
                std::string bonus;
                if (enh.bonus.attack) bonus += " ATK+" + std::to_string(enh.bonus.attack);
                if (enh.bonus.defense) bonus += " DEF+" + std::to_string(enh.bonus.defense);
                if (enh.bonus.view_radius) bonus += " VIS+" + std::to_string(enh.bonus.view_radius);
                if (enh.bonus.quickness) bonus += " QCK+" + std::to_string(enh.bonus.quickness);
                ctx.text(0, y, line, Color::Green);
                if (!bonus.empty()) ctx.text(static_cast<int>(line.size()), y, bonus, Color::Cyan);
            } else {
                ctx.text(0, y, " [" + std::to_string(si + 1) + "] empty", Color::DarkGray);
            }
            y++;
        }
    }
}

// --- PopupMenu ---

void PopupMenu::add_option(char key, std::string_view label) {
    options_.push_back({key, std::string(label)});
}

void PopupMenu::set_title(std::string_view title) { title_ = title; }
void PopupMenu::set_body(std::string_view body) { body_ = body; }
void PopupMenu::set_max_width_frac(float frac) { max_width_frac_ = frac; }
void PopupMenu::set_footer(std::string_view footer) { footer_ = footer; }

void PopupMenu::open() { open_ = true; selection_ = 0; }
void PopupMenu::close() { open_ = false; options_.clear(); title_.clear(); body_.clear(); footer_.clear(); }
bool PopupMenu::is_open() const { return open_; }

char PopupMenu::selected_key() const {
    if (selection_ >= 0 && selection_ < static_cast<int>(options_.size()))
        return options_[selection_].key;
    return 0;
}

MenuResult PopupMenu::handle_input(int key) {
    if (!open_) return MenuResult::None;

    if (key == 27) { // ESC
        close();
        return MenuResult::Closed;
    }

    int count = static_cast<int>(options_.size());

    if (key == KEY_UP) { selection_ = (selection_ - 1 + count) % count; return MenuResult::None; }
    if (key == KEY_DOWN) { selection_ = (selection_ + 1) % count; return MenuResult::None; }

    // Enter/space confirms selection
    if (key == '\n' || key == '\r' || key == ' ') {
        open_ = false;
        return MenuResult::Selected;
    }

    // Hotkey press
    for (int i = 0; i < count; ++i) {
        if (key == options_[i].key) {
            selection_ = i;
            open_ = false;
            return MenuResult::Selected;
        }
    }

    return MenuResult::None;
}

void PopupMenu::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_ || options_.empty()) return;

    // Calculate content width from options
    int content_w = 0;
    for (const auto& opt : options_) {
        int entry_w = 15 + static_cast<int>(opt.label.size());
        if (entry_w > content_w) content_w = entry_w;
    }
    bool has_title = !title_.empty();
    if (has_title) {
        int tw = 4 + static_cast<int>(title_.size());
        if (tw > content_w) content_w = tw;
    }
    bool has_footer = !footer_.empty();
    if (has_footer) {
        int fw = 4 + static_cast<int>(footer_.size());
        if (fw > content_w) content_w = fw;
    }

    int max_w = static_cast<int>(screen_w * max_width_frac_);
    if (max_w < 24) max_w = 24;
    int win_w = std::min(content_w + 2, max_w);
    // When body text is present, use full max width for readability
    if (!body_.empty()) win_w = max_w;

    // Word-wrap body text
    bool has_body = !body_.empty();
    std::vector<std::string> body_lines;
    if (has_body) {
        int inner_w = win_w - 5; // 3 left pad + 1 right pad + 1 border
        if (inner_w < 10) inner_w = 10;
        // Word-wrap: accumulate words, break at last space before width limit
        std::string line;
        int vis_len = 0;
        int last_space_pos = -1;     // byte position of last space in line
        for (size_t i = 0; i < body_.size(); ++i) {
            char ch = body_[i];
            if (ch == '\n') {
                body_lines.push_back(line);
                line.clear();
                vis_len = 0;
                last_space_pos = -1;
                continue;
            }
            if (ch == COLOR_BEGIN && i + 1 < body_.size()) {
                line += ch;
                line += body_[++i];
                continue;
            }
            if (ch == COLOR_END) {
                line += ch;
                continue;
            }
            if (static_cast<unsigned char>(ch) >= 0x80 &&
                (static_cast<unsigned char>(ch) & 0xC0) == 0x80) {
                // UTF-8 continuation byte — add to line but don't count as visible
                line += ch;
            } else {
                line += ch;
                ++vis_len;
                if (ch == ' ') {
                    last_space_pos = static_cast<int>(line.size()) - 1;
                }
            }
            if (vis_len >= inner_w) {
                if (last_space_pos > 0) {
                    // Break at the last space
                    std::string remainder = line.substr(last_space_pos + 1);
                    line.resize(last_space_pos);
                    body_lines.push_back(line);
                    line = remainder;
                    // Recount visible chars in remainder
                    vis_len = 0;
                    for (size_t j = 0; j < line.size(); ++j) {
                        if (line[j] == COLOR_BEGIN && j + 1 < line.size()) { ++j; continue; }
                        if (line[j] == COLOR_END) continue;
                        unsigned char uc = static_cast<unsigned char>(line[j]);
                        if (uc >= 0x80 && (uc & 0xC0) == 0x80) continue; // UTF-8 continuation
                        ++vis_len;
                    }
                } else {
                    // No space found — hard break
                    body_lines.push_back(line);
                    line.clear();
                    vis_len = 0;
                }
                last_space_pos = -1;
            }
        }
        if (!line.empty()) body_lines.push_back(line);
    }

    // Layout: top(1) + title(1)? + sep(1)? + body? + sep(1)? + blank(1) + options(n*2-1) + blank(1) + sep(1)? + footer(1)? + bottom(1)
    int option_count = static_cast<int>(options_.size());
    int option_rows = option_count * 2 - 1;
    int title_rows = has_title ? 2 : 0;
    int body_rows = has_body ? static_cast<int>(body_lines.size()) + 3 : 0; // blank + lines + blank + sep
    int footer_rows = has_footer ? 2 : 0;
    int win_h = 1 + title_rows + body_rows + 1 + option_rows + 1 + footer_rows + 1;

    int mx = (screen_w - win_w) / 2;
    int my = (screen_h - win_h) / 2;

    // Use Panel-style half-block borders (▐▀▌▄)
    Rect bounds{mx, my, win_w, win_h};
    DrawContext ctx(renderer, bounds);
    ctx.fill(' ');

    Color border = Color::White;

    // Top border: ▐▀▀▀▀▌
    ctx.put(0, 0, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < win_w - 1; ++x)
        ctx.put(x, 0, BoxDraw::UPPER_HALF, border);
    ctx.put(win_w - 1, 0, BoxDraw::LEFT_HALF, border);

    // Side borders: ▐ ... ▌
    for (int y = 1; y < win_h - 1; ++y) {
        ctx.put(0, y, BoxDraw::RIGHT_HALF, border);
        ctx.put(win_w - 1, y, BoxDraw::LEFT_HALF, border);
    }

    // Bottom border: ▐▄▄▄▄▌
    ctx.put(0, win_h - 1, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < win_w - 1; ++x)
        ctx.put(x, win_h - 1, BoxDraw::LOWER_HALF, border);
    ctx.put(win_w - 1, win_h - 1, BoxDraw::LEFT_HALF, border);

    int y = 1;

    // Title + separator
    if (has_title) {
        int tx = (win_w - static_cast<int>(title_.size())) / 2;
        ctx.text(tx, y, title_, Color::White);
        y++;
        for (int x = 1; x < win_w - 1; ++x)
            ctx.put(x, y, BoxDraw::H, Color::DarkGray);
        y++;
    }

    // Body text (NPC speech etc.)
    if (has_body) {
        y++; // blank line before body
        for (const auto& bl : body_lines) {
            ctx.text_rich(3, y, bl, Color::Cyan);
            y++;
        }
        y++; // blank line after body
        // Separator between body and options
        for (int x = 1; x < win_w - 1; ++x)
            ctx.put(x, y, BoxDraw::H, Color::DarkGray);
        y++;
    }

    y++; // blank line before options

    // Options with blank lines between
    for (int i = 0; i < option_count; ++i) {
        const auto& opt = options_[i];
        bool sel = (selection_ == i);
        int ox = 5;

        if (sel) ctx.put(ox - 2, y, '>', Color::Yellow);

        ctx.put(ox, y, '[', Color::White);
        ctx.put(ox + 1, y, opt.key, Color::Yellow);
        ctx.put(ox + 2, y, ']', Color::White);
        ox += 4;

        int max_label = win_w - ox - 2; // leave 2 chars before right border
        bool highlighted = false;
        for (int ci = 0; ci < static_cast<int>(opt.label.size()) && ci < max_label; ++ci) {
            char ch = opt.label[ci];
            if (!highlighted && (ch == opt.key || ch == (opt.key - 32) || ch == (opt.key + 32))) {
                ctx.put(ox + ci, y, ch, Color::Yellow);
                highlighted = true;
            } else {
                ctx.put(ox + ci, y, ch, sel ? Color::White : Color::DarkGray);
            }
        }
        y += 2;
    }

    // Footer separator + text
    if (has_footer) {
        int sep_y = win_h - 3;
        for (int x = 1; x < win_w - 1; ++x)
            ctx.put(x, sep_y, BoxDraw::H, Color::DarkGray);

        std::string footer = footer_;
        int fx = (win_w - static_cast<int>(footer.size())) / 2;
        if (fx < 1) fx = 1;
        int fy = win_h - 2;
        for (size_t i = 0; i < footer.size(); ++i) {
            if (footer[i] == '[') {
                ctx.put(fx++, fy, '[', Color::White);
                size_t end = footer.find(']', i + 1);
                if (end != std::string::npos) {
                    for (size_t j = i + 1; j < end; ++j)
                        ctx.put(fx++, fy, footer[j], Color::Yellow);
                    ctx.put(fx++, fy, ']', Color::White);
                    i = end;
                }
            } else {
                ctx.put(fx++, fy, footer[i], Color::DarkGray);
            }
        }
    }
}

} // namespace astra
