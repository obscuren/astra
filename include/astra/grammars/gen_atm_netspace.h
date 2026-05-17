#pragma once

// ATM grammar — Phase 4.
// See docs/design/netspace.md § "ATM — dense, urgent, money everywhere"
// for the canonical ASCII reference.

#include "astra/netspace.h"

namespace astra {

Netspace gen_atm_netspace(const TargetDescriptor& desc);

}  // namespace astra
