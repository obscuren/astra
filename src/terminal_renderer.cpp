#include "astra/terminal_renderer.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace astra {

// --- Signal-safe terminal restore ---

static struct termios s_orig_termios;
static volatile sig_atomic_t s_raw_mode = 0;
static volatile sig_atomic_t s_resized = 0;

static void restore_terminal() {
    if (s_raw_mode) {
        const char seq[] = "\033[0m\033[?25h\033[?1049l";
        write(STDOUT_FILENO, seq, sizeof(seq) - 1);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_orig_termios);
        s_raw_mode = 0;
    }
}

static void signal_handler(int sig) {
    restore_terminal();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void sigwinch_handler(int) {
    s_resized = 1;
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

// --- Impl (hides termios from header) ---

struct TerminalRenderer::Impl {
    struct termios orig;
};

// --- TerminalRenderer ---

TerminalRenderer::TerminalRenderer() : impl_(std::make_unique<Impl>()) {}
TerminalRenderer::~TerminalRenderer() {
    if (s_raw_mode) shutdown();
}

void TerminalRenderer::init() {
    tcgetattr(STDIN_FILENO, &impl_->orig);
    s_orig_termios = impl_->orig;

    struct termios raw = impl_->orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    s_raw_mode = 1;
    std::atexit(restore_terminal);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGWINCH, sigwinch_handler);

    // Query terminal size
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        width_ = ws.ws_col;
        height_ = ws.ws_row;
    }

    rebuild_buffer();

    // Hide cursor, switch to alternate screen
    std::printf("\033[?1049h");
    std::printf("\033[?25l");
    std::fflush(stdout);
}

void TerminalRenderer::shutdown() {
    restore_terminal();
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGWINCH, SIG_DFL);
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

            if (cell.ch == '\x01') {
                out_buf_ += "\xe2\x96\x88"; // UTF-8 for █ (U+2588)
            } else {
                out_buf_ += cell.ch;
            }
        }
        if (y < height_ - 1) {
            out_buf_ += '\n';
        }
    }

    // Reset color at end
    if (prev_color != Color::Default) {
        out_buf_ += "\033[0m";
    }

    fwrite(out_buf_.data(), 1, out_buf_.size(), stdout);
    std::fflush(stdout);
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
    if (!s_resized) return;
    s_resized = 0;

    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        width_ = ws.ws_col;
        height_ = ws.ws_row;
        rebuild_buffer();
    }
}

int TerminalRenderer::poll_input() {
    check_resize();

    char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) {
        return -1;
    }

    // Escape sequence: arrow keys are \033 [ A/B/C/D
    if (ch == '\033') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
            }
        }
        return '\033';
    }

    return static_cast<int>(ch);
}

int TerminalRenderer::wait_input() {
    // Block until first byte arrives
    struct termios blocking = impl_->orig;
    blocking.c_lflag &= ~(ECHO | ICANON);
    blocking.c_cc[VMIN] = 1;
    blocking.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &blocking);

    char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) {
        check_resize();
        return -1;
    }

    // Switch to non-blocking for escape sequence reads
    struct termios raw = impl_->orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    check_resize();

    if (ch == '\033') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'Z': return KEY_SHIFT_TAB;
            }
        }
        return '\033';
    }

    return static_cast<int>(ch);
}

int TerminalRenderer::wait_input_timeout(int timeout_ms) {
    // Use select() to wait with timeout
    struct termios raw = impl_->orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    check_resize();
    if (ret <= 0) return -1;

    char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) return -1;

    if (ch == '\033') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'Z': return KEY_SHIFT_TAB;
            }
        }
        return '\033';
    }

    return static_cast<int>(ch);
}

} // namespace astra
