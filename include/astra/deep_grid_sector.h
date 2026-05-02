#pragma once

#include "astra/grid_sector.h"

namespace astra {

// 60×40 hand-authored deep-Grid base. Plan 5 Cut 3 — replaces Plan 4's
// 30×20 layout. Three regions:
//   * Anchor (~12×10, top-left): spawn hub with lore-archive DataNode.
//   * Atlas (~24×20, middle): WarpAnchor population area (Cut 3 Task 31).
//   * Frontier (~24×10, right): firewalled placeholder for Plan 7 content.
GridSector make_deep_grid_base();

} // namespace astra
