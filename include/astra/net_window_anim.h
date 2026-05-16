#pragma once
#include "astra/netspace.h"      // WindowState
#include "astra/net_session.h"   // WindowSequence
#include <cstdint>
#include <string>

namespace astra {

// Window-state trace bands. Deliberately LEAD the ICE-spawn breakpoints
// (kTraceBreakpoint1/2/3 = 50/75/100) so window corruption foreshadows
// the mechanical escalation rather than coinciding with it.
constexpr int kWindowStressed   = 40;
constexpr int kWindowHunted     = 70;
constexpr int kWindowCritical   = 95;
constexpr int kWindowHysteresis = 5;   // drop this far below entry to de-escalate

// Pure: derive the trace-band WindowState. Hysteresis is applied against
// `prev`. Black ICE present pins Critical. Never returns the transient
// Opening/Closing/Takeover/Blackwall states (those are driven elsewhere).
WindowState window_band(int trace, WindowState prev, bool black_ice_present);

// Pure: corrupt a true HP percentage for display. Each digit independently
// becomes '?' or a random digit with probability rising across the band;
// `phase` (free-running render-frame counter) makes it shimmer. Returns a
// 3-char-ish string WITHOUT the '%'. Stable/none -> exact digits.
std::string hp_lie(int true_pct, WindowState ws, uint32_t phase);

// Pure: a stable, plausible, wrong RAM value. Deterministic from
// `turn_seed` (per-net-turn) so it is constant within a turn and biased
// to read HIGHER than true. Critical only; otherwise returns true_ram.
int ram_lie(int true_ram, int ram_max, WindowState ws, uint32_t turn_seed);

// Advance a WindowSequence by its accumulated elapsed_ms using the
// per-kind frame table. Sets kind=None when the last frame elapses.
// Returns the kind that JUST FINISHED this call, or WindowSeqKind::None
// if the sequence is still running or nothing was active.
WindowSeqKind window_seq_advance(WindowSequence& q);

// Total frame count for a sequence kind (0 for None).
int window_seq_frame_count(WindowSeqKind k);

}  // namespace astra
