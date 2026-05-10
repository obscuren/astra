#pragma once

#include "astra/fragment.h"
#include "astra/program_compiler.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

// Designer-defined named composite. The fragment_seq is matched as a contiguous
// sub-sequence anywhere in the program tree (top-level chain or container body).
// When matched, the named effect overrides generic composition for those fragments.
struct Pattern {
    std::string name;                        // "BLIND", "MIND CONTROL", ...
    std::string description;                 // Compiler / Patterns overlay copy
    std::vector<FragmentId> fragment_seq;    // ordered fragments that form the pattern
};

const std::vector<Pattern>& pattern_catalog();

struct PatternMatch {
    const Pattern* pattern = nullptr;
    int  start_index_in_chain = -1;          // for top-level matches; -1 if inside body
};

// Find all pattern matches in a tree. Returns names lit; mutates spec for the
// override (e.g. BLIND adds a status flag). Sub-trees inside container bodies
// are also matched.
std::vector<std::string> apply_patterns(const std::vector<ProgramNode>& chain,
                                        EffectSpec& spec);

}  // namespace astra
