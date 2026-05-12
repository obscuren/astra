#pragma once

// Camera grammar — Phase 1 Step 8.
// See docs/design/netspace.md § "Camera — horizontal scan, surveillance grid"
// for the canonical ASCII reference.

#include "astra/netspace.h"

namespace astra {

Netspace gen_camera_netspace(const TargetDescriptor& desc);

}  // namespace astra
