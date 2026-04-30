#pragma once

#include "astra/grid_network.h"

#include <cstdint>
#include <vector>

namespace astra {

enum class GridTile : uint8_t {
    Floor,            // .
    Firewall,         // # (impassable, breachable)
    DataNode,         // $
    Gateway,          // G
    ExitNode,         // X
    EncryptedFile,    // ?
    Wall,             // outside-of-sector
};

struct GridSector {
    int                   w = 0;
    int                   h = 0;
    std::vector<GridTile> tiles;       // size w*h, row-major
    int                   spawn_x = 0;
    int                   spawn_y = 0;
    GridNodeId            source_node;  // which network node this sector belongs to

    // Resolved targets for special tiles. Each entry maps an (x,y) gateway tile
    // to the destination node id it leads to (cracked or not).
    struct GatewayLink { int x, y; GridNodeId dst; };
    std::vector<GatewayLink> gateways;

    GridTile at(int x, int y) const;
    void     set(int x, int y, GridTile t);
    bool     in_bounds(int x, int y) const;
    bool     passable(int x, int y) const; // floor + walked-through gateway/exit tiles
};

// Procedural generators. Same seed -> same layout (stable revisits).
GridSector gen_subnet_sector(uint32_t seed, int security_tier);
GridSector gen_regional_sector(uint32_t seed, int security_tier);
// Hand-authored -- see grid_anchor_layout.cpp.
GridSector make_consciousness_anchor_sector();

} // namespace astra
