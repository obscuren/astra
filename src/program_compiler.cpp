#include "astra/program_compiler.h"

#include "astra/fragment.h"
#include "astra/telegraph.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace astra {

namespace {

constexpr int kBaseDamageVolt    = 5;
constexpr int kBaseDamagePyre    = 5;
constexpr int kBaseDamageDrain   = 4;
constexpr int kBaseDamageDecay   = 3;        // small, but ignores armor
// WARP / JITTER / SLAG are non-damage primitives; damage = 0.

float amplify_factor()  { return 1.5f; }
float broadcast_per_target() { return 0.4f; }
float relay_falloff()   { return 0.5f; }
float tick_per_tick()   { return 0.5f; }
float loop_per_iter()   { return 0.5f; }

int round_dmg(float v) {
    return std::max(0, static_cast<int>(v + 0.5f));
}

void apply_producer(EffectSpec& s, FragmentId id) {
    switch (id) {
        case FragmentId::Volt:
            s.damage = kBaseDamageVolt; s.damage_kind = EffectSpec::DamageKind::Volt;
            s.dominant_attr = DominantAttr::Damage; break;
        case FragmentId::Pyre:
            s.damage = kBaseDamagePyre; s.damage_kind = EffectSpec::DamageKind::Pyre;
            s.dominant_attr = DominantAttr::Damage; break;
        case FragmentId::Drain:
            s.damage = kBaseDamageDrain; s.damage_kind = EffectSpec::DamageKind::Drain;
            s.returns_hp_pct = 50;
            s.target_filter = EffectSpec::TargetFilter::LivingMinds;
            s.dominant_attr = DominantAttr::Damage; break;
        case FragmentId::Decay:
            s.damage = kBaseDamageDecay; s.damage_kind = EffectSpec::DamageKind::Decay;
            s.ignores_armor = true;
            s.dominant_attr = DominantAttr::Damage; break;
        case FragmentId::Warp:
            s.applies_warp = true; s.damage_kind = EffectSpec::DamageKind::Warp;
            s.target_filter = EffectSpec::TargetFilter::ActionTakers;
            s.status_duration = 1;
            s.dominant_attr = DominantAttr::Duration; break;
        case FragmentId::Jitter:
            s.applies_jitter = true; s.status_duration = 3;
            s.dominant_attr = DominantAttr::Duration; break;
        case FragmentId::Slag:
            s.applies_slag = true; s.status_duration = 3;
            s.dominant_attr = DominantAttr::Duration; break;
        default: break;
    }
}

void apply_transformer(EffectSpec& s, FragmentId id) {
    switch (id) {
        case FragmentId::Amplify:
            switch (s.dominant_attr) {
                case DominantAttr::Damage:
                    s.damage = round_dmg(s.damage * amplify_factor()); break;
                case DominantAttr::Radius:
                    s.radius += 1; break;
                case DominantAttr::Duration:
                    s.status_duration = round_dmg(s.status_duration * amplify_factor()); break;
                case DominantAttr::Hops:
                    s.relay_hops += 1; break;
                case DominantAttr::None: break;
            }
            break;
        case FragmentId::Broadcast:
            s.radius          = std::max(s.radius, 1);
            s.per_target_mult = broadcast_per_target();
            s.dominant_attr   = DominantAttr::Radius;
            break;
        case FragmentId::Relay:
            s.relay_hops      = std::max(s.relay_hops, 1);
            s.relay_falloff   = relay_falloff();
            s.dominant_attr   = DominantAttr::Hops;
            break;
        default: break;
    }
}

void apply_container(EffectSpec& s, FragmentId id, int n) {
    switch (id) {
        case FragmentId::Tick:
            s.tick_count          = std::max(1, n);
            s.tick_intensity_mult = tick_per_tick();
            // Tick wraps a body — but the BODY's effects already populated s.
            // Tick just marks the spec as ticking.
            break;
        case FragmentId::Loop:
            s.loop_count          = std::max(1, n);
            s.loop_intensity_mult = loop_per_iter();
            s.loop_ram_held       = n + 2;        // sustain RAM cost
            break;
        default: break;
    }
}

// Walks the tree. Container nodes: recursive call into body, then apply container wrap.
void walk(const std::vector<ProgramNode>& chain, EffectSpec& spec) {
    for (const auto& node : chain) {
        const FragmentDef* def = find_fragment(node.fragment);
        if (!def) continue;
        switch (def->kind) {
            case FragmentKind::Producer:
                apply_producer(spec, node.fragment);
                break;
            case FragmentKind::Transformer:
                apply_transformer(spec, node.fragment);
                break;
            case FragmentKind::Container:
                walk(node.body, spec);
                apply_container(spec, node.fragment, node.param);
                break;
        }
    }
}

void derive_telegraph(EffectSpec& s) {
    if (s.radius >= 1) {
        s.telegraph.shape = TelegraphShape::Burst;
        s.telegraph.range = 1 + s.radius;     // pick range — Burst uses range as radius
    } else {
        // Single-target: Burst radius 0 (smart-pick handles single-cell)
        s.telegraph.shape = TelegraphShape::Burst;
        s.telegraph.range = 1;                // 0 may not render; minimal range
    }
    s.telegraph.diagonals     = true;
    s.telegraph.stop_at_wall  = true;
}

void sum_costs(const std::vector<ProgramNode>& chain, int& exec, int& heat, int& ram) {
    for (const auto& node : chain) {
        const FragmentDef* def = find_fragment(node.fragment);
        if (!def) continue;
        exec += def->exec_cost;
        heat += def->heat_cost;
        // Container RAM (LOOP only): N + 2 for LOOP; 0 for TICK.
        if (def->kind == FragmentKind::Container && def->ram_per_n > 0) {
            ram += def->ram_per_n * std::max(1, node.param) + def->ram_base;
        }
        // Recurse into container bodies.
        sum_costs(node.body, exec, heat, ram);
    }
}

}  // namespace

std::string auto_name(const std::vector<ProgramNode>& chain) {
    std::string out;
    bool first = true;
    auto append_node = [&](const ProgramNode& n) {
        const FragmentDef* def = find_fragment(n.fragment);
        if (!def) return;
        if (!first) out += "-";
        out += def->name;
        first = false;
    };
    std::function<void(const std::vector<ProgramNode>&)> walk_names =
        [&](const std::vector<ProgramNode>& xs) {
            for (const auto& n : xs) {
                append_node(n);
                if (!n.body.empty()) walk_names(n.body);
            }
        };
    walk_names(chain);
    if (out.empty()) out = "blank";
    return out;
}

CompiledProgram compile_program(const std::vector<ProgramNode>& chain,
                                const std::string& name) {
    CompiledProgram out;
    out.chain = chain;
    out.name  = name.empty() ? auto_name(chain) : name;

    walk(chain, out.resolved);
    derive_telegraph(out.resolved);
    // Pattern overlay applied in Task 4 (program_pattern.cpp).

    sum_costs(chain, out.exec_cost, out.heat_cost, out.ram_held);
    out.resolved.loop_ram_held = out.ram_held;

    return out;
}

// Stub — real implementation lands in Task 10 (telegraph integration / fire).
std::string fire_program(Game& /*game*/, const CompiledProgram& prog,
                         int /*tx*/, int /*ty*/) {
    return prog.name + " fired (stub).";
}

}  // namespace astra
