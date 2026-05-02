#include "astra/grid_netmap_widget.h"

#include "astra/grid_network.h"
#include "astra/renderer.h"
#include "astra/ui.h"
#include "astra/ui_types.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace astra {

namespace {

struct NodeView {
    GridNodeId   id;
    int          x, y;     // panel-local cell coords for the LEFT edge of the label
    std::string  label;
    GridNodeKind kind;
    bool         locked;
};

bool zoom_match(GridNodeKind k, NetmapZoom z) {
    switch (z) {
        case NetmapZoom::Regional:
            return k == GridNodeKind::Subnet
                || k == GridNodeKind::RegionalDarknet
                || k == GridNodeKind::LanRoot;
        case NetmapZoom::DeepGrid:
            return k == GridNodeKind::DeepGridAnchor;
    }
    return false;
}

// Edges are locked when they require a breach the player hasn't cleared.
bool edge_locked(const GridEdge& e) {
    return e.gateway_tier > 0 && !e.cracked;
}

// A node is "locked" only when it has at least one edge AND every one of
// those edges is still locked — i.e. there's no open route to it. A node
// that has any open edge (even alongside other locked ones) is reachable
// and should accept Enter. A node with no edges at all is reachable as a
// standalone entry point.
bool node_is_locked(const GridNetwork& net, GridNodeId id) {
    bool has_edge = false;
    for (const auto& e : net.edges()) {
        if (e.from != id && e.to != id) continue;
        has_edge = true;
        if (!edge_locked(e)) return false;
    }
    return has_edge;
}

std::vector<NodeView> visible_nodes(const GridNetwork& net, NetmapZoom zoom) {
    std::vector<NodeView> out;
    for (const auto& n : net.nodes()) {
        if (!zoom_match(n.kind, zoom)) continue;
        NodeView v;
        v.id     = n.id;
        v.label  = n.label;
        v.kind   = n.kind;
        v.locked = node_is_locked(net, n.id);
        v.x      = n.layout_x;
        v.y      = n.layout_y;
        out.push_back(std::move(v));
    }
    return out;
}

UITag tag_for(GridNodeKind k, bool locked, bool selected) {
    if (selected) return UITag::TextBright;
    if (locked)   return UITag::TextDim;
    switch (k) {
        case GridNodeKind::Subnet:          return UITag::TextDefault;
        case GridNodeKind::LanRoot:         return UITag::TextAccent;
        case GridNodeKind::RegionalDarknet: return UITag::TextAccent;
        case GridNodeKind::DeepGridAnchor:  return UITag::TextWarning;
    }
    return UITag::TextDefault;
}

const char* kind_tag(GridNodeKind k) {
    switch (k) {
        case GridNodeKind::Subnet:          return "[subnet]";
        case GridNodeKind::LanRoot:         return "[lan]";
        case GridNodeKind::RegionalDarknet: return "[regional]";
        case GridNodeKind::DeepGridAnchor:  return "[anchor]";
    }
    return "";
}

// Width of the bracketed label drawn at a node, in cells. Used so edges
// know where to start/stop.
int label_cell_width(const std::string& s) {
    return 2 + static_cast<int>(s.size());   // [label]
}

// L-shape connector between two label tags. Anchors the horizontal run on
// the upper node's row so it never overpaints the lower label, drops a
// proper corner glyph at the bend, and (for locked edges) stamps a ╳ on
// the longer segment.
void draw_edge_lshape(UIContext& ctx,
                      int u_l, int u_r, int u_y,
                      int l_l, int l_r, int l_y,
                      bool locked) {
    Color line_col = locked ? Color::Red : Color::DarkGray;

    if (u_y == l_y) {
        int x_lo = std::min(u_r, l_r);
        int x_hi = std::max(u_l, l_l);
        for (int x = x_lo + 1; x < x_hi; ++x) ctx.put(x, u_y, BoxDraw::H, line_col);
        if (locked && x_hi - x_lo >= 2) {
            ctx.put((x_lo + x_hi) / 2, u_y, "\xe2\x95\xb3", Color::Red);
        }
        return;
    }

    // u_y < l_y by construction at the call site.
    bool lower_right = (l_l > u_r);
    bool lower_left  = (l_r < u_l);

    int corner_x;
    int h_from, h_to;
    if (lower_right) {
        corner_x = l_l;
        h_from   = u_r;
        h_to     = l_l;
    } else if (lower_left) {
        corner_x = l_r;
        h_from   = l_r;
        h_to     = u_l;
    } else {
        // x-ranges overlap — straight vertical from upper.bottom to lower.top
        corner_x = std::max(u_l, l_l);
        h_from   = h_to = corner_x;
    }

    int x_lo = std::min(h_from, h_to);
    int x_hi = std::max(h_from, h_to);
    for (int x = x_lo + 1; x < x_hi; ++x) ctx.put(x, u_y, BoxDraw::H, line_col);

    for (int y = u_y + 1; y < l_y; ++y) ctx.put(corner_x, y, BoxDraw::V, line_col);

    if (lower_right)      ctx.put(corner_x, u_y, BoxDraw::TR, line_col);
    else if (lower_left)  ctx.put(corner_x, u_y, BoxDraw::TL, line_col);

    if (locked && l_y - u_y >= 2) {
        ctx.put(corner_x, (u_y + l_y) / 2, "\xe2\x95\xb3", Color::Red);
    }
}

} // namespace

void GridNetmapWidget::open() {
    open_       = true;
    cursor_idx_ = 0;
}

void GridNetmapWidget::close() {
    open_ = false;
}

uint32_t GridNetmapWidget::take_jack_in_request() {
    uint32_t v = pending_jack_in_;
    pending_jack_in_ = 0;
    return v;
}

bool GridNetmapWidget::handle_key(const GridNetwork& net, int key) {
    if (!open_) return false;

    auto nodes = visible_nodes(net, zoom_);

    if (key == 27) { close(); return true; }
    if (key == ',') {
        zoom_       = (zoom_ == NetmapZoom::Regional) ? NetmapZoom::DeepGrid
                                                      : NetmapZoom::Regional;
        cursor_idx_ = 0;
        return true;
    }

    if (nodes.empty()) return true;

    if (cursor_idx_ < 0 || cursor_idx_ >= static_cast<int>(nodes.size())) {
        cursor_idx_ = 0;
    }

    auto step_cursor = [&](int dx, int dy) {
        const auto& cur = nodes[cursor_idx_];
        int best = cursor_idx_;
        int best_dist = (1 << 30);
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (static_cast<int>(i) == cursor_idx_) continue;
            const auto& n = nodes[i];
            int ddx = n.x - cur.x;
            int ddy = n.y - cur.y;
            if (dx > 0 && ddx <= 0) continue;
            if (dx < 0 && ddx >= 0) continue;
            if (dy > 0 && ddy <= 0) continue;
            if (dy < 0 && ddy >= 0) continue;
            int dist = ddx * ddx + ddy * ddy;
            if (dist < best_dist) { best_dist = dist; best = static_cast<int>(i); }
        }
        cursor_idx_ = best;
    };

    switch (key) {
        case KEY_LEFT:  step_cursor(-1, 0); return true;
        case KEY_RIGHT: step_cursor(+1, 0); return true;
        case KEY_UP:    step_cursor(0, -1); return true;
        case KEY_DOWN:  step_cursor(0, +1); return true;
        case '\n':
        case '\r': {
            const auto& sel = nodes[cursor_idx_];
            if (sel.locked) return true;
            pending_jack_in_ = sel.id.value;
            close();
            return true;
        }
        case 'b': {
            // Breach UX deferred to Plan 5 — swallow the key.
            return true;
        }
    }
    return true;
}

void GridNetmapWidget::render(UIContext& outer, const GridNetwork& net) const {
    if (!open_) return;

    const char* title = (zoom_ == NetmapZoom::Regional)
                        ? " NETMAP — REGIONAL "
                        : " NETMAP — DEEP-GRID ";

    auto panel = outer.panel({
        .title = title,
        .footer = "[arrows] cursor  [enter] jack  [b] breach  [,] zoom  [esc] close",
        .tag = UITag::Border});

    auto nodes = visible_nodes(net, zoom_);
    if (nodes.empty()) {
        const char* msg = (zoom_ == NetmapZoom::Regional)
            ? "(no networks discovered — find a Precursor console)"
            : "(no deep-Grid anchors — unlock ConsciousnessAnchor)";
        panel.text({.x = 2, .y = 2, .content = msg, .tag = UITag::TextDim});
        return;
    }

    // Layout origin inside the panel — one row of padding plus the panel's
    // own border. Nodes use their layout_x/layout_y as cell offsets from here.
    const int pad_x = 2;
    const int pad_y = 1;

    // Node centres for edge drawing — vertical centre of the label row.
    auto node_x_left = [&](const NodeView& v) { return pad_x + v.x; };
    auto node_y      = [&](const NodeView& v) { return pad_y + v.y; };

    // Draw edges first so node text overprints. Anchor each connector on
    // the upper node's row so it never crosses the lower label.
    for (const auto& e : net.edges()) {
        const NodeView* a = nullptr;
        const NodeView* b = nullptr;
        for (const auto& v : nodes) {
            if (v.id == e.from) a = &v;
            if (v.id == e.to)   b = &v;
        }
        if (!a || !b) continue;

        const NodeView* upper = (node_y(*a) <= node_y(*b)) ? a : b;
        const NodeView* lower = (upper == a) ? b : a;

        int u_l = node_x_left(*upper);
        int u_r = u_l + label_cell_width(upper->label) - 1;
        int u_y = node_y(*upper);
        int l_l = node_x_left(*lower);
        int l_r = l_l + label_cell_width(lower->label) - 1;
        int l_y = node_y(*lower);

        draw_edge_lshape(panel, u_l, u_r, u_y, l_l, l_r, l_y, edge_locked(e));
    }

    // Draw node labels.
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& v = nodes[i];
        bool selected = (static_cast<int>(i) == cursor_idx_);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "[%s]", v.label.c_str());
        panel.text({.x = node_x_left(v), .y = node_y(v), .content = buf,
                    .tag = tag_for(v.kind, v.locked, selected)});
    }

    // Legend just above the detail strip.
    int legend_y = panel.height() - 4;
    panel.text({.x = 2, .y = legend_y,
                .content = "\xe2\x97\x89 you  \xe2\x97\x8b available  "
                           "\xe2\x95\xb3 locked  \xe2\x8c\xac deep-grid gateway",
                .tag = UITag::TextDim});

    // Detail strip for the selected node.
    if (cursor_idx_ >= 0 && cursor_idx_ < static_cast<int>(nodes.size())) {
        const auto& v   = nodes[cursor_idx_];
        const auto* src = net.find(v.id);
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%-30s %-12s T%d%s",
                      v.label.c_str(),
                      kind_tag(v.kind),
                      src ? src->security_tier : 0,
                      v.locked ? "  [locked]" : "");
        panel.text({.x = 2, .y = panel.height() - 2, .content = buf,
                    .tag = v.locked ? UITag::TextDim : UITag::TextBright});
    }
}

} // namespace astra
