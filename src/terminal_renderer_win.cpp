#include "astra/terminal_renderer.h"

#ifndef _WIN32
#error "This file should only be compiled on Windows"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <wchar.h>

namespace astra {

// Decode a UTF-8 sequence to a Unicode codepoint and return its terminal width.
static int utf8_cell_width(const char* utf8) {
    if (!utf8 || !utf8[0]) return 1;

    unsigned char c = static_cast<unsigned char>(utf8[0]);
    if (c < 0x80) return 1;

    wchar_t cp = 0;
    if ((c & 0xE0) == 0xC0) {
        cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(utf8[1]) & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(utf8[1]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(utf8[2]) & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(utf8[1]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(utf8[2]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(utf8[3]) & 0x3F);
    }

    // On Windows, use a simple lookup for common wide ranges
    // CJK Unified Ideographs, Fullwidth Forms, etc.
    if ((cp >= 0x1100 && cp <= 0x115F) ||   // Hangul Jamo
        cp == 0x2329 || cp == 0x232A ||      // Angle brackets
        (cp >= 0x2E80 && cp <= 0x303E) ||    // CJK
        (cp >= 0x3040 && cp <= 0x33BF) ||    // Japanese
        (cp >= 0x3400 && cp <= 0x4DBF) ||    // CJK Ext A
        (cp >= 0x4E00 && cp <= 0xA4CF) ||    // CJK Unified
        (cp >= 0xAC00 && cp <= 0xD7A3) ||    // Hangul
        (cp >= 0xF900 && cp <= 0xFAFF) ||    // CJK Compat
        (cp >= 0xFE30 && cp <= 0xFE6F) ||    // CJK Compat Forms
        (cp >= 0xFF01 && cp <= 0xFF60) ||    // Fullwidth Forms
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||    // Fullwidth Signs
        (cp >= 0x20000 && cp <= 0x2FFFD) ||  // CJK Ext B+
        (cp >= 0x30000 && cp <= 0x3FFFD)) {  // CJK Ext G+
        return 2;
    }
    return 1;
}

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

            if (cell.continuation) continue;

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

void TerminalRenderer::draw_char(int x, int y, char ch, Color fg, Color bg) {
    draw_char(x, y, ch, fg);
    (void)bg; // bg not supported on Windows terminal yet
}

void TerminalRenderer::draw_char(int x, int y, char ch, Color fg) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        auto& cell = buffer_[y][x];
        if (cell.wide && x + 1 < width_) {
            auto& next = buffer_[y][x + 1];
            next.continuation = false;
            next.ch[0] = ' ';
            next.ch[1] = '\0';
        }
        cell.ch[0] = ch;
        cell.ch[1] = '\0';
        cell.fg = fg;
        cell.wide = false;
        cell.continuation = false;
    }
}

void TerminalRenderer::draw_glyph(int x, int y, const char* utf8, Color fg) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        auto& cell = buffer_[y][x];
        if (cell.wide && x + 1 < width_) {
            auto& old_next = buffer_[y][x + 1];
            old_next.continuation = false;
            old_next.ch[0] = ' ';
            old_next.ch[1] = '\0';
        }
        int i = 0;
        while (i < 4 && utf8[i]) {
            cell.ch[i] = utf8[i];
            ++i;
        }
        cell.ch[i] = '\0';
        cell.fg = fg;
        cell.continuation = false;

        int w = utf8_cell_width(utf8);
        cell.wide = (w > 1);

        if (cell.wide) {
            for (int dx = 1; dx < w && x + dx < width_; ++dx) {
                auto& next = buffer_[y][x + dx];
                next.ch[0] = '\0';
                next.continuation = true;
                next.wide = false;
            }
        }
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

int TerminalRenderer::wait_input_timeout(int timeout_ms) {
    DWORD result = WaitForSingleObject(impl_->h_in, static_cast<DWORD>(timeout_ms));
    if (result != WAIT_OBJECT_0) return -1;
    return poll_input();
}

} // namespace astra
