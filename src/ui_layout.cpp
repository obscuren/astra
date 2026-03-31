// src/ui_layout.cpp
#include "astra/ui.h"
#include <algorithm>

namespace astra {

static std::vector<int> resolve_sizes(const std::vector<Size>& sizes, int total) {
    std::vector<int> result(sizes.size(), 0);
    int remaining = total;
    int fill_count = 0;

    // Pass 1: allocate fixed and fraction
    for (size_t i = 0; i < sizes.size(); ++i) {
        switch (sizes[i].kind) {
            case Size::Fixed:
                result[i] = static_cast<int>(sizes[i].value);
                remaining -= result[i];
                break;
            case Size::Fraction:
                result[i] = static_cast<int>(sizes[i].value * total);
                remaining -= result[i];
                break;
            case Size::Fill:
                fill_count++;
                break;
        }
    }

    // Pass 2: distribute remaining to fills
    if (fill_count > 0 && remaining > 0) {
        int per_fill = remaining / fill_count;
        int extra = remaining % fill_count;
        for (size_t i = 0; i < sizes.size(); ++i) {
            if (sizes[i].kind == Size::Fill) {
                result[i] = per_fill + (extra > 0 ? 1 : 0);
                if (extra > 0) extra--;
            }
        }
    }

    // Clamp negatives
    for (auto& s : result) s = std::max(s, 0);

    return result;
}

std::vector<UIContext> UIContext::rows(const std::vector<Size>& sizes) const {
    auto resolved = resolve_sizes(sizes, bounds_.h);
    std::vector<UIContext> result;
    int y = bounds_.y;
    for (size_t i = 0; i < sizes.size(); ++i) {
        result.emplace_back(renderer_, Rect{bounds_.x, y, bounds_.w, resolved[i]});
        y += resolved[i];
    }
    return result;
}

std::vector<UIContext> UIContext::columns(const std::vector<Size>& sizes) const {
    auto resolved = resolve_sizes(sizes, bounds_.w);
    std::vector<UIContext> result;
    int x = bounds_.x;
    for (size_t i = 0; i < sizes.size(); ++i) {
        result.emplace_back(renderer_, Rect{x, bounds_.y, resolved[i], bounds_.h});
        x += resolved[i];
    }
    return result;
}

} // namespace astra
