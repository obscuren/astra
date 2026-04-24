#include "astra/terminal_renderer.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

namespace astra {

// Decode a UTF-8 sequence to a Unicode codepoint and return its terminal width.
// Returns 1 for normal chars, 2 for wide (CJK, block elements on some terminals).
static int utf8_cell_width(const char* utf8) {
    if (!utf8 || !utf8[0]) return 1;

    unsigned char c = static_cast<unsigned char>(utf8[0]);
    if (c < 0x80) return 1; // ASCII is always 1

    // Decode to codepoint
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

    int w = wcwidth(cp);
    return (w > 1) ? w : 1;
}

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

static volatile sig_atomic_t s_quit_requested = 0;
static volatile sig_atomic_t s_needs_redraw = 0;

static void signal_handler(int) {
    s_quit_requested = 1;
}

static void sigwinch_handler(int) {
    s_resized = 1;
}

static void sigtstp_handler(int) {
    // Restore terminal before suspending
    restore_terminal();
    std::printf("\033[0m\033[?25h\033[?1049l"); // reset colors, show cursor, main screen
    std::fflush(stdout);
    std::fprintf(stderr,
        "\n"
        "ASTRA has been suspended. Run `fg` to bring ASTRA back.\n"
        "note: ctrl + z suspends ASTRA\n"
        "\n");

    // Re-raise SIGTSTP with default handler to actually suspend
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

static void sigcont_handler(int) {
    // Re-init terminal after resume from suspend
    struct termios raw = s_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    s_raw_mode = 1;

    // Restore alternate screen, hide cursor
    std::printf("\033[?1049h");
    std::printf("\033[?25l");
    std::fflush(stdout);

    // Re-register SIGTSTP handler (was reset to SIG_DFL before suspend)
    struct sigaction sa{};
    sa.sa_handler = sigtstp_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTSTP, &sa, nullptr);

    // Trigger resize + redraw
    s_resized = 1;
    s_needs_redraw = 1;
}

// Append a 256-color foreground escape sequence, or fg-only reset for Default.
// Uses \033[39m (default-foreground-only) instead of \033[0m (reset-all) so
// that a cell with fg=Default, bg=<non-default> correctly preserves the bg
// when fg flips back from a colored value to Default mid-row.
static void append_color(std::string& buf, Color c) {
    if (c == Color::Default) {
        buf += "\033[39m";
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

    // Use sigaction without SA_RESTART for all signals so they
    // interrupt blocking read() calls immediately.
    struct sigaction sa_quit{};
    sa_quit.sa_handler = signal_handler;
    sa_quit.sa_flags = 0; // no SA_RESTART — interrupts read()
    sigemptyset(&sa_quit.sa_mask);
    sigaction(SIGINT, &sa_quit, nullptr);
    sigaction(SIGTERM, &sa_quit, nullptr);

    struct sigaction sa_winch{};
    sa_winch.sa_handler = sigwinch_handler;
    sa_winch.sa_flags = 0;
    sigemptyset(&sa_winch.sa_mask);
    sigaction(SIGWINCH, &sa_winch, nullptr);

    struct sigaction sa_tstp{};
    sa_tstp.sa_handler = sigtstp_handler;
    sa_tstp.sa_flags = 0;
    sigemptyset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, nullptr);

    struct sigaction sa_cont{};
    sa_cont.sa_handler = sigcont_handler;
    sa_cont.sa_flags = 0;
    sigemptyset(&sa_cont.sa_mask);
    sigaction(SIGCONT, &sa_cont, nullptr);

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
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGWINCH, &sa, nullptr);
    sigaction(SIGTSTP, &sa, nullptr);
    sigaction(SIGCONT, &sa, nullptr);
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

    Color prev_fg = Color::Default;
    Color prev_bg = Color::Default;

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto& cell = buffer_[y][x];

            // Skip continuation cells — the wide glyph to the left covers this
            if (cell.continuation) continue;

            if (cell.fg != prev_fg) {
                append_color(out_buf_, cell.fg);
                prev_fg = cell.fg;
            }

            if (cell.bg != prev_bg) {
                if (cell.bg == Color::Default) {
                    out_buf_ += "\033[49m";
                } else {
                    char seq[16];
                    int len = std::snprintf(seq, sizeof(seq), "\033[48;5;%dm",
                                            static_cast<uint8_t>(cell.bg));
                    out_buf_.append(seq, len);
                }
                prev_bg = cell.bg;
            }

            out_buf_ += cell.ch;
        }
        if (y < height_ - 1) {
            out_buf_ += '\n';
        }
    }

    // Reset color at end
    if (prev_fg != Color::Default || prev_bg != Color::Default) {
        out_buf_ += "\033[0m";
    }

    fwrite(out_buf_.data(), 1, out_buf_.size(), stdout);
    std::fflush(stdout);
}

void TerminalRenderer::draw_char(int x, int y, char ch) {
    draw_char(x, y, ch, Color::Default);
}

void TerminalRenderer::draw_char(int x, int y, char ch, Color fg) {
    draw_char(x, y, ch, fg, Color::Default);
}

void TerminalRenderer::draw_char(int x, int y, char ch, Color fg, Color bg) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        auto& cell = buffer_[y][x];
        // If this cell was wide, clear its continuation cell
        if (cell.wide && x + 1 < width_) {
            auto& next = buffer_[y][x + 1];
            next.continuation = false;
            next.ch[0] = ' ';
            next.ch[1] = '\0';
        }
        cell.ch[0] = ch;
        cell.ch[1] = '\0';
        cell.fg = fg;
        cell.bg = bg;
        cell.wide = false;
        cell.continuation = false;
    }
}

void TerminalRenderer::draw_glyph(int x, int y, const char* utf8, Color fg) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        auto& cell = buffer_[y][x];
        // If this cell was previously wide, clean up old continuation
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

        // Mark following cells as continuations
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

void TerminalRenderer::draw_glyph(int x, int y, const char* utf8, Color fg, Color bg) {
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
        cell.bg = bg;
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

bool TerminalRenderer::consume_quit_request() {
    if (s_quit_requested) {
        s_quit_requested = 0;
        return true;
    }
    return false;
}

int TerminalRenderer::get_width() const { return width_; }
int TerminalRenderer::get_height() const { return height_; }

bool TerminalRenderer::read_cell(int x, int y, char* glyph_out, Color& fg_out) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return false;
    const auto& cell = buffer_[y][x];
    if (cell.continuation) return false;
    for (int i = 0; i < 5; ++i) glyph_out[i] = cell.ch[i];
    fg_out = cell.fg;
    return true;
}

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
        if (seq[0] == 'O') {
            if (seq[1] == 'P') return KEY_F1;
            if (seq[1] == 'Q') return KEY_F2;
            if (seq[1] == 'R') return KEY_F3;
            if (seq[1] == 'S') return KEY_F4;
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
    while (true) {
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n == 1) break;
        // Interrupted by signal (SIGWINCH, SIGINT, SIGCONT, etc.)
        bool was_resize = s_resized;
        check_resize();
        if (s_quit_requested) return -1;
        if (s_needs_redraw) { s_needs_redraw = 0; return -1; }
        if (was_resize) return -1; // trigger redraw after resize
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
                case '3': case '5': case '6': {
                    char tilde;
                    if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        if (seq[1] == '3') return KEY_DELETE;
                        return seq[1] == '5' ? KEY_PAGE_UP : KEY_PAGE_DOWN;
                    }
                    break;
                }
                case '1': {
                    char c3, tilde;
                    if (read(STDIN_FILENO, &c3, 1) == 1 &&
                        read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        if (c3 == '1') return KEY_F1;
                        if (c3 == '2') return KEY_F2;
                        if (c3 == '3') return KEY_F3;
                        if (c3 == '4') return KEY_F4;
                    }
                    break;
                }
            }
        }
        if (seq[0] == 'O') {
            if (seq[1] == 'P') return KEY_F1;
            if (seq[1] == 'Q') return KEY_F2;
            if (seq[1] == 'R') return KEY_F3;
            if (seq[1] == 'S') return KEY_F4;
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
                case '3': case '5': case '6': {
                    char tilde;
                    if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        if (seq[1] == '3') return KEY_DELETE;
                        return seq[1] == '5' ? KEY_PAGE_UP : KEY_PAGE_DOWN;
                    }
                    break;
                }
                case '1': {
                    char c3, tilde;
                    if (read(STDIN_FILENO, &c3, 1) == 1 &&
                        read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        if (c3 == '1') return KEY_F1;
                        if (c3 == '2') return KEY_F2;
                        if (c3 == '3') return KEY_F3;
                        if (c3 == '4') return KEY_F4;
                    }
                    break;
                }
            }
        }
        if (seq[0] == 'O') {
            if (seq[1] == 'P') return KEY_F1;
            if (seq[1] == 'Q') return KEY_F2;
            if (seq[1] == 'R') return KEY_F3;
            if (seq[1] == 'S') return KEY_F4;
        }
        return '\033';
    }

    return static_cast<int>(ch);
}

} // namespace astra

// Semantic rendering implementations — kept after the anonymous namespace
// to access resolve() / resolve_animation() from the terminal theme.
#include "astra/render_descriptor.h"
#include "terminal_theme.h"

namespace astra {

void TerminalRenderer::draw_entity(int x, int y, const RenderDescriptor& desc) {
    auto visual = resolve(desc);
    if (visual.utf8)
        draw_glyph(x, y, visual.utf8, visual.fg);
    else if (visual.bg != Color::Default)
        draw_char(x, y, visual.glyph, visual.fg, visual.bg);
    else
        draw_char(x, y, visual.glyph, visual.fg);
}

void TerminalRenderer::draw_animation(int x, int y, AnimationType type, int frame_index) {
    auto visual = resolve_animation(type, frame_index);
    if (visual.utf8) {
        if (visual.bg != Color::Default)
            draw_glyph(x, y, visual.utf8, visual.fg, visual.bg);
        else
            draw_glyph(x, y, visual.utf8, visual.fg);
    } else {
        if (visual.bg != Color::Default)
            draw_char(x, y, visual.glyph, visual.fg, visual.bg);
        else
            draw_char(x, y, visual.glyph, visual.fg);
    }
}

} // namespace astra
