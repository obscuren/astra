#pragma once

#include "astra/grid_network.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astra {

// FixtureType is defined in tilemap.h. Forward-declare here to keep grid_sector.h
// independent of the (large) tilemap.h header — the field below is a by-value
// enum, fine with a forward declaration as long as users include tilemap.h.
enum class FixtureType : uint8_t;

enum class GridTile : uint8_t {
    Floor,            // .
    Firewall,         // # (impassable, breachable)
    DataNode,         // $
    Gateway,          // G (subnet gateway)
    ExitNode,         // X (jack-out)
    EncryptedFile,    // ?
    Wall,             // outside-of-sector
    Connector,           // NEW (Plan 5 Cut 2): visible bus-trace wiring (DarkGray ═║...)
    DeepGridGateway,     // NEW (Plan 5 Cut 2): connected-LAN ⊕ portal to deep-Grid (BrightCyan)
    WarpAnchor,          // NEW (Plan 5 Cut 3): Atlas warp tile (BrightWhite ◉)
    DeviceAvatar,        // NEW (Plan 5 Cut 2.6): wall-mounted device representation in subnet sector
};

struct GridSector {
    int                   w = 0;
    int                   h = 0;
    std::vector<GridTile> tiles;       // size w*h, row-major
    int                   spawn_x = 0;
    int                   spawn_y = 0;
    GridNodeId            source_node;  // which network node this sector belongs to

    // Plan 5 Cut 2.6: source FixtureType for subnet sectors. Drives the
    // wall-mounted device-avatar glyph (camera ▤, door ║, healpod ⊞, ...).
    // Default to value 0 — subnet generators stamp the actual type.
    FixtureType source_fixture_type = static_cast<FixtureType>(0);

    // Resolved targets for special tiles. Each entry maps an (x,y) gateway tile
    // to the destination node id it leads to (cracked or not).
    struct GatewayLink { int x, y; GridNodeId dst; };
    std::vector<GatewayLink> gateways;

    // Plan 5 Cut 2: per-tile target_node_id lookup for ⌬ Gateway and ⊕
    // DeepGridGateway tiles. Used by breach.exe + traversal to resolve
    // which network node a gateway tile leads to without scanning edges.
    struct PairHash {
        size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
        }
    };
    std::unordered_map<std::pair<int,int>, GridNodeId, PairHash> gateway_target;

    GridTile at(int x, int y) const;
    void     set(int x, int y, GridTile t);
    bool     in_bounds(int x, int y) const;
    bool     passable(int x, int y) const; // floor + walked-through gateway/exit tiles
};

// Procedural generators. Same seed -> same layout (stable revisits).
GridSector gen_subnet_sector(uint32_t seed, int security_tier);
// Plan 5 Cut 2.6: subnet-sector overload that stamps a wall-mounted device
// avatar themed on the source FixtureType.
GridSector gen_subnet_sector(uint32_t seed, int security_tier, FixtureType source_type);
GridSector gen_regional_sector(uint32_t seed, int security_tier);
// Hand-authored -- see grid_anchor_layout.cpp.
GridSector make_consciousness_anchor_sector();
GridSector make_player_deep_grid_base();

} // namespace astra
