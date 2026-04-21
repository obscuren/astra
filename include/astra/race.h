#pragma once

#include <cstdint>

namespace astra {

enum class Race : uint8_t {
    Human,
    Veldrani,   // tall, blue-skinned traders and diplomats
    Kreth,      // stocky, mineral-skinned engineers
    Sylphari,   // wispy, luminescent wanderers
    Xytomorph,  // hostile chitinous predators
    Stellari,   // luminous stellar engineers, ancient and enigmatic
    Mechanical, // automated chassis — drones, turrets, war machines
};

const char* race_name(Race r);

} // namespace astra
