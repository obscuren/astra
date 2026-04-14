#include "astra/ui.h"
#include "astra/display_name.h"
#include "astra/item.h"
#include "terminal_theme.h"

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

// --- UIContext ---

UIContext::UIContext(Renderer* r, Rect bounds)
    : renderer_(r), bounds_(bounds) {}

void UIContext::put(int x, int y, char ch) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_char(ax, ay, ch);
    }
}

void UIContext::put(int x, int y, char ch, Color fg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_char(ax, ay, ch, fg);
    }
}

void UIContext::put(int x, int y, char ch, Color fg, Color bg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_char(ax, ay, ch, fg, bg);
    }
}

void UIContext::put(int x, int y, const char* utf8, Color fg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_glyph(ax, ay, utf8, fg);
    }
}

void UIContext::put(int x, int y, const char* utf8, Color fg, Color bg) {
    int ax = bounds_.x + x;
    int ay = bounds_.y + y;
    if (bounds_.contains(ax, ay)) {
        renderer_->draw_glyph(ax, ay, utf8, fg, bg);
    }
}

void UIContext::text(int x, int y, std::string_view s, Color fg) {
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

void UIContext::text_rich(int x, int y, std::string_view s, Color default_fg) {
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

void UIContext::text(int x, int y, std::string_view s, Color fg, Color bg) {
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        put(x + i, y, s[i], fg, bg);
    }
}

void UIContext::hline(int y, char ch) {
    for (int x = 0; x < bounds_.w; ++x) {
        put(x, y, ch);
    }
}

void UIContext::hline(int y, const char* utf8, Color fg) {
    for (int x = 0; x < bounds_.w; ++x) {
        put(x, y, utf8, fg);
    }
}

void UIContext::vline(int x, char ch) {
    for (int y = 0; y < bounds_.h; ++y) {
        put(x, y, ch);
    }
}

void UIContext::vline(int x, const char* utf8, Color fg) {
    for (int y = 0; y < bounds_.h; ++y) {
        put(x, y, utf8, fg);
    }
}

void UIContext::border(char h, char v, char corner) {
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

void UIContext::box(Color fg) {
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

void UIContext::fill(char ch) {
    for (int y = 0; y < bounds_.h; ++y) {
        for (int x = 0; x < bounds_.w; ++x) {
            put(x, y, ch);
        }
    }
}

void UIContext::text_left(int y, std::string_view s, Color fg) {
    text(0, y, s, fg);
}

void UIContext::text_center(int y, std::string_view s, Color fg) {
    int x = (bounds_.w - static_cast<int>(s.size())) / 2;
    if (x < 0) x = 0;
    text(x, y, s, fg);
}

void UIContext::text_right(int y, std::string_view s, Color fg) {
    int x = bounds_.w - static_cast<int>(s.size());
    if (x < 0) x = 0;
    text(x, y, s, fg);
}

int UIContext::label_value(int x, int y,
                             std::string_view label, Color label_color,
                             std::string_view value, Color value_color) {
    text(x, y, label, label_color);
    int vx = x + static_cast<int>(label.size());
    text(vx, y, value, value_color);
    return vx + static_cast<int>(value.size());
}

int UIContext::bar(int x, int y, int bar_width, int value, int max_value,
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

UIContext UIContext::sub(Rect local_rect) const {
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

    return UIContext(renderer_, {ax, ay, aw, ah});
}

const Rect& UIContext::bounds() const { return bounds_; }
int UIContext::width() const { return bounds_.w; }
int UIContext::height() const { return bounds_.h; }

// --- TextList ---

void TextList::draw(UIContext& ctx, const std::deque<std::string>& lines,
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


// --- Item name rendering ---

int draw_item_name(UIContext& ctx, int x, int y, const Item& item, bool selected) {
    Color name_color = selected ? Color::White : rarity_color(item.rarity);
    ctx.text(x, y, item.name, name_color);
    x += static_cast<int>(item.name.size());

    // Dice suffix for weapons/grenades (in dim white)
    if (!item.damage_dice.empty()) {
        std::string dice = " - " + item.damage_dice.to_string();
        ctx.text(x, y, dice, Color::DarkGray);
        x += static_cast<int>(dice.size());
    }

    if (item.stackable && item.stack_count > 1) {
        std::string stack = " x" + std::to_string(item.stack_count);
        ctx.text(x, y, stack, Color::White);
        x += static_cast<int>(stack.size());
    }
    return x;
}

// --- Item info ---

void draw_item_info(UIContext& ctx, const Item& item) {
    int y = 0;

    auto vis = item_visual(item.item_def_id);
    ctx.put(0, y, vis.glyph, vis.fg);
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

    if (!item.damage_dice.empty() && y < ctx.height()) {
        ctx.label_value(0, y, "Damage:    ", Color::DarkGray,
            item.damage_dice.to_string(), Color::White);
        y++;
        ctx.text(0, y, "Type:      ", Color::DarkGray);
        ctx.text_rich(11, y, display_name(item.damage_type), Color::Default);
        y++;
    }

    const auto& m = item.modifiers;
    if (m.av) {
        ctx.label_value(0, y, "AV:        ", Color::DarkGray,
            (m.av > 0 ? "+" : "") + std::to_string(m.av), Color::Red);
        y++;
    }
    if (m.dv) {
        ctx.label_value(0, y, "DV:        ", Color::DarkGray,
            (m.dv > 0 ? "+" : "") + std::to_string(m.dv), Color::Blue);
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
                if (enh.bonus.av) bonus += " AV+" + std::to_string(enh.bonus.av);
                if (enh.bonus.dv) bonus += " DV+" + std::to_string(enh.bonus.dv);
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

// --- MenuState ---

void MenuState::add_option(char key, std::string_view label) {
    options.push_back({key, std::string(label)});
}

void MenuState::reset() {
    open = false;
    selection = 0;
    options.clear();
    title.clear();
    body.clear();
    footer.clear();
}

char MenuState::selected_key() const {
    if (selection >= 0 && selection < static_cast<int>(options.size()))
        return options[selection].key;
    return 0;
}

MenuResult MenuState::handle_input(int key) {
    if (!open) return MenuResult::None;

    if (key == 27) { // ESC
        reset();
        return MenuResult::Closed;
    }

    int count = static_cast<int>(options.size());

    if (key == KEY_UP) { selection = (selection - 1 + count) % count; return MenuResult::None; }
    if (key == KEY_DOWN) { selection = (selection + 1) % count; return MenuResult::None; }

    if (key == '\n' || key == '\r' || key == ' ') {
        open = false;
        return MenuResult::Selected;
    }

    for (int i = 0; i < count; ++i) {
        if (key == options[i].key) {
            selection = i;
            open = false;
            return MenuResult::Selected;
        }
    }

    return MenuResult::None;
}

} // namespace astra
