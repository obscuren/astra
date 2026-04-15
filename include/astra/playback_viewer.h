#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace astra {

class Renderer;

enum class PlaybackStyle : uint8_t {
    AudioLog,      // "[ TRANSMISSION ]" header
    Inscription,   // reserved for future ruins work
};

class PlaybackViewer {
public:
    PlaybackViewer() = default;

    bool is_open() const { return open_; }
    // True while revealing chars; the run loop uses this to poll fast.
    bool is_revealing() const;

    void open(PlaybackStyle style,
              std::string title,
              std::vector<std::string> lines);
    void close() { open_ = false; }

    // Returns true if the key was consumed.
    bool handle_input(int key);
    void draw(Renderer* r, int screen_w, int screen_h);

    // Advance the reveal cursor using wall-clock delta since the last call.
    // Mirrors AnimationManager::tick — invoked each iteration from Game::run.
    void tick();

private:
    using Clock = std::chrono::steady_clock;

    bool open_ = false;
    PlaybackStyle style_ = PlaybackStyle::AudioLog;
    std::string title_;
    std::vector<std::string> lines_;
    int total_chars_ = 0;               // sum of line lengths, set at open()
    float reveal_cursor_ = 0.0f;        // fractional chars revealed; floor() = rendered
    Clock::time_point last_tick_ = Clock::now();
    int scroll_ = 0;                    // vertical scroll (only when body overflows)
};

} // namespace astra
