#pragma once

#include <cstdint>
#include <tuple>

namespace astra {

// LocationKey: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}
using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int>;

} // namespace astra
