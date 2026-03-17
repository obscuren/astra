#include "astra/terminal_renderer.h"

#ifndef _WIN32
#error "This file should only be compiled on Windows"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace astra {

// Append a 256-color foreground escape sequence, or reset for Default.
static void append_color(std::string& buf, Color c) {
    if (c == Color::Default) {
        buf += "\033[0m";
    } else {
        char seq[16];
        int len = std::snprintf(seq, sizeof(seq), "\033[38;5;%dm",
                                static_cast<uint8_t>(c));
        buf.append(seq, len);
    }
}

// --- Impl (hides Win32 handles from header) ---

struct TerminalRenderer::Impl {
    HANDLE h_in = INVALID_HANDLE_VALUE;
    HANDLE h_out = INVALID_HANDLE_VALUE;
    DWORD orig_in_mode = 0;
    DWORD orig_out_mode = 0;
    bool raw_mode = false;
};

// --- TerminalRenderer ---

TerminalRenderer::TerminalRenderer() : impl_(std::make_unique<Impl>()) {}
TerminalRenderer::~TerminalRenderer() {
    if (impl_->raw_mode) shutdown();
}

void TerminalRenderer::init() {
    impl_->h_in = GetStdHandle(STD_INPUT_HANDLE);
    impl_->h_out = GetStdHandle(STD_OUTPUT_HANDLE);

    // Save original console modes
    GetConsoleMode(impl_->h_in, &impl_->orig_in_mode);
    GetConsoleMode(impl_->h_out, &impl_->orig_out_mode);

    // Enable virtual terminal processing for ANSI escape sequences
    DWORD out_mode = impl_->orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                                           | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(impl_->h_out, out_mode);

    // Raw input: no echo, no line input, no processed input
    DWORD in_mode = ENABLE_WINDOW_INPUT;
    SetConsoleMode(impl_->h_in, in_mode);

    impl_->raw_mode = true;

    // Query console size
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(impl_->h_out, &csbi)) {
        width_ = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height_ = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

    rebuild_buffer();

    // Switch to alternate screen, hide cursor
    std::printf("\033[?1049h");
    std::printf("\033[?25l");
    std::fflush(stdout);
}

void TerminalRenderer::shutdown() {
    if (!impl_->raw_mode) return;

    // Reset colors, show cursor, restore main screen
    std::printf("\033[0m\033[?25h\033[?1049l");
    std::fflush(stdout);

    // Restore original console modes
    SetConsoleMode(impl_->h_in, impl_->orig_in_mode);
    SetConsoleMode(impl_->h_out, impl_->orig_out_mode);

    impl_->raw_mode = false;
}

void TerminalRenderer::rebuild_buffer() {
    buffer_.assign(height_, std::vector<Cell>(width_));
}

void TerminalRenderer::clear() {
    for (auto& row : buffer_) {
        for (auto& cell : row) {
            cell = Cell{};
        }
    }
}

void TerminalRenderer::present() {
    out_buf_.clear();
    out_buf_.reserve(width_ * height_ * 2 + height_ + 64);

    out_buf_ += "\033[H"; // cursor home

    Color prev_color = Color::Default;

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto& cell = buffer_[y][x];

            if (cell.fg != prev_color) {
                append_color(out_buf_, cell.fg);
                prev_color = cell.fg;
            }

            out_buf_ += cell.ch;
        }
        if (y < height_ - 1) {
            out_buf_ += '\n';
        }
    }

    if (prev_color != Color::Default) {
        out_buf_ += "\033[0m";
    }

    // Use WriteConsoleA for reliable output
    DWORD written;
    WriteConsoleA(impl_->h_out, out_buf_.data(),
                  static_cast<DWORD>(out_buf_.size()), &written, nullptr);
}

void TerminalRenderer::draw_char(int x, int y, char ch) {
    draw_char(x, y, ch, Color::Default);
}

void TerminalRenderer::draw_char(int x, int y, char ch, Color fg) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        buffer_[y][x] = {ch, fg};
    }
}

void TerminalRenderer::draw_string(int x, int y, const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        draw_char(x + static_cast<int>(i), y, text[i]);
    }
}

int TerminalRenderer::get_width() const { return width_; }
int TerminalRenderer::get_height() const { return height_; }

void TerminalRenderer::check_resize() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(impl_->h_out, &csbi)) {
        int new_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int new_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (new_w != width_ || new_h != height_) {
            width_ = new_w;
            height_ = new_h;
            rebuild_buffer();
        }
    }
}

int TerminalRenderer::poll_input() {
    check_resize();

    DWORD events_available = 0;
    GetNumberOfConsoleInputEvents(impl_->h_in, &events_available);
    if (events_available == 0) return -1;

    INPUT_RECORD record;
    DWORD events_read = 0;
    if (!ReadConsoleInputA(impl_->h_in, &record, 1, &events_read) || events_read == 0) {
        return -1;
    }

    if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
        return -1;
    }

    const auto& key = record.Event.KeyEvent;

    // Arrow keys via virtual key codes
    switch (key.wVirtualKeyCode) {
        case VK_UP:    return KEY_UP;
        case VK_DOWN:  return KEY_DOWN;
        case VK_LEFT:  return KEY_LEFT;
        case VK_RIGHT: return KEY_RIGHT;
        case VK_ESCAPE: return '\033';
        case VK_RETURN: return '\n';
        case VK_TAB:    return '\t';
    }

    // Regular character
    char ch = key.uChar.AsciiChar;
    if (ch != 0) {
        // Handle Ctrl+key combinations
        if (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
            return ch; // Already comes through as 1-26 for Ctrl+A through Ctrl+Z
        }
        return static_cast<int>(ch);
    }

    return -1;
}

int TerminalRenderer::wait_input() {
    // Block until we get a meaningful key
    for (;;) {
        WaitForSingleObject(impl_->h_in, INFINITE);
        int key = poll_input();
        if (key != -1) return key;
    }
}

} // namespace astra
