#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class GridNodeKind : uint8_t {
    Subnet,           // 1 device, 1 small sector
    RegionalDarknet,  // station/asteroid scope, 3-4 sectors
    DeepGridAnchor,   // hand-authored, persists across rebirth (Plan 4)
};

struct GridNodeId {
    uint32_t value = 0;
    bool valid() const { return value != 0; }
    bool operator==(const GridNodeId&) const = default;
};

struct GridEdge {
    GridNodeId from;
    GridNodeId to;
    int        gateway_tier   = 0;     // 0 = open, 1+ = needs breach
    bool       cracked        = false; // set true once breached
};

struct GridNode {
    GridNodeId        id;
    GridNodeKind      kind          = GridNodeKind::Subnet;
    uint32_t          source_seed   = 0;        // (network_id<<16)|device_id for subnets
    int               security_tier = 1;        // 1..3 — drives ICE composition
    std::string       label;                    // "Hangar.Turret-7", "Station.Spine"
    // For RegionalDarknet+: pre-generated sector list (one per "room")
    std::vector<uint32_t> sector_seeds;
};

class GridNetwork {
public:
    GridNodeId add_node(GridNode node);
    void       add_edge(GridEdge edge);
    const GridNode* find(GridNodeId id) const;
    GridNode*       find_mut(GridNodeId id);
    std::vector<GridNodeId> neighbors(GridNodeId id) const;
    const std::vector<GridNode>& nodes() const { return nodes_; }
    const std::vector<GridEdge>& edges() const { return edges_; }
    void clear();
private:
    std::vector<GridNode> nodes_;
    std::vector<GridEdge> edges_;
    uint32_t              next_id_ = 1;
};

// Returns the regional darknet node id for the given (region_label).
// Creates the node lazily on first call. Idempotent.
GridNodeId ensure_regional_darknet(GridNetwork& net,
                                   const std::string& region_label,
                                   uint32_t region_seed,
                                   int security_tier);

// Registers a Precursor console as a deep-Grid gateway.
// Creates a deep-Grid anchor node if none exists yet (Plan 3: at most one
// per galaxy). Wires:
//   regional_darknet ─── (gateway tier 2) ─── deep_grid_anchor
// Returns the regional darknet node id (the actual jack-in target).
GridNodeId register_precursor_console(GridNetwork& net,
                                      const std::string& region_label,
                                      uint32_t region_seed,
                                      int security_tier);

} // namespace astra
