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
    skip_offset_ = 0;
    start_time_ = Clock::now();
    scroll_ = 0;
}

int PlaybackViewer::reveal_cursor() const {
    if (!open_) return 0;
    float elapsed = std::chrono::duration<float>(Clock::now() - start_time_).count();
    int cursor = static_cast<int>(elapsed * kCharsPerSecond) + skip_offset_;
    if (cursor > total_chars_) cursor = total_chars_;
    return cursor;
}

bool PlaybackViewer::is_revealing() const {
    return open_ && reveal_cursor() < total_chars_;
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
        skip_offset_ = total_chars_;   // clamps inside reveal_cursor()
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
