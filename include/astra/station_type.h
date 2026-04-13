#pragma once

#include <cstdint>
#include <string>

namespace astra {

enum class StationType : uint8_t {
    NormalHub,
    Scav,
    Pirate,
    Abandoned,
    Infested,
};

enum class StationSpecialty : uint8_t {
    Generic,
    Mining,
    Research,
    Frontier,
    Trade,
    Industrial,
};

struct StationContext {
    bool is_tha = false;
    StationType type = StationType::NormalHub;
    StationSpecialty specialty = StationSpecialty::Generic;
    uint64_t keeper_seed = 0;
    std::string station_name;
};

const char* to_string(StationType);
const char* to_string(StationSpecialty);

}  // namespace astra
