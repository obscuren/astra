#include "crawler/terminal_renderer.h"

#include <cstdio>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

namespace crawler {

void TerminalRenderer::init() {
    // Switch to raw mode for unbuffered input
    tcgetattr(STDIN_FILENO, &orig_termios_);
    struct termios raw = orig_termios_;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Query terminal size
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        width_ = ws.ws_col;
        height_ = ws.ws_row;
    }

    buffer_.assign(height_, std::vector<char>(width_, ' '));

    // Hide cursor, switch to alternate screen
    std::printf("\033[?1049h");
    std::printf("\033[?25l");
    std::fflush(stdout);
}

void TerminalRenderer::shutdown() {
    // Show cursor, restore main screen
    std::printf("\033[?25h");
    std::printf("\033[?1049l");
    std::fflush(stdout);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
}

void TerminalRenderer::clear() {
    for (auto& row : buffer_) {
        std::fill(row.begin(), row.end(), ' ');
    }
}

void TerminalRenderer::present() {
    std::printf("\033[H"); // cursor home
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            std::putchar(buffer_[y][x]);
        }
        if (y < height_ - 1) {
            std::putchar('\n');
        }
    }
    std::fflush(stdout);
}

void TerminalRenderer::draw_char(int x, int y, char ch) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        buffer_[y][x] = ch;
    }
}

void TerminalRenderer::draw_string(int x, int y, const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        draw_char(x + static_cast<int>(i), y, text[i]);
    }
}

int TerminalRenderer::get_width() const { return width_; }
int TerminalRenderer::get_height() const { return height_; }

int TerminalRenderer::poll_input() {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        return static_cast<int>(ch);
    }
    return -1;
}

} // namespace crawler
