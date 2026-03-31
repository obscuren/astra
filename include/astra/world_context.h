// include/astra/world_context.h
#pragma once

#include "astra/renderer.h"
#include "astra/ui.h"              // Rect
#include "astra/render_descriptor.h"

namespace astra {

// Rendering context for game-world entities.
// Translates local coordinates to screen coordinates and delegates
// to Renderer::draw_entity(). Has no knowledge of glyphs or colors.
class WorldContext {
public:
    WorldContext(Renderer* r, Rect bounds)
        : renderer_(r), bounds_(bounds) {}

    void put(int x, int y, const RenderDescriptor& desc) {
        int sx = bounds_.x + x;
        int sy = bounds_.y + y;
        if (sx < bounds_.x || sx >= bounds_.x + bounds_.w) return;
        if (sy < bounds_.y || sy >= bounds_.y + bounds_.h) return;
        renderer_->draw_entity(sx, sy, desc);
    }

    void put_animation(int x, int y, AnimationType type, int frame_index) {
        int sx = bounds_.x + x;
        int sy = bounds_.y + y;
        if (sx < bounds_.x || sx >= bounds_.x + bounds_.w) return;
        if (sy < bounds_.y || sy >= bounds_.y + bounds_.h) return;
        renderer_->draw_animation(sx, sy, type, frame_index);
    }

    const Rect& bounds() const { return bounds_; }
    int width() const { return bounds_.w; }
    int height() const { return bounds_.h; }

private:
    Renderer* renderer_;
    Rect bounds_;
};

} // namespace astra
