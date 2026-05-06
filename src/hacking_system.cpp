#include "astra/hacking_system.h"

#include "astra/anchor.h"
#include "astra/tilemap.h"
#include "astra/consciousness_save.h"
#include "astra/cyberdeck.h"
#include "astra/deep_grid_sector.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/grid_constants.h"
#include "astra/grid_display.h"
#include "astra/grid_ice.h"
#include "astra/grid_sector.h"
#include "astra/hackable.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/lan.h"
#include "astra/lan_sector_generator.h"
#include "astra/npc.h"
#include "astra/program.h"
#include "astra/program_effects.h"   // Task 9 will populate; Task 7 ships a stub
#include "astra/sector_runtime_state.h"
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

// Plan 7: forward-declared anchors for the per-cmd TU registrations. Each
// `cmd_*.cpp` defines a static initializer that registers HackCommands into
// the global registry; calling these anchors from a TU we know is always
// linked guarantees the registrations survive whole-program-optimization.
void register_universal_hack_commands();

namespace {
struct HackCommandAnchor {
    HackCommandAnchor() {
        register_universal_hack_commands();
    }
};
const HackCommandAnchor k_hack_command_anchor;
}

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
    // Plan 7: device shell rides world ticks for its long-channel progress.
    // Ritual char-streaming uses the frame-tick, driven from Game's idle path.
    if (auto* dev = device_shell()) {
        dev->tick_world(game);
    }

    // Plan 7 Phase B — decrement Hackable runtime countdowns (optics_blind,
    // disarmed, surge/kill/dim/halt, etc.). Per spec §13 these fields are
    // ephemeral and reset on save/load; per-world-tick decrement keeps them
    // tracking the world clock.
    {
        auto& m = game.world().map();
        for (int i = 0; i < m.fixture_count(); ++i) {
            auto& fd = m.fixture_mut(i);
            if (fd.cyber) tick_runtime_state(*fd.cyber, 1);
        }
        for (auto& npc : game.world().npcs()) {
            if (npc.cyber) tick_runtime_state(*npc.cyber, 1);
        }
    }

    // Plan 8 B4: mirror Anchor positions to follow NPC RW movement.
    // Runs every in-Grid world tick (tick_real_world calls hacking_.tick).
    if (session_) {
        AnchorProjection proj = make_anchor_projection(session_->sector, game.world());
        auto& npcs = game.world().npcs();
        for (size_t i = 0; i < npcs.size(); ++i) {
            Npc& npc = npcs[i];
            if (!npc.alive()) continue;
            if (npc.anchor_id < 0) continue;

            Anchor* a = session_->anchor_for_npc(static_cast<int>(i));
            if (!a) continue;
            if (a->severed()) continue;  // dead anchors don't move

            int nx, ny;
            project_rw_to_site(proj, npc.x, npc.y, nx, ny);

            // If the projected tile is unwalkable in the Site, fall back to the
            // nearest walkable cell (Chebyshev expansion, max radius 4).
            if (!session_->sector.passable(nx, ny)) {
                int best_dx = 0, best_dy = 0;
                int best_d = 1 << 30;
                for (int rad = 1; rad <= 4 && best_d == (1 << 30); ++rad) {
                    for (int dy = -rad; dy <= rad; ++dy) {
                        for (int dx = -rad; dx <= rad; ++dx) {
                            if (std::max(std::abs(dx), std::abs(dy)) != rad) continue;
                            int tx = nx + dx, ty = ny + dy;
                            if (!session_->sector.passable(tx, ty)) continue;
                            int d = std::abs(dx) + std::abs(dy);
                            if (d < best_d) {
                                best_d = d;
                                best_dx = dx;
                                best_dy = dy;
                            }
                        }
                    }
                }
                if (best_d == (1 << 30)) continue;  // no walkable neighbour — leave anchor in place
                nx += best_dx;
                ny += best_dy;
            }

            a->x = nx;
            a->y = ny;
        }
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

bool HackingSystem::open_device_shell(Game& game, Hackable& target,
                                      ShellTier requested_tier, ShellVia via,
                                      bool manual_ssh,
                                      const std::string& requested_user) {
    // Manual ssh strict-reject: root@locked-unescalated → no shell, just the
    // permission-denied beat. Plan 7 §4.
    bool locked = has_tag(target.tags, HackTag::Locked);
    bool wants_root = (requested_tier == ShellTier::Root);
    if (manual_ssh && wants_root && locked && !target.escalated) {
        // Caller is responsible for printing the message into pda> shell.
        return false;
    }

    // Floor the actual tier against device state. Locked-unescalated capped
    // to Guest, regardless of what user the player typed.
    ShellTier actual_tier = requested_tier;
    if (locked && !target.escalated) actual_tier = ShellTier::Guest;

    // Resolve faction for flavor pack: use the current star system's
    // controlling_faction. Empty / unknown -> Civilian fallback (handled
    // inside flavor_for).
    std::string faction;
    {
        const auto& nav = game.world().navigation();
        for (const auto& sys : nav.systems) {
            if (sys.id == nav.current_system_id) {
                faction = sys.controlling_faction;
                break;
            }
        }
    }

    // If a device shell is already on top, close it first (one shell at a
    // time per spec). This shouldn't happen in normal flow.
    if (device_shell()) {
        if (shell_sink_) shell_stack_.pop(*shell_sink_, game);
    }

    auto dev = std::make_unique<DeviceShell>();
    dev->bind_sink(shell_sink_);
    dev->set_faction(std::move(faction));
    dev->open(game, &target, actual_tier, via, requested_user);
    // shell_sink_ should always be bound by Game construction. If it isn't
    // (defensive — e.g. tests construct HackingSystem standalone), we drop
    // the lifecycle-hook benefits but still push so the stack tracks the
    // session. on_push for DeviceShell is empty (open() did the banner).
    if (shell_sink_) {
        shell_stack_.push(std::move(dev), *shell_sink_, game);
    } else {
        // Push without firing on_push — direct stack mutation.
        // (No public setter; this branch is unreachable in normal builds.)
        // Fall back to constructing a no-op sink locally.
        struct NullSink : ShellOutputSink {
            void shell_emit_line(const std::string&, UITag) override {}
            void shell_clear_scroll() override {}
            void shell_set_progress_line(const std::string&, UITag) override {}
            void shell_commit_progress_line() override {}
        };
        static NullSink null_sink;
        shell_stack_.push(std::move(dev), null_sink, game);
    }
    return true;
}

void HackingSystem::close_device_shell(Game& game) {
    if (!device_shell()) return;
    // pop() invokes on_pop, which calls DeviceShell::close — emits the
    // logout pair and yanks the cable. Then the unique_ptr is destroyed.
    if (shell_sink_) {
        shell_stack_.pop(*shell_sink_, game);
    } else {
        struct NullSink : ShellOutputSink {
            void shell_emit_line(const std::string&, UITag) override {}
            void shell_clear_scroll() override {}
            void shell_set_progress_line(const std::string&, UITag) override {}
            void shell_commit_progress_line() override {}
        };
        static NullSink null_sink;
        shell_stack_.pop(null_sink, game);
    }
}

void HackingSystem::reset() {
    targeting_ = false;
    target_x_ = 0;
    target_y_ = 0;
    blink_phase_ = 0;
    // Plan 7: device contexts do NOT survive new_game / load_save.
    // CyberdeckShellContext (when added) will be re-pushed on first PDA open
    // with a deck equipped.
    shell_stack_.force_clear();
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

namespace {
// Place an ICE actor in the sector, far from the avatar if possible.
bool place_ice_far(GridSession& s, IceColor color, int hp,
                   uint32_t seed_xor, int min_distance) {
    std::mt19937 rng(static_cast<uint32_t>(s.entry_node.value) ^ seed_xor);
    std::uniform_int_distribution<int> xd(0, s.sector.w - 1);
    std::uniform_int_distribution<int> yd(0, s.sector.h - 1);
    for (int tries = 0; tries < 96; ++tries) {
        int x = xd(rng);
        int y = yd(rng);
        if (!s.sector.passable(x, y)) continue;
        int d = std::abs(x - s.avatar_x) + std::abs(y - s.avatar_y);
        if (d < min_distance) continue;
        bool occupied = false;
        for (auto& i : s.ice) if (i.x == x && i.y == y) { occupied = true; break; }
        if (occupied) continue;
        GridIce ice;
        ice.x = x; ice.y = y;
        ice.color = color;
        ice.hp = hp;
        s.ice.push_back(ice);
        return true;
    }
    return false;
}
}

void HackingSystem::spawn_black_ice_(GridSession& s) {
    place_ice_far(s, IceColor::Black, /*hp*/4, /*seed*/0xB1ACC1CEu, /*min_distance*/4);
}

void HackingSystem::spawn_gray_ice_reinforcement_(GridSession& s) {
    place_ice_far(s, IceColor::Gray, /*hp*/2, /*seed*/0xC9A41CEu, /*min_distance*/3);
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
    // Post-Grid-death shock locks the player out until the GE expires.
    // Both the short Disoriented (NonBlackDeath) and long Convulsing
    // (BlackIceDeath) shocks share EffectId::BlackIceShock.
    if (has_effect(game.player().effects, EffectId::BlackIceShock)) {
        game.log("Your body is still convulsing — neural link refuses to bind.");
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

    // Follow a single redirect hop (per-Precursor Subnets point at their
    // regional darknet so the netmap and fixture-menu routes land you in
    // the same sector). One hop only — we don't chase chains.
    if (node->entry_redirect.valid()) {
        if (auto* redirect = net.find(node->entry_redirect)) {
            node = redirect;
            entry_node = redirect->id;
        }
    }

    GridSession s;
    s.entry_node   = entry_node;
    s.current_node = entry_node;
    s.body_x       = game.player().x;
    s.body_y       = game.player().y;
    s.body_state   = GameState::Playing;

    // Diegetic flow for fixture-menu Jack In: the player plugs their cable
    // into a specific device (Console, ShipTerminal, etc.), so they land in
    // THAT device's Subnet sector. To preserve the "back to the LAN" path,
    // pre-set return_node to the Subnet's owning LAN root — so when the
    // player walks onto the subnet's ⊙ ExitNode, on_step's bounce-back
    // logic traverses to the LAN sector instead of jacking out to the world.
    const auto& meta = game.world().lan_metadata();
    if (node->kind == GridNodeKind::Subnet) {
        if (meta.lan_root.valid()) {
            s.return_node = meta.lan_root;
        }
    }

    // Avatar HP from skill+deck. NeuralFortitude raises max by 1.
    bool nf = player_has_skill(game.player(), SkillId::NeuralFortitude);
    s.avatar_hp_max = 3 + (nf ? 1 : 0);
    s.avatar_hp     = s.avatar_hp_max;

    s.ram_max = cd.stats.ram_max;
    s.ram     = cd.ram_current;

    // Tier-driven Trace tick
    switch (node->kind) {
        case GridNodeKind::Subnet:           s.trace_tick_per_turn = 1; break;
        case GridNodeKind::LanRoot:          s.trace_tick_per_turn = 2; break;
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
    resolve_sector_for_(game, s, *node);
    // Plan 8: for Subnet jack-ins, source_node is the LAN root (the sector's
    // identity), not the target Subnet. The Subnet target lives in s.current_node.
    s.sector.source_node =
        (node->kind == GridNodeKind::Subnet) ? meta.lan_root : node->id;
    s.avatar_x = s.sector.spawn_x;
    s.avatar_y = s.sector.spawn_y;

    // Spawn ICE per tier (anchor stays empty; all other node kinds — including
    // LanRoot v2 sectors — consume ice_seeds seeded at generation time).
    // Persisted killed-ICE coordinates suppress respawn on re-entry.
    if (node->kind != GridNodeKind::DeepGridAnchor) {
        // Plan 8 Cut 5: v2 sectors populate ice_seeds at generation time;
        // use them when present. Fall back to v1 scatter spawn.
        if (!s.sector.ice_seeds.empty()) {
            grid_ice::spawn_from_seeds(s);
        } else {
            grid_ice::spawn_for_sector(s, node->source_seed, node->security_tier);
        }
        // Drop any ICE that the player previously killed in this sector.
        // Plan 8: Subnet jacks now load the LAN sector, so both LanRoot and
        // Subnet kinds share the same mutation bucket (lan_sector_state).
        const SectorRuntimeState* state = nullptr;
        if (node->kind == GridNodeKind::Subnet || node->kind == GridNodeKind::LanRoot) {
            state = &meta.lan_sector_state;
        }
        if (state && !state->killed_ice.empty()) {
            s.ice.erase(std::remove_if(s.ice.begin(), s.ice.end(),
                [&](const GridIce& ice) {
                    for (const auto& k : state->killed_ice) {
                        if (k.first == ice.x && k.second == ice.y) return true;
                    }
                    return false;
                }), s.ice.end());
        }
    }

    // Spec 1: spawn an Anchor per hostile, Crystal-bearing NPC on the
    // current map. Each NPC's anchor_id is set; the Anchor's Site
    // coordinates mirror the NPC's RW position via linear projection.
    // D2: also spawn Anchors for Bind-marked NPCs (force_bind == true)
    // even if they carry no native Electronic Crystal.
    {
        AnchorProjection proj = make_anchor_projection(s.sector, game.world());
        auto& npcs = game.world().npcs();
        for (size_t i = 0; i < npcs.size(); ++i) {
            Npc& npc = npcs[i];
            if (!npc.alive()) continue;
            if (!is_hostile_to_player(npc.faction, game.player())) continue;

            bool has_native_crystal = npc.cyber && has_tag(npc.cyber->tags, HackTag::Electronic);
            bool bound_target       = npc.force_bind;
            if (!has_native_crystal && !bound_target) continue;

            int sx, sy;
            project_rw_to_site(proj, npc.x, npc.y, sx, sy);
            Anchor* a = s.add_anchor_for_npc(
                static_cast<int>(i),
                sx, sy,
                npc.level,   // npc.level used as threat-tier proxy (B3)
                /*bound=*/bound_target);
            npc.anchor_id = a->id;
        }
    }

    // Body phase-out
    add_effect(game.player().effects, make_grid_exposed_ge());

    session_ = std::move(s);
    game.set_state(GameState::Grid);
    game.log("Uploading consciousness... You jack in.");
    return true;
}

// Spec 1: inject a pre-built GridSession (Imprint sector). Bypasses the
// network-node lookup; preconditions checked by caller (walk_imprint).
void HackingSystem::inject_imprint_session(Game& game, GridSession s) {
    // Spawn ICE from seeds if the generator populated them.
    if (!s.sector.ice_seeds.empty()) {
        grid_ice::spawn_from_seeds(s);
    }
    // Body phase-out effect (same as normal jack-in).
    add_effect(game.player().effects, make_grid_exposed_ge());
    session_ = std::move(s);
    game.set_state(GameState::Grid);
}

void HackingSystem::resolve_sector_for_(Game& game, GridSession& s,
                                        const GridNode& node) {
    const auto& meta = game.world().lan_metadata();
    switch (node.kind) {
        case GridNodeKind::LanRoot: {
            s.sector = generate_lan_sector_v2(meta, game.world().grid_network(), game.world());
            apply_mutations(s.sector, meta.lan_sector_state);
            break;
        }
        case GridNodeKind::DeepGridAnchor: {
            // Owned-anchor dispatch: if this DeepGridAnchor belongs to the
            // current consciousness AND a customised base has been written
            // (ConsciousnessAnchor capstone taken + edits made), serve the
            // saved sector with runtime overlay. Otherwise serve the
            // canonical 60×40 hand-authored hub (Anchor + Atlas + Frontier).
            ConsciousnessSave cs;
            const bool have = read_consciousness(cs);
            if (have &&
                node.owned_by_consciousness_id == cs.consciousness_id &&
                cs.consciousness_id != 0 &&
                cs.deep_grid_base.w > 0)
            {
                s.sector = cs.deep_grid_base;
                apply_mutations(s.sector, cs.deep_grid_sector_state);
            } else {
                s.sector = make_player_deep_grid_base();
            }
            break;
        }
        case GridNodeKind::RegionalDarknet: {
            s.sector = gen_regional_sector(node.source_seed, node.security_tier);
            break;
        }
        case GridNodeKind::Subnet: {
            // Plan 8 flat-model: jacking into a Subnet loads the LAN sector
            // and spawns at the Subnet's per_node_spawn cell within it.
            if (!meta.lan_root.valid()) {
                game.log("[ERR] resolve_sector_for_: Subnet has no LAN root — empty sector.");
                s.sector = GridSector{};
                break;
            }
            s.sector = generate_lan_sector_v2(meta, game.world().grid_network(), game.world());
            apply_mutations(s.sector, meta.lan_sector_state);

            // Override spawn to land inside the target Subnet's room.
            auto it = s.sector.per_node_spawn.find(node.id);
            if (it != s.sector.per_node_spawn.end()) {
                s.sector.spawn_x = it->second.first;
                s.sector.spawn_y = it->second.second;
            }
            // Otherwise fall through to the generator's default lobby spawn — defensive.
            break;
        }
        default: {
            // Truly unexpected — log and serve empty.
            game.log("[ERR] resolve_sector_for_: unexpected node kind. Serving empty sector.");
            s.sector = GridSector{};
            break;
        }
    }
}

bool HackingSystem::traverse_to(Game& game, GridNodeId target_id) {
    if (!session_) return false;
    GridSession& s = *session_;
    auto& net = game.world().grid_network();
    const auto* node = net.find(target_id);
    if (!node) {
        game.log("traverse: unknown node.");
        return false;
    }

    GridNodeId prev = s.current_node;

    // Plan 8 flat-model: Subnet traversal within the current LAN sector
    // is a teleport, not a sector regen. ICE, mutations, and content
    // already live in the sector — only the avatar position + current_node
    // change.
    if (node->kind == GridNodeKind::Subnet) {
        auto it = s.sector.per_node_spawn.find(target_id);
        if (it != s.sector.per_node_spawn.end()) {
            s.avatar_x           = it->second.first;
            s.avatar_y           = it->second.second;
            s.current_node       = target_id;
            s.return_node        = prev;
            s.trace_tick_per_turn = 1;  // Subnet tier
            return true;
        }
        // No per_node_spawn entry — must be cross-LAN or a v1 sector. Fall
        // through to the v1 sector-regen path.
    }

    // Existing v1 logic — sector regen for LanRoot, DeepGridAnchor,
    // RegionalDarknet, and v1 Subnet fallback.
    resolve_sector_for_(game, s, *node);
    s.sector.source_node = node->id;
    s.current_node       = target_id;
    s.return_node        = prev;
    s.avatar_x           = s.sector.spawn_x;
    s.avatar_y           = s.sector.spawn_y;

    // Re-spawn ICE for the new sector; only the hand-authored Anchor stays
    // empty. LanRoot v2 sectors now consume ice_seeds just like Subnets.
    s.ice.clear();
    if (node->kind != GridNodeKind::DeepGridAnchor) {
        // Plan 8 Cut 5: v2 sectors populate ice_seeds at generation time;
        // use them when present. Fall back to v1 scatter spawn.
        if (!s.sector.ice_seeds.empty()) {
            grid_ice::spawn_from_seeds(s);
        } else {
            grid_ice::spawn_for_sector(s, node->source_seed, node->security_tier);
        }
        const auto& meta = game.world().lan_metadata();
        // Plan 8: Subnet traversal (v1 fallback) loads the LAN sector, so
        // both LanRoot and Subnet kinds share lan_sector_state.
        const SectorRuntimeState* state = nullptr;
        if (node->kind == GridNodeKind::Subnet || node->kind == GridNodeKind::LanRoot) {
            state = &meta.lan_sector_state;
        }
        if (state && !state->killed_ice.empty()) {
            s.ice.erase(std::remove_if(s.ice.begin(), s.ice.end(),
                [&](const GridIce& ice) {
                    for (const auto& k : state->killed_ice) {
                        if (k.first == ice.x && k.second == ice.y) return true;
                    }
                    return false;
                }), s.ice.end());
        }
    }

    // Tier-driven trace tick mirrors jack_in's switch.
    switch (node->kind) {
        case GridNodeKind::Subnet:           s.trace_tick_per_turn = 1; break;
        case GridNodeKind::LanRoot:          s.trace_tick_per_turn = 2; break;
        case GridNodeKind::RegionalDarknet:  s.trace_tick_per_turn = 2; break;
        case GridNodeKind::DeepGridAnchor:   s.trace_tick_per_turn = 3; break;
    }
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
                remove_effect(game.player().effects, EffectId::GridExposed);
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

    // Spec 1: mark the source corpse exhausted when leaving a transient
    // Imprint sector, regardless of jack-out kind (voluntary, death, etc.).
    if (s.is_imprint_transient && s.corpse_fid >= 0) {
        auto& map = game.world().map();
        if (s.corpse_fid < static_cast<int>(map.fixtures_vec().size())) {
            FixtureData& corpse_fd = map.fixture_mut(s.corpse_fid);
            if (corpse_fd.cyber) corpse_fd.cyber->corpse_imprint_exhausted = true;
        }
    }

    remove_effect(game.player().effects, EffectId::GridExposed);
    game.set_state(s.body_state);
    session_.reset();
}

void HackingSystem::tick_grid(Game& game) {
    if (!session_) return;
    auto& s = *session_;

    // 1. ICE actions (gray/black approach + attack; white patrols).
    grid_ice::tick_all(s, game);
    // 1a. Promote any ICE seeds that became eligible this tick (trace-gated).
    grid_ice::promote_pending_seeds(s);

    // 1b. DaemonHijack countdown. tick_all already cleared the handle if the
    // puppet died this tick; here we just count down on a still-live hijack.
    if (s.hijacked_ice_idx >= 0 && s.hijacked_turns_left > 0) {
        --s.hijacked_turns_left;
        if (s.hijacked_turns_left == 0) {
            s.hijacked_ice_idx = -1;
            s.push_log(">> " + display_name(ProgramId::DaemonHijack) + ": control released.");
        }
    }

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
        s.trace = std::min(kTraceMax, s.trace + delta);
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
    }
}

} // namespace astra
