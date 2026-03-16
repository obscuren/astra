#include "astra/visibility_map.h"

namespace astra {

VisibilityMap::VisibilityMap(int width, int height)
    : width_(width), height_(height),
      cells_(width * height, Visibility::Unexplored) {}

void VisibilityMap::clear_visible() {
    for (auto& c : cells_) {
        if (c == Visibility::Visible) {
            c = Visibility::Explored;
        }
    }
}

void VisibilityMap::set_visible(int x, int y) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        cells_[y * width_ + x] = Visibility::Visible;
    }
}

Visibility VisibilityMap::get(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return Visibility::Unexplored;
    return cells_[y * width_ + x];
}

} // namespace astra
