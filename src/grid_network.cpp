#include "astra/grid_network.h"

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
    return net.add_node(std::move(node));
}

GridNodeId register_precursor_console(GridNetwork& net,
                                      const std::string& region_label,
                                      uint32_t region_seed,
                                      int security_tier) {
    GridNodeId regional = ensure_regional_darknet(net, region_label, region_seed,
                                                  security_tier);

    GridNodeId anchor;
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) { anchor = n.id; break; }
    }
    if (!anchor.valid()) {
        GridNode a;
        a.kind          = GridNodeKind::DeepGridAnchor;
        a.label         = "Consciousness.Anchor";
        a.security_tier = 3;
        anchor = net.add_node(std::move(a));
    }

    for (const auto& ex : net.edges()) {
        if (ex.from == regional && ex.to == anchor) return regional;
    }
    GridEdge e;
    e.from         = regional;
    e.to           = anchor;
    e.gateway_tier = 2;
    net.add_edge(e);
    return regional;
}

} // namespace astra
