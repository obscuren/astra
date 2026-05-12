#include "astra/program_compiler.h"

#include "astra/cyberdeck.h"
#include "astra/display_name.h"
#include "astra/faction.h"
#include "astra/fragment.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/npc.h"
#include "astra/program_pattern.h"
#include "astra/telegraph.h"
#include "astra/world_manager.h"

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

// Scaling constants. The 0.5 falloff is shared by Relay/Tick/Loop on purpose
// — moving any one of them away from 0.5 should be a deliberate design call.
constexpr float kAmplifyFactor      = 1.5f;
constexpr float kBroadcastPerTarget = 0.4f;
constexpr float kFalloffPerHop      = 0.5f;

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
                    s.damage = round_dmg(s.damage * kAmplifyFactor); break;
                case DominantAttr::Radius:
                    s.radius += 1; break;
                case DominantAttr::Duration:
                    s.status_duration = round_dmg(s.status_duration * kAmplifyFactor); break;
                case DominantAttr::Hops:
                    s.relay_hops += 1; break;
                case DominantAttr::None: break;
            }
            break;
        case FragmentId::Broadcast:
            s.radius          = std::max(s.radius, 1);
            s.per_target_mult = kBroadcastPerTarget;
            s.dominant_attr   = DominantAttr::Radius;
            break;
        case FragmentId::Relay:
            s.relay_hops      = std::max(s.relay_hops, 1);
            s.relay_falloff   = kFalloffPerHop;
            s.dominant_attr   = DominantAttr::Hops;
            break;
        default: break;
    }
}

void apply_container(EffectSpec& s, FragmentId id, int n) {
    switch (id) {
        case FragmentId::Tick:
            s.tick_count          = std::max(1, n);
            s.tick_intensity_mult = kFalloffPerHop;
            // Tick wraps a body — but the BODY's effects already populated s.
            // Tick just marks the spec as ticking.
            break;
        case FragmentId::Loop:
            s.loop_count          = std::max(1, n);
            s.loop_intensity_mult = kFalloffPerHop;
            // RAM accounting (per-loop N+2) lives in sum_costs so it composes
            // across multiple LOOPs in one program; the per-spec field is set
            // from the program total in compile_program.
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
    // Burst handles everything: the cursor is the target tile, `width` is
    // the burst radius (0 = single tile, 1 = 3x3, 2 = 5x5), and `range` is
    // how far the cursor can move from the player. For chain (RELAY)
    // programs the chain happens automatically at fire time once the
    // primary target is picked.
    s.telegraph.shape         = TelegraphShape::Burst;
    s.telegraph.width         = s.radius;        // 0 for single-target
    s.telegraph.range         = 8;               // reasonable targeting reach
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
    out.patterns_lit = apply_patterns(chain, out.resolved);

    sum_costs(chain, out.exec_cost, out.heat_cost, out.ram_held);
    out.resolved.loop_ram_held = out.ram_held;

    return out;
}

namespace {

void apply_to_hackable(Hackable& h, const EffectSpec& s) {
    // Status fields — set runtime-state countdowns the existing system
    // already supports for QH effects (see hackable.cpp & hacking_system.cpp).
    if (s.applies_jitter || s.applies_slag || s.applies_warp) {
        h.state = HackState::Compromised;   // visual cue
    }
    (void)h;
    (void)s;
}

void apply_to_npc(Game& game, Npc& npc, const EffectSpec& s) {
    int dmg = s.damage;
    if (s.per_target_mult != 1.0f) {
        dmg = static_cast<int>(dmg * s.per_target_mult + 0.5f);
    }
    if (dmg > 0) {
        // Use existing damage-application API to keep AV / death routing consistent.
        npc.hp -= dmg;
        if (npc.hp < 0) npc.hp = 0;
        if (npc.hp == 0) {
            game.log(display_name(npc) + " is defeated.");
        }
    }
    if (s.returns_hp_pct > 0 && dmg > 0) {
        int heal = (dmg * s.returns_hp_pct) / 100;
        game.player().hp = std::min(game.player().effective_max_hp(),
                                    game.player().hp + heal);
        if (heal > 0) {
            game.log("You drain " + colored(std::to_string(heal), Color::Green)
                   + " HP from " + display_name(npc) + ".");
        }
    }
}

}  // namespace (fire helpers)

void apply_effect_at(Game& game, const EffectSpec& spec, int tx, int ty) {
    auto& world = game.world();
    auto& map = world.map();
    int radius = spec.radius;
    int min_x = tx - radius, max_x = tx + radius;
    int min_y = ty - radius, max_y = ty + radius;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) continue;

            Tile t = map.get(x, y);
            if (t == Tile::Fixture) {
                int fid = map.fixture_id(x, y);
                if (fid >= 0 && map.fixture(fid).cyber) {
                    apply_to_hackable(*map.fixture_mut(fid).cyber, spec);
                }
            }
            for (auto& npc : world.npcs()) {
                if (npc.x != x || npc.y != y || !npc.alive()) continue;
                // Programs only affect HOSTILE NPCs. Station keepers,
                // merchants, civilians, and friendlies are immune by faction
                // — no friendly fire from the cyberdeck, and the player
                // can't accidentally torch the people they're trying to
                // trade with.
                if (!is_hostile_to_player(npc.faction, game.player())) continue;
                apply_to_npc(game, npc, spec);
                if (npc.cyber) apply_to_hackable(*npc.cyber, spec);
            }
        }
    }
}

std::string fire_program(Game& game, const CompiledProgram& prog, int tx, int ty) {
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        return "No cyberdeck equipped.";
    }
    auto& deck = *(*deck_slot)->deck;

    // Reboot lock: the deck is cycling back up after an overheat — refuse
    // to fire anything until the DeckRebooting effect expires.
    if (has_effect(game.player().effects, EffectId::DeckRebooting)) {
        const auto* eff = find_effect(game.player().effects, EffectId::DeckRebooting);
        int remaining = eff ? eff->remaining : 0;
        return "Cyberdeck rebooting \xe2\x80\x94 "
             + colored(std::to_string(remaining), Color::Red)
             + " turns left.";
    }

    // Firing a program is a combat action — prevent idle-quickness bonus next turn.
    game.player().last_action_was_attack = true;

    // Charge heat + reserve RAM up front; heat-cap overflow triggers the
    // existing force_reboot path on the next tick check.
    cyberdeck_add_heat(deck, prog.heat_cost);
    if (prog.ram_held > 0) {
        deck.ram_current = std::max(0, deck.ram_current - prog.ram_held);
    }

    apply_effect_at(game, prog.resolved, tx, ty);

    // If this is a sustained (LOOP) program, register an active sustain so
    // the body re-fires each turn until the loop count expires.
    if (prog.resolved.loop_count > 0) {
        game.hacking().register_sustain(prog, tx, ty);
    }

    return colored(prog.name, Color::Cyan) + " fired ("
         + colored(std::to_string(prog.exec_cost), Color::Yellow) + " exec, "
         + colored(std::to_string(prog.heat_cost), Color::Yellow) + " heat).";
}

}  // namespace astra
