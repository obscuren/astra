#pragma once

#include "astra/map_generator.h"
#include "astra/station_type.h"

#include <memory>

namespace astra {

std::unique_ptr<MapGenerator> make_pirate_station_generator(const StationContext& ctx);

} // namespace astra
