#include "astra/station_roll.h"

namespace astra {

namespace {
uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
}

StationType roll_station_type(uint64_t seed) {
    // 70 / 10 / 7 / 7 / 6
    uint32_t r = (uint32_t)(splitmix(seed ^ 0xA1) % 100);
    if (r < 70) return StationType::NormalHub;
    if (r < 80) return StationType::Scav;
    if (r < 87) return StationType::Pirate;
    if (r < 94) return StationType::Abandoned;
    return StationType::Infested;
}

StationSpecialty roll_station_specialty(uint64_t seed) {
    uint32_t r = (uint32_t)(splitmix(seed ^ 0xB2) % 6);
    switch (r) {
        case 0: return StationSpecialty::Generic;
        case 1: return StationSpecialty::Mining;
        case 2: return StationSpecialty::Research;
        case 3: return StationSpecialty::Frontier;
        case 4: return StationSpecialty::Trade;
        default: return StationSpecialty::Industrial;
    }
}

uint64_t derive_keeper_seed(uint64_t seed) {
    return splitmix(seed ^ 0xC3);
}

}  // namespace astra
