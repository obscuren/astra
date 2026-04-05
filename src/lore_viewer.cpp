#include "astra/lore_viewer.h"
#include "astra/lore_generator.h"

#include <sstream>

namespace astra {

void LoreViewer::open(const WorldLore& lore) {
    open_ = true;
    scroll_ = 0;
    lines_.clear();

    std::string history = LoreGenerator::format_history(lore);
    std::istringstream stream(history);
    std::string line;
    while (std::getline(stream, line)) {
        lines_.push_back(std::move(line));
    }
}

bool LoreViewer::handle_input(int key) {
    if (!open_) return false;

    switch (key) {
        case 27: case 'q':
            open_ = false;
            return true;
        case KEY_UP: case 'k':
            if (scroll_ > 0) --scroll_;
            return true;
        case KEY_DOWN: case 'j':
            ++scroll_;
            return true;
        case KEY_PAGE_UP:
            scroll_ = std::max(0, scroll_ - 10);
            return true;
        case KEY_PAGE_DOWN:
            scroll_ += 10;
            return true;
    }
    return true; // consume all input while open
}

void LoreViewer::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_ || screen_w < 10 || screen_h < 5) return;

    // Full-screen panel with 2-cell margin
    int margin = 2;
    Rect bounds = {margin, margin, screen_w - margin * 2, screen_h - margin * 2};
    UIContext ctx(renderer, bounds);

    // Background
    ctx.fill(' ');
    ctx.box(Color::DarkGray);

    // Title bar
    ctx.text(2, 0, " GALACTIC HISTORY ", Color::Cyan);

    // Content area (inside the box)
    int content_w = bounds.w - 2;
    int content_h = bounds.h - 2; // box top + box bottom (footer overlaps bottom)
    if (content_h <= 0) return;

    // Clamp scroll
    int max_scroll = std::max(0, static_cast<int>(lines_.size()) - content_h);
    if (scroll_ > max_scroll) scroll_ = max_scroll;

    // Draw lines
    for (int i = 0; i < content_h && (scroll_ + i) < static_cast<int>(lines_.size()); ++i) {
        const auto& line = lines_[scroll_ + i];

        // Color coding based on content
        Color fg = Color::Default;
        if (line.find("===") != std::string::npos ||
            line.find("---") != std::string::npos) {
            fg = Color::DarkGray;
        } else if (line.find("Epoch") != std::string::npos ||
                   line.find("Human Epoch") != std::string::npos) {
            fg = Color::Cyan;
        } else if (line.find("WORLD LORE") != std::string::npos) {
            fg = Color::BrightYellow;
        } else if (line.find("Artifacts:") != std::string::npos ||
                   line.find("[weapon]") != std::string::npos ||
                   line.find("[navigation") != std::string::npos ||
                   line.find("[knowledge") != std::string::npos ||
                   line.find("[key]") != std::string::npos ||
                   line.find("[anomaly]") != std::string::npos) {
            fg = Color::Magenta;
        } else if (line.find("Figures:") != std::string::npos ||
                   line.find("Founder") != std::string::npos ||
                   line.find("Conqueror") != std::string::npos ||
                   line.find("Sage") != std::string::npos ||
                   line.find("Explorer") != std::string::npos ||
                   line.find("Builder") != std::string::npos ||
                   line.find("Last") != std::string::npos ||
                   line.find("Traitor") != std::string::npos) {
            fg = Color::Yellow;
        } else if (line.find("Bya]") != std::string::npos) {
            fg = Color::Green;
        } else if (line.find("Factions:") != std::string::npos) {
            fg = Color::Cyan;
        }

        // Truncate line to fit
        std::string_view display = line;
        if (static_cast<int>(display.size()) > content_w)
            display = display.substr(0, content_w);

        ctx.text(1, 1 + i, display, fg);
    }

    // Footer
    std::string footer = "[Up/Down] scroll  [q/Esc] close";
    int scroll_pct = lines_.empty() ? 100 :
        static_cast<int>(100.0f * (scroll_ + content_h) / lines_.size());
    if (scroll_pct > 100) scroll_pct = 100;
    std::string pos = " (" + std::to_string(scroll_pct) + "%)";
    ctx.text(1, bounds.h - 1, footer, Color::DarkGray);
    ctx.text(1 + static_cast<int>(footer.size()), bounds.h - 1, pos, Color::DarkGray);
}

} // namespace astra
