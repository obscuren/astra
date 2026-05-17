#pragma once

// Elevator netspace grammar — vertical, multi-floor press-your-luck ladder.
// See docs/design/netspace.md § "Elevator — vertical, multi-floor metaphor".

#include "astra/netspace.h"

namespace astra {

// Generate the elevator netspace: a vertical stack of full-width floor rooms
// (LOBBY at bottom = jack-in, ascending to PENTHOUSE), connected by a single
// vertical pipe spine. No horizontal branching. The SECURITY gate floor has a
// breakwall that must be Breach'd before the avatar can ascend further.
Netspace gen_elevator_netspace(const TargetDescriptor& desc);

// Map an avatar y-row to a floor index (0 = lobby/jack-in floor, increasing
// upward). Deterministic from the same row math the grammar lays out.
int elevator_floor_for_y(const Netspace& ns, int y);

}  // namespace astra
