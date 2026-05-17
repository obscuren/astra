#pragma once

// Corpse / Dead Cyberdeck grammar — Phase 4 Task 7.
// See docs/design/netspace.md § "Corpse / Dead Cyberdeck — half-corrupted, mournful"
// for the canonical ASCII reference and layout spec.

#include "astra/netspace.h"

namespace astra {

Netspace gen_corpse_netspace(const TargetDescriptor& desc);

}  // namespace astra
