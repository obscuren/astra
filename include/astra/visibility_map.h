#pragma once

#include <cstdint>
#include <vector>

namespace astra {

enum class Visibility : uint8_t {
    Unexplored,
    Explored,
    Visible,
};

class VisibilityMap {
public:
    VisibilityMap() = default;
    VisibilityMap(int width, int height);

    // Demote all Visible tiles to Explored
    void clear_visible();

    void set_visible(int x, int y);
    Visibility get(int x, int y) const;

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Visibility> cells_;
};

} // namespace astra
