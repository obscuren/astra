#include "astra/character.h"

namespace astra {

const char* class_name(PlayerClass c) {
    switch (c) {
        case PlayerClass::Smuggler:  return "Smuggler";
        case PlayerClass::Engineer:  return "Engineer";
        case PlayerClass::Marine:    return "Marine";
        case PlayerClass::Navigator: return "Navigator";
        case PlayerClass::Scavenger: return "Scavenger";
    }
    return "Unknown";
}

} // namespace astra
