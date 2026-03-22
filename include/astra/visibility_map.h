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
    void reveal_all();
    void explore_all();
    Visibility get(int x, int y) const;

    int width() const { return width_; }
    int height() const { return height_; }

    // Const accessor for serialization
    const std::vector<Visibility>& cells() const { return cells_; }

    // Bulk load from deserialized data
    void load_from(int w, int h, std::vector<Visibility> cells);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Visibility> cells_;
};

} // namespace astra
