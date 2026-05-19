#pragma once

// Netspace tactical-combat mode (Slice 1). COMBAT locks free movement
// while a directly pipe-connected node holds a live hostile ICE; the
// existing arm/Tab/Space cast flow stays available. Pure functions over
// NetSession so they are Game-free and selftest-constructible.

#include "astra/net_session.h"

namespace astra {

// True if any node directly pipe-connected to the avatar's node holds a
// live, un-charmed, non-White ICE (White is ambient — never locks).
bool combat_should_lock(const NetSession& s);

// Recompute s.combat_mode for this world turn and manage the caption +
// one-shot enter/leave log. combat_manual pins COMBAT until cleared.
// Returns true on a NORMAL->COMBAT or COMBAT->NORMAL transition.
bool update_combat_lock(NetSession& s);

// Phase 5 S2: execute the CORE action at deck index idx (0=q,1=w,2=e,3=r).
// No-ops on None or out-of-range. Stub behaviours: SNIFF logs ICE/in-flight
// counts; CHANNEL adds +2 RAM (clamped); BRACE sets brace_turns=1; RUN logs.
void core_action_perform(NetSession& s, int idx);

}  // namespace astra
