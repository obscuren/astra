#pragma once

#include "astra/fragment.h"
#include "astra/program_compiler.h"

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

// Find all pattern matches in a tree. Returns the deduped list of pattern
// names lit anywhere in the tree (top-level chain or any container body),
// and mutates `spec` so its `named_pattern`/`pattern_description` reflect the
// LAST pattern matched (last-write-wins). Callers that need the full multi-
// pattern list should read the returned vector — `EffectSpec::named_pattern`
// is intended for the UI's single-line "primary pattern" highlight only.
std::vector<std::string> apply_patterns(const std::vector<ProgramNode>& chain,
                                        EffectSpec& spec);

}  // namespace astra
