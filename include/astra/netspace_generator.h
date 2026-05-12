#pragma once

#include "astra/netspace.h"

namespace astra {

// Phase 0 stub. Produces a deterministic 14x8 walkable room with a
// jack-in tile at (1, 4) and an exit tile at (12, 4). Used as the
// fallback for NetspaceTargetKind values that don't yet have a real
// grammar.
Netspace gen_empty_netspace(const TargetDescriptor& desc);

// Dispatch: pick the right per-target grammar for `desc.kind` and
// return its Netspace. Kinds without a Phase 1 implementation fall
// back to gen_empty_netspace.
Netspace gen_for_target(const TargetDescriptor& desc);

}  // namespace astra
