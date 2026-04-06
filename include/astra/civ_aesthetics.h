#pragma once

#include "astra/lore_types.h"
#include "astra/tilemap.h"

namespace astra {

// Map an Architecture type to its alien Biome for terrain rendering.
inline Biome alien_biome_for_architecture(Architecture arch) {
    switch (arch) {
        case Architecture::Crystalline: return Biome::AlienCrystalline;
        case Architecture::Organic:     return Biome::AlienOrganic;
        case Architecture::Geometric:   return Biome::AlienGeometric;
        case Architecture::VoidCarved:  return Biome::AlienVoid;
        case Architecture::LightWoven:  return Biome::AlienLight;
    }
    return Biome::AlienGeometric;
}

// Scar biome by intensity threshold.
// Call with scar_intensity already confirmed > scar_light_threshold.
inline Biome scar_biome_for_intensity(float intensity) {
    return intensity >= 0.7f ? Biome::ScarredGlassed : Biome::ScarredScorched;
}

// Does this alien architecture have pulsing/animated fixtures?
inline bool architecture_has_animation(Architecture arch) {
    return arch == Architecture::Organic || arch == Architecture::LightWoven;
}

}  // namespace astra
