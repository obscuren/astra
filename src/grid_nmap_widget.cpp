#include "astra/grid_nmap_widget.h"

#include "astra/consciousness_save.h"
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

bool zoom_match(GridNodeKind k, NmapMode z) {
    switch (z) {
        case NmapMode::Lan:
            return k == GridNodeKind::Subnet
                || k == GridNodeKind::RegionalDarknet
                || k == GridNodeKind::LanRoot;
        case NmapMode::Atlas:
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

// Plan 5.5: when the active LAN root is provided, restrict LAN-mode nodes
// to: the root itself + every Subnet directly edged from that root +
// RegionalDarknet nodes (which are global). Sibling maps' LAN trees (post-
// multi-map LAN persistence) stay hidden so the player only sees their
// current LAN. Atlas mode is unaffected — anchors are global.
std::vector<NodeView> visible_nodes(const GridNetwork& net, NmapMode zoom,
                                    GridNodeId active_lan_root = {}) {
    std::vector<NodeView> out;

    auto include_lan_node = [&](const GridNode& n) {
        if (!active_lan_root.valid()) return true;   // no filter
        if (n.kind == GridNodeKind::RegionalDarknet) return true;
        if (n.kind == GridNodeKind::LanRoot) return n.id == active_lan_root;
        if (n.kind == GridNodeKind::Subnet) {
            for (const auto& e : net.edges()) {
                if (e.from == active_lan_root && e.to == n.id) return true;
            }
            return false;
        }
        return true;
    };

    for (const auto& n : net.nodes()) {
        if (!zoom_match(n.kind, zoom)) continue;
        if (zoom == NmapMode::Lan && !include_lan_node(n)) continue;
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

void GridNmapWidget::open(bool in_deep_grid) {
    open_          = true;
    in_deep_grid_  = in_deep_grid;
    cursor_idx_    = 0;
    // Default to LAN view; Tab swaps to Atlas when the player is in the
    // deep-Grid sector.
    mode_          = NmapMode::Lan;
}

void GridNmapWidget::close() {
    open_ = false;
}

uint32_t GridNmapWidget::take_jack_in_request() {
    uint32_t v = pending_jack_in_;
    pending_jack_in_ = 0;
    return v;
}

GridNmapBreachRequest GridNmapWidget::take_breach_request() {
    GridNmapBreachRequest r = pending_breach_;
    pending_breach_ = {};
    return r;
}

bool GridNmapWidget::handle_key(const GridNetwork& net, int key,
                                GridNodeId active_lan_root) {
    if (!open_) return false;

    if (key == 27) { close(); return true; }

    // Tab cycles LAN ↔ Atlas. Plan 5 §10 wants this gated to "only in deep
    // Grid", but the host doesn't currently surface that signal — the
    // `in_deep_grid_` flag is wired through `open()` for future polish but
    // unused as a gate today. For Cut 4 the cycle is always allowed; Atlas
    // mode is informative regardless of where the player is standing.
    if (key == '\t') {
        mode_       = (mode_ == NmapMode::Lan) ? NmapMode::Atlas
                                               : NmapMode::Lan;
        cursor_idx_ = 0;
        return true;
    }

    // Atlas mode: arrows + Esc only for now. Enter/breach UX in Atlas is
    // out of scope for Cut 4 — Atlas is a read-only listing.
    if (mode_ == NmapMode::Atlas) {
        return true;
    }

    auto nodes = visible_nodes(net, mode_, active_lan_root);

    if (key == ',') {
        // Legacy zoom toggle — kept as an alias for Tab while existing
        // muscle memory transitions to Tab. Always allowed, like Tab.
        mode_       = NmapMode::Atlas;
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

    // Plan 7 §17 A3: per-device netmap-side `b` (breach) is removed.
    // breach.exe survives only for region-scope firewalls walked-up-to in
    // the spatial sector. The locked_edge_under_cursor lambda + 'b'
    // handler are gone.

    switch (key) {
        case KEY_LEFT:  step_cursor(-1, 0); return true;
        case KEY_RIGHT: step_cursor(+1, 0); return true;
        case KEY_UP:    step_cursor(0, -1); return true;
        case KEY_DOWN:  step_cursor(0, +1); return true;
        case '\n':
        case '\r': {
            const auto& sel = nodes[cursor_idx_];
            const GridNode* n = net.find(sel.id);
            if (!n) return true;

            // Self-anchor bypass (Plan 5 Task 41): the player owns this
            // DeepGridAnchor — the lock predicate doesn't apply, since
            // the consciousness save is the source of truth and the
            // deep-Grid base is always reachable for its owner.
            bool self_owned = false;
            if (n->kind == GridNodeKind::DeepGridAnchor &&
                n->owned_by_consciousness_id != 0) {
                ConsciousnessSave cs;
                if (read_consciousness(cs) && cs.consciousness_id != 0 &&
                    n->owned_by_consciousness_id == cs.consciousness_id) {
                    self_owned = true;
                }
            }

            if (!self_owned && sel.locked) return true;

            pending_jack_in_ = sel.id.value;
            close();
            return true;
        }
    }
    return true;
}

void GridNmapWidget::render(UIContext& outer, const GridNetwork& net,
                            GridNodeId active_lan_root) const {
    if (!open_) return;

    if (mode_ == NmapMode::Atlas) {
        render_atlas(outer);
        return;
    }
    render_lan(outer, net, active_lan_root);
}

void GridNmapWidget::render_lan(UIContext& outer, const GridNetwork& net,
                                GridNodeId active_lan_root) const {
    auto panel = outer.panel({
        .title = " NMAP — LAN ",
        .footer = "[arrows] cursor  [enter] jack  [tab] atlas  [esc] close",
        .tag = UITag::Border});

    auto nodes = visible_nodes(net, NmapMode::Lan, active_lan_root);
    if (nodes.empty()) {
        panel.text({.x = 2, .y = 2,
                    .content = "(no networks discovered — find a Precursor console)",
                    .tag = UITag::TextDim});
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

// Plan 5 Task 42: read-only Atlas listing. Shows every WarpAnchorRecord in
// the consciousness save, grouped by galaxy. Past-galaxy entries (warpable
// == false) render dimmed; the player has them as memorial-only after a
// rebirth wipes their galaxy.
void GridNmapWidget::render_atlas(UIContext& outer) const {
    auto panel = outer.panel({
        .title = " NMAP — ATLAS ",
        .footer = "[tab] LAN view  [esc] close",
        .tag = UITag::Border});

    ConsciousnessSave cs;
    bool have = read_consciousness(cs);

    if (!have || cs.warp_anchors.empty()) {
        panel.text({.x = 2, .y = 2,
                    .content = "(no warp anchors discovered — crack a connected LAN's "
                               "\xe2\x8a\x95)",
                    .tag = UITag::TextDim});
        return;
    }

    int y = 2;
    uint16_t last_galaxy = 0;
    bool     first       = true;
    for (const auto& a : cs.warp_anchors) {
        if (first || a.galaxy_id != last_galaxy) {
            if (!first) ++y;   // blank line between galaxy groups
            char hdr[96];
            std::snprintf(hdr, sizeof hdr, "GALAXY: #%u%s",
                          static_cast<unsigned>(a.galaxy_id),
                          a.warpable ? "" : "  (past life)");
            panel.text({.x = 2, .y = y++, .content = hdr,
                        .tag = a.warpable ? UITag::TextAccent : UITag::TextDim});
            last_galaxy = a.galaxy_id;
            first       = false;
        }
        char line[160];
        std::snprintf(line, sizeof line, "  [%s]   %d/%d cracked",
                      a.lan_display_name.c_str(),
                      a.nodes_cracked, a.nodes_total);
        panel.text({.x = 2, .y = y++,
                    .content = line,
                    .tag = a.warpable ? UITag::TextDefault : UITag::TextDim});
        if (y >= panel.height() - 2) break;   // overflow guard
    }
}

} // namespace astra
