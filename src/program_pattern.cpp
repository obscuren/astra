#include "astra/program_pattern.h"

#include "astra/fragment.h"
#include "astra/program_compiler.h"

#include <algorithm>
#include <vector>

namespace astra {

const std::vector<Pattern>& pattern_catalog() {
    static const std::vector<Pattern> defs = {
        { "BLIND",
          "Target loses perception/aim for 1 turn (scales with wrappers).",
          { FragmentId::Warp, FragmentId::Amplify } },
        { "MIND CONTROL",
          "Target acts against own faction for 3 turns.",
          { FragmentId::Warp, FragmentId::Jitter } },
        { "CHAIN LIGHTNING",
          "Cleanly arcs between living/electronic.",
          { FragmentId::Volt, FragmentId::Relay } },
        { "SCORCH",
          "Standard burning DoT (proper visuals).",
          { FragmentId::Pyre, FragmentId::Tick } },
        { "LIFEWELL",
          "Player heals over the LOOP duration.",
          { FragmentId::Drain, FragmentId::Loop } },
        { "DEGRADE FIELD",
          "Area armor-shred; everyone in radius loses AV.",
          { FragmentId::Decay, FragmentId::Broadcast } },
    };
    return defs;
}

namespace {

// Match `seq` against a contiguous run starting at index `start` in `chain`.
// We compare only fragment ids; param values are not part of the pattern key.
bool seq_matches_at(const std::vector<ProgramNode>& chain, size_t start,
                    const std::vector<FragmentId>& seq) {
    if (start + seq.size() > chain.size()) return false;
    for (size_t i = 0; i < seq.size(); ++i) {
        if (chain[start + i].fragment != seq[i]) return false;
    }
    return true;
}

void apply_named_override(EffectSpec& s, const Pattern& p) {
    s.named_pattern        = p.name;
    s.pattern_description  = p.description;
    // Pattern-specific tagging — purely cosmetic here; the underlying spec
    // already has the relevant fields set by the producers/transformers/containers.
    // Future patterns may write distinct flags.
}

void scan_tree(const std::vector<ProgramNode>& chain,
               EffectSpec& spec,
               std::vector<std::string>& out) {
    for (size_t i = 0; i < chain.size(); ++i) {
        for (const auto& p : pattern_catalog()) {
            if (seq_matches_at(chain, i, p.fragment_seq)) {
                apply_named_override(spec, p);
                out.push_back(p.name);
            }
        }
        if (!chain[i].body.empty()) {
            scan_tree(chain[i].body, spec, out);
        }
    }
}

}  // namespace

std::vector<std::string> apply_patterns(const std::vector<ProgramNode>& chain,
                                        EffectSpec& spec) {
    std::vector<std::string> lit;
    scan_tree(chain, spec, lit);
    std::sort(lit.begin(), lit.end());
    lit.erase(std::unique(lit.begin(), lit.end()), lit.end());
    return lit;
}

}  // namespace astra
