#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

// FixtureType lives in tilemap.h. Forward-declared here so GridNode can carry
// the source-fixture hint (Plan 5 Cut 2.6) without a heavy header dependency.
enum class FixtureType : uint8_t;


enum class GridNodeKind : uint8_t {
    Subnet,           // 1 device, 1 small sector
    LanRoot,          // Plan 5: per-LAN root node — subnets edge from this; deep-Grid edge from this
    RegionalDarknet,  // RETIRED in Plan 5 — kept for save compat through Cut 1; do not generate new ones
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
    // Plan 4: graph-view position and ownership (DeepGridAnchor nodes only)
    int               layout_x = 0;
    int               layout_y = 0;
    uint64_t          owned_by_consciousness_id = 0;   // 0 = unowned
    // Plan 4: when set, jacking into THIS node delivers the player into
    // the redirect target's sector instead of building one for this node.
    // Used to make per-Precursor Subnets show up as distinct netmap entries
    // while still landing the player in the shared regional darknet sector
    // (same experience as the fixture-menu jack-in).
    GridNodeId        entry_redirect;

    // Plan 5 Cut 2.6: for Subnet nodes only — the FixtureType that this
    // subnet mirrors (Door, Console, HealPod, ...). Stamped at registration
    // time and forwarded into gen_subnet_sector() so the sector renders a
    // wall-mounted device-avatar themed to the real-world fixture.
    FixtureType       source_fixture_type = static_cast<FixtureType>(0);
};

class GridNetwork {
public:
    GridNodeId add_node(GridNode node);
    void       add_edge(GridEdge edge);
    void       load_raw(GridNode node);  // load-time: preserves node id, updates next_id_
    const GridNode* find(GridNodeId id) const;
    GridNode*       find_mut(GridNodeId id);
    std::vector<GridNodeId> neighbors(GridNodeId id) const;
    const std::vector<GridNode>& nodes() const { return nodes_; }
    const std::vector<GridEdge>& edges() const { return edges_; }
    std::vector<GridEdge>&       edges_mut()    { return edges_; }
    void clear();
private:
    std::vector<GridNode> nodes_;
    std::vector<GridEdge> edges_;
    uint32_t              next_id_ = 1;
};

// Returns the regional darknet node id for the given (region_label).
// Creates the node lazily on first call. Idempotent. Layout coords are
// stamped on first creation and never moved.
GridNodeId ensure_regional_darknet(GridNetwork& net,
                                   const std::string& region_label,
                                   uint32_t region_seed,
                                   int security_tier);

// Registers a per-device Subnet node under the given region. Each call
// adds a new Subnet (no dedup — every Hackable in the world is its own
// node), wires an open (tier-0) edge to the regional darknet, and stamps
// a deterministic layout position so the netmap can render it without
// extra logic. Returns the new Subnet's id; the caller is responsible
// for stashing it on the Hackable's `jack_in_node_id` if jacking should
// land in the per-device subnet sector.
GridNodeId register_hackable_subnet(GridNetwork& net,
                                    const std::string& region_label,
                                    uint32_t region_seed,
                                    int security_tier,
                                    const std::string& device_label);

// Registers a Precursor console: ensures regional + Subnet (via the
// helper above) + deep-Grid anchor + the regional<->anchor tier-2
// gateway. Returns the Subnet id, but jacking semantics from the
// fixture menu still target the regional darknet — the caller writes
// that id explicitly.
struct PrecursorRegistration {
    GridNodeId subnet;        // unique per console — netmap entry
    GridNodeId regional;      // shared per region — jack-in target
};
PrecursorRegistration register_precursor_console(GridNetwork& net,
                                                  const std::string& region_label,
                                                  uint32_t region_seed,
                                                  int security_tier,
                                                  const std::string& device_label);

} // namespace astra
