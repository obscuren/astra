#pragma once

// Door grammar — first Phase 1 per-target generator.
// See docs/design/netspace.md § "Door — linear, satisfying" for the
// canonical ASCII reference the layout must match:
//
//   ┌─────┐    ┌─────┐    ┌─────┐    ┌─────┐
//   │ ◄── │════│ ░░░ │════│ ▒▒▒ │════│ ▓▓▓ │  ┌─────┐
//   │JACK │    │LOCK │    │LOCK │    │BOLT │══│ OUT │
//   │ @   │    │  1  │    │  2  │    │  ◊  │  │ ►── │
//   └─────┘    └─────┘    └─────┘    └─────┘  └─────┘

#include "astra/netspace.h"

namespace astra {

Netspace gen_door_netspace(const TargetDescriptor& desc);

}  // namespace astra
