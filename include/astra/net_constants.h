#pragma once

namespace astra {

// Shared Grid-mode tuning constants. See docs/design/mechanics.md.
constexpr int kTraceMax     = 100;
constexpr int kIceVisionRange = 4;
constexpr int kKillIceTrace = 3;

// Ghost dialog outcome tuning.
constexpr int kGhostStashCredits = 50;  // credits granted for stash-lead outcome
constexpr int kGhostProvokeTrace = 8;   // trace penalty for provoke outcome

} // namespace astra
