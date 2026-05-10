#pragma once

#include <cstdint>
#include <vector>

namespace astra {

// 12-fragment alphabet for the program-compilation system.
// See docs spec at .claude/specs/program-fragment-system.md for design.
enum class FragmentId : uint16_t {
    None      = 0,

    // Producers (seed an effect)
    Volt      = 1,
    Pyre      = 2,
    Drain     = 3,
    Warp      = 4,
    Decay     = 5,
    Jitter    = 6,
    Slag      = 7,

    // Transformers (modify an effect)
    Relay     = 8,
    Broadcast = 9,
    Amplify   = 10,

    // Containers (wrap a body)
    Tick      = 11,   // takes parameter N
    Loop      = 12,   // takes parameter N
};

enum class FragmentKind : uint8_t {
    Producer,
    Transformer,
    Container,
};

struct FragmentDef {
    FragmentId   id          = FragmentId::None;
    const char*  name        = "";          // display name, lowercased ("volt")
    const char*  display     = "";          // UI display ("VOLT")
    FragmentKind kind        = FragmentKind::Producer;

    // Costs
    int          exec_cost   = 0;           // action-cost units
    int          heat_cost   = 0;           // dumped at fire
    int          ram_per_n   = 0;           // 0 unless Container sustain (LOOP). Total RAM = ram_per_n * N + ram_base.
    int          ram_base    = 0;

    // Container behavior
    bool         takes_param = false;       // LOOP(N), TICK(N)
    int          default_n   = 1;
    int          min_n       = 1;
    int          max_n       = 9;

    // Brief inline description for UI
    const char*  description = "";
};

// Alphabet accessors.
const std::vector<FragmentDef>& fragment_catalog();
const FragmentDef* find_fragment(FragmentId id);

// Lookup helpers used by the dev console and Compiler UI.
const FragmentDef* find_fragment_by_name(const char* name);

} // namespace astra
