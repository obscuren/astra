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
};

enum class Color : uint8_t {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
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
};

} // namespace astra
