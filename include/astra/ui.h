#pragma once

#include "astra/rect.h"
#include "astra/renderer.h"
#include "astra/ui_types.h"
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace astra {

namespace BoxDraw {
    // Single-line box drawing
    constexpr const char* H     = "\xe2\x94\x80";  // ─  horizontal
    constexpr const char* V     = "\xe2\x94\x82";  // │  vertical
    constexpr const char* TL    = "\xe2\x94\x8c";  // ┌  top-left
    constexpr const char* TR    = "\xe2\x94\x90";  // ┐  top-right
    constexpr const char* BL    = "\xe2\x94\x94";  // └  bottom-left
    constexpr const char* BR    = "\xe2\x94\x98";  // ┘  bottom-right
    constexpr const char* LT    = "\xe2\x94\x9c";  // ├  left T
    constexpr const char* RT    = "\xe2\x94\xa4";  // ┤  right T
    constexpr const char* TT    = "\xe2\x94\xac";  // ┬  top T
    constexpr const char* BT    = "\xe2\x94\xb4";  // ┴  bottom T
    constexpr const char* CROSS = "\xe2\x94\xbc";  // ┼  cross

    // Double-line accents (for ornaments)
    constexpr const char* DH    = "\xe2\x95\x90";  // ═  double horizontal
    constexpr const char* DV    = "\xe2\x95\x91";  // ║  double vertical
    constexpr const char* DL    = "\xe2\x95\x9e";  // ╞  single-vert + double-horiz left
    constexpr const char* DR    = "\xe2\x95\xa1";  // ╡  single-vert + double-horiz right

    // Block elements
    constexpr const char* UPPER_HALF = "\xe2\x96\x80"; // ▀ upper half block
    constexpr const char* LOWER_HALF = "\xe2\x96\x84"; // ▄ lower half block
    constexpr const char* LEFT_HALF  = "\xe2\x96\x8c"; // ▌ left half block
    constexpr const char* RIGHT_HALF = "\xe2\x96\x90"; // ▐ right half block
    constexpr const char* FULL       = "\xe2\x96\x88"; // █ full block
}

class UIContext {
public:
    UIContext(Renderer* r, Rect bounds);

    // Core — all coords local to bounds, clipped
    void put(int x, int y, char ch);
    void put(int x, int y, char ch, Color fg);
    void put(int x, int y, char ch, Color fg, Color bg);
    void put(int x, int y, const char* utf8, Color fg);
    void put(int x, int y, const char* utf8, Color fg, Color bg);
    void text(int x, int y, std::string_view s, Color fg = Color::Default);
    void text(int x, int y, std::string_view s, Color fg, Color bg);
    // Render text with inline COLOR_BEGIN/COLOR_END markers
    void text_rich(int x, int y, std::string_view s, Color default_fg = Color::Default);

    // Lines
    void hline(int y, char ch = '-');
    void hline(int y, const char* utf8, Color fg = Color::DarkGray);
    void vline(int x, char ch = '|');
    void vline(int x, const char* utf8, Color fg = Color::DarkGray);
    void border(char h = '-', char v = '|', char corner = '+');
    void box(Color fg = Color::DarkGray);
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

    // Semantic UI components — delegates to renderer
    UIContext panel(const PanelDesc& desc);
    void progress_bar(const ProgressBarDesc& desc);
    void text(const TextDesc& desc);
    void styled_text(const StyledTextDesc& desc);
    void list(const ListDesc& desc);
    void tab_bar(const TabBarDesc& desc);
    void widget_bar(const WidgetBarDesc& desc);
    void separator(const SeparatorDesc& desc);
    void label_value(const LabelValueDesc& desc);
    void galaxy_map(const GalaxyMapDesc& desc);

    // Layout — split this context into sub-regions
    std::vector<UIContext> rows(const std::vector<Size>& sizes) const;
    std::vector<UIContext> columns(const std::vector<Size>& sizes) const;

    // Sub-region
    UIContext sub(Rect local_rect) const;

    const Rect& bounds() const;
    int width() const;
    int height() const;

private:
    Renderer* renderer_;
    Rect bounds_;
};

struct TextList {
    static void draw(UIContext& ctx, const std::deque<std::string>& lines,
                     int scroll_offset = -1, Color fg = Color::Default);
};

// Draw item inspection content into a UIContext (no window frame — caller provides that).
struct Item; // forward declare
void draw_item_info(UIContext& ctx, const Item& item);

// Draw an item name: name in rarity color (or white if selected), stack count in white.
// Returns the x position after the last character drawn.
int draw_item_name(UIContext& ctx, int x, int y, const Item& item, bool selected = false);

// Lightweight menu state — selection cursor, options, open/close.
// No rendering; each callsite renders with panel() + list().
struct MenuOption {
    char key = 0;
    std::string label;
};

enum class MenuResult {
    None,
    Closed,     // ESC pressed
    Selected,   // option chosen
};

struct MenuState {
    std::string title;
    std::string body;
    std::string footer;
    std::vector<MenuOption> options;
    int selection = 0;
    bool open = false;

    void add_option(char key, std::string_view label);
    char selected_key() const;
    MenuResult handle_input(int key);
    void reset();  // close + clear all fields
};

} // namespace astra
