#pragma once

#include "astra/renderer.h"
#include "astra/ui.h"

namespace astra {

class HelpScreen {
public:
    HelpScreen() = default;

    bool is_open() const { return open_; }
    void open();
    void close() { open_ = false; }

    // Returns true if input was consumed
    bool handle_input(int key);
    void draw(Renderer* renderer, int screen_w, int screen_h);

private:
    bool open_ = false;
    int tab_ = 0;
    int scroll_ = 0;
    static constexpr int tab_count_ = 4;
};

} // namespace astra
