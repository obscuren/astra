#pragma once

namespace astra {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    Rect inset(int n) const;
    Rect inset(int horiz, int vert) const;
    Rect row(int index) const;
    Rect rows(int start, int count) const;
    Rect split_left(int width) const;
    Rect split_right(int width) const;
    Rect split_top(int height) const;
    Rect split_bottom(int height) const;
    bool contains(int px, int py) const;
    bool empty() const;
};

} // namespace astra
