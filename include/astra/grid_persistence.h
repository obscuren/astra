#pragma once

#include "astra/grid_sector.h"
#include "astra/sector_runtime_state.h"

namespace astra {

class Game;
struct GridSession;

// Returns the SectorRuntimeState bucket for the session's current sector,
// or nullptr if there is no active session. Routes LAN-root mutations into
// `lan_metadata.lan_sector_state` and subnet mutations into
// `lan_metadata.subnet_states[node_id]`.
//
// DeepGridAnchor / RegionalDarknet sectors return nullptr for now (Cut 2
// scope is LAN + Subnet persistence; Plan 4 owns deep-Grid base persistence).
SectorRuntimeState* active_runtime_state(Game& game);

// Convenience: append a tile mutation to the active runtime state.
// No-op if there is no active session or no LAN bucket.
void record_sector_mutation(Game& game, int x, int y, GridTile new_tile);

// Convenience: append a killed-ICE coordinate to the active runtime state.
void record_killed_ice(Game& game, int x, int y);

} // namespace astra
