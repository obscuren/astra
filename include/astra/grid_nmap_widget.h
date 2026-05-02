#pragma once

#include <cstdint>

namespace astra {

class GridNetwork;
class UIContext;

enum class NmapMode : uint8_t { Lan, Atlas };

// One-shot breach request emitted when the user presses `b` on a locked edge
// in the netmap. Identifies the edge by its (from, to) node id pair. The host
// (game_input) is responsible for charging breach.exe cost and flipping
// `cracked = true` — the widget never mutates the network directly.
struct GridNmapBreachRequest {
    uint32_t from_id = 0;
    uint32_t to_id   = 0;
    bool valid() const { return from_id != 0 && to_id != 0; }
};

// Modal overlay rendered on top of the PDA Hacking tab. Two zoom layers:
//
// * Lan   — Subnet + LanRoot nodes laid out in an auto-grid.
// * Atlas — DeepGridAnchor nodes (e.g. Your.Anchor after Plan 4 capstone).
//
// Pure UI: the widget never calls into HackingSystem directly. When the user
// confirms a jack-in, `take_jack_in_request()` returns the chosen node id
// once and the host (PdaScreen) is responsible for invoking the hacking
// pipeline — same shape as `hack_term_cmd_jack`'s deferred jack flow.
class GridNmapWidget {
public:
    // `in_deep_grid` controls whether Tab cycles to Atlas. When false, Tab is
    // a no-op (LAN-only). The host sets this from the active GridSession's
    // current_node kind when invoking the widget.
    void open(bool in_deep_grid = false);
    void close();
    bool is_open() const { return open_; }
    NmapMode mode() const { return mode_; }

    // Returns true if the key was consumed.
    bool handle_key(const GridNetwork& net, int key);

    void render(UIContext& ctx, const GridNetwork& net) const;

    // One-shot jack-in request: returns the GridNodeId.value the user picked
    // (or 0 if none pending) and clears the slot.
    uint32_t take_jack_in_request();

    // One-shot breach request: returns the from/to node id pair the user
    // picked with `b` (or {0, 0} if none pending) and clears the slot.
    GridNmapBreachRequest take_breach_request();

private:
    void render_lan(UIContext& ctx, const GridNetwork& net) const;
    void render_atlas(UIContext& ctx) const;

    bool                  open_              = false;
    bool                  in_deep_grid_      = false;
    NmapMode              mode_              = NmapMode::Lan;
    int                   cursor_idx_        = 0;
    uint32_t              pending_jack_in_   = 0;
    GridNmapBreachRequest pending_breach_    = {};
};

} // namespace astra
