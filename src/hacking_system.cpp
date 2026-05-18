#include "astra/hacking_system.h"
#include "astra/net_voice.h"
#include "astra/net_window_anim.h"

#include "astra/tilemap.h"
#include "astra/cyberdeck.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/net_combat.h"
#include "astra/net_display.h"
#include "astra/net_ice.h"
#include "astra/hackable.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/lan.h"
#include "astra/netspace_generator.h"
#include "astra/npc.h"
#include "astra/program.h"
#include "astra/program_effects.h"
#include "astra/skill_defs.h"
#include "astra/visibility_map.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace astra {

// Process-lifetime flag: set true once the first Opening sequence completes.
// Enables the skip-held fast-forward on subsequent jack-ins.
static bool s_seen_ritual = false;

// No-op audio hook. Replace with real engine call when audio lands.
static void play_sound_hook(const char* /*tag*/) {
    // TODO(audio): trigger sound by tag
}

bool HackingSystem::in_blocking_transition() const {
    if (!session_) return false;
    switch (session_->window_seq.kind) {
        case WindowSeqKind::Opening:
        case WindowSeqKind::ClosingNormal:
        case WindowSeqKind::ClosingPanic:
        case WindowSeqKind::ForcedHold:
        case WindowSeqKind::BlackIceTakeover:
            return true;
        default:
            return false;
    }
}

bool HackingSystem::consume_panic_meat_glitch() {
    bool v = panic_meat_glitch_;
    panic_meat_glitch_ = false;
    return v;
}

void HackingSystem::notify_sequence_finished(WindowSeqKind k) {
    finished_seq_ = k;
}

bool HackingSystem::has_seen_ritual() const {
    return s_seen_ritual;
}

void HackingSystem::on_window_sequence_complete(Game& game) {
    if (!session_) return;
    WindowSeqKind k = finished_seq_;
    finished_seq_ = WindowSeqKind::None;
    if (k == WindowSeqKind::None) return;
    auto& s = *session_;
    if (k == WindowSeqKind::Opening) {
        s_seen_ritual = true;
        bool black = false;
        for (auto& i : s.ice) if (i.color == IceColor::Black) { black = true; break; }
        s.netspace.window_state = window_band(s.trace, WindowState::Stable, black);
        play_sound_hook("jack_in_arrive");
        return;
    }
    if (k == WindowSeqKind::ClosingNormal || k == WindowSeqKind::ClosingPanic
        || k == WindowSeqKind::ForcedHold) {
        if (k == WindowSeqKind::ClosingPanic) panic_meat_glitch_ = true;
        finalize_jack_out_(game, pending_jack_out_);
        return;
    }
    if (k == WindowSeqKind::BlackIceTakeover) {
        bool black = false;
        for (auto& i : s.ice) if (i.color == IceColor::Black) { black = true; break; }
        s.netspace.window_state = window_band(s.trace, WindowState::Hunted, black);
        return;
    }
}

void HackingSystem::request_takeover() {
    if (!session_) return;
    auto& s = *session_;
    if (in_blocking_transition()) return;       // don't stomp a jack-out/closing/opening
    s.netspace.window_state = WindowState::BlackIceTakeover;
    s.window_seq = WindowSequence{};
    s.window_seq.kind = WindowSeqKind::BlackIceTakeover;
    play_sound_hook("black_ice_takeover");
}

namespace {
constexpr int kDetectionDecayInterval = 5;   // tick every N world steps, -1 to value
constexpr int kDetectionMax = 100;
constexpr int kDetectionMin = 0;

// Grid-side tuning constants (see docs/design/mechanics.md).
// kTraceMax lives in grid_constants.h; everything below is local to this TU.
constexpr int kHeatTraceCouplingThreshold = 5;   // +1 Trace/turn while heat exceeds this
constexpr int kRebootTracePenalty         = 10;
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
                t.name = tag_summary(fd.cyber->tags);
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

void HackingSystem::register_sustain(const CompiledProgram& prog, int tx, int ty) {
    ActiveSustain s;
    s.program          = prog;
    s.turns_remaining  = prog.resolved.loop_count;
    s.target_x         = tx;
    s.target_y         = ty;
    s.ram_held         = prog.ram_held;
    sustains_.push_back(std::move(s));
}

void HackingSystem::tick(Game& game) {
    // ── Cyberdeck per-turn: heat decay + LOOP sustain ticker ─────────────
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
        auto& deck = *(*deck_slot)->deck;
        int cool_bonus = session_ ? session_->cooling_rate_bonus : 0;
        int cap_bonus  = session_ ? session_->heat_cap_bonus     : 0;
        cyberdeck_decay_heat(deck, cool_bonus);

        if (cyberdeck_overheated(deck, cap_bonus)) {
            // Force reboot — RAM is wiped, all sustains lose their RAM
            // reservation (it's gone with the reboot) and are dropped.
            // Apply a 1-turn DeckRebooting effect so no programs can fire
            // while the deck cycles back up.
            std::string deck_name = (*deck_slot)->name;
            cyberdeck_force_reboot(deck);
            sustains_.clear();
            add_effect(game.player().effects, make_deck_rebooting_ge());
            game.log(colored(deck_name, Color::Cyan)
                   + " overheated \xe2\x80\x94 forced reboot. "
                   + colored("RAM lost", Color::Red) + ".");
        } else {
            // Tick sustains: re-fire the body at scaled intensity, decrement
            // counter; release RAM + drop when expired.
            for (auto it = sustains_.begin(); it != sustains_.end(); ) {
                if (it->turns_remaining > 0) {
                    EffectSpec scaled = it->program.resolved;
                    float k = scaled.loop_intensity_mult;
                    scaled.damage          = static_cast<int>(scaled.damage * k + 0.5f);
                    scaled.status_duration = static_cast<int>(scaled.status_duration * k + 0.5f);
                    apply_effect_at(game, scaled, it->target_x, it->target_y);
                    --it->turns_remaining;
                    ++it;
                } else {
                    // Release reserved RAM.
                    deck.ram_current = std::min(deck.stats.ram_max,
                                                deck.ram_current + it->ram_held);
                    if (it->ram_held > 0) {
                        game.log("Loop ended: "
                               + colored(it->program.name, Color::Cyan)
                               + " (released "
                               + colored(std::to_string(it->ram_held), Color::Green)
                               + " RAM).");
                    }
                    it = sustains_.erase(it);
                }
            }
        }
    } else {
        // No deck (e.g., unequipped mid-run) — drop any orphan sustains.
        sustains_.clear();
    }

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
                if (slot_is_empty(slot)) continue;
                // Player-compiled programs: always offered (targeting filter
                // is the Telegraph's job at fire time).
                if (slot.compiled.has_value()) {
                    menu_slots.push_back(i);
                    continue;
                }
                // Legacy programs go through the ProgramDef tag filter.
                Item probe = build_by_def_id(slot.program_def_id);
                if (probe.compiled_program.has_value()) {
                    // Migrated legacy program — same as player-compiled.
                    menu_slots.push_back(i);
                    continue;
                }
                if (!probe.program) continue;
                const ProgramDef* def = find_program(probe.program->id);
                if (!def || def->kind != ProgramKind::Qh) continue;
                bool match = std::any_of(def->target_filter.begin(),
                                         def->target_filter.end(),
                                         [&](TagSet req){ return covers(t.hack->tags, req); });
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
                          [&](TagSet req){ return covers(target.tags, req); });
    if (!ok) {
        return std::string("Program rejects ") + tag_summary(target.tags) + ".";
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

void HackingSystem::commit_loot_(Game& game, NetLootBuffer& loot, int pct) {
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
    loot = NetLootBuffer{};
}

namespace {
// Place an ICE actor in the sector, far from the avatar if possible.
bool place_ice_far(NetSession& s, IceColor color, int hp,
                   uint32_t seed_xor, int min_distance) {
    std::mt19937 rng(seed_xor);
    std::uniform_int_distribution<int> xd(0, s.netspace.w - 1);
    std::uniform_int_distribution<int> yd(0, s.netspace.h - 1);
    for (int tries = 0; tries < 96; ++tries) {
        int x = xd(rng);
        int y = yd(rng);
        if (!s.netspace.passable(x, y)) continue;
        int d = std::abs(x - s.avatar_x) + std::abs(y - s.avatar_y);
        if (d < min_distance) continue;
        bool occupied = false;
        for (auto& i : s.ice) if (i.x == x && i.y == y) { occupied = true; break; }
        if (occupied) continue;
        Ice ice;
        ice.x = x; ice.y = y;
        ice.color = color;
        ice.hp = hp;
        s.ice.push_back(ice);
        return true;
    }
    return false;
}
}

void HackingSystem::spawn_black_ice_(NetSession& s) {
    place_ice_far(s, IceColor::Black, /*hp*/4, /*seed*/0xB1ACC1CEu, /*min_distance*/4);
}

void HackingSystem::spawn_gray_ice_reinforcement_(NetSession& s) {
    place_ice_far(s, IceColor::Gray, /*hp*/2, /*seed*/0xC9A41CEu, /*min_distance*/3);
}

bool HackingSystem::jack_in(Game& game, TargetDescriptor desc) {
    if (session_) {
        game.log("Already jacked in.");
        return false;
    }
    if (!game.player().has_implant_of_type(ItemType::RelayCortex)) {
        game.log("You have no neural interface. Install a " + colored("Relay Cortex", Color::Cyan) + ".");
        return false;
    }
    if (has_effect(game.player().effects, EffectId::BlackIceShock)) {
        game.log("Your body is still convulsing — neural link refuses to bind.");
        return false;
    }
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    const CyberdeckData* cd_ptr = (deck_slot && *deck_slot && (*deck_slot)->deck)
                                    ? &(*(*deck_slot)->deck)
                                    : nullptr;

    NetSession s;
    s.body_x       = game.player().x;
    s.body_y       = game.player().y;
    s.body_state   = GameState::Playing;

    bool nf = player_has_skill(game.player(), SkillId::NeuralFortitude);
    s.avatar_hp_max = 3 + (nf ? 1 : 0);
    s.avatar_hp     = s.avatar_hp_max;

    {
        auto im = game.player().implant_modifiers();
        int deck_ram = cd_ptr ? cd_ptr->stats.ram_max : 0;
        int deck_cur = cd_ptr ? cd_ptr->ram_current   : 0;
        s.ram_max = deck_ram + im.ram_cap_bonus;
        s.ram     = deck_cur + im.ram_cap_bonus;
        if (s.ram > s.ram_max) s.ram = s.ram_max;
        s.heat_cap_bonus       = im.heat_cap_bonus;
        s.cooling_rate_bonus   = im.cooling_rate_bonus;
        s.trace_resistance_pct = im.trace_resistance_pct;
    }
    s.skill_intrusion          = player_has_skill(game.player(), SkillId::Intrusion);
    s.skill_icebreaking        = player_has_skill(game.player(), SkillId::IceBreaking);
    s.skill_daemon_mastery     = player_has_skill(game.player(), SkillId::DaemonMastery);
    s.skill_ghost_protocol     = player_has_skill(game.player(), SkillId::GhostProtocol);
    s.skill_deepgrid_navigator = player_has_skill(game.player(), SkillId::DeepGridNavigator);
    s.skill_neural_fortitude   = nf;

    // Dispatch to the per-target grammar. Phase 1 lights up Door
    // (and vending / camera in Steps 7 + 8); unimplemented kinds fall
    // back to the empty stub.
    s.netspace = gen_for_target(desc);

    // Phase 4: grammar-declared trace rate (ATM fast, others default 1).
    s.trace_tick_per_turn = s.netspace.trace_tick_hint > 0
                            ? s.netspace.trace_tick_hint
                            : 1;

    // Phase 4: grammar-declared ICE seeded into the live session.
    for (const auto& ice : s.netspace.initial_ice) s.ice.push_back(ice);

    s.avatar_x = s.netspace.jack_in_x;
    s.avatar_y = s.netspace.jack_in_y;

    // Body phase-out
    add_effect(game.player().effects, make_net_exposed_ge());

    session_ = std::move(s);

    // Jack-in intro lines — runner command-line voice (storyboard frame 1).
    {
        const std::string title = session_->netspace.title.empty()
                                      ? "netspace"
                                      : session_->netspace.title;
        const std::string tier_str = std::to_string(session_->netspace.target.tier);
        session_->push_log(net_voice::cmd("jacked in. topology resolved."));
        session_->push_log(net_voice::cmd(title + ", tier " + tier_str + "."));
        session_->push_log(net_voice::cmd("trace " + std::to_string(session_->trace) + "%. clean entry."));
    }

    session_->meat_clock_base_secs = game.world().world_tick();

    game.set_state(GameState::Net);

    // Start the Opening ritual — the window sequence owns the display
    // until it completes; on_window_sequence_complete then recomputes the band.
    session_->netspace.window_state = WindowState::Opening;
    session_->window_seq = WindowSequence{};
    session_->window_seq.kind = WindowSeqKind::Opening;
    play_sound_hook("jack_in_begin");

    game.log("Uploading consciousness... You jack in.");
    return true;
}

void HackingSystem::jack_out(Game& game, JackOutKind kind) {
    if (!session_) return;
    auto& s = *session_;
    if (in_blocking_transition()) return;   // already tearing down
    pending_jack_out_ = kind;
    switch (kind) {
        case JackOutKind::Voluntary:
            s.netspace.window_state = WindowState::Closing;
            s.window_seq = WindowSequence{};
            s.window_seq.kind = WindowSeqKind::ClosingNormal;
            play_sound_hook("jack_out_normal");
            return;
        case JackOutKind::HardJackOut:
            s.netspace.window_state = WindowState::Closing;
            s.window_seq = WindowSequence{};
            s.window_seq.kind = WindowSeqKind::ClosingPanic;
            play_sound_hook("jack_out_panic");
            return;
        case JackOutKind::NonBlackDeath:
        case JackOutKind::BlackIceDeath:
            s.netspace.window_state = WindowState::Closing;
            s.window_seq = WindowSequence{};
            s.window_seq.kind = WindowSeqKind::ForcedHold;
            play_sound_hook("jack_out_forced");
            return;
        case JackOutKind::SoftDisconnect:
            finalize_jack_out_(game, kind);   // load-time recovery: immediate
            return;
    }
}

void HackingSystem::turret_outcome(Game& game, NetSession& s, bool flip) {
    const int N = 8 + s.netspace.target.tier * 4;
    int tx = s.netspace.target.src_x, ty = s.netspace.target.src_y;
    if (tx < 0 || ty < 0) {
        game.log(flip ? std::string("[dev] Turret would flip to PlayerAllied for ")
                           + std::to_string(N) + " turns."
                       : std::string("[dev] Turret would go inert for ")
                           + std::to_string(N) + " turns.");
        return;
    }
    for (auto& npc : game.world().npcs()) {
        if (npc.x == tx && npc.y == ty && npc.alive()) {
            if (flip) {
                if (npc.pre_hijack_faction.empty())
                    npc.pre_hijack_faction = npc.faction;
                npc.faction = "PlayerAllied";
                add_effect(npc.effects, make_turret_allied_ge(N));
                game.log("The turret swings toward your enemies.");
            } else {
                // No NPC effect: jack-out is instantaneous after this, and a
                // disarmed turret is simply left neutral (no timed state needed
                // in Phase 4 scope).
                game.log("The turret powers down.");
            }
            return;
        }
    }
}

void HackingSystem::finalize_jack_out_(Game& game, JackOutKind kind) {
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
        case JackOutKind::NonBlackDeath: {
            auto im = game.player().implant_modifiers();
            if (!im.blackice_shock_immunity) {
                Effect e = make_blackice_shock_short_ge();
                if (im.blackice_shock_duration_pct != 0) {
                    int adj = e.duration + e.duration * im.blackice_shock_duration_pct / 100;
                    if (adj < 1) adj = 1;
                    e.duration  = adj;
                    e.remaining = adj;
                }
                add_effect(game.player().effects, e);
                game.log("Avatar wiped. You wake disoriented at the console.");
            } else {
                game.log("Avatar wiped — but your Stoic Cortex absorbs the shock.");
            }
            break;
        }
        case JackOutKind::BlackIceDeath: {
            int bleed = s.skill_neural_fortitude ? 5 : 10;
            game.player().hp -= bleed;
            if (game.player().hp < 0) game.player().hp = 0;
            if (game.player().hp <= 0) {
                remove_effect(game.player().effects, EffectId::NetExposed);
                game.set_death_message("Killed by black ICE in the Grid.");
                game.set_state(GameState::GameOver);
                session_.reset();
                return;
            }
            auto im = game.player().implant_modifiers();
            if (!im.blackice_shock_immunity) {
                Effect e = make_blackice_shock_long_ge();
                if (im.blackice_shock_duration_pct != 0) {
                    int adj = e.duration + e.duration * im.blackice_shock_duration_pct / 100;
                    if (adj < 1) adj = 1;
                    e.duration  = adj;
                    e.remaining = adj;
                }
                add_effect(game.player().effects, e);
                game.log("BLACK ICE BLEED-THROUGH. You convulse and slump.");
            } else {
                game.log("Black ICE bleed-through — your Stoic Cortex holds the line.");
            }
            break;
        }
        case JackOutKind::SoftDisconnect:
            // Load-time recovery — no penalty, no loot.
            break;
    }

    remove_effect(game.player().effects, EffectId::NetExposed);
    game.set_state(s.body_state);
    session_.reset();
}

void HackingSystem::tick_grid(Game& game) {
    if (!session_) return;
    auto& s = *session_;
    ++s.net_turn;

    // 1. ICE actions (gray/black approach + attack; white patrols).
    net_ice::tick_all(s, game);
    // 1a. Promote any ICE seeds that became eligible this tick (trace-gated).
    net_ice::promote_pending_seeds(s);

    // 1b. Phase 4: grammar triggers (trace-/turn-gated one-shot spawns).
    for (auto& tr : s.netspace.triggers) {
        if (tr.fired) continue;
        bool hit = (tr.cond == NetTriggerCond::TraceAtLeast)
                       ? (s.trace >= tr.threshold)
                       : (s.net_turn >= static_cast<uint32_t>(tr.threshold));
        if (!hit) continue;
        tr.fired = true;
        std::vector<std::pair<int,int>> cells = tr.spawn.cells;
        if (cells.empty()) {
            // domain-sep so trigger shuffles are independent of other gen RNG
            std::mt19937 rng(s.netspace.target.seed ^ 0x2444u ^ static_cast<uint32_t>(tr.threshold));
            std::vector<std::pair<int,int>> pool;
            for (auto& kv : s.netspace.glyph_overrides)
                if (kv.second == "$") pool.push_back(kv.first);
            std::shuffle(pool.begin(), pool.end(), rng);
            for (int i = 0; i < tr.spawn.count && i < static_cast<int>(pool.size()); ++i)
                cells.push_back(pool[i]);
        }
        int made = 0;
        for (auto& c : cells) {
            if (made >= tr.spawn.count) break;
            bool occ = false;
            for (const auto& existing : s.ice)
                if (existing.x == c.first && existing.y == c.second) { occ = true; break; }
            if (occ) continue;
            Ice ice; ice.x = c.first; ice.y = c.second;
            ice.color = tr.spawn.color; ice.hp = tr.spawn.hp;
            s.ice.push_back(ice);
            ++made;
        }
        if (made > 0)
            s.push_log(">> Hostile process spawned (" + std::to_string(made) + ").");
    }

    // 1c. DaemonHijack countdown. tick_all already cleared the handle if the
    // puppet died this tick; here we just count down on a still-live hijack.
    if (s.hijacked_ice_idx >= 0 && s.hijacked_turns_left > 0) {
        --s.hijacked_turns_left;
        if (s.hijacked_turns_left == 0) {
            s.hijacked_ice_idx = -1;
            s.push_log(">> " + display_name(ProgramId::DaemonHijack) + ": control released.");
        }
    }

    // Phase 5 slice 3a: advance in-flight programs. Resolved AFTER ICE
    // act (combat.md turn order: enemies advance, then player programs
    // that complete this turn resolve) and AFTER the hijack countdown
    // (1c) so a DaemonHijack that completes this turn isn't decremented
    // on the same tick it's set. Each entry occupies its deck slot +
    // reserves RAM until it completes; the bespoke effect resolves when
    // the countdown hits 0, then the reserved RAM returns (not on cancel).
    for (auto it = s.in_flight.begin(); it != s.in_flight.end(); ) {
        if (it->compiled) {
            // Per-iteration: apply the EffectSpec once each turn for the
            // program's loop/tick duration. Flat damage per iteration
            // (loop_intensity_mult falloff = documented tuning deferral).
            std::string msg = apply_effect_in_net(game, s, it->spec,
                                                  it->target_x, it->target_y);
            if (!msg.empty())
                s.push_log("  " + it->prog_name + ": " + msg);
            if (--it->turns_left > 0) { ++it; continue; }
            s.ram = std::min(s.ram_max, s.ram + it->ram_held);
            it = s.in_flight.erase(it);
        } else {
            if (--it->turns_left > 0) { ++it; continue; }
            ProgramId pid = static_cast<ProgramId>(it->program_id);
            NetProgramContext ctx{game, s, it->target_x, it->target_y};
            std::string msg = apply_program_in_grid(pid, ctx);
            if (!msg.empty()) s.push_log(std::string("  ") + msg);
            s.ram = std::min(s.ram_max, s.ram + it->ram_held);
            it = s.in_flight.erase(it);
        }
    }

    // 2. Heat decay on equipped deck + heat→trace coupling + forced reboot.
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
        auto& cd = *(*deck_slot)->deck;
        cyberdeck_decay_heat(cd, s.cooling_rate_bonus);

        if (cd.heat_current > kHeatTraceCouplingThreshold) {
            s.gain_trace(1);
        }
        if (cyberdeck_overheated(cd, s.heat_cap_bonus)) {
            cyberdeck_force_reboot(cd);
            s.ram = 0;
            s.gain_trace(kRebootTracePenalty);
            s.push_log("[WARN] Deck overheated — forced reboot. RAM lost. Trace +10.");
        }
    }

    // 3. Tier baseline trace tick. Note: still applies on overheated turns.
    // Accumulate into trace_carry; emit +1 Trace per 2 carry units. This halves
    // the effective baseline (subnet 1→0.5/turn, regional 2→1/turn,
    // anchor 3→1.5/turn) while preserving the tier hierarchy.
    s.trace_carry += s.trace_tick_per_turn;
    if (s.trace_carry >= 2) {
        int delta = s.trace_carry / 2;
        s.trace_carry %= 2;
        s.gain_trace(delta);
    }

    // 4. Breakpoint side effects.
    if (s.trace >= kTraceBreakpoint3 && s.trace_alert_pulses < 3) {
        spawn_black_ice_(s);
        s.trace_alert_pulses = 3;
        s.push_log(">> " + display_name(IceColor::Black) + " CONVERGING.");
    } else if (s.trace >= kTraceBreakpoint2 && s.trace_alert_pulses < 2) {
        spawn_gray_ice_reinforcement_(s);
        s.trace_alert_pulses = 2;
        s.push_log(">> " + display_name(IceColor::Gray) + " reinforcements detected.");
    } else if (s.trace >= kTraceBreakpoint1 && s.trace_alert_pulses < 1) {
        s.trace_alert_pulses = 1;
        s.push_log("[WARN] Trace at 50%.");
    }

    // 5. Avatar HP zero check.
    if (s.avatar_hp <= 0) {
        JackOutKind kind = (s.last_killer_color == IceColor::Black)
                         ? JackOutKind::BlackIceDeath
                         : JackOutKind::NonBlackDeath;
        jack_out(game, kind);
        return;   // session_ may be reset by jack_out; s is now dangling.
                  // Nothing further this tick (band recompute below skipped).
    }

    // 6. Window-state band (skip while a transient sequence owns the window).
    if (!in_blocking_transition()) {
        bool black_present = false;
        for (const auto& i : s.ice)
            if (i.color == IceColor::Black) { black_present = true; break; }
        s.netspace.window_state =
            window_band(s.trace, s.netspace.window_state, black_present);
    }
}

} // namespace astra
