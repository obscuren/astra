#include "astra/ui.h"
#include "astra/cyberdeck.h"
#include "astra/display_name.h"
#include "astra/effect.h"
#include "astra/grenade.h"
#include "astra/hackable.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/trap.h"
#include "terminal_theme.h"

#include <algorithm>
#include <string>
#include <vector>

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

int UIContext::rich_visible_length(std::string_view s) {
    int col = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < s.size()) {
            i += 2;
            continue;
        }
        if (ch == static_cast<unsigned char>(COLOR_END)) {
            ++i;
            continue;
        }
        if (ch < 0x80) {
            ++i;
        } else {
            int seq_len = 1;
            if ((ch & 0xE0) == 0xC0) seq_len = 2;
            else if ((ch & 0xF0) == 0xE0) seq_len = 3;
            else if ((ch & 0xF8) == 0xF0) seq_len = 4;
            i += seq_len;
        }
        ++col;
    }
    return col;
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
    int col = 0;
    int i = 0;
    int len = static_cast<int>(s.size());
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            put(x + col, y, s[i], fg, bg);
            ++i;
        } else {
            int seq_len = 1;
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;
            char buf[5] = {};
            for (int j = 0; j < seq_len && i + j < len; ++j) {
                buf[j] = s[i + j];
            }
            put(x + col, y, buf, fg, bg);
            i += seq_len;
        }
        ++col;
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

int draw_item_name(UIContext& ctx, int x, int y, const Item& item, bool /*selected*/) {
    // Selection highlight is conveyed by the caller's '>' cursor; the rich
    // display includes glyph + name + slots + dice + energy + stack.
    std::string rich = display_name(item);
    ctx.text_rich(x, y, rich);
    return x + UIContext::rich_visible_length(rich);
}

// --- Item info ---

void draw_item_info(UIContext& ctx, const Item& item, const Player* player) {
    int y = 0;

    auto vis = item_visual(item.item_def_id);
    ctx.put(0, y, vis.glyph, vis.fg);
    ctx.text(2, y, rarity_name(item.rarity), rarity_color(item.rarity));
    y++;

    if (!item.description.empty()) {
        y++;
        // Split on explicit newlines first; each paragraph is wrapped independently.
        std::string desc_str = item.description;
        // Split into paragraphs on "\n"
        std::vector<std::string> paragraphs;
        {
            size_t start = 0;
            while (start <= desc_str.size()) {
                size_t nl = desc_str.find('\n', start);
                if (nl == std::string::npos) {
                    paragraphs.push_back(desc_str.substr(start));
                    break;
                }
                paragraphs.push_back(desc_str.substr(start, nl - start));
                start = nl + 1;
            }
        }
        int max_w = ctx.width();
        for (const std::string& para : paragraphs) {
            if (y >= ctx.height()) break;
            if (para.empty()) {
                // blank line between paragraphs
                y++;
                continue;
            }
            // Word-wrap the paragraph, respecting COLOR marker boundaries.
            // Walk byte-by-byte counting visible columns; never split inside a
            // COLOR_BEGIN <byte> ... COLOR_END triplet.
            std::string_view sv = para;
            while (!sv.empty() && y < ctx.height()) {
                if (UIContext::rich_visible_length(sv) <= max_w) {
                    ctx.text_rich(0, y++, sv, Color::Default);
                    break;
                }
                // Find the last safe break point at or before max_w visible cols.
                // Walk and track last space position (byte index) and its visible col.
                int vis_col = 0;
                size_t i = 0;
                size_t last_space_byte = 0;
                bool found_space = false;
                bool in_color = false;
                while (i < sv.size()) {
                    unsigned char ch = static_cast<unsigned char>(sv[i]);
                    // COLOR_BEGIN <byte>: skip both bytes, no visible column
                    if (ch == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < sv.size()) {
                        in_color = true;
                        i += 2;
                        continue;
                    }
                    // COLOR_END: skip byte, no visible column
                    if (ch == static_cast<unsigned char>(COLOR_END)) {
                        in_color = false;
                        ++i;
                        continue;
                    }
                    // Determine byte length of this character
                    int seq_len = 1;
                    if (ch >= 0x80) {
                        if ((ch & 0xE0) == 0xC0) seq_len = 2;
                        else if ((ch & 0xF0) == 0xE0) seq_len = 3;
                        else if ((ch & 0xF8) == 0xF0) seq_len = 4;
                    }
                    // If this visible column would exceed max_w, stop
                    if (vis_col >= max_w) break;
                    // Record space positions for word-wrap break
                    if (ch == ' ' && !in_color) {
                        last_space_byte = i;
                        found_space = true;
                    }
                    i += seq_len;
                    ++vis_col;
                }
                // Determine cut point
                size_t cut_byte;
                if (found_space && last_space_byte > 0) {
                    cut_byte = last_space_byte;
                } else {
                    // No space found — hard-cut at the byte position where we stopped
                    cut_byte = i;
                }
                ctx.text_rich(0, y++, sv.substr(0, cut_byte), Color::Default);
                sv = sv.substr(cut_byte);
                // Skip leading space after the break
                if (!sv.empty() && sv[0] == ' ') sv = sv.substr(1);
            }
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

    if (item.type == ItemType::Mine && y < ctx.height()) {
        TrapKind kind = trap_kind_for_item_id(item.item_def_id);
        const TrapDef& def = trap_def_for(kind);
        EffectId status = static_cast<EffectId>(def.status);

        if (kind == TrapKind::DecoyMine) {
            ctx.label_value(0, y, "Effect:    ", Color::DarkGray,
                "Noise pull (r5, 5t)", Color::Yellow);
            y++;
        } else if (kind == TrapKind::Caltrops) {
            ctx.label_value(0, y, "Damage:    ", Color::DarkGray,
                std::to_string(def.damage) + " per step", Color::White);
            y++;
            ctx.label_value(0, y, "Coverage:  ", Color::DarkGray,
                "4 of 9 in 3x3", Color::White);
            y++;
            ctx.label_value(0, y, "Activations:", Color::DarkGray,
                " 3 per tile", Color::White);
            y++;
            ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                "Slow (" + std::to_string(def.status_duration) + "t)",
                Color::Cyan);
            y++;
        } else {
            ctx.label_value(0, y, "Damage:    ", Color::DarkGray,
                std::to_string(def.damage), Color::White);
            y++;
            if (def.burst_radius > 0) {
                int side = 2 * def.burst_radius + 1;
                ctx.label_value(0, y, "Burst:     ", Color::DarkGray,
                    std::to_string(side) + "x" + std::to_string(side),
                    Color::White);
                y++;
            }
            if (status == EffectId::Burn) {
                ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                    "Burn " + std::to_string(def.status_duration) + "t @ "
                        + std::to_string(def.status_tick_damage) + "/t",
                    Color::Red);
                y++;
            } else if (status == EffectId::EmpDisabled) {
                ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                    "EMP-Disabled " + std::to_string(def.status_duration) + "t",
                    Color::Blue);
                y++;
            } else if (status == EffectId::Slow) {
                ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                    "Slow " + std::to_string(def.status_duration) + "t",
                    Color::Cyan);
                y++;
            }
        }
        ctx.label_value(0, y, "Throw rng: ", Color::DarkGray,
            std::to_string(trap_throw_range(kind)) + " tiles",
            Color::White);
        y++;
        // Detection DC for hidden kinds (visible kinds skip the line).
        int dc = 0;
        switch (kind) {
            case TrapKind::ProximityMine:  dc = 12; break;
            case TrapKind::EmpMine:        dc = 13; break;
            case TrapKind::IncendiaryMine: dc = 11; break;
            default: break;
        }
        if (dc > 0) {
            ctx.label_value(0, y, "Detect DC: ", Color::DarkGray,
                std::to_string(dc), Color::DarkGray);
            y++;
        }
    }

    if (item.type == ItemType::Grenade && y < ctx.height()) {
        GrenadeKind kind = grenade_kind_for_item_id(item.item_def_id);
        const GrenadeDef& def = grenade_def_for(kind);
        EffectId status = static_cast<EffectId>(def.status);

        if (def.damage > 0) {
            ctx.label_value(0, y, "Damage:    ", Color::DarkGray,
                std::to_string(def.damage), Color::White);
            y++;
        }
        if (def.burst_radius > 0) {
            int side = 2 * def.burst_radius + 1;
            ctx.label_value(0, y, "Blast:     ", Color::DarkGray,
                std::to_string(side) + "x" + std::to_string(side),
                Color::White);
            y++;
        }
        if (status == EffectId::Burn) {
            ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                "Burn " + std::to_string(def.status_duration) + "t @ "
                    + std::to_string(def.status_tick_damage) + "/t",
                Color::Red);
            y++;
        } else if (status == EffectId::EmpDisabled) {
            ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                "EMP-Disabled " + std::to_string(def.status_duration) + "t",
                Color::Blue);
            y++;
        } else if (status == EffectId::Slow) {
            ctx.label_value(0, y, "Status:    ", Color::DarkGray,
                "Slow " + std::to_string(def.status_duration) + "t",
                Color::Cyan);
            y++;
        }
        ctx.label_value(0, y, "Throw rng: ", Color::DarkGray,
            std::to_string(grenade_throw_range(kind)) + " tiles",
            Color::White);
        y++;
        ctx.text(0, y, "Detonates on impact.", Color::DarkGray);
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

    if (item.energy) {
        const auto& e = *item.energy;
        ctx.text(0, y, "Charge: ", Color::DarkGray);
        int bar_w = std::min(16, ctx.width() - 10);
        if (bar_w > 0) {
            ctx.bar(8, y, bar_w, e.current, e.capacity,
                    Color::Cyan, Color::DarkGray);
        }
        std::string charge_str = std::to_string(e.current) + "/"
                               + std::to_string(e.capacity);
        ctx.text(8 + bar_w + 1, y, charge_str, Color::Cyan);
        y++;
    }
    if (item.consumer) {
        ctx.label_value(0, y, "Energy/use:", Color::DarkGray,
            std::to_string(item.consumer->energy_per_use), Color::White);
        y++;
    }
    if (item.ranged) {
        ctx.label_value(0, y, "Range:     ", Color::DarkGray,
            std::to_string(item.ranged->max_range), Color::White);
        y++;
    }
    if (item.proc && item.proc->kind != CellProcKind::None) {
        const auto& p = *item.proc;
        std::string desc;
        switch (p.kind) {
            case CellProcKind::ShieldOvercharge:
                desc = "+" + std::to_string(p.magnitude) + " shield overcharge per " +
                       std::to_string(p.threshold) + " drained";
                break;
            case CellProcKind::WeaponOvercharge:
                desc = "+" + std::to_string(p.magnitude) + " weapon overcharge per " +
                       std::to_string(p.threshold) + " drained";
                break;
            case CellProcKind::DefenseBoost:
                desc = "+" + std::to_string(p.magnitude) + " DV for " +
                       std::to_string(p.duration) + " turns per " +
                       std::to_string(p.threshold) + " drained";
                break;
            case CellProcKind::AdrenalineRush:
                desc = "Adrenaline (" + std::to_string(p.duration) +
                       " turns) per " + std::to_string(p.threshold) + " drained";
                break;
            case CellProcKind::None:
                break;
        }
        if (!desc.empty()) {
            ctx.label_value(0, y, "Proc:      ", Color::DarkGray, desc, Color::Magenta);
            y++;
            if (p.accumulator > 0) {
                ctx.label_value(0, y, "  charge:  ", Color::DarkGray,
                    std::to_string(p.accumulator) + "/" + std::to_string(p.threshold),
                    Color::DarkGray);
                y++;
            }
        }
    }
    for (const auto& enh : item.enhancements) {
        if (!enh.committed) continue;
        if (enh.solar_panel) {
            const auto& sp = *enh.solar_panel;
            std::string mod = std::string(sp.active ? "Solar Panel (active, +" : "Solar Panel (off, +") +
                              std::to_string(sp.energy_per_tick) + "/" +
                              std::to_string(sp.tick_interval) + " turns)";
            ctx.label_value(0, y, "Mod:       ", Color::DarkGray, mod, Color::Yellow);
            y++;
        }
        if (enh.energy_bonus.capacity_bonus) {
            ctx.label_value(0, y, "Mod:       ", Color::DarkGray,
                "+" + std::to_string(enh.energy_bonus.capacity_bonus) + " capacity",
                Color::Yellow);
            y++;
        }
        if (enh.energy_bonus.charge_rate_bonus) {
            ctx.label_value(0, y, "Mod:       ", Color::DarkGray,
                "+" + std::to_string(enh.energy_bonus.charge_rate_bonus) + "% charge rate",
                Color::Yellow);
            y++;
        }
        if (enh.energy_bonus.discharge_efficiency) {
            ctx.label_value(0, y, "Mod:       ", Color::DarkGray,
                "+1 free per " + std::to_string(enh.energy_bonus.discharge_efficiency) + " transferred",
                Color::Yellow);
            y++;
        }
    }

    if (item.type == ItemType::Cyberdeck && item.deck && y < ctx.height()) {
        const auto& d = *item.deck;
        // Fold in the player's implant bonuses so the deck panel reflects
        // the *effective* values once jacked in. Show the bonus separately
        // (e.g. "4 +2") so the source remains visible.
        int ram_bonus  = 0, heat_bonus = 0, cool_bonus = 0;
        if (player) {
            auto im      = player->implant_modifiers();
            ram_bonus    = im.ram_cap_bonus;
            heat_bonus   = im.heat_cap_bonus;
            cool_bonus   = im.cooling_rate_bonus;
        }
        auto with_bonus = [](int base, int bonus) {
            if (bonus == 0) return std::to_string(base);
            return std::to_string(base + bonus) + " (" + std::to_string(base)
                 + (bonus > 0 ? "+" : "") + std::to_string(bonus) + ")";
        };
        // RAM bonus bumps both current and cap (implant is pre-charged).
        int eff_ram_cur = d.ram_current + ram_bonus;
        int eff_ram_max = d.stats.ram_max + ram_bonus;
        if (eff_ram_cur > eff_ram_max) eff_ram_cur = eff_ram_max;
        ctx.label_value(0, y, "RAM:       ", Color::DarkGray,
            std::to_string(eff_ram_cur) + "/"
                + with_bonus(d.stats.ram_max, ram_bonus),
            Color::Cyan);
        y++;
        ctx.label_value(0, y, "CPU:       ", Color::DarkGray,
            std::to_string(d.stats.cpu), Color::White);
        y++;
        ctx.label_value(0, y, "Slots:     ", Color::DarkGray,
            std::to_string(d.stats.slots), Color::White);
        y++;
        ctx.label_value(0, y, "Stealth:   ", Color::DarkGray,
            "+" + std::to_string(d.stats.stealth), Color::Cyan);
        y++;
        ctx.label_value(0, y, "Cooling:   ", Color::DarkGray,
            with_bonus(d.stats.cooling_rate, cool_bonus) + "/turn", Color::White);
        y++;
        ctx.label_value(0, y, "Heat cap:  ", Color::DarkGray,
            with_bonus(d.stats.heat_cap, heat_bonus), Color::White);
        y++;

        // Loaded programs (live slots only)
        int loaded_count = 0;
        for (int i = 0; i < d.stats.slots; ++i) {
            if (d.loaded[i].program_def_id != 0) ++loaded_count;
        }
        if (loaded_count > 0 && y < ctx.height()) {
            y++;
            ctx.text(0, y, "Loaded:", Color::White);
            y++;
            for (int i = 0; i < d.stats.slots && y < ctx.height(); ++i) {
                if (d.loaded[i].program_def_id == 0) continue;
                Item probe = build_by_def_id(d.loaded[i].program_def_id);
                if (!probe.program) continue;
                const ProgramDef* def = find_program(probe.program->id);
                if (!def) continue;
                std::string line = " [" + std::to_string(i) + "] " +
                                   def->filename + " " +
                                   program_kind_short(def->kind);
                ctx.text(0, y, line, Color::Cyan);
                y++;
            }
        }
    }

    if ((item.type == ItemType::Implant || item.type == ItemType::RelayCortex)
        && y < ctx.height()) {
        // Slot row — always shown.
        ctx.label_value(0, y, "Slot:      ", Color::DarkGray,
            implant_slot_requirement_name(item.required_implant_slot),
            Color::Cyan);
        y++;

        // Wired stat modifiers — render only non-zero / non-false values.
        auto wired_attr = [&](const char* label, int value, Color val_col) {
            if (value == 0 || y >= ctx.height()) return;
            std::string val = (value > 0 ? "+" : "") + std::to_string(value);
            ctx.label_value(0, y, label, Color::DarkGray, val, val_col);
            y++;
        };
        auto wired_attr_suffix = [&](const char* label, int value,
                                     const char* suffix, Color val_col) {
            if (value == 0 || y >= ctx.height()) return;
            std::string val = (value > 0 ? "+" : "") + std::to_string(value) + suffix;
            ctx.label_value(0, y, label, Color::DarkGray, val, val_col);
            y++;
        };
        auto col = [](int v) { return v >= 0 ? Color::Cyan : Color::Red; };

        wired_attr("INT:       ", item.modifiers.intelligence, col(item.modifiers.intelligence));
        wired_attr("WIL:       ", item.modifiers.willpower,    col(item.modifiers.willpower));
        wired_attr("HP:        ", item.modifiers.max_hp,       col(item.modifiers.max_hp));
        wired_attr("AV:        ", item.modifiers.av,           col(item.modifiers.av));
        wired_attr("DV:        ", item.modifiers.dv,           col(item.modifiers.dv));
        wired_attr("Vision:    ", item.modifiers.view_radius,  col(item.modifiers.view_radius));
        wired_attr("Quickness: ", item.modifiers.quickness,    col(item.modifiers.quickness));
        wired_attr("RAM cap:   ", item.modifiers.ram_cap_bonus,
                   col(item.modifiers.ram_cap_bonus));
        wired_attr("Heat cap:  ", item.modifiers.heat_cap_bonus,
                   col(item.modifiers.heat_cap_bonus));
        wired_attr_suffix("Cooling:   ", item.modifiers.cooling_rate_bonus, "/turn",
                   col(item.modifiers.cooling_rate_bonus));
        wired_attr_suffix("Trace res: ", item.modifiers.trace_resistance_pct, "%",
                   col(item.modifiers.trace_resistance_pct));
        wired_attr_suffix("Shock dur: ", item.modifiers.blackice_shock_duration_pct, "%",
                   col(item.modifiers.blackice_shock_duration_pct));
        if (item.modifiers.blackice_shock_immunity && y < ctx.height()) {
            ctx.label_value(0, y, "Shock imm: ", Color::DarkGray, "yes", Color::Cyan);
            y++;
        }

        // Phase A implant stats
        wired_attr("STR:       ", item.modifiers.strength_bonus,
                   item.modifiers.strength_bonus >= 0 ? Color::Cyan : Color::Red);
        if (item.modifiers.pistol_agility_bonus != 0 && y < ctx.height()) {
            std::string val = (item.modifiers.pistol_agility_bonus > 0 ? "+" : "")
                            + std::to_string(item.modifiers.pistol_agility_bonus) + " (pistol)";
            ctx.label_value(0, y, "AGI:       ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.pistol_hit_bonus_pct != 0 && y < ctx.height()) {
            std::string val = (item.modifiers.pistol_hit_bonus_pct > 0 ? "+" : "")
                            + std::to_string(item.modifiers.pistol_hit_bonus_pct) + "% (pistol)";
            ctx.label_value(0, y, "Hit:       ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.view_radius_dark_bonus != 0 && y < ctx.height()) {
            std::string val = "+" + std::to_string(item.modifiers.view_radius_dark_bonus) + " (dark)";
            ctx.label_value(0, y, "Vision:    ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.quickness_when_idle != 0 && y < ctx.height()) {
            std::string val = "+" + std::to_string(item.modifiers.quickness_when_idle) + " (idle)";
            ctx.label_value(0, y, "Quickness: ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.detect_cloaked && y < ctx.height()) {
            ctx.label_value(0, y, "Detect:    ", Color::DarkGray, "cloaked NPCs", Color::Cyan);
            y++;
        }
        if (item.modifiers.knockback_immune && y < ctx.height()) {
            ctx.label_value(0, y, "Knockback: ", Color::DarkGray, "immune", Color::Cyan);
            y++;
        }
        if (item.modifiers.slip_immune && y < ctx.height()) {
            ctx.label_value(0, y, "Slip:      ", Color::DarkGray, "immune", Color::Cyan);
            y++;
        }

        // Phase B implant stats — melee procs (Vibro-Tip Fingers, Static Palm)
        if (item.modifiers.melee_kinetic_bonus != 0 && y < ctx.height()) {
            std::string val = (item.modifiers.melee_kinetic_bonus > 0 ? "+" : "")
                            + std::to_string(item.modifiers.melee_kinetic_bonus) + " kinetic (melee)";
            ctx.label_value(0, y, "Damage:    ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.melee_bleed_proc_pct != 0 && y < ctx.height()) {
            std::string val = std::to_string(item.modifiers.melee_bleed_proc_pct) + "% bleed";
            ctx.label_value(0, y, "Proc:      ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.melee_emp_proc_pct != 0 && y < ctx.height()) {
            std::string val = std::to_string(item.modifiers.melee_emp_proc_pct) + "% EMP";
            ctx.label_value(0, y, "Proc:      ", Color::DarkGray, val, Color::Cyan);
            y++;
        }
        if (item.modifiers.melee_extra_hit_proc_pct != 0 && y < ctx.height()) {
            std::string val = std::to_string(item.modifiers.melee_extra_hit_proc_pct) + "% extra hit";
            ctx.label_value(0, y, "Proc:      ", Color::DarkGray, val, Color::Cyan);
            y++;
        }

        // Ranged proc (Wrist Rocket)
        if (item.modifiers.ranged_rocket_proc_pct != 0 && y < ctx.height()) {
            std::string val = std::to_string(item.modifiers.ranged_rocket_proc_pct) + "% rocket";
            ctx.label_value(0, y, "Proc:      ", Color::DarkGray, val, Color::Cyan);
            y++;
        }

        // Phase C — stateful / UI / active tags
        if (item.modifiers.show_enemy_threat && y < ctx.height()) {
            ctx.label_value(0, y, "Optics:    ", Color::DarkGray, "enemy HP + status", Color::Cyan);
            y++;
        }
        if (item.modifiers.has_adrenal_pump && y < ctx.height()) {
            ctx.label_value(0, y, "Trigger:   ", Color::DarkGray,
                "<30% HP -> +1 Quickness 5t (1x/combat)", Color::Cyan);
            y++;
        }
        if (item.modifiers.has_emp_buffer && y < ctx.height()) {
            ctx.label_value(0, y, "Trigger:   ", Color::DarkGray,
                "absorb 1st EMP per level", Color::Cyan);
            y++;
        }
        if (item.modifiers.has_burst_pistons && y < ctx.height()) {
            ctx.label_value(0, y, "Ability:   ", Color::DarkGray,
                "[d] Dash 3 (8t cd)", Color::Cyan);
            y++;
        }
    }

    if (item.type == ItemType::Program && item.program && y < ctx.height()) {
        const ProgramDef* def = find_program(item.program->id);
        if (def) {
            ctx.label_value(0, y, "Kind:      ", Color::DarkGray,
                program_kind_name(def->kind), Color::White);
            y++;
            ctx.label_value(0, y, "Tier:      ", Color::DarkGray,
                std::to_string(def->tier), Color::White);
            y++;
            ctx.label_value(0, y, "RAM cost:  ", Color::DarkGray,
                std::to_string(def->ram_cost), Color::Cyan);
            y++;
            if (def->kind == ProgramKind::Qh) {
                ctx.label_value(0, y, "Detection: ", Color::DarkGray,
                    "+" + std::to_string(def->detection_cost), Color::Yellow);
                y++;
                if (!def->target_filter.empty() && y < ctx.height()) {
                    std::string targets;
                    for (size_t i = 0; i < def->target_filter.size(); ++i) {
                        if (i > 0) targets += ", ";
                        targets += tag_set_describe(def->target_filter[i]);
                    }
                    ctx.label_value(0, y, "Targets:   ", Color::DarkGray,
                        targets, Color::Cyan);
                    y++;
                }
            } else {
                ctx.label_value(0, y, "Heat cost: ", Color::DarkGray,
                    std::to_string(def->heat_cost), Color::Red);
                y++;
            }
        }
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
                if (enh.stat_bonus.av) bonus += " AV+" + std::to_string(enh.stat_bonus.av);
                if (enh.stat_bonus.dv) bonus += " DV+" + std::to_string(enh.stat_bonus.dv);
                if (enh.stat_bonus.view_radius) bonus += " VIS+" + std::to_string(enh.stat_bonus.view_radius);
                if (enh.stat_bonus.quickness) bonus += " QCK+" + std::to_string(enh.stat_bonus.quickness);
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
