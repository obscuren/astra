#pragma once

// LEGACY — transient mirror of Netspace tiles populated by jack_in for the
// existing renderer / input. Removed when the renderer + input pivot to
// read Netspace directly (Phase 0 Step 8.5 / Phase 1).

#include "astra/grid_ice.h"

#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace astra {

enum class GridTile : uint8_t {
    Floor,
    Firewall,
    DataNode,
    ExitNode,
    EncryptedFile,
    Wall,
    Connector,
    DeepGridGateway,
    WarpAnchor,
    DeviceAvatar,
    Door,
    Void,
};

struct GridSector {
    int                   w = 0;
    int                   h = 0;
    std::vector<GridTile> tiles;
    int                   spawn_x = 0;
    int                   spawn_y = 0;

    struct PairHash {
        size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
        }
    };

    std::unordered_set<std::pair<int,int>, PairHash> locked_doors;

    GridTile at(int x, int y) const;
    void     set(int x, int y, GridTile t);
    bool     in_bounds(int x, int y) const;
    bool     passable(int x, int y) const;
    bool     is_locked_door(int x, int y) const;
    void     unlock_door(int x, int y);
};

} // namespace astra
