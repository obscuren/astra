#pragma once

#include "astra/fragment.h"
#include "astra/telegraph.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

// Tree node — the AST for a compiled program.
// Container nodes (TICK, LOOP) carry a non-empty body; other nodes have body == empty.
struct ProgramNode {
    FragmentId fragment = FragmentId::None;
    int        param    = 0;                  // meaningful for parameterized fragments (TICK/LOOP)
    std::vector<ProgramNode> body;            // empty unless container
};

// What attribute AMPLIFY scales next, derived during compile.
enum class DominantAttr : uint8_t {
    Damage,
    Radius,
    Duration,
    Hops,
    None,
};

// Compile-time effect description. Resolved at fire-time into actual world events.
// Fields are set as the compiler walks the tree. Patterns can override behavior.
struct EffectSpec {
    // Damage block
    int    damage              = 0;
    bool   ignores_armor       = false;
    int    returns_hp_pct      = 0;          // % returned to player (DRAIN)
    enum class DamageKind : uint8_t {
        None, Volt, Pyre, Decay, Drain, Warp,
    } damage_kind = DamageKind::None;

    // Status block (composes — multiple statuses can be applied)
    bool   applies_jitter      = false;
    bool   applies_slag        = false;
    bool   applies_warp        = false;     // "next action misfires" tag
    int    status_duration     = 3;         // turns; default for primitives

    // Shape block
    int    radius              = 0;          // 0 = single target
    int    relay_hops          = 0;          // 0 = no chain
    float  relay_falloff       = 0.5f;
    float  per_target_mult     = 1.0f;       // BROADCAST sets to 0.4

    // Timing block
    int    tick_count          = 0;          // 0 = instant; >0 = TICK rounds
    float  tick_intensity_mult = 0.5f;       // per-tick multiplier
    int    loop_count          = 0;          // 0 = no loop; >0 = LOOP rounds
    float  loop_intensity_mult = 0.5f;       // per-loop multiplier
    int    loop_ram_held       = 0;          // RAM reserved for loop's duration

    // Pattern overlay
    std::string named_pattern;               // empty if no pattern matched
    std::string pattern_description;         // shown in Compiler preview

    // AMPLIFY tracking — what attribute the next AMPLIFY would scale
    DominantAttr dominant_attr = DominantAttr::None;

    // Telegraph
    TelegraphSpec telegraph{};               // derived at compile

    // Targeting filter — set when WARP / DRAIN narrow the valid target set.
    enum class TargetFilter : uint8_t {
        Any, ActionTakers, LivingMinds,
    } target_filter = TargetFilter::Any;
};

// A fully compiled program — equipped to a cyberdeck slot, fired in combat.
struct CompiledProgram {
    std::vector<ProgramNode> chain;          // top-level pipeline
    std::string name;                        // auto-generated, player-renameable

    // Cached at compile time:
    EffectSpec resolved;
    int  exec_cost   = 0;                    // total exec; QN-scaled at fire
    int  heat_cost   = 0;
    int  ram_held    = 0;                    // 0 unless any node is a sustain

    std::vector<std::string> patterns_lit;   // names of patterns matched anywhere in tree
};

// Compile a fragment chain into a CompiledProgram. Pure function — no side effects.
// Computes EffectSpec, costs, telegraph, and pattern matches.
CompiledProgram compile_program(const std::vector<ProgramNode>& chain,
                                const std::string& name);

// Generate a default name for a program from its chain. e.g. [VOLT, BROADCAST] -> "volt-broadcast".
std::string auto_name(const std::vector<ProgramNode>& chain);

// Apply a compiled program at fire time. Caller has already used the Telegraph
// to confirm targeting; tx/ty is the confirmed target tile/entity.
// Returns a human-readable result string for the log.
class Game;
struct Hackable;
std::string fire_program(Game& game, const CompiledProgram& prog, int tx, int ty);

} // namespace astra
