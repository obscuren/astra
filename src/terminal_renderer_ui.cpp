// src/terminal_renderer_ui.cpp
// Semantic UI method implementations for TerminalRenderer

#include "astra/terminal_renderer.h"
#include "astra/ui_types.h"
#include "astra/rect.h"
#include "terminal_ui_theme.h"
#include "terminal_theme.h"        // resolve() for world entities, item_visual(), npc_glyph()
#include "astra/render_descriptor.h"

#include <algorithm>

namespace astra {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Resolve EntityRef to terminal visual (glyph + color) using the world entity theme
static ResolvedVisual resolve_entity_visual(const EntityRef& ref) {
    switch (ref.kind) {
        case EntityRef::Kind::Npc: {
            RenderDescriptor desc;
            desc.category = RenderCategory::Npc;
            desc.type_id = ref.id;
            desc.seed = ref.seed;
            return resolve(desc);
        }
        case EntityRef::Kind::Item:
            return item_visual(ref.id);
        case EntityRef::Kind::Fixture: {
            RenderDescriptor desc;
            desc.category = RenderCategory::Fixture;
            desc.type_id = ref.id;
            desc.seed = ref.seed;
            return resolve(desc);
        }
        default:
            return {'?', nullptr, Color::Magenta, Color::Default};
    }
}

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
// Render a UTF-8 string, honouring inline color markers:
//   \x02 <color_byte> ... \x03  — override fg for enclosed span.
// Strings without markers render identically to the default-fg path.
static void render_utf8_string(Renderer* r, int x, int y,
                               const std::string& s, Color fg) {
    int col = 0;
    int i = 0;
    int len = static_cast<int>(s.size());
    Color cur = fg;
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < len) {
            cur = static_cast<Color>(static_cast<unsigned char>(s[i + 1]));
            i += 2;
            continue;
        }
        if (c == static_cast<unsigned char>(COLOR_END)) {
            cur = fg;
            ++i;
            continue;
        }
        if (c < 0x80) {
            r->draw_char(x + col, y, s[i], cur);
            ++i;
        } else {
            int seq_len = 1;
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;

            char buf[5] = {};
            for (int j = 0; j < seq_len && i + j < len; ++j)
                buf[j] = s[i + j];
            r->draw_glyph(x + col, y, buf, cur);
            i += seq_len;
        }
        ++col;
    }
}

// Return visual (column) length of a UTF-8 string, skipping color markers.
static int utf8_display_len(const std::string& s) {
    int col = 0;
    int i = 0;
    int len = static_cast<int>(s.size());
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < len) {
            i += 2;
            continue;
        }
        if (c == static_cast<unsigned char>(COLOR_END)) {
            ++i;
            continue;
        }
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
        // Footer is embedded in the separator line: ──┤footer text├──
        UIStyle sep_style = resolve_ui_tag(UITag::Separator);
        int fy = bounds.y + h - 2;  // one row above bottom border
        int inner_w = w - 2;        // inside left/right borders
        int footer_len = utf8_display_len(desc.footer);
        int text_w = footer_len + 2; // +2 for ┤ and ├
        constexpr int pad = 3;       // minimum ─── padding on each side

        // Compute footer text start position based on alignment
        int text_start; // offset from left border (inner coords)
        if (desc.footer_align == TextAlign::Left) {
            text_start = pad;
        } else if (desc.footer_align == TextAlign::Right) {
            text_start = inner_w - pad - text_w;
        } else { // Center
            text_start = (inner_w - text_w) / 2;
        }
        if (text_start < pad) text_start = pad;
        if (text_start + text_w > inner_w - pad) text_start = inner_w - pad - text_w;
        if (text_start < 1) text_start = 1;

        // Draw the full line with T-junctions framing the text
        int bx = bounds.x + 1; // first inner cell
        for (int x = 0; x < inner_w; ++x) {
            int ax = bx + x;
            if (x == text_start) {
                draw_glyph(ax, fy, "\xe2\x94\xa4", sep_style.fg); // ┤
            } else if (x == text_start + text_w - 1) {
                draw_glyph(ax, fy, "\xe2\x94\x9c", sep_style.fg); // ├
            } else if (x > text_start && x < text_start + text_w - 1) {
                // Footer text area — will be overwritten below
                draw_char(ax, fy, ' ');
            } else {
                draw_glyph(ax, fy, HLINE, sep_style.fg);
            }
        }

        // Render footer text with [key] highlighting inside the ┤...├ area
        UIStyle bracket_style = resolve_ui_tag(UITag::TextBright);
        UIStyle key_style = resolve_ui_tag(UITag::KeyLabel);
        UIStyle footer_style = resolve_ui_tag(UITag::Footer);
        int col = bx + text_start + 1; // after ┤
        const std::string& footer = desc.footer;
        size_t pos = 0;
        while (pos < footer.size()) {
            size_t bracket = footer.find('[', pos);
            if (bracket == std::string::npos) {
                std::string rest = footer.substr(pos);
                render_utf8_string(this, col, fy, rest, footer_style.fg);
                col += utf8_display_len(rest);
                break;
            }
            if (bracket > pos) {
                std::string plain = footer.substr(pos, bracket - pos);
                render_utf8_string(this, col, fy, plain, footer_style.fg);
                col += utf8_display_len(plain);
            }
            size_t close = footer.find(']', bracket + 1);
            if (close == std::string::npos) {
                std::string rest = footer.substr(bracket);
                render_utf8_string(this, col, fy, rest, footer_style.fg);
                col += utf8_display_len(rest);
                break;
            }
            draw_char(col++, fy, '[', bracket_style.fg);
            std::string key_text = footer.substr(bracket + 1, close - bracket - 1);
            render_utf8_string(this, col, fy, key_text, key_style.fg);
            col += utf8_display_len(key_text);
            draw_char(col++, fy, ']', bracket_style.fg);
            pos = close + 1;
        }
    }

    // Compute content rect
    int top = has_title ? 3 : 1;
    int bottom = has_footer ? 2 : 1; // footer is 1 row (embedded in separator) + bottom border
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

    // For right/center alignment, compute total display width first
    if (desc.align == TextAlign::Right || desc.align == TextAlign::Center) {
        int total_w = 0;
        for (const auto& seg : desc.segments) {
            if (seg.entity.has_value())
                total_w += 1;
            else
                total_w += utf8_display_len(seg.text);
        }
        if (desc.align == TextAlign::Right)
            col = x - total_w;
        else
            col = x - total_w / 2;
    }

    for (const auto& seg : desc.segments) {
        if (seg.entity.has_value()) {
            // Resolve glyph+color from entity identity
            auto vis = resolve_entity_visual(seg.entity);
            if (vis.utf8)
                draw_glyph(col, y, vis.utf8, vis.fg);
            else
                draw_char(col, y, vis.glyph, vis.fg);
            col += 1; // entity glyph is always 1 display cell
        } else {
            UIStyle style = resolve_ui_tag(seg.tag);
            render_utf8_string(this, col, y, seg.text, style.fg);
            col += utf8_display_len(seg.text);
        }
    }
}

// ---------------------------------------------------------------------------
// draw_list
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_list(const Rect& bounds, const ListDesc& desc) {
    int visible_h = bounds.h;
    if (visible_h <= 0) return;

    bool conversation = (desc.tag == UITag::ConversationOption);
    int row_stride = conversation ? 2 : 1; // extra blank line between conversation options

    int total = static_cast<int>(desc.items.size());
    int visible_items = conversation ? (visible_h + 1) / 2 : visible_h; // account for spacing
    int start = std::min(desc.scroll_offset, std::max(0, total - visible_items));
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < total && row < visible_h; ++i, row += row_stride) {
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
        // Render label with [key] highlighting
        UIStyle bracket_style = resolve_ui_tag(UITag::TextBright);
        UIStyle key_style = resolve_ui_tag(UITag::KeyLabel);
        int lx = col + 2;
        const std::string& label = item.label;
        size_t pos = 0;
        while (pos < label.size()) {
            size_t bracket = label.find('[', pos);
            if (bracket == std::string::npos) {
                std::string rest = label.substr(pos);
                render_utf8_string(this, lx, bounds.y + row, rest, style.fg);
                lx += utf8_display_len(rest);
                break;
            }
            if (bracket > pos) {
                std::string plain = label.substr(pos, bracket - pos);
                render_utf8_string(this, lx, bounds.y + row, plain, style.fg);
                lx += utf8_display_len(plain);
            }
            size_t close = label.find(']', bracket + 1);
            if (close == std::string::npos) {
                std::string rest = label.substr(bracket);
                render_utf8_string(this, lx, bounds.y + row, rest, style.fg);
                lx += utf8_display_len(rest);
                break;
            }
            draw_char(lx++, bounds.y + row, '[', bracket_style.fg);
            std::string key_text = label.substr(bracket + 1, close - bracket - 1);
            render_utf8_string(this, lx, bounds.y + row, key_text, key_style.fg);
            lx += utf8_display_len(key_text);
            draw_char(lx++, bounds.y + row, ']', bracket_style.fg);
            pos = close + 1;
        }
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
// draw_widget_bar
// ---------------------------------------------------------------------------

void TerminalRenderer::draw_widget_bar(const Rect& bounds, const WidgetBarDesc& desc) {
    UIStyle active_style = resolve_ui_tag(UITag::TabActive);
    UIStyle inactive_style = resolve_ui_tag(UITag::TabInactive);
    UIStyle bracket_style = resolve_ui_tag(UITag::TextBright);
    UIStyle key_style = resolve_ui_tag(UITag::NavKey);

    int col = bounds.x;
    for (size_t i = 0; i < desc.entries.size(); ++i) {
        const auto& e = desc.entries[i];
        UIStyle style = e.active ? active_style : inactive_style;
        Color name_fg = e.focused ? active_style.fg : style.fg;

        if (e.active) {
            draw_char(col++, bounds.y, '[', bracket_style.fg);
            for (char c : e.hotkey)
                draw_char(col++, bounds.y, c, key_style.fg);
            draw_char(col++, bounds.y, ']', bracket_style.fg);
            render_utf8_string(this, col, bounds.y, e.name, name_fg);
            col += utf8_display_len(e.name);
        } else {
            draw_char(col++, bounds.y, ' ', inactive_style.fg);
            for (char c : e.hotkey)
                draw_char(col++, bounds.y, c, key_style.fg);
            draw_char(col++, bounds.y, ' ', inactive_style.fg);
            render_utf8_string(this, col, bounds.y, e.name, inactive_style.fg);
            col += utf8_display_len(e.name);
        }

        // Focused indicator: underline the name area
        if (e.focused && e.active) {
            // Draw a small marker after the name
            draw_char(col++, bounds.y, '*', active_style.fg);
        }

        if (i < desc.entries.size() - 1) {
            draw_char(col++, bounds.y, ' ');
        }
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
