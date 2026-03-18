#pragma once

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
    BrightMagenta = 13,
};

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
    virtual void draw_string(int x, int y, const std::string& text) = 0;

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
