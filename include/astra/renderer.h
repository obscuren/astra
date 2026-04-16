#pragma once

#include "astra/rect.h"
#include <cstdint>
#include <string>

namespace astra {

// Virtual key codes for non-ASCII keys
enum Key {
    KEY_UP    = 256,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SHIFT_TAB,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_DELETE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
};

// 256-color palette indices stored directly in the enum.
// Named values map to xterm-256 indices (0–15 = standard/bright colors).
// Cast any 0–255 value for extended palette: static_cast<Color>(208).
enum class Color : uint8_t {
    Default       = 255,  // sentinel: reset / no color
    Black         = 0,
    Red           = 1,
    Green         = 2,
    Yellow        = 3,
    Blue          = 4,
    Magenta       = 5,
    Cyan          = 6,
    White         = 7,
    DarkGray      = 8,
    BrightYellow  = 11,
    BrightMagenta = 13,
    BrightWhite   = 15,
};

// UTF-8 full-block glyph (█).
static constexpr const char* BLOCK_GLYPH = "\xe2\x96\x88";

// Inline color markers for styled log messages.
// COLOR_BEGIN is followed by one byte (Color value). COLOR_END resets to default.
static constexpr char COLOR_BEGIN = '\x02';
static constexpr char COLOR_END   = '\x03';

// Wrap a string in inline color markers for styled log messages.
inline std::string colored(const std::string& text, Color c) {
    std::string s;
    s += COLOR_BEGIN;
    s += static_cast<char>(static_cast<uint8_t>(c));
    s += text;
    s += COLOR_END;
    return s;
}

// Forward declarations for semantic rendering
struct RenderDescriptor;
enum class AnimationType : uint8_t;

// Forward declarations for semantic UI
struct PanelDesc;
struct ProgressBarDesc;
struct TextDesc;
struct StyledTextDesc;
struct ListDesc;
struct TabBarDesc;
struct WidgetBarDesc;
struct SeparatorDesc;
struct LabelValueDesc;
struct GalaxyMapDesc;

// Abstract rendering interface.
// Terminal now, SDL later — game logic never touches this directly.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual void clear() = 0;
    virtual void present() = 0;

    virtual void draw_char(int x, int y, char ch) = 0;
    virtual void draw_char(int x, int y, char ch, Color fg) = 0;
    virtual void draw_char(int x, int y, char ch, Color fg, Color bg) {
        draw_char(x, y, ch, fg); // default: ignore bg
        (void)bg;
    }
    virtual void draw_glyph(int x, int y, const char* utf8, Color fg) = 0;
    virtual void draw_glyph(int x, int y, const char* utf8, Color fg, Color bg) {
        draw_glyph(x, y, utf8, fg); // default: ignore bg
    }
    virtual void draw_string(int x, int y, const std::string& text) = 0;

    // Read back the glyph and color at a screen position.
    // glyph_out is filled with the UTF-8 string (up to 4 bytes + null).
    // Returns false if position is out of bounds.
    virtual bool read_cell(int x, int y, char* glyph_out, Color& fg_out) const {
        (void)x; (void)y; (void)glyph_out; (void)fg_out;
        return false;
    }

    // Semantic rendering — render a game-world entity from its descriptor.
    // Each renderer implementation resolves the descriptor to backend-specific visuals.
    virtual void draw_entity(int x, int y, const RenderDescriptor& desc) = 0;

    // Semantic animation — render an animation frame at a position.
    virtual void draw_animation(int x, int y, AnimationType type, int frame_index) = 0;

    // Semantic UI rendering
    virtual Rect draw_panel(const Rect& bounds, const PanelDesc& desc) = 0;
    virtual void draw_progress_bar(int x, int y, const ProgressBarDesc& desc) = 0;
    virtual void draw_ui_text(int x, int y, const TextDesc& desc) = 0;
    virtual void draw_styled_text(int x, int y, const StyledTextDesc& desc) = 0;
    virtual void draw_list(const Rect& bounds, const ListDesc& desc) = 0;
    virtual void draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) = 0;
    virtual void draw_widget_bar(const Rect& bounds, const WidgetBarDesc& desc) = 0;
    virtual void draw_separator(const Rect& bounds, const SeparatorDesc& desc) = 0;
    virtual void draw_label_value(int x, int y, const LabelValueDesc& desc) = 0;

    // Semantic galaxy map — star field, orbital diagrams, labels.
    // Renders into the given bounds rect; no panel chrome or info sidebar.
    virtual void draw_galaxy_map(const Rect& bounds, const GalaxyMapDesc& desc) = 0;

    // Returns true if the user requested quit (e.g. Ctrl+C), and clears the flag.
    virtual bool consume_quit_request() { return false; }

    virtual int get_width() const = 0;
    virtual int get_height() const = 0;

    // Returns -1 on no input, otherwise a key code.
    virtual int poll_input() = 0;

    // Blocks until a key is pressed. Returns the key code.
    virtual int wait_input() = 0;

    // Blocks up to timeout_ms milliseconds. Returns -1 on timeout.
    virtual int wait_input_timeout(int timeout_ms) = 0;
};

} // namespace astra
