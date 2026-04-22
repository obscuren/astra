#pragma once

#include "astra/tilemap.h"

#include <cstdint>
#include <vector>

namespace astra {

enum class ShipClass : uint8_t {
    EscapePod,
    Freighter,
    Corvette,
};

enum class ShipOrientation : uint8_t {
    East,
    West,
    South,
    North,
};

// Inclusive dx range for one room along the ship-local axis.
struct ShipRoomExtent {
    int dx_min;
    int dx_max;
};

// Tunable parameters per ship class. All coordinates are in the
// ship-local "east-facing" frame (nose at +dx), rotated at stamp time.
struct ShipClassSpec {
    ShipClass class_id;
    const char* name;

    // Hull
    int hull_len;              // full length along dx (must be even)
    int body_half_h;           // widest half-height (dy bound)
    int nose_taper_len;        // tiles over which nose narrows (0 = flat nose)
    int stern_taper;           // tiles the stern half-height shrinks (0 = flat)
    float hull_coverage;       // 0..1 fraction of edge tiles kept as wall

    // Rooms & bulkheads (ordered stern -> nose)
    std::vector<ShipRoomExtent> rooms;
    std::vector<int> bulkhead_dx;

    // Engine nacelles (behind the stern, on the top/bottom flanks)
    int nacelle_len;           // 0 = no nacelles; 1-2 tiles behind the stern

    // Mid-section wings (perpendicular stubs)
    int wing_span;             // 0 = no wings; 1-2 tiles perpendicular reach
    int wing_width;            // 1-3 tiles wide along the dx axis

    // Skid mark
    int skid_min;
    int skid_max;

    // Debris field
    int debris_radius;
    int debris_min;
    int debris_max;

    // Hull breaches
    int breach_min;
    int breach_max;

    // Fixtures per room index (matches rooms[]).
    std::vector<std::vector<FixtureType>> fixtures_by_room;
};

} // namespace astra
