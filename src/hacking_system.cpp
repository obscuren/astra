#include "astra/hacking_system.h"

#include "astra/cyberdeck.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/grid_ice.h"
#include "astra/grid_sector.h"
#include "astra/hackable.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/npc.h"
#include "astra/program.h"
#include "astra/program_effects.h"   // Task 9 will populate; Task 7 ships a stub
#include "astra/skill_defs.h"
#include "astra/visibility_map.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace astra {

namespace {
constexpr int kDetectionDecayInterval = 5;   // tick every N world steps, -1 to value
constexpr int kDetectionMax = 100;
constexpr int kDetectionMin = 0;

// Grid-side tuning constants (see docs/mechanics.md).
constexpr int kHeatTraceCouplingThreshold = 5;   // +1 Trace/turn while heat exceeds this
constexpr int kRebootTracePenalty         = 10;
constexpr int kTraceMax                   = 100;
constexpr int kTraceBreakpoint1           = 50;
constexpr int kTraceBreakpoint2           = 75;
constexpr int kTraceBreakpoint3           = 100;

struct HackTarget {
    Hackable* hack = nullptr;
    int tx = 0, ty = 0;
    std::string name;
};

HackTarget hackable_at(Game& game, int x, int y) {
    HackTarget t{};
    t.tx = x; t.ty = y;
    auto& world = game.world();
    Tile tile = world.map().get(x, y);
    if (tile == Tile::Fixture) {
        int fid = world.map().fixture_id(x, y);
        if (fid >= 0) {
            FixtureData& fd = world.map().fixture_mut(fid);
            if (fd.cyber) {
                t.hack = &*fd.cyber;
                t.name = device_kind_name(fd.cyber->device_kind);
                return t;
            }
        }
    }
    for (auto& npc : world.npcs()) {
        if (npc.x == x && npc.y == y && npc.cyber && npc.alive()) {
            t.hack = &*npc.cyber;
            t.name = npc.label();
            return t;
        }
    }
    return t;
}

} // namespace

void HackingSystem::add_detection(int delta) {
    int prev = detection_.value;
    detection_.value = std::clamp(detection_.value + delta, kDetectionMin, kDetectionMax);

    auto crossed = [&](int t) { return prev < t && detection_.value >= t; };
    if (crossed(50))  on_detection_threshold_(50);
    if (crossed(75))  on_detection_threshold_(75);
    if (crossed(100)) on_detection_threshold_(100);
}

void HackingSystem::on_detection_threshold_(int threshold) {
    if (!game_) return;
    switch (threshold) {
        case 50:
            game_->log("Detected: nearby personnel are investigating.");
            break;
        case 75: {
            game_->log("Local network is broadcasting your signature.");
            std::string fac = game_->dominant_faction_in_current_map();
            if (!fac.empty()) {
                modify_faction_standing(game_->player(), fac, -10);
                game_->log("Reputation with " + fac + " worsens.");
            }
            break;
        }
        case 100: {
            game_->log("ZONE ALARM. The grid lights up.");
            auto& m = game_->world().map();
            for (int fid = 0; fid < m.fixture_count(); ++fid) {
                auto& fd = m.fixture_mut(fid);
                if (fd.cyber) fd.cyber->state = HackState::Alarmed;
            }
            for (auto& npc : game_->world().npcs()) {
                if (npc.cyber) npc.cyber->state = HackState::Alarmed;
            }
            std::string fac = game_->dominant_faction_in_current_map();
            if (!fac.empty()) {
                modify_faction_standing(game_->player(), fac, -25);
            }
            break;
        }
    }
}

void HackingSystem::reset_zone() {
    detection_.value = 0;
    detection_.decay_acc = 0;
}

uint64_t HackingSystem::compute_zone_signature(const Game& game) {
    // Hash navigation state + zone coords. The exact composition is internal
    // — we only need a stable uint64_t that changes when the player enters a
    // distinct zone.
    const auto& nav = game.world().navigation();
    uint64_t s = nav.current_system_id;
    s = (s * 31u) ^ static_cast<uint64_t>(nav.current_body_index + 2);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.current_moon_index + 2);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.at_station ? 1 : 0);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.on_ship ? 1 : 0);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.current_depth + 1);
    s = (s * 31u) ^ static_cast<uint64_t>(game.world().zone_x() + 1);
    s = (s * 31u) ^ static_cast<uint64_t>(game.world().zone_y() + 1);
    s = (s * 31u) ^ static_cast<uint64_t>(static_cast<int>(game.world().surface_mode()));
    return s;
}

void HackingSystem::tick(Game& game) {
    uint64_t sig = compute_zone_signature(game);
    if (sig != last_zone_signature_) {
        last_zone_signature_ = sig;
        reset_zone();
        return;
    }
    if (detection_.value <= kDetectionMin) return;
    if (++detection_.decay_acc >= kDetectionDecayInterval) {
        detection_.decay_acc = 0;
        detection_.value = std::max(kDetectionMin, detection_.value - 1);
    }
}

void HackingSystem::reset() {
    targeting_ = false;
    target_x_ = 0;
    target_y_ = 0;
    blink_phase_ = 0;
}

void HackingSystem::begin_quickhack_targeting(Game& game) {
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        game.log("You need an equipped cyberdeck to quickhack.");
        return;
    }
    targeting_ = true;
    blink_phase_ = 0;

    // Snap cursor to nearest visible Hackable.
    int best_d = 9999;
    int best_x = game.player().x;
    int best_y = game.player().y;
    auto& world = game.world();
    for (int y = 0; y < world.map().height(); ++y) {
        for (int x = 0; x < world.map().width(); ++x) {
            if (world.visibility().get(x, y) != Visibility::Visible) continue;
            auto t = hackable_at(game, x, y);
            if (!t.hack) continue;
            int d = std::abs(x - game.player().x) + std::abs(y - game.player().y);
            if (d < best_d) { best_d = d; best_x = x; best_y = y; }
        }
    }
    target_x_ = best_x;
    target_y_ = best_y;
    game.log("Quickhack targeting. Move cursor, [Enter] confirm, [Esc] cancel.");
}

void HackingSystem::handle_targeting_input(int key, Game& game) {
    auto step = [&](int dx, int dy) {
        int nx = target_x_ + dx;
        int ny = target_y_ + dy;
        auto& world = game.world();
        if (nx < 0 || nx >= world.map().width()) return;
        if (ny < 0 || ny >= world.map().height()) return;
        if (world.visibility().get(nx, ny) != Visibility::Visible) return;
        target_x_ = nx; target_y_ = ny;
    };
    switch (key) {
        case 'k': case KEY_UP:    step( 0, -1); break;
        case 'j': case KEY_DOWN:  step( 0,  1); break;
        case 'h': case KEY_LEFT:  step(-1,  0); break;
        case 'l': case KEY_RIGHT: step( 1,  0); break;
        case '\033':
            targeting_ = false;
            game.log("Quickhack cancelled.");
            break;
        case '\n': case '\r': {
            auto t = hackable_at(game, target_x_, target_y_);
            if (!t.hack) {
                game.log("No hackable target there.");
                return;
            }
            auto* deck_slot = game.player().equipment.equipped_cyberdeck();
            if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
                targeting_ = false;
                game.log("No deck equipped.");
                return;
            }
            std::vector<int> menu_slots;
            for (int i = 0; i < (*deck_slot)->deck->stats.slots; ++i) {
                const auto& slot = (*deck_slot)->deck->loaded[i];
                if (slot.program_def_id == 0) continue;
                Item probe = build_by_def_id(slot.program_def_id);
                if (!probe.program) continue;
                const ProgramDef* def = find_program(probe.program->id);
                if (!def || def->kind != ProgramKind::Qh) continue;
                bool match = std::any_of(def->target_filter.begin(),
                                         def->target_filter.end(),
                                         [&](DeviceKind k){ return k == t.hack->device_kind; });
                if (match) menu_slots.push_back(i);
            }
            if (menu_slots.empty()) {
                game.log("No loaded quickhack matches " + t.name + ".");
                return;
            }
            game.open_qh_picker(target_x_, target_y_, menu_slots);
            targeting_ = false;
            return;
        }
        default: break;
    }
}

std::string HackingSystem::execute_quickhack(Game& game, const Item& program,
                                             Hackable& target, int tx, int ty) {
    if (!program.program) return "Not a program.";
    const ProgramDef* def = find_program(program.program->id);
    if (!def) return "Unknown program.";
    if (def->kind != ProgramKind::Qh)
        return "Only .qh programs can be fired in the real world.";

    bool ok = std::any_of(def->target_filter.begin(), def->target_filter.end(),
                          [&](DeviceKind k){ return k == target.device_kind; });
    if (!ok) {
        return std::string("Program rejects ") + device_kind_name(target.device_kind) + ".";
    }

    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) return "No cyberdeck equipped.";
    auto& deck = *(*deck_slot)->deck;
    if (deck.ram_current < def->ram_cost) {
        return "Not enough RAM (" + std::to_string(deck.ram_current) + "/" +
               std::to_string(def->ram_cost) + ").";
    }
    deck.ram_current -= def->ram_cost;

    add_detection(def->detection_cost);

    apply_program_effect(def->id, game, target, tx, ty);

    target.state = HackState::Compromised;
    return std::string(def->name) + " executed.";
}

void HackingSystem::commit_loot_(Game& game, GridLootBuffer& loot, int pct) {
    if (pct <= 0) return;
    int credits = (loot.credits * pct) / 100;
    game.player().money += credits;
    if (credits > 0) {
        game.log("Looted " + std::to_string(credits) + " credits.");
    }
    // Lore unlocks always carry on Voluntary; on HardJackOut they drop too.
    if (pct >= 100) {
        for (const auto& lore : loot.lore_unlocked) {
            game.log("Lore archive unlocked: " + lore);
        }
    }
    // TODO Plan 4: commit code_fragments and acquired programs as inventory items.
    loot = GridLootBuffer{};
}

void HackingSystem::spawn_black_ice_(GridSession& /*s*/) {
    // Stub — Task 16 implements this.
}

void HackingSystem::spawn_gray_ice_reinforcement_(GridSession& /*s*/) {
    // Stub — Task 16 implements.
}

bool HackingSystem::jack_in(Game& game, GridNodeId entry_node) {
    if (session_) {
        game.log("Already jacked in.");
        return false;
    }
    if (!player_has_skill(game.player(), SkillId::Cat_Hacking)) {
        game.log("You lack the Hacking skill category.");
        return false;
    }
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        game.log("No cyberdeck equipped.");
        return false;
    }
    const auto& cd = *(*deck_slot)->deck;
    auto& net = game.world().grid_network();
    auto* node = net.find(entry_node);
    if (!node) {
        game.log("Unknown network node.");
        return false;
    }

    GridSession s;
    s.entry_node   = entry_node;
    s.current_node = entry_node;
    s.body_x       = game.player().x;
    s.body_y       = game.player().y;
    s.body_state   = GameState::Playing;

    // Avatar HP from skill+deck. NeuralFortitude raises max by 1.
    bool nf = player_has_skill(game.player(), SkillId::NeuralFortitude);
    s.avatar_hp_max = 3 + (nf ? 1 : 0);
    s.avatar_hp     = s.avatar_hp_max;

    s.ram_max = cd.stats.ram_max;
    s.ram     = cd.ram_current;

    // Tier-driven Trace tick
    switch (node->kind) {
        case GridNodeKind::Subnet:           s.trace_tick_per_turn = 1; break;
        case GridNodeKind::RegionalDarknet:  s.trace_tick_per_turn = 2; break;
        case GridNodeKind::DeepGridAnchor:   s.trace_tick_per_turn = 3; break;
    }

    // Cache skill flags
    s.skill_intrusion          = player_has_skill(game.player(), SkillId::Intrusion);
    s.skill_icebreaking        = player_has_skill(game.player(), SkillId::IceBreaking);
    s.skill_daemon_mastery     = player_has_skill(game.player(), SkillId::DaemonMastery);
    s.skill_ghost_protocol     = player_has_skill(game.player(), SkillId::GhostProtocol);
    s.skill_deepgrid_navigator = player_has_skill(game.player(), SkillId::DeepGridNavigator);
    s.skill_neural_fortitude   = nf;

    // Resolve sector
    if (node->kind == GridNodeKind::DeepGridAnchor) {
        s.sector = make_consciousness_anchor_sector();
    } else if (node->kind == GridNodeKind::RegionalDarknet) {
        s.sector = gen_regional_sector(node->source_seed, node->security_tier);
    } else {
        s.sector = gen_subnet_sector(node->source_seed, node->security_tier);
    }
    s.sector.source_node = entry_node;
    s.avatar_x = s.sector.spawn_x;
    s.avatar_y = s.sector.spawn_y;

    // Spawn ICE per tier (anchor stays empty in v1)
    if (node->kind != GridNodeKind::DeepGridAnchor) {
        grid_ice::spawn_for_sector(s, node->source_seed, node->security_tier);
    }

    // Body phase-out
    add_effect(game.player().effects, make_grid_invulnerable_ge());

    session_ = std::move(s);
    game.set_state(GameState::Grid);
    game.log("Uploading consciousness... You jack in.");
    return true;
}

void HackingSystem::jack_out(Game& game, JackOutKind kind) {
    if (!session_) return;
    auto& s = *session_;

    switch (kind) {
        case JackOutKind::Voluntary:
            commit_loot_(game, s.loot, /*pct*/100);
            game.log("Disconnect channel complete. You wake at the console.");
            break;
        case JackOutKind::HardJackOut:
            // Spec: hard jack-out costs Trace +10 in-session. Since we
            // tear down the session immediately after, the meaningful
            // penalty is the 50% loot drop and the broadcast log line.
            commit_loot_(game, s.loot, /*pct*/50);
            game.log("Hard jack-out. Your trail blares.");
            break;
        case JackOutKind::NonBlackDeath:
            add_effect(game.player().effects, make_blackice_shock_short_ge());
            game.log("Avatar wiped. You wake disoriented at the console.");
            break;
        case JackOutKind::BlackIceDeath: {
            int bleed = s.skill_neural_fortitude ? 5 : 10;
            game.player().hp -= bleed;
            if (game.player().hp < 0) game.player().hp = 0;
            if (game.player().hp <= 0) {
                remove_effect(game.player().effects, EffectId::GridInvulnerable);
                game.set_death_message("Killed by black ICE in the Grid.");
                game.set_state(GameState::GameOver);
                session_.reset();
                return;
            }
            add_effect(game.player().effects, make_blackice_shock_long_ge());
            game.log("BLACK ICE BLEED-THROUGH. You convulse and slump.");
            break;
        }
        case JackOutKind::SoftDisconnect:
            // Load-time recovery — no penalty, no loot.
            break;
    }

    remove_effect(game.player().effects, EffectId::GridInvulnerable);
    game.set_state(s.body_state);
    session_.reset();
}

void HackingSystem::tick_grid(Game& game) {
    if (!session_) return;
    auto& s = *session_;

    // 1. ICE actions (gray/black approach + attack; white patrols).
    grid_ice::tick_all(s, game);

    // 2. Heat decay on equipped deck + heat→trace coupling + forced reboot.
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
        auto& cd = *(*deck_slot)->deck;
        cyberdeck_decay_heat(cd);

        if (cd.heat_current > kHeatTraceCouplingThreshold) {
            s.trace = std::min(kTraceMax, s.trace + 1);
        }
        if (cyberdeck_overheated(cd)) {
            cyberdeck_force_reboot(cd);
            s.ram = 0;
            s.trace = std::min(kTraceMax, s.trace + kRebootTracePenalty);
            game.log("Deck overheated — forced reboot. RAM lost. Trace +10.");
        }
    }

    // 3. Tier baseline trace tick. Note: still applies on overheated turns.
    s.trace = std::min(kTraceMax, s.trace + s.trace_tick_per_turn);

    // 4. Breakpoint side effects.
    if (s.trace >= kTraceBreakpoint3 && s.trace_alert_pulses < 3) {
        spawn_black_ice_(s);
        s.trace_alert_pulses = 3;
        game.log("BLACK ICE CONVERGING.");
    } else if (s.trace >= kTraceBreakpoint2 && s.trace_alert_pulses < 2) {
        spawn_gray_ice_reinforcement_(s);
        s.trace_alert_pulses = 2;
        game.log("Gray ICE reinforcements detected.");
    } else if (s.trace >= kTraceBreakpoint1 && s.trace_alert_pulses < 1) {
        s.trace_alert_pulses = 1;
        game.log("Alert: Trace at 50%.");
    }

    // 5. Avatar HP zero check.
    if (s.avatar_hp <= 0) {
        JackOutKind kind = (s.last_killer_color == IceColor::Black)
                         ? JackOutKind::BlackIceDeath
                         : JackOutKind::NonBlackDeath;
        jack_out(game, kind);
    }
}

} // namespace astra
