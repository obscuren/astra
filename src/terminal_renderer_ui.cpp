// src/terminal_renderer_ui.cpp
// Semantic UI method implementations for TerminalRenderer

#include "astra/terminal_renderer.h"
#include "astra/ui_types.h"
#include "astra/rect.h"
#include "terminal_ui_theme.h"

#include <algorithm>

namespace astra {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Half-block border glyphs (matching Panel::draw() style)
static constexpr const char* BORDER_LEFT   = "\xe2\x96\x90"; // ▐
static constexpr const char* BORDER_RIGHT  = "\xe2\x96\x8c"; // ▌
static constexpr const char* BORDER_TOP    = "\xe2\x96\x80"; // ▀
static constexpr const char* BORDER_BOTTOM = "\xe2\x96\x84"; // ▄

// Progress bar glyphs
static constexpr const char* BAR_FILL  = "\xe2\x96\xb0"; // ▰
static constexpr const char* BAR_EMPTY = "\xe2\x96\xb1"; // ▱

// Separator glyph
static constexpr const char* HLINE = "\xe2\x94\x80"; // ─
static constexpr const char* VLINE = "\xe2\x94\x82"; // │

// Render a UTF-8 string using draw_glyph for multi-byte and draw_char for ASCII.
static void render_utf8_string(Renderer* r, int x, int y,
                               const std::string& s, Color fg) {
    int col = 0;
    int i = 0;
    int len = static_cast<int>(s.size());
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            r->draw_char(x + col, y, s[i], fg);
            ++i;
        } else {
            int seq_len = 1;
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;

            char buf[5] = {};
            for (int j = 0; j < seq_len && i + j < len; ++j)
                buf[j] = s[i + j];
            r->draw_glyph(x + col, y, buf, fg);
            i += seq_len;
        }
        ++col;
    }
}

// Return visual (column) length of a UTF-8 string.
static int utf8_display_len(const std::string& s) {
    int col = 0;
    int i = 0;
    int len = static_cast<int>(s.size());
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ++i;
        } else {
            int seq_len = 1;
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;
            i += seq_len;
        }
        ++col;
    }
    return col;
}

// ---------------------------------------------------------------------------
// draw_panel
// ---------------------------------------------------------------------------

Rect TerminalRenderer::draw_panel(const Rect& bounds, const PanelDesc& desc) {
    int w = bounds.w;
    int h = bounds.h;
    if (w <= 0 || h <= 0) return bounds;

    UIStyle border_style = resolve_ui_tag(desc.tag);
    Color border = border_style.fg;

    // Clear interior
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            draw_char(bounds.x + x, bounds.y + y, ' ');

    // Top border: ▐▀▀▀▌
    draw_glyph(bounds.x, bounds.y, BORDER_LEFT, border);
    for (int x = 1; x < w - 1; ++x)
        draw_glyph(bounds.x + x, bounds.y, BORDER_TOP, border);
    draw_glyph(bounds.x + w - 1, bounds.y, BORDER_RIGHT, border);

    // Side borders: ▐ ... ▌
    for (int y = 1; y < h - 1; ++y) {
        draw_glyph(bounds.x, bounds.y + y, BORDER_LEFT, border);
        draw_glyph(bounds.x + w - 1, bounds.y + y, BORDER_RIGHT, border);
    }

    // Bottom border: ▐▄▄▄▌
    draw_glyph(bounds.x, bounds.y + h - 1, BORDER_LEFT, border);
    for (int x = 1; x < w - 1; ++x)
        draw_glyph(bounds.x + x, bounds.y + h - 1, BORDER_BOTTOM, border);
    draw_glyph(bounds.x + w - 1, bounds.y + h - 1, BORDER_RIGHT, border);

    // Title row (row 1) + separator (row 2)
    bool has_title = !desc.title.empty();
    if (has_title) {
        UIStyle title_style = resolve_ui_tag(UITag::Title);
        int title_len = utf8_display_len(desc.title);
        int tx = bounds.x + (w - title_len) / 2;
        render_utf8_string(this, tx, bounds.y + 1, desc.title, title_style.fg);

        UIStyle sep_style = resolve_ui_tag(UITag::Separator);
        for (int x = 1; x < w - 1; ++x)
            draw_glyph(bounds.x + x, bounds.y + 2, HLINE, sep_style.fg);
    }

    // Footer separator + text
    bool has_footer = !desc.footer.empty();
    if (has_footer) {
        UIStyle sep_style = resolve_ui_tag(UITag::Separator);
        int sep_y = bounds.y + h - 3;
        for (int x = 1; x < w - 1; ++x)
            draw_glyph(bounds.x + x, sep_y, HLINE, sep_style.fg);

        int footer_len = utf8_display_len(desc.footer);
        int fx = bounds.x + (w - footer_len) / 2;
        if (fx < bounds.x + 1) fx = bounds.x + 1;
        int fy = bounds.y + h - 2;

        // Render footer with [key] highlighting via UITags
        UIStyle bracket_style = resolve_ui_tag(UITag::TextBright);
        UIStyle key_style = resolve_ui_tag(UITag::KeyLabel);
        UIStyle footer_style = resolve_ui_tag(UITag::Footer);
        const std::string& footer = desc.footer;
        int col = fx;
        for (size_t i = 0; i < footer.size(); ++i) {
            if (footer[i] == '[') {
                draw_char(col++, fy, '[', bracket_style.fg);
                size_t end = footer.find(']', i + 1);
                if (end != std::string::npos) {
                    for (size_t j = i + 1; j < end; ++j)
                        draw_char(col++, fy, footer[j], key_style.fg);
                    draw_char(col++, fy, ']', bracket_style.fg);
                    i = end;
                }
            } else {
                draw_char(col++, fy, footer[i], footer_style.fg);
            }
        }
    }

    // Compute content rect
    int top = has_title ? 3 : 1;
    int bottom = has_footer ? 3 : 1;
    return Rect{
        bounds.x + 1,
        bounds.y + top,
        w - 2,
        h - top - bottom
    };
}

// ---------------------------------------------------------------------------
// draw_progress_bar
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_progress_bar(int x, int y, const ProgressBarDesc& desc) {
    UIStyle fill_style = resolve_ui_tag(desc.tag, desc.value, desc.max);
    UIStyle empty_style = resolve_ui_tag(UITag::TextDim);

    int bar_w = desc.width;
    int filled = (desc.max > 0) ? (desc.value * bar_w / desc.max) : 0;
    if (filled < 0) filled = 0;
    if (filled > bar_w) filled = bar_w;

    draw_char(x, y, '[');
    for (int i = 0; i < bar_w; ++i) {
        if (i < filled) {
            draw_glyph(x + 1 + i, y, BAR_FILL, fill_style.fg);
        } else {
            draw_glyph(x + 1 + i, y, BAR_EMPTY, empty_style.fg);
        }
    }
    draw_char(x + 1 + bar_w, y, ']');
}

// ---------------------------------------------------------------------------
// draw_ui_text
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_ui_text(int x, int y, const TextDesc& desc) {
    UIStyle style = resolve_ui_tag(desc.tag);
    render_utf8_string(this, x, y, desc.content, style.fg);
}

// ---------------------------------------------------------------------------
// draw_styled_text
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_styled_text(int x, int y, const StyledTextDesc& desc) {
    int col = x;
    for (const auto& seg : desc.segments) {
        UIStyle style = resolve_ui_tag(seg.tag);
        render_utf8_string(this, col, y, seg.text, style.fg);
        col += utf8_display_len(seg.text);
    }
}

// ---------------------------------------------------------------------------
// draw_list
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_list(const Rect& bounds, const ListDesc& desc) {
    int visible_h = bounds.h;
    if (visible_h <= 0) return;

    int total = static_cast<int>(desc.items.size());
    int start = std::min(desc.scroll_offset, std::max(0, total - visible_h));
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < total && row < visible_h; ++i, ++row) {
        const auto& item = desc.items[i];
        UIStyle style = resolve_ui_tag(item.selected ? desc.selected_tag : item.tag);

        int col = bounds.x;
        if (item.selected) {
            draw_char(col, bounds.y + row, '>', style.fg);
            draw_char(col + 1, bounds.y + row, ' ', style.fg);
        } else {
            draw_char(col, bounds.y + row, ' ');
            draw_char(col + 1, bounds.y + row, ' ');
        }
        render_utf8_string(this, col + 2, bounds.y + row, item.label, style.fg);
    }
}

// ---------------------------------------------------------------------------
// draw_tab_bar
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) {
    UIStyle nav_style = resolve_ui_tag(UITag::NavKey);
    UIStyle bracket_style = resolve_ui_tag(UITag::TextBright);

    // Measure total width for alignment
    int total_w = 0;
    if (desc.show_nav) total_w += 4; // "[Q] "
    for (int i = 0; i < static_cast<int>(desc.tabs.size()); ++i) {
        bool active = (i == desc.active);
        total_w += static_cast<int>(desc.tabs[i].size()) + 2; // brackets/spaces + text
        if (i < static_cast<int>(desc.tabs.size()) - 1) total_w += 1; // space between
    }
    if (desc.show_nav) total_w += 4; // " [E]"

    // Compute start x based on alignment
    int col = bounds.x;
    if (desc.align == TextAlign::Center) {
        col = bounds.x + (bounds.w - total_w) / 2;
        if (col < bounds.x) col = bounds.x;
    } else if (desc.align == TextAlign::Right) {
        col = bounds.x + bounds.w - total_w;
        if (col < bounds.x) col = bounds.x;
    }

    // Left nav key
    if (desc.show_nav) {
        draw_char(col++, bounds.y, '[', bracket_style.fg);
        for (char c : desc.nav_left_label)
            draw_char(col++, bounds.y, c, nav_style.fg);
        draw_char(col++, bounds.y, ']', bracket_style.fg);
        draw_char(col++, bounds.y, ' ');
    }

    // Tabs
    for (int i = 0; i < static_cast<int>(desc.tabs.size()); ++i) {
        const auto& tab = desc.tabs[i];
        bool active = (i == desc.active);
        UIStyle style = resolve_ui_tag(active ? desc.active_tag : desc.inactive_tag);

        if (active) {
            draw_char(col++, bounds.y, '[', bracket_style.fg);
            render_utf8_string(this, col, bounds.y, tab, style.fg);
            col += utf8_display_len(tab);
            draw_char(col++, bounds.y, ']', bracket_style.fg);
        } else {
            draw_char(col++, bounds.y, ' ', style.fg);
            render_utf8_string(this, col, bounds.y, tab, style.fg);
            col += utf8_display_len(tab);
            draw_char(col++, bounds.y, ' ', style.fg);
        }
        if (i < static_cast<int>(desc.tabs.size()) - 1) {
            draw_char(col++, bounds.y, ' ');
        }
    }

    // Right nav key
    if (desc.show_nav) {
        draw_char(col++, bounds.y, ' ');
        draw_char(col++, bounds.y, '[', bracket_style.fg);
        for (char c : desc.nav_right_label)
            draw_char(col++, bounds.y, c, nav_style.fg);
        draw_char(col++, bounds.y, ']', bracket_style.fg);
    }
}

// ---------------------------------------------------------------------------
// draw_separator
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_separator(const Rect& bounds, const SeparatorDesc& desc) {
    UIStyle style = resolve_ui_tag(desc.tag);
    if (desc.vertical) {
        for (int y = bounds.y; y < bounds.y + bounds.h; ++y)
            draw_glyph(bounds.x, y, VLINE, style.fg);
    } else {
        for (int x = bounds.x; x < bounds.x + bounds.w; ++x)
            draw_glyph(x, bounds.y, HLINE, style.fg);
    }
}

// ---------------------------------------------------------------------------
// draw_label_value
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_label_value(int x, int y, const LabelValueDesc& desc) {
    UIStyle label_style = resolve_ui_tag(desc.label_tag);
    UIStyle value_style = resolve_ui_tag(desc.value_tag);

    render_utf8_string(this, x, y, desc.label, label_style.fg);
    int vx = x + utf8_display_len(desc.label);
    render_utf8_string(this, vx, y, desc.value, value_style.fg);
}

} // namespace astra
