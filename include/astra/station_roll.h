#pragma once
#include "astra/station_type.h"
#include <cstdint>

namespace astra {

// Deterministic. Same seed, same result. THA is handled by the caller.
StationType roll_station_type(uint64_t station_seed);
StationSpecialty roll_station_specialty(uint64_t station_seed);
uint64_t derive_keeper_seed(uint64_t station_seed);

}  // namespace astra
