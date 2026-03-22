#include "astra/visibility_map.h"

#include <algorithm>

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

void VisibilityMap::reveal_all() {
    std::fill(cells_.begin(), cells_.end(), Visibility::Visible);
}

void VisibilityMap::explore_all() {
    for (auto& c : cells_) {
        if (c == Visibility::Unexplored)
            c = Visibility::Explored;
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

void VisibilityMap::load_from(int w, int h, std::vector<Visibility> cells) {
    width_ = w;
    height_ = h;
    cells_ = std::move(cells);
}

} // namespace astra
