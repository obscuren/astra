#pragma once

#include "astra/renderer.h"

namespace astra {

class BootSequence {
public:
    explicit BootSequence(Renderer* renderer);

    // Play the full boot sequence. Returns false if user pressed a key to skip.
    bool play();

private:
    // Render current state, wait for delay. Returns true if key was pressed (skip).
    bool delay(int ms);
    void draw_title();

    Renderer* renderer_;
};

} // namespace astra
