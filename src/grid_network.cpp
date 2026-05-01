#include "astra/grid_network.h"

#include <algorithm>

namespace astra {

GridNodeId GridNetwork::add_node(GridNode node) {
    node.id.value = next_id_++;
    GridNodeId id = node.id;
    nodes_.push_back(std::move(node));
    return id;
}

void GridNetwork::add_edge(GridEdge edge) {
    edges_.push_back(edge);
}

const GridNode* GridNetwork::find(GridNodeId id) const {
    for (const auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

GridNode* GridNetwork::find_mut(GridNodeId id) {
    for (auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

std::vector<GridNodeId> GridNetwork::neighbors(GridNodeId id) const {
    std::vector<GridNodeId> result;
    for (const auto& e : edges_) {
        if (e.from == id) result.push_back(e.to);
        if (e.to   == id) result.push_back(e.from);
    }
    return result;
}

void GridNetwork::clear() {
    nodes_.clear();
    edges_.clear();
    next_id_ = 1;
}

void GridNetwork::load_raw(GridNode node) {
    if (node.id.value >= next_id_) next_id_ = node.id.value + 1;
    nodes_.push_back(std::move(node));
}

// Layout policy — kept here so all node creators agree on a single grid.
// Coordinates are panel-local cells; the widget draws nodes at
// (panel_origin + layout_x, panel_origin + layout_y).
namespace {
constexpr int kRegionalY      = 2;
constexpr int kRegionalSpread = 24;   // x-distance between two regionals
constexpr int kSubnetTopY     = 5;
constexpr int kSubnetColW     = 14;   // wide enough for short device labels
constexpr int kSubnetRowH     = 2;
constexpr int kSubnetCols     = 3;
constexpr int kAnchorX        = 8;
constexpr int kAnchorY        = 5;

int next_regional_x(const GridNetwork& net) {
    int x = 2;
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::RegionalDarknet) {
            x = std::max(x, n.layout_x + kRegionalSpread);
        }
    }
    return x;
}

void stamp_subnet_layout(GridNetwork& net, GridNode& node, GridNodeId regional) {
    int sibling_count = 0;
    for (const auto& e : net.edges()) {
        if (e.from != regional && e.to != regional) continue;
        const GridNodeId other = (e.from == regional) ? e.to : e.from;
        const GridNode* o = net.find(other);
        if (o && o->kind == GridNodeKind::Subnet) ++sibling_count;
    }
    const GridNode* parent = net.find(regional);
    int base_x = parent ? parent->layout_x : 2;
    int col = sibling_count % kSubnetCols;
    int row = sibling_count / kSubnetCols;
    node.layout_x = base_x + col * kSubnetColW;
    node.layout_y = kSubnetTopY + row * kSubnetRowH;
}
} // namespace

GridNodeId ensure_regional_darknet(GridNetwork& net,
                                   const std::string& region_label,
                                   uint32_t region_seed,
                                   int security_tier) {
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::RegionalDarknet && n.label == region_label) {
            return n.id;
        }
    }
    GridNode node;
    node.kind          = GridNodeKind::RegionalDarknet;
    node.source_seed   = region_seed;
    node.security_tier = security_tier;
    node.label         = region_label;
    node.layout_x      = next_regional_x(net);
    node.layout_y      = kRegionalY;
    return net.add_node(std::move(node));
}

GridNodeId register_hackable_subnet(GridNetwork& net,
                                    const std::string& region_label,
                                    uint32_t region_seed,
                                    int security_tier,
                                    const std::string& device_label) {
    GridNodeId regional = ensure_regional_darknet(net, region_label, region_seed,
                                                  security_tier);

    GridNode node;
    node.kind          = GridNodeKind::Subnet;
    node.source_seed   = region_seed;
    node.security_tier = security_tier;
    node.label         = device_label;
    stamp_subnet_layout(net, node, regional);
    GridNodeId subnet  = net.add_node(std::move(node));

    GridEdge e;
    e.from         = regional;
    e.to           = subnet;
    e.gateway_tier = 0;   // open
    e.cracked      = true;
    net.add_edge(e);
    return subnet;
}

PrecursorRegistration register_precursor_console(GridNetwork& net,
                                                  const std::string& region_label,
                                                  uint32_t region_seed,
                                                  int security_tier,
                                                  const std::string& device_label) {
    PrecursorRegistration r;
    r.subnet   = register_hackable_subnet(net, region_label, region_seed,
                                          security_tier, device_label);
    r.regional = ensure_regional_darknet(net, region_label, region_seed,
                                          security_tier);

    // Make the netmap node for this Precursor jack into the regional
    // darknet — the same sector the fixture menu lands in.
    if (GridNode* sub = net.find_mut(r.subnet)) {
        sub->entry_redirect = r.regional;
    }

    GridNodeId anchor;
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) { anchor = n.id; break; }
    }
    if (!anchor.valid()) {
        GridNode a;
        a.kind          = GridNodeKind::DeepGridAnchor;
        a.label         = "Consciousness.Anchor";
        a.security_tier = 3;
        a.layout_x      = kAnchorX;
        a.layout_y      = kAnchorY;
        anchor = net.add_node(std::move(a));
    }

    bool linked = false;
    for (const auto& ex : net.edges()) {
        if ((ex.from == r.regional && ex.to == anchor) ||
            (ex.from == anchor && ex.to == r.regional)) { linked = true; break; }
    }
    if (!linked) {
        GridEdge e;
        e.from         = r.regional;
        e.to           = anchor;
        e.gateway_tier = 2;
        net.add_edge(e);
    }
    return r;
}

} // namespace astra
