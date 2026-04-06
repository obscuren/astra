#pragma once

#include "astra/lore_types.h"
#include "astra/renderer.h"
#include "astra/ui.h"

#include <string>
#include <vector>

namespace astra {

class LoreViewer {
public:
    LoreViewer() = default;

    bool is_open() const { return open_; }
    void open(const WorldLore& lore);
    void close() { open_ = false; }

    // Returns true if input was consumed
    bool handle_input(int key);
    void draw(Renderer* renderer, int screen_w, int screen_h);

private:
    bool open_ = false;
    int scroll_ = 0;
    std::vector<std::string> lines_;
};

} // namespace astra
