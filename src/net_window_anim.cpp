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

std::string hp_lie(int true_pct, WindowState ws, uint32_t phase) {
    if (true_pct < 0) true_pct = 0;
    if (true_pct > 100) true_pct = 100;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", true_pct);
    std::string s(buf);
    if (ws != WindowState::Hunted && ws != WindowState::Critical &&
        ws != WindowState::Blackwall)
        return s;
    // Per-digit corruption probability (out of 1000). Hunted ~350,
    // Critical/Blackwall ~800.
    uint32_t pmil = (ws == WindowState::Hunted) ? 350u : 800u;
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t h = hash32(phase * 131u + static_cast<uint32_t>(i) * 977u
                            + static_cast<uint32_t>(true_pct) * 17u);
        if (h % 1000u < pmil) {
            uint32_t pick = (h >> 10) % 11u;     // 0..9 -> digit, 10 -> '?'
            s[i] = (pick == 10u) ? '?' : static_cast<char>('0' + pick);
        }
    }
    return s;
}

int ram_lie(int true_ram, int ram_max, WindowState ws, uint32_t turn_seed) {
    if (ws != WindowState::Critical && ws != WindowState::Blackwall)
        return true_ram;
    uint32_t h = hash32(turn_seed * 2654435761u + 0x9E3779B9u);
    int bump = 1 + static_cast<int>(h % 2u);     // +1..+2, biased high
    int shown = true_ram + bump;
    if (shown > ram_max) shown = ram_max;
    if (shown < 0) shown = 0;
    return shown;
}
int window_seq_frame_count(WindowSeqKind) { return 0; }
void window_seq_advance(WindowSequence& q) { q.kind = WindowSeqKind::None; }

}  // namespace astra
