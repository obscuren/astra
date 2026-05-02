#pragma once

#include <cstdint>
#include <functional>
#include <tuple>

namespace astra {

// LocationKey: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}
using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int>;

// Hasher for LocationKey so it can be used as a key in std::unordered_map /
// std::unordered_set. Mixes each tuple component with a 32-bit splitmix step.
struct LocationKeyHash {
    std::size_t operator()(const LocationKey& k) const noexcept {
        auto mix = [](std::size_t h, std::size_t v) {
            // boost::hash_combine
            return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        };
        std::size_t h = std::hash<uint32_t>{}(std::get<0>(k));
        h = mix(h, std::hash<int>{}(std::get<1>(k)));
        h = mix(h, std::hash<int>{}(std::get<2>(k)));
        h = mix(h, std::hash<bool>{}(std::get<3>(k)));
        h = mix(h, std::hash<int>{}(std::get<4>(k)));
        h = mix(h, std::hash<int>{}(std::get<5>(k)));
        h = mix(h, std::hash<int>{}(std::get<6>(k)));
        return h;
    }
};

} // namespace astra
