#pragma once

#include "astra/grid_sector.h"
#include "astra/lan.h"
#include "astra/grid_network.h"
#include "astra/world_manager.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

// Generates the LAN sector: a flat layout containing every subnet as
// its own packed-walled room, joined by 3-tile bridges, clustered into
// per-tier zones. `world` is used to look up each subnet's Hackable
// (HackTagMask drives the room template; source_type drives the
// wall-mounted DeviceAvatar glyph).
GridSector generate_lan_sector_v2(const LanMetadata& meta,
                                  const GridNetwork& net,
                                  const WorldManager& world);

// ---------------------------------------------------------------------------
// Phase 1 — sector sizing
// ---------------------------------------------------------------------------

// Plan 8 size envelope. Sized from the total subnet count + lobby invariant.
struct LanV2SizeParams {
    int width;
    int height;
    int zone_count;   // 1, 2, or 3
};

LanV2SizeParams compute_lan_v2_size(const LanMetadata& meta);

// ---------------------------------------------------------------------------
// Phase 2 — zone partition
// ---------------------------------------------------------------------------

struct LanV2ZoneRegion {
    int x, y, w, h;          // zone bounding box in sector coords
    int tier;                 // 1, 2, or 3
    int anchor_x, anchor_y;  // seed cell for the zone's anchor room
    std::string name;         // banner — pulled from LanZone.name
};

// Bug-3 fix: net + world are now required to read Hackable.security_tier for
// per-subnet tier detection (LanZone.tier was always mostly T1 post-cluster_rooms).
// Bug-2 fix: banners are now generic ("LOBBY"/"OPERATIONS"/"VAULT").
std::vector<LanV2ZoneRegion> partition_zones(const LanMetadata& meta,
                                             const LanV2SizeParams& size,
                                             const GridNetwork& net,
                                             const WorldManager& world);

// ---------------------------------------------------------------------------
// Phase 3 — room placement
// ---------------------------------------------------------------------------

struct LanV2Room {
    int x, y, w, h;
    int tier;
    GridNodeId source_subnet;      // empty for the lobby
    bool is_lobby        = false;
    bool is_zone_anchor  = false;
    int  zone_index      = 0;
};

std::vector<LanV2Room> place_rooms(const LanMetadata& meta,
                                   const std::vector<LanV2ZoneRegion>& zones,
                                   const GridNetwork& net,
                                   const WorldManager& world,
                                   uint32_t seed);

// ---------------------------------------------------------------------------
// Phase 4 — Connectivity
// ---------------------------------------------------------------------------

struct LanV2Edge {
    int from_idx;
    int to_idx;
    int dist;
};

} // namespace astra
