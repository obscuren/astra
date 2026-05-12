#pragma once

#include "astra/grid_network.h"
#include "astra/hackable.h"
#include "astra/rect.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astra {

enum class LanFlavour : uint8_t { Station, Asteroid, Dungeon, Precursor };

// Lowercases alphanumerics, converts spaces / underscores / hyphens to a single
// '-' and strips everything else. Trailing hyphens are trimmed. Used both for
// AI contact ids and for nmap hostname construction.
std::string slugify(const std::string& s);

struct LanZone {
    std::string name;                                 // "security", "lobby", "exec"; never rendered in-sector
    Rect        extents = {};
    int         tier = 1;                             // 1=open doorway, 2=breach-required, 3=inner sanctum
    std::vector<GridNodeId> contained_subnets;
};

class WorldManager;

struct LanMetadata {
    GridNodeId      lan_root;
    bool            has_deep_grid_edge = false;       // true iff connected (LanRoot → shared DeepGridAnchor)
    std::string     region_label;                     // "Heavens Above"
    std::string     display_name;                     // "Concourse LAN"
    LanFlavour      flavour = LanFlavour::Station;
    int             security_tier = 1;
    bool            connected = false;
    uint32_t        gen_seed = 0;
    uint32_t        subnet_base = 0;                  // packed 10.X.Y.0
    std::vector<LanZone>           zones;
    uint64_t        last_visited_tick = 0;
    int             nodes_total = 0;
    int             nodes_cracked = 0;
    int             ice_killed = 0;
    int             lore_extracted = 0;
    uint16_t        origin_galaxy_id = 0;
};

// Plan 8: zone banner label for grid-layout HUD overlay. Returns
// "<name> (T<n>)" when LanZone.name is non-empty, else "T<n> ZONE".
std::string zone_banner_label(const LanZone& room);

// Build a per-device hostname for nmap output. Format:
//   <short-tag>-<host_octet>.<region-slug>.lan
// Examples: "console-3.tha.lan", "door-7.hub-lyra.lan", "conduit-12.tha.lan".
//
// short-tag derives from tag_summary(h.tags) (the dominant single-word
// label); host_octet is `h.ip & 0xFF`; region-slug is slugify(meta.region_label).
std::string lan_hostname(const Hackable& h, const LanMetadata& meta);

// Walk the active world map's electrical fixtures + NPC implants. Each
// Hackable is assigned an IP and registered in the GridNetwork as a Subnet
// node with an edge from a per-LAN LanRoot node. If the LAN is "connected"
// (Station/Asteroid/Precursor flavour), an additional tier-2 edge
// LanRoot -> shared DeepGridAnchor is added (anchor is found via existing
// network nodes; lazy-created if absent).
//
// Idempotent: callers wipe `meta` and the prior LAN's nodes/edges before
// invoking when re-running the sweep (see lan_full_reset in Task 10).
//
// TODO(Cut 2): extend with `int map_id` once World tracks multiple maps.
void register_hackables_in_lan(WorldManager& world, GridNetwork& net, LanMetadata& meta);

// Infer flavour from the active map's kind/biome/location.
LanFlavour infer_flavour(const WorldManager& world);

// Whether a LAN of this flavour gets a Deep-Grid edge.
bool       infer_connected(LanFlavour f);

// Plan 5 Cut 3 Task 31: when the player cracks a connected LAN's ⊕
// DeepGridGateway for the first time, register the LAN as an Atlas warp
// anchor in consciousness.dat and stamp a `WarpAnchor` tile in the deep-Grid
// Atlas region (cols 14-44, rows 1-30).
//
// `lan_root_id` identifies the LAN whose ⊕ was cracked. Idempotent — if a
// record for this (galaxy_id, region_seed) already exists, the call is a
// no-op. Returns true iff a new record was added.
//
// Silent no-op when:
//   - `lan_root_id` is invalid
//   - `lan_root_id` doesn't match the active LAN's lan_root (safety)
//   - the consciousness save can't be read, or `deep_grid_base` is empty
//     (player hasn't taken the ConsciousnessAnchor capstone yet — without
//     an Atlas there's nothing to stamp).
bool register_deep_grid_warp_anchor(WorldManager& world, GridNodeId lan_root_id);

} // namespace astra
