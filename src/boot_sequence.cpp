#include "astra/boot_sequence.h"

#include <string>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// Title splash — thick block letters
// ---------------------------------------------------------------------------

static const char* title_art[] = {
    R"(    ###    )",
    R"(   ## ##   )",
    R"(  ##   ##  )",
    R"( ##     ## )",
    R"( ######### )",
    R"( ##     ## )",
    R"( ##     ## )",
};
static constexpr int letter_height = 7;

static const char* letter_S[] = {
    R"(  #######  )",
    R"( ##     ## )",
    R"( ##        )",
    R"(  #######  )",
    R"(        ## )",
    R"( ##     ## )",
    R"(  #######  )",
};

static const char* letter_T[] = {
    R"( ######### )",
    R"(    ###    )",
    R"(    ###    )",
    R"(    ###    )",
    R"(    ###    )",
    R"(    ###    )",
    R"(    ###    )",
};

static const char* letter_R[] = {
    R"( ########  )",
    R"( ##     ## )",
    R"( ##     ## )",
    R"( ########  )",
    R"( ##   ##   )",
    R"( ##    ##  )",
    R"( ##     ## )",
};

static const char* title_letters[][7] = {
    // A
    {title_art[0], title_art[1], title_art[2], title_art[3],
     title_art[4], title_art[5], title_art[6]},
    // S
    {letter_S[0], letter_S[1], letter_S[2], letter_S[3],
     letter_S[4], letter_S[5], letter_S[6]},
    // T
    {letter_T[0], letter_T[1], letter_T[2], letter_T[3],
     letter_T[4], letter_T[5], letter_T[6]},
    // R
    {letter_R[0], letter_R[1], letter_R[2], letter_R[3],
     letter_R[4], letter_R[5], letter_R[6]},
    // A (reuse)
    {title_art[0], title_art[1], title_art[2], title_art[3],
     title_art[4], title_art[5], title_art[6]},
};

static constexpr int letter_count = 5;
static constexpr int letter_width = 11; // each letter block is 11 chars wide
static constexpr int letter_gap = 3;    // gap between letters

// ---------------------------------------------------------------------------
// Boot sequence text
// ---------------------------------------------------------------------------

struct BootLine {
    std::string text;
    Color color;
    int delay_after; // ms to wait after displaying this line
};

static const std::vector<BootLine> boot_lines = {
    {"ASTRA SYSTEMS v0.1.0",                    Color::Cyan,     200},
    {"",                                        Color::Default,  100},
    {"Initializing subsystems...",               Color::DarkGray, 180},
    {"  Navigation computer          [OK]",     Color::Green,    150},
    {"  Shadowcasting engine         [OK]",     Color::Green,    140},
    {"  Procedural generator         [OK]",     Color::Green,    160},
    {"  Life support                 [OK]",     Color::Green,    130},
    {"  Weapons array                [OK]",     Color::Green,    170},
    {"  Shield harmonics    [WARN: degraded]",  Color::Yellow,   300},
    {"",                                        Color::Default,  100},
    {"Loading star charts... done.",             Color::DarkGray, 200},
    {"Connecting to The Heavens Above...",       Color::DarkGray, 400},
    {"",                                        Color::Default,  100},
    {"Welcome aboard, commander.",               Color::White,    800},
};

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

BootSequence::BootSequence(Renderer* renderer)
    : renderer_(renderer) {}

bool BootSequence::delay(int ms) {
    int remaining = ms;
    while (remaining > 0) {
        int chunk = (remaining > 50) ? 50 : remaining;
        int key = renderer_->wait_input_timeout(chunk);
        if (key != -1) return true;
        remaining -= chunk;
    }
    return false;
}

void BootSequence::draw_title() {
    int w = renderer_->get_width();
    int h = renderer_->get_height();

    int total_w = letter_count * letter_width + (letter_count - 1) * letter_gap;
    int start_x = (w - total_w) / 2;
    int start_y = (h - letter_height) / 2;

    for (int li = 0; li < letter_count; ++li) {
        int lx = start_x + li * (letter_width + letter_gap);
        for (int row = 0; row < letter_height; ++row) {
            const char* line = title_letters[li][row];
            for (int c = 0; line[c] != '\0'; ++c) {
                int px = lx + c;
                int py = start_y + row;
                if (px < 0 || px >= w || py < 0 || py >= h) continue;
                if (line[c] == '#') {
                    renderer_->draw_char(px, py, BLOCK_CHAR, Color::White);
                }
            }
        }
    }
}

bool BootSequence::play() {
    int w = renderer_->get_width();
    int h = renderer_->get_height();

    // --- Phase 1: Title splash ---
    renderer_->clear();
    renderer_->present();
    if (delay(300)) return false;

    // Fade in: draw title
    renderer_->clear();
    draw_title();
    renderer_->present();
    if (delay(2000)) return false;

    // Brief pause then clear
    renderer_->clear();
    renderer_->present();
    if (delay(400)) return false;

    // --- Phase 2: Boot sequence ---

    // Initial delay before first line
    renderer_->clear();
    renderer_->present();
    if (delay(500)) return false;

    // Center the boot text vertically
    int total_lines = static_cast<int>(boot_lines.size());
    int start_y = (h - total_lines) / 2;
    if (start_y < 2) start_y = 2;

    // Left margin
    int margin = 4;

    for (int i = 0; i < total_lines; ++i) {
        const auto& line = boot_lines[i];

        renderer_->clear();

        for (int j = 0; j <= i; ++j) {
            const auto& prev = boot_lines[j];
            if (prev.text.empty()) continue;

            int y = start_y + j;
            if (y < 0 || y >= h) continue;

            auto bracket = prev.text.rfind('[');
            if (bracket != std::string::npos && bracket > 0) {
                std::string prefix = prev.text.substr(0, bracket);
                for (int c = 0; c < static_cast<int>(prefix.size()); ++c) {
                    if (margin + c < w)
                        renderer_->draw_char(margin + c, y, prefix[c], Color::DarkGray);
                }
                std::string tag = prev.text.substr(bracket);
                for (int c = 0; c < static_cast<int>(tag.size()); ++c) {
                    if (margin + static_cast<int>(bracket) + c < w)
                        renderer_->draw_char(margin + static_cast<int>(bracket) + c, y,
                                             tag[c], prev.color);
                }
            } else {
                for (int c = 0; c < static_cast<int>(prev.text.size()); ++c) {
                    if (margin + c < w)
                        renderer_->draw_char(margin + c, y, prev.text[c], prev.color);
                }
            }
        }

        // Blinking cursor after last visible line
        int cursor_y = start_y + i;
        int cursor_x = margin;
        if (!line.text.empty()) {
            cursor_x = margin + static_cast<int>(line.text.size()) + 1;
        }
        if (cursor_x < w && cursor_y < h) {
            renderer_->draw_char(cursor_x, cursor_y, '_', Color::DarkGray);
        }

        renderer_->present();

        if (delay(line.delay_after)) return false;
    }

    // Final pause with cursor blinking
    for (int blink = 0; blink < 6; ++blink) {
        renderer_->clear();

        for (int j = 0; j < total_lines; ++j) {
            const auto& prev = boot_lines[j];
            if (prev.text.empty()) continue;
            int y = start_y + j;
            if (y < 0 || y >= h) continue;

            auto bracket = prev.text.rfind('[');
            if (bracket != std::string::npos && bracket > 0) {
                std::string prefix = prev.text.substr(0, bracket);
                for (int c = 0; c < static_cast<int>(prefix.size()); ++c) {
                    if (margin + c < w)
                        renderer_->draw_char(margin + c, y, prefix[c], Color::DarkGray);
                }
                std::string tag = prev.text.substr(bracket);
                for (int c = 0; c < static_cast<int>(tag.size()); ++c) {
                    if (margin + static_cast<int>(bracket) + c < w)
                        renderer_->draw_char(margin + static_cast<int>(bracket) + c, y,
                                             tag[c], prev.color);
                }
            } else {
                for (int c = 0; c < static_cast<int>(prev.text.size()); ++c) {
                    if (margin + c < w)
                        renderer_->draw_char(margin + c, y, prev.text[c], prev.color);
                }
            }
        }

        // Blink cursor
        int last_y = start_y + total_lines - 1;
        int last_x = margin + static_cast<int>(boot_lines.back().text.size()) + 1;
        if (blink % 2 == 0 && last_x < w && last_y < h) {
            renderer_->draw_char(last_x, last_y, '_', Color::DarkGray);
        }

        renderer_->present();
        if (delay(200)) return false;
    }

    return true;
}

} // namespace astra
