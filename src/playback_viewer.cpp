#include "astra/playback_viewer.h"

#include "astra/renderer.h"

namespace astra {

static constexpr float kCharsPerSecond = 30.0f;

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

void PlaybackViewer::draw(Renderer* /*r*/, int /*screen_w*/, int /*screen_h*/) {
    // Task 3 wires draw.
}

} // namespace astra
