#pragma once

#include <cstdint>

namespace astra {

class GridNetwork;
class UIContext;

enum class NetmapZoom : uint8_t { Regional, DeepGrid };

// Modal overlay rendered on top of the PDA Hacking tab. Two zoom layers:
//
// * Regional — Subnet + RegionalDarknet nodes laid out in an auto-grid.
// * DeepGrid — DeepGridAnchor nodes (e.g. Your.Anchor after Plan 4 capstone).
//
// Pure UI: the widget never calls into HackingSystem directly. When the user
// confirms a jack-in, `take_jack_in_request()` returns the chosen node id
// once and the host (PdaScreen) is responsible for invoking the hacking
// pipeline — same shape as `hack_term_cmd_jack`'s deferred jack flow.
class GridNetmapWidget {
public:
    void open();
    void close();
    bool is_open() const { return open_; }
    NetmapZoom zoom() const { return zoom_; }

    // Returns true if the key was consumed.
    bool handle_key(const GridNetwork& net, int key);

    void render(UIContext& ctx, const GridNetwork& net) const;

    // One-shot jack-in request: returns the GridNodeId.value the user picked
    // (or 0 if none pending) and clears the slot.
    uint32_t take_jack_in_request();

private:
    bool       open_              = false;
    NetmapZoom zoom_              = NetmapZoom::Regional;
    int        cursor_idx_        = 0;
    uint32_t   pending_jack_in_   = 0;
};

} // namespace astra
