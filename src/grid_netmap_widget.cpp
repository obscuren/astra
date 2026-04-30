#include "astra/grid_netmap_widget.h"

#include "astra/grid_network.h"
#include "astra/renderer.h"
#include "astra/ui.h"
#include "astra/ui_types.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace astra {

namespace {

struct NodeView {
    int          src_idx;     // index into GridNetwork::nodes()
    GridNodeId   id;
    int          x, y;        // layout cell (column, row), 0-indexed
    std::string  label;
    GridNodeKind kind;
    bool         locked;
};

// Cells are spaced 12 cols × 3 rows so a [label] tag fits comfortably.
constexpr int kCellW = 14;
constexpr int kCellH = 3;

bool zoom_match(GridNodeKind k, NetmapZoom z) {
    switch (z) {
        case NetmapZoom::Regional:
            return k == GridNodeKind::Subnet || k == GridNodeKind::RegionalDarknet;
        case NetmapZoom::DeepGrid:
            return k == GridNodeKind::DeepGridAnchor;
    }
    return false;
}

// Edges are locked when they require a breach the player hasn't cleared.
bool edge_locked(const GridEdge& e) {
    return e.gateway_tier > 0 && !e.cracked;
}

// Returns true if any edge incident on `id` is still locked.
bool node_has_locked_inbound_edge(const GridNetwork& net, GridNodeId id) {
    for (const auto& e : net.edges()) {
        if ((e.from == id || e.to == id) && edge_locked(e)) return true;
    }
    return false;
}

std::vector<NodeView> visible_nodes(const GridNetwork& net, NetmapZoom zoom) {
    std::vector<NodeView> out;
    int n_in_zoom = 0;
    for (const auto& n : net.nodes()) {
        if (zoom_match(n.kind, zoom)) ++n_in_zoom;
    }
    int cols = std::max(1, std::min(4, n_in_zoom));
    int slot = 0;
    for (size_t i = 0; i < net.nodes().size(); ++i) {
        const auto& n = net.nodes()[i];
        if (!zoom_match(n.kind, zoom)) continue;

        NodeView v;
        v.src_idx = static_cast<int>(i);
        v.id      = n.id;
        v.label   = n.label;
        v.kind    = n.kind;
        v.locked  = node_has_locked_inbound_edge(net, n.id);

        // Anchor nodes carry their own layout coords (set on capstone unlock).
        if (n.kind == GridNodeKind::DeepGridAnchor &&
            (n.layout_x != 0 || n.layout_y != 0)) {
            v.x = n.layout_x;
            v.y = n.layout_y;
        } else {
            v.x = slot % cols;
            v.y = slot / cols;
        }
        ++slot;
        out.push_back(std::move(v));
    }
    return out;
}

UITag tag_for(GridNodeKind k, bool locked, bool selected) {
    if (locked)   return UITag::TextDim;
    if (selected) return UITag::TextBright;
    switch (k) {
        case GridNodeKind::Subnet:          return UITag::TextDefault;
        case GridNodeKind::RegionalDarknet: return UITag::TextBright;
        case GridNodeKind::DeepGridAnchor:  return UITag::TextAccent;
    }
    return UITag::TextDefault;
}

const char* kind_tag(GridNodeKind k) {
    switch (k) {
        case GridNodeKind::Subnet:          return "[subnet]";
        case GridNodeKind::RegionalDarknet: return "[regional]";
        case GridNodeKind::DeepGridAnchor:  return "[anchor]";
    }
    return "";
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

    // Common keys regardless of node count.
    if (key == 27) { close(); return true; }   // Esc
    if (key == ',') {
        zoom_       = (zoom_ == NetmapZoom::Regional) ? NetmapZoom::DeepGrid
                                                      : NetmapZoom::Regional;
        cursor_idx_ = 0;
        return true;
    }

    if (nodes.empty()) return true; // swallow input but no movement to do.

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
            if (sel.locked) {
                // No log channel from here; the host can surface a hint.
                return true;
            }
            pending_jack_in_ = sel.id.value;
            close();
            return true;
        }
        case 'b': {
            // Breach: deferred to a dedicated breach UI; for now consume the
            // key so it doesn't fall through to the terminal. Plan 5 will
            // wire the real flow.
            return true;
        }
    }
    return true; // overlay swallows everything else while open
}

void GridNetmapWidget::render(UIContext& outer, const GridNetwork& net) const {
    if (!open_) return;

    const char* title = (zoom_ == NetmapZoom::Regional)
                        ? "NETMAP — REGIONAL"
                        : "NETMAP — DEEP-GRID";

    auto inner = outer.panel({.title = title,
                              .footer = "[arrows] step  [enter] jack  "
                                        "[,] zoom  [esc] close",
                              .tag = UITag::Border});

    auto nodes = visible_nodes(net, zoom_);
    if (nodes.empty()) {
        const char* msg = (zoom_ == NetmapZoom::Regional)
            ? "(no networks discovered — visit Precursor consoles)"
            : "(no deep-Grid anchors — unlock ConsciousnessAnchor)";
        inner.text({.x = 2, .y = 2, .content = msg, .tag = UITag::TextDim});
        return;
    }

    // Edges first so node tags overprint.
    for (const auto& e : net.edges()) {
        int from_i = -1, to_i = -1;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].id == e.from) from_i = static_cast<int>(i);
            if (nodes[i].id == e.to)   to_i   = static_cast<int>(i);
        }
        if (from_i < 0 || to_i < 0) continue;
        int x1 = 2 + nodes[from_i].x * kCellW + kCellW / 2;
        int y1 = 2 + nodes[from_i].y * kCellH + kCellH / 2;
        int x2 = 2 + nodes[to_i].x * kCellW + kCellW / 2;
        int y2 = 2 + nodes[to_i].y * kCellH + kCellH / 2;
        bool locked = edge_locked(e);
        const char* glyph = locked ? BoxDraw::DH : BoxDraw::H;
        Color c = locked ? Color::Red : Color::DarkGray;

        // Single straight horizontal then vertical run — terminal-friendly.
        int x_lo = std::min(x1, x2), x_hi = std::max(x1, x2);
        for (int x = x_lo + 1; x < x_hi; ++x) inner.put(x, y1, glyph, c);
        int y_lo = std::min(y1, y2), y_hi = std::max(y1, y2);
        for (int y = y_lo + 1; y < y_hi; ++y) inner.put(x2, y, BoxDraw::V, c);
    }

    // Node tags.
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& v = nodes[i];
        bool selected = (static_cast<int>(i) == cursor_idx_);
        int  cx = 2 + v.x * kCellW;
        int  cy = 2 + v.y * kCellH + kCellH / 2;

        char buf[96];
        std::snprintf(buf, sizeof(buf), "[%s]%s", v.label.c_str(),
                      v.locked ? " *" : "");
        inner.text({.x = cx, .y = cy, .content = buf,
                    .tag = tag_for(v.kind, v.locked, selected)});
    }

    // Detail strip for the selected node.
    if (cursor_idx_ >= 0 && cursor_idx_ < static_cast<int>(nodes.size())) {
        const auto& v   = nodes[cursor_idx_];
        const auto* src = net.find(v.id);
        int detail_y    = inner.height() - 3;
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%-30s %-12s T%d%s",
                      v.label.c_str(),
                      kind_tag(v.kind),
                      src ? src->security_tier : 0,
                      v.locked ? "  [locked]" : "");
        inner.text({.x = 2, .y = detail_y, .content = buf,
                    .tag = v.locked ? UITag::TextDim : UITag::TextBright});
    }
}

} // namespace astra
