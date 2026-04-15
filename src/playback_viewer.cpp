#include "astra/playback_viewer.h"

#include "astra/renderer.h"
#include "astra/ui.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace astra {

static constexpr float kCharsPerSecond = 30.0f;

struct PlaybackStyleDef {
    const char* header_label;
    Color       header_color;
    Color       border_color;
};

static constexpr PlaybackStyleDef kStyles[] = {
    { "[ TRANSMISSION ]", Color::Cyan,   Color::Cyan   },   // AudioLog
    { "[ INSCRIPTION ]",  Color::Yellow, Color::Yellow },   // Inscription (reserved)
};

static const PlaybackStyleDef& style_def(PlaybackStyle s) {
    return kStyles[static_cast<size_t>(s)];
}

// Wrap a single logical line to a column width, breaking at the last
// space when possible.
static std::vector<std::string> wrap_line(const std::string& in, int width) {
    std::vector<std::string> out;
    if (width <= 0 || in.empty()) { out.push_back(in); return out; }
    size_t i = 0;
    while (i < in.size()) {
        size_t take = std::min<size_t>(static_cast<size_t>(width), in.size() - i);
        if (i + take < in.size()) {
            size_t sp = in.rfind(' ', i + take);
            if (sp != std::string::npos && sp > i) take = sp - i;
        }
        out.emplace_back(in.substr(i, take));
        i += take;
        while (i < in.size() && in[i] == ' ') ++i;
    }
    if (out.empty()) out.emplace_back("");
    return out;
}

void PlaybackViewer::open(PlaybackStyle style,
                          std::string title,
                          std::vector<std::string> lines) {
    open_ = true;
    style_ = style;
    title_ = std::move(title);
    lines_ = std::move(lines);
    total_chars_ = 0;
    for (const auto& l : lines_) total_chars_ += static_cast<int>(l.size());
    reveal_cursor_ = 0.0f;
    last_tick_ = Clock::now();
    scroll_ = 0;
}

void PlaybackViewer::tick() {
    if (!open_) { last_tick_ = Clock::now(); return; }
    auto now = Clock::now();
    float dt = std::chrono::duration<float>(now - last_tick_).count();
    last_tick_ = now;
    reveal_cursor_ += dt * kCharsPerSecond;
    if (reveal_cursor_ > static_cast<float>(total_chars_))
        reveal_cursor_ = static_cast<float>(total_chars_);
}

bool PlaybackViewer::is_revealing() const {
    return open_ && static_cast<int>(reveal_cursor_) < total_chars_;
}

bool PlaybackViewer::handle_input(int key) {
    if (!open_) return false;

    // Esc / q closes
    if (key == 27 || key == 'q' || key == 'Q') {
        close();
        return true;
    }

    // Space skips remaining reveal
    if (key == ' ') {
        reveal_cursor_ = static_cast<float>(total_chars_);
        return true;
    }

    // Scroll (clamping happens in draw())
    if (key == KEY_UP)      { if (scroll_ > 0) --scroll_; return true; }
    if (key == KEY_DOWN)    { ++scroll_; return true; }
    if (key == KEY_PAGE_UP) { scroll_ -= 10; if (scroll_ < 0) scroll_ = 0; return true; }
    if (key == KEY_PAGE_DOWN) { scroll_ += 10; return true; }

    // Consume everything else (viewer has focus)
    return true;
}

void PlaybackViewer::draw(Renderer* r, int screen_w, int screen_h) {
    if (!open_ || !r || screen_w < 10 || screen_h < 8) return;

    const auto& s = style_def(style_);

    int box_w = std::max(40, screen_w * 6 / 10);
    if (box_w > screen_w - 4) box_w = screen_w - 4;
    int inner_w = box_w - 4;
    if (inner_w < 20) inner_w = 20;

    // Wrap all logical lines.
    std::vector<std::string> wrapped;
    for (const auto& logical : lines_) {
        if (logical.empty()) { wrapped.emplace_back(""); continue; }
        auto pieces = wrap_line(logical, inner_w);
        for (auto& p : pieces) wrapped.push_back(std::move(p));
    }

    // Layout: top border(1) + header(1) + title(1) + rule(1) + body + footer(1) + bottom border(1)
    constexpr int chrome_rows = 6;
    int body_budget = screen_h - 4 - chrome_rows;
    if (body_budget < 4) body_budget = 4;

    int body_rows_visible =
        std::min<int>(static_cast<int>(wrapped.size()), body_budget);
    if (body_rows_visible < 1) body_rows_visible = 1;
    int box_h = chrome_rows + body_rows_visible;

    int bx = (screen_w - box_w) / 2;
    int by = (screen_h - box_h) / 2;

    int overflow = static_cast<int>(wrapped.size()) - body_rows_visible;
    if (overflow < 0) overflow = 0;
    if (scroll_ > overflow) scroll_ = overflow;
    if (scroll_ < 0) scroll_ = 0;

    // Draw panel via UIContext — mirrors LoreViewer idiom.
    Rect bounds{bx, by, box_w, box_h};
    UIContext ctx(r, bounds);
    ctx.fill(' ');
    ctx.box(s.border_color);

    // Header row (local y=1), centered
    ctx.text_center(1, s.header_label, s.header_color);

    // Title row (local y=2), centered
    ctx.text_center(2, title_, Color::BrightWhite);

    // Rule row (local y=3)
    {
        std::string rule(static_cast<size_t>(inner_w), '-');
        ctx.text(2, 3, rule, Color::DarkGray);
    }

    // Body — reveal across all lines combined.
    // `emitted` tracks chars on lines at or above the current row, including
    // any scrolled-past lines so the reveal cursor maps to the correct row.
    int cursor = static_cast<int>(reveal_cursor_);
    int body_local_y = 4;
    int emitted = 0;
    for (int idx = 0; idx < scroll_ && idx < static_cast<int>(wrapped.size()); ++idx) {
        emitted += static_cast<int>(wrapped[idx].size());
    }
    for (int i = 0; i < body_rows_visible; ++i) {
        int idx = i + scroll_;
        if (idx >= static_cast<int>(wrapped.size())) break;
        const std::string& wl = wrapped[idx];
        int row_y = body_local_y + i;

        int available = cursor - emitted;
        if (available > 0) {
            int take = std::min<int>(static_cast<int>(wl.size()), available);
            if (take > 0) {
                ctx.text(2, row_y, std::string_view(wl).substr(0, take));
            }
            if (take < static_cast<int>(wl.size())) {
                bool blink = (std::chrono::duration_cast<std::chrono::milliseconds>(
                                  Clock::now().time_since_epoch()).count() / 400) & 1;
                if (blink) {
                    ctx.put(2 + take, row_y, '_', Color::White);
                }
            }
        }
        emitted += static_cast<int>(wl.size());
    }

    // Footer row (local y = box_h - 2 relative to top border? actually box_h-2
    // since box_h-1 is the bottom border)
    {
        std::string footer = is_revealing() ? "[Space] Skip   [Esc] Close"
                                            : "[Esc] Close";
        ctx.text_center(box_h - 2, footer, Color::DarkGray);
    }
}

} // namespace astra
