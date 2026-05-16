#include "astra/net_window_anim.h"
#include <cstdint>
#include <cstdio>

namespace astra {

namespace {
uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16; return x;
}
}  // namespace

WindowState window_band(int trace, WindowState prev, bool black_ice_present) {
    if (black_ice_present) return WindowState::Critical;

    // Raw band from thresholds.
    WindowState raw =
        trace >= kWindowCritical ? WindowState::Critical :
        trace >= kWindowHunted   ? WindowState::Hunted   :
        trace >= kWindowStressed ? WindowState::Stressed :
                                   WindowState::Stable;

    // Only apply hysteresis when de-escalating from a real band state.
    auto rank = [](WindowState w) {
        switch (w) {
            case WindowState::Stable:   return 0;
            case WindowState::Stressed: return 1;
            case WindowState::Hunted:   return 2;
            case WindowState::Critical: return 3;
            default:                    return -1;  // transient: ignore
        }
    };
    int rp = rank(prev), rr = rank(raw);
    if (rp < 0) return raw;                 // prev was transient
    if (rr >= rp) return raw;               // escalating or same: take raw

    // De-escalating: require trace below (entry - hysteresis) to step down.
    int entry =
        prev == WindowState::Critical ? kWindowCritical :
        prev == WindowState::Hunted   ? kWindowHunted   :
        prev == WindowState::Stressed ? kWindowStressed : 0;
    if (trace < entry - kWindowHysteresis) return raw;
    return prev;                            // hold the higher band
}

// hp_lie / ram_lie / window_seq_advance / window_seq_frame_count:
// implemented in Tasks 3 and 6. Provide compiling stubs now.
std::string hp_lie(int true_pct, WindowState, uint32_t) {
    char b[8]; std::snprintf(b, sizeof(b), "%d", true_pct); return b;
}
int ram_lie(int true_ram, int, WindowState, uint32_t) { return true_ram; }
int window_seq_frame_count(WindowSeqKind) { return 0; }
void window_seq_advance(WindowSequence& q) { q.kind = WindowSeqKind::None; }

}  // namespace astra
