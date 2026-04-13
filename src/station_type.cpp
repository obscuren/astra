#include "astra/station_type.h"

namespace astra {

const char* to_string(StationType t) {
    switch (t) {
        case StationType::NormalHub: return "NormalHub";
        case StationType::Scav:      return "Scav";
        case StationType::Pirate:    return "Pirate";
        case StationType::Abandoned: return "Abandoned";
        case StationType::Infested:  return "Infested";
    }
    return "?";
}

const char* to_string(StationSpecialty s) {
    switch (s) {
        case StationSpecialty::Generic:    return "Generic";
        case StationSpecialty::Mining:     return "Mining";
        case StationSpecialty::Research:   return "Research";
        case StationSpecialty::Frontier:   return "Frontier";
        case StationSpecialty::Trade:      return "Trade";
        case StationSpecialty::Industrial: return "Industrial";
    }
    return "?";
}

}  // namespace astra
