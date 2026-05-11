#include "astra/fragment.h"

#include <cstring>

namespace astra {

const std::vector<FragmentDef>& fragment_catalog() {
    static const std::vector<FragmentDef> defs = {
        // PRODUCERS
        { FragmentId::Volt,      "volt",      "VOLT",      FragmentKind::Producer,
          60, 2, 0, 0,  false, 1, 1, 9,
          "Electric damage; bias toward machines/cybernetics." },
        { FragmentId::Pyre,      "pyre",      "PYRE",      FragmentKind::Producer,
          60, 2, 0, 0,  false, 1, 1, 9,
          "Heat damage; bias toward living/unarmored." },
        { FragmentId::Drain,     "drain",     "DRAIN",     FragmentKind::Producer,
          70, 3, 0, 0,  false, 1, 1, 9,
          "Neural siphon; returns 50% damage as HP to player." },
        { FragmentId::Warp,      "warp",      "WARP",      FragmentKind::Producer,
          50, 1, 0, 0,  false, 1, 1, 9,
          "Substrate distortion; target's next action misfires." },
        { FragmentId::Decay,     "decay",     "DECAY",     FragmentKind::Producer,
          60, 2, 0, 0,  false, 1, 1, 9,
          "Entropy damage; ignores armor." },
        { FragmentId::Jitter,    "jitter",    "JITTER",    FragmentKind::Producer,
          40, 1, 0, 0,  false, 1, 1, 9,
          "-30% to-hit on target's attacks for 3 turns." },
        { FragmentId::Slag,      "slag",      "SLAG",      FragmentKind::Producer,
          50, 2, 0, 0,  false, 1, 1, 9,
          "-2 movement, -1 AV on target for 3 turns." },

        // TRANSFORMERS
        { FragmentId::Relay,     "relay",     "RELAY",     FragmentKind::Transformer,
          30, 1, 0, 0,  false, 1, 1, 9,
          "Effect chains to one nearest similar target (hop x0.5)." },
        { FragmentId::Broadcast, "broadcast", "BROADCAST", FragmentKind::Transformer,
          40, 2, 0, 0,  false, 1, 1, 9,
          "Effect applies in 3x3 around target tile (per-target x0.4)." },
        { FragmentId::Amplify,   "amplify",   "AMPLIFY",   FragmentKind::Transformer,
          40, 1, 0, 0,  false, 1, 1, 9,
          "+50% to dominant attribute (damage / radius / duration / hops)." },

        // CONTAINERS
        { FragmentId::Tick,      "tick",      "TICK",      FragmentKind::Container,
          30, 1, 0, 0,  true,  2, 1, 9,
          "Run body each tick for N turns (per-tick x0.5; total x0.5*N)." },
        { FragmentId::Loop,      "loop",      "LOOP",      FragmentKind::Container,
          50, 1, 1, 2,  true,  2, 1, 9,    // ram = N + 2
          "Re-run body N times across N turns; sustain holds RAM." },
    };
    return defs;
}

const FragmentDef* find_fragment(FragmentId id) {
    for (const auto& f : fragment_catalog()) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

const FragmentDef* find_fragment_by_name(const char* name) {
    if (!name) return nullptr;
    for (const auto& f : fragment_catalog()) {
        if (std::strcmp(f.name, name) == 0) return &f;
        if (std::strcmp(f.display, name) == 0) return &f;
    }
    return nullptr;
}

} // namespace astra
