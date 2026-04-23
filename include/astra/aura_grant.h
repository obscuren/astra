#pragma once

#include <cstdint>

namespace astra {

struct Effect;

// Declaration that a source (item, effect, skill) contributes one aura
// to its holder. Uses a factory pointer rather than a stored Effect to
// avoid Effect-in-Effect cycles. The factory runs once per rebuild to
// produce the runtime Aura's template_effect.
//
// target_mask defaults to AuraTarget::Player (1u << 0) — we can't
// include aura.h here without pulling in the full cycle, so the
// default is spelled as a raw bit to match.
struct AuraGrant {
    Effect (*make_effect)() = nullptr;
    int      radius       = 1;
    uint32_t target_mask  = 1u;   // AuraTarget::Player
};

} // namespace astra
