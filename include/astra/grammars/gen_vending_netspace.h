#pragma once

// Vending-machine grammar — Phase 1 Step 7.
// See docs/design/netspace.md § "Vending Machine — joke run, candy mode"
// for the canonical ASCII reference.

#include "astra/netspace.h"

namespace astra {

Netspace gen_vending_netspace(const TargetDescriptor& desc);

}  // namespace astra
