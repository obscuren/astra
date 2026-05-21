#include "astra/dev_console.h"
#include "astra/animation.h"
#include "astra/cyberdeck.h"
#include "astra/daemon.h"
#include "astra/grammars/gen_elevator_netspace.h"
#include "astra/net_combat.h"
#include "astra/net_combat_mode.h"
#include "astra/net_ice_telegraph.h"
#include "astra/net_pipe_path.h"
#include "astra/net_renderer.h"
#include "astra/net_window_anim.h"
#include "astra/netspace_generator.h"
#include "astra/aura.h"
#include "astra/consciousness_save.h"
#include "astra/biome_profile.h"
#include "astra/body_presets.h"
#include "astra/display_name.h"
#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/puzzles.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/fragment.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/loot_table.h"
#include "astra/lore_generator.h"
#include "astra/npc.h"
#include "astra/quest_fixture.h"
#include "astra/skill_defs.h"
#include "astra/skill_grant.h"
#include "astra/soul_mirror.h"
#include "astra/star_chart.h"
#include "astra/station_type.h"
#include "astra/tilemap.h"
#include "astra/tinkering.h"
#include "astra/trap.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>

namespace astra {

static std::optional<Rarity> parse_rarity_arg(std::string_view s) {
    auto eq_ci = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
            char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
            if (ca != cb) return false;
        }
        return true;
    };
    if (eq_ci(s, "c") || eq_ci(s, "common"))    return Rarity::Common;
    if (eq_ci(s, "u") || eq_ci(s, "uncommon"))  return Rarity::Uncommon;
    if (eq_ci(s, "r") || eq_ci(s, "rare"))      return Rarity::Rare;
    if (eq_ci(s, "e") || eq_ci(s, "epic"))      return Rarity::Epic;
    if (eq_ci(s, "l") || eq_ci(s, "legendary")) return Rarity::Legendary;
    return std::nullopt;
}

static std::string_view rarity_short_name(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "common";
        case Rarity::Uncommon:  return "uncommon";
        case Rarity::Rare:      return "rare";
        case Rarity::Epic:      return "epic";
        case Rarity::Legendary: return "legendary";
    }
    return "?";
}

void DevConsole::toggle() {
    open_ = !open_;
    input_.clear();
    cursor_ = 0;
}

void DevConsole::log(const std::string& msg) {
    output_.push_back(msg);
    if (output_.size() > max_output_) {
        output_.pop_front();
    }
}

bool DevConsole::handle_input(int key, Game& game) {
    if (!open_) return false;

    switch (key) {
        case 27: // Esc
            open_ = false;
            return true;
        case '\n': case '\r':
            if (!input_.empty()) {
                history_.push_back(input_);
                if (history_.size() > max_history_)
                    history_.pop_front();
                log("> " + input_);
                execute_command(input_, game);
                input_.clear();
                cursor_ = 0;
                scroll_ = 0;
                history_idx_ = -1;
            }
            return true;
        case 127: case 8:
            if (cursor_ > 0) {
                input_.erase(cursor_ - 1, 1);
                --cursor_;
            }
            return true;
        case KEY_DELETE:
            if (cursor_ < input_.size()) {
                input_.erase(cursor_, 1);
            }
            return true;
        case KEY_LEFT:
            if (cursor_ > 0) --cursor_;
            return true;
        case KEY_RIGHT:
            if (cursor_ < input_.size()) ++cursor_;
            return true;
        case KEY_UP: {
            int sz = static_cast<int>(history_.size());
            if (sz > 0) {
                if (history_idx_ < 0) history_idx_ = sz;
                if (history_idx_ > 0) {
                    --history_idx_;
                    input_ = history_[history_idx_];
                    cursor_ = input_.size();
                }
            }
            return true;
        }
        case KEY_DOWN: {
            int sz = static_cast<int>(history_.size());
            if (history_idx_ >= 0) {
                ++history_idx_;
                if (history_idx_ >= sz) {
                    history_idx_ = -1;
                    input_.clear();
                } else {
                    input_ = history_[history_idx_];
                }
                cursor_ = input_.size();
            }
            return true;
        }
        case KEY_PAGE_UP:
            scroll_++;
            return true;
        case KEY_PAGE_DOWN:
            if (scroll_ > 0) scroll_--;
            return true;
        default:
            if (key >= 32 && key < 127) {
                input_.insert(cursor_, 1, static_cast<char>(key));
                ++cursor_;
            }
            return true;
    }
}

// --------------------------------------------------------------------------
// Plan 5 Tasks 16-17 — unified `:spawn` dispatcher helpers.
//
// `:spawn-hackable` was retired and folded into `:spawn fixture <FixtureType>`.
// `:spawn` now dispatches: npc / fixture / ice / trap subkinds with a fallback
// auto-detect (NPC-role first, then FixtureType).
// --------------------------------------------------------------------------

static bool npc_role_exists(const std::string& s) {
    return s == "archon_remnant" || s == "void_reaver" || s == "archon_sentinel" ||
           s == "conclave_sentry" || s == "heavy_conclave_sentry" || s == "rust_hound" ||
           s == "sentry_drone" || s == "conclave_sentry_drone" || s == "archon_sentry_drone" ||
           s == "archon_automaton";
}

static std::optional<FixtureType> fixture_type_from_name(std::string_view s) {
    auto eq = [&](const char* needle) {
        if (s.size() != std::strlen(needle)) return false;
        for (size_t i = 0; i < s.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s[i])) !=
                std::tolower(static_cast<unsigned char>(needle[i]))) return false;
        }
        return true;
    };
    if (eq("Console"))         return FixtureType::Console;
    if (eq("CommandTerminal")) return FixtureType::CommandTerminal;
    if (eq("ShipTerminal"))    return FixtureType::ShipTerminal;
    if (eq("DataTerminal"))    return FixtureType::DataTerminal;
    if (eq("StarChart"))       return FixtureType::StarChart;
    if (eq("Door"))            return FixtureType::Door;
    if (eq("Gate"))            return FixtureType::Gate;
    if (eq("Conduit"))         return FixtureType::Conduit;
    if (eq("Lamp"))            return FixtureType::Lamp;
    if (eq("HoloLight"))       return FixtureType::HoloLight;
    if (eq("Locker"))          return FixtureType::Locker;
    if (eq("SupplyLocker"))    return FixtureType::SupplyLocker;
    if (eq("HealPod"))         return FixtureType::HealPod;
    if (eq("FoodTerminal"))    return FixtureType::FoodTerminal;
    if (eq("WeaponDisplay"))   return FixtureType::WeaponDisplay;
    if (eq("RepairBench"))     return FixtureType::RepairBench;
    if (eq("RestPod"))         return FixtureType::RestPod;
    return std::nullopt;
}

static void cmd_spawn_npc(DevConsole& con, Game& game, const std::string& role_arg) {
    std::string role_name;
    if      (role_arg == "archon_remnant")        role_name = "Archon Remnant";
    else if (role_arg == "void_reaver")           role_name = "Void Reaver";
    else if (role_arg == "archon_sentinel")       role_name = "Archon Sentinel";
    else if (role_arg == "conclave_sentry")       role_name = "Conclave Sentry";
    else if (role_arg == "heavy_conclave_sentry") role_name = "Heavy Conclave Sentry";
    else if (role_arg == "rust_hound")            role_name = "Rust Hound";
    else if (role_arg == "sentry_drone")          role_name = "Sentry Drone";
    else if (role_arg == "conclave_sentry_drone") role_name = "Conclave Sentry Drone";
    else if (role_arg == "archon_sentry_drone")   role_name = "Archon Sentry Drone";
    else if (role_arg == "archon_automaton")      role_name = "Archon Automaton";
    else {
        con.log("spawn: unknown role '" + role_arg +
                "' (archon_remnant|void_reaver|archon_sentinel|conclave_sentry|heavy_conclave_sentry|rust_hound|sentry_drone|archon_automaton)");
        return;
    }

    Npc npc = create_npc_by_role(role_name, game.world().rng());
    // Walk the 8 neighbours until a passable empty tile is found.
    const int dx[] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
    bool placed = false;
    for (int i = 0; i < 8 && !placed; ++i) {
        int nx = game.player().x + dx[i];
        int ny = game.player().y + dy[i];
        if (nx < 0 || nx >= game.world().map().width()) continue;
        if (ny < 0 || ny >= game.world().map().height()) continue;
        if (!game.world().map().passable(nx, ny)) continue;
        bool occupied = false;
        for (const auto& other : game.world().npcs()) {
            if (other.alive() && other.x == nx && other.y == ny) {
                occupied = true;
                break;
            }
        }
        if (occupied) continue;
        npc.x = nx;
        npc.y = ny;
        game.world().add_npc(std::move(npc));
        con.log("Spawned " + role_name + " at (" + std::to_string(nx) +
                "," + std::to_string(ny) + ")");
        placed = true;
    }
    if (!placed) con.log("spawn: no adjacent passable tile");
}

static void cmd_spawn_fixture_with_type(DevConsole& con, Game& game, FixtureType type) {
    auto& m = game.world().map();
    auto& player = game.player();
    static const int dxs[] = {1, -1, 0, 0};
    static const int dys[] = {0, 0, 1, -1};
    bool placed = false;
    int placed_x = 0, placed_y = 0;
    for (int i = 0; i < 4 && !placed; ++i) {
        int nx = player.x + dxs[i];
        int ny = player.y + dys[i];
        if (m.passable(nx, ny) && m.fixture_id(nx, ny) < 0) {
            FixtureData fd = make_fixture(type);
            fd.interactable = true;
            // make_fixture already auto-attaches Hackable for electrical types
            // (Plan 5 Tasks 11-13 centralized this in tilemap.cpp).
            m.add_fixture(nx, ny, std::move(fd));
            placed = true;
            placed_x = nx;
            placed_y = ny;
        }
    }
    if (!placed) {
        con.log("spawn fixture: no adjacent passable tile");
        return;
    }
    con.log("spawned " + std::string(fixture_type_name(type)) + " at (" +
            std::to_string(placed_x) + "," + std::to_string(placed_y) + ")");
    if (HackTagMask t = tags_for_fixture(type); t != 0) {
        game.world().lan_full_reset();
        con.log("  -> LAN reset (cracked/loot/decrypt state wiped - testing only).");
    }
}

static void cmd_spawn_fixture(DevConsole& con, Game& game, const std::string& type_name) {
    auto type = fixture_type_from_name(type_name);
    if (!type) {
        con.log("spawn fixture: unknown FixtureType '" + type_name + "'");
        return;
    }
    cmd_spawn_fixture_with_type(con, game, *type);
}

static void cmd_spawn_ice(DevConsole& con, Game& game, const std::string& color_arg) {
    if (!game.hacking().jacked_in()) {
        con.log("Not jacked in.");
        return;
    }
    IceColor color;
    int hp;
    if      (color_arg == "white") { color = IceColor::White; hp = 1; }
    else if (color_arg == "gray")  { color = IceColor::Gray;  hp = 2; }
    else if (color_arg == "black") { color = IceColor::Black; hp = 4; }
    else { con.log("unknown color: " + color_arg); return; }

    auto* sess = game.hacking().session();
    static const int dxs[4] = { 0, 0, -1, 1 };
    static const int dys[4] = { -1, 1, 0, 0 };
    for (int d = 0; d < 4; ++d) {
        int nx = sess->avatar_x + dxs[d];
        int ny = sess->avatar_y + dys[d];
        if (!sess->netspace.passable(nx, ny)) continue;
        bool occupied = false;
        for (auto& i : sess->ice) if (i.x == nx && i.y == ny) { occupied = true; break; }
        if (occupied) continue;
        Ice ice;
        ice.x = nx; ice.y = ny; ice.color = color; ice.hp = hp;
        sess->ice.push_back(ice);
        con.log("Spawned " + color_arg + " ICE.");
        return;
    }
    con.log("No adjacent passable tile to spawn ICE.");
}

static void cmd_spawn_trap(DevConsole& con, Game& game, const std::string& kind_arg) {
    TrapKind k;
    if      (kind_arg == "prox")       k = TrapKind::ProximityMine;
    else if (kind_arg == "emp")        k = TrapKind::EmpMine;
    else if (kind_arg == "incendiary") k = TrapKind::IncendiaryMine;
    else if (kind_arg == "decoy")      k = TrapKind::DecoyMine;
    else if (kind_arg == "caltrops")   k = TrapKind::Caltrops;
    else if (kind_arg == "dungeon")    k = TrapKind::DungeonGeneric;
    else { con.log("unknown trap kind: " + kind_arg); return; }
    place_dungeon_trap(game.world(), game.player().x, game.player().y, k);
    con.log("Spawned " + display_name(k));
}

static void run_net_selftest(DevConsole& con) {
    int fails = 0;
    auto check = [&](bool ok, const std::string& what) {
        if (!ok) { ++fails; con.log("FAIL " + what); }
    };
    using astra::WindowState;
    // window_band: thresholds + hysteresis + black-ice pin
    check(astra::window_band(0,  WindowState::Stable, false) == WindowState::Stable,   "band@0");
    check(astra::window_band(45, WindowState::Stable, false) == WindowState::Stressed, "band@45");
    check(astra::window_band(80, WindowState::Stable, false) == WindowState::Hunted,   "band@80");
    check(astra::window_band(97, WindowState::Stable, false) == WindowState::Critical, "band@97");
    check(astra::window_band(20, WindowState::Stable, true)  == WindowState::Critical, "blackpin");
    // de-escalate from Critical: 91 holds, 89 drops
    check(astra::window_band(91, WindowState::Critical, false) == WindowState::Critical,"hyst-hold");
    check(astra::window_band(89, WindowState::Critical, false) == WindowState::Hunted,  "hyst-drop");
    // ram_lie biased high & bounded; stable within a turn seed
    int a = astra::ram_lie(3, 6, WindowState::Critical, 42);
    int b = astra::ram_lie(3, 6, WindowState::Critical, 42);
    check(a == b,            "ram-stable");
    check(a > 3 && a <= 6,   "ram-biased-bounded");
    check(astra::ram_lie(3, 6, WindowState::Stable, 42) == 3, "ram-honest-stable");
    // hp_lie honest at Stable, length-preserving when lying
    check(astra::hp_lie(82, WindowState::Stable, 1) == "82", "hp-honest");
    check(astra::hp_lie(82, WindowState::Hunted, 1).size() == 2, "hp-len");
    // Phase 4: declarative layer plumbing.
    {
        astra::TargetDescriptor d;
        d.kind = astra::NetspaceTargetKind::Empty;
        astra::Netspace ns = astra::gen_for_target(d);
        check(ns.action_node_at(0, 0) == -1, "p4-node-empty");
        astra::NetNode n; n.x = 2; n.y = 3; n.kind = astra::NetNodeKind::Stash;
        ns.action_nodes.push_back(n);
        check(ns.action_node_at(2, 3) == 0, "p4-node-hit");
        ns.action_nodes[0].consumed = true;
        check(ns.action_node_at(2, 3) == -1, "p4-node-consumed");
    }
    // ATM grammar invariants.
    {
        astra::TargetDescriptor d; d.kind = astra::NetspaceTargetKind::Atm;
        d.tier = 2; d.seed = 7;
        astra::Netspace a  = astra::gen_for_target(d);
        astra::Netspace b2 = astra::gen_for_target(d);
        check(a.tiles == b2.tiles && a.action_nodes.size() == b2.action_nodes.size(),
              "atm-deterministic");
        check(a.trace_tick_hint >= 2, "atm-fast-trace");
        int vault = 0, stash = 0, tr_trace = 0, tr_turn = 0;
        for (auto& n : a.action_nodes) {
            if (n.kind == astra::NetNodeKind::VaultGrab) ++vault;
            if (n.kind == astra::NetNodeKind::Stash)     ++stash;
        }
        for (auto& t : a.triggers) {
            if (t.cond == astra::NetTriggerCond::TraceAtLeast && t.threshold == 100) ++tr_trace;
            if (t.cond == astra::NetTriggerCond::TurnCountAtLeast) ++tr_turn;
        }
        check(vault == 1 && stash == 1, "atm-nodes");
        check(tr_trace == 1 && tr_turn == 1, "atm-triggers");
    }
    // Turret grammar invariants.
    {
        astra::TargetDescriptor d; d.kind = astra::NetspaceTargetKind::Turret;
        d.tier = 3; d.seed = 4;
        astra::Netspace a = astra::gen_for_target(d);
        astra::Netspace a2 = astra::gen_for_target(d);
        check(a.tiles == a2.tiles && a.initial_ice.size() == a2.initial_ice.size(),
              "turret-deterministic");
        check(!a.initial_ice.empty(), "turret-ice");
        bool all_gray = true;
        for (auto& ic : a.initial_ice) if (ic.color != astra::IceColor::Gray) all_gray = false;
        check(all_gray, "turret-ice-gray");
        int dis=0, fl=0;
        for (auto& nd : a.action_nodes) {
            if (nd.kind == astra::NetNodeKind::TurretDisarm) ++dis;
            if (nd.kind == astra::NetNodeKind::TurretFlip)   ++fl;
        }
        check(dis == 1 && fl == 1, "turret-nodes");
    }
    // PlayerAllied: friendly to player; never hostile to itself.
    {
        astra::Player p;  // default-constructed; reputation empty
        check(astra::is_hostile_to_player("PlayerAllied", p) == false, "pa-not-vs-player");
        check(astra::is_hostile("PlayerAllied", "PlayerAllied") == false, "pa-self");
        check(astra::is_hostile("Hijacked", "anything") == true, "hijacked-still-univ");
    }
    // Elevator grammar invariants.
    {
        astra::TargetDescriptor d; d.kind = astra::NetspaceTargetKind::Elevator;
        d.tier = 3; d.seed = 11;
        astra::Netspace a  = astra::gen_for_target(d);
        astra::Netspace a2 = astra::gen_for_target(d);
        check(a.tiles == a2.tiles, "elev-deterministic");
        check(a.floor_count >= 4, "elev-floors");
        bool has_h = false;
        for (auto t : a.tiles) if (t == astra::NetTile::PipeH) has_h = true;
        check(!has_h, "elev-vertical-only");
        bool has_bw = !a.breakwalls.empty();
        check(has_bw, "elev-gated");
        int f_lobby = astra::elevator_floor_for_y(a, a.jack_in_y);
        check(f_lobby == 0, "elev-lobby-floor0");
        // press-your-luck now reachable: a non-LOBBY floor has an Exit tile
        // whose y maps to a floor > 0 via elevator_floor_for_y.
        bool upper_exit = false;
        for (int yy = 0; yy < a.h && !upper_exit; ++yy)
            for (int xx = 0; xx < a.w; ++xx)
                if (a.at(xx, yy) == astra::NetTile::Exit &&
                    astra::elevator_floor_for_y(a, yy) > 0) { upper_exit = true; break; }
        check(upper_exit, "elev-press-luck-live");
    }
    // Corpse grammar invariants.
    {
        astra::TargetDescriptor d; d.kind = astra::NetspaceTargetKind::Corpse;
        d.tier = 2; d.seed = 9;
        astra::Netspace a = astra::gen_for_target(d);
        astra::Netspace a2 = astra::gen_for_target(d);
        check(a.tiles == a2.tiles && a.title == a2.title, "corpse-deterministic");
        int gt=0, st=0;
        for (auto& nd : a.action_nodes) {
            if (nd.kind == astra::NetNodeKind::GhostTalk) ++gt;
            if (nd.kind == astra::NetNodeKind::Stash)     ++st;
        }
        check(gt == 1 && st == 1, "corpse-nodes");
        bool zalgo = false;
        for (unsigned char c : a.title) if (c == 0xCC) { zalgo = true; break; } // combining-mark lead byte
        check(zalgo, "corpse-zalgo");
        bool corrupt = false;
        for (int yy=0; yy<a.h && !corrupt; ++yy)
            for (int xx=0; xx<a.w; ++xx)
                if (a.is_wall(xx,yy)) { corrupt = true; break; }
        check(corrupt, "corpse-corruption");
    }
    {
        std::string be;
        check(astra::net_renderer::selftest_bands(be), "bands" + (be.empty() ? std::string() : (" " + be)));
    }
    {
        // Slice 2 action economy — statically-decidable invariants.
        // (move-XOR-cast / telegraph-confirm-commits / observe-is-free are
        // integration paths verified in-game per the working agreement.)
        astra::NetSession ns;
        ns.ram_max = 6; ns.ram = 4;
        if (ns.ram < ns.ram_max) ++ns.ram;          // idle regen
        check(ns.ram == 5, "slice2-idle-ram-regen");
        ns.ram = ns.ram_max;
        if (ns.ram < ns.ram_max) ++ns.ram;          // idle regen at cap
        check(ns.ram == ns.ram_max, "slice2-idle-ram-clamped");
        check(ns.committed_this_key == false, "slice2-commit-flag-default");
    }
    {
        // Slice 3a in-flight queue — decidable invariants.
        astra::NetSession ns;
        ns.ram_max = 6; ns.ram = 6;
        astra::NetInFlight f;
        f.slot = 2; f.turns_total = 3; f.turns_left = 3; f.ram_held = 4;
        ns.ram -= f.ram_held;                       // reserved at cast
        ns.in_flight.push_back(f);
        check(ns.slot_in_flight(2), "s3a-slot-busy");
        check(!ns.slot_in_flight(0), "s3a-slot-free");
        // advance 3 turns -> completes, RAM returned, dequeued
        for (int t = 0; t < 3; ++t) {
            auto& e = ns.in_flight.back();
            if (--e.turns_left == 0) {
                ns.ram = std::min(ns.ram_max, ns.ram + e.ram_held);
                ns.in_flight.pop_back();
            }
        }
        check(ns.in_flight.empty(), "s3a-completed-dequeued");
        check(ns.ram == 6, "s3a-ram-returned-on-complete");
        // cancel path: reserve, cancel -> dequeued, NO refund
        astra::NetSession nc; nc.ram_max = 6; nc.ram = 6;
        astra::NetInFlight g; g.slot = 1; g.ram_held = 3;
        nc.ram -= g.ram_held; nc.in_flight.push_back(g);
        nc.in_flight.clear();                        // cancel = drop, no refund
        check(nc.ram == 3, "s3a-cancel-no-refund");
    }
    {
        // Slice 3b — compiled in-flight path (decidable without a Game).
        // apply_effect_in_net's damage path needs a live Game& (grant_net_xp);
        // it is exercised in-game via :netprog (see mechanics.md), not unit-
        // asserted here.
        astra::NetSession ns;
        astra::EffectSpec sp; sp.damage = 4; sp.radius = 0;
        astra::NetInFlight f; f.compiled = true; f.turns_total = 3;
        f.turns_left = 3; f.spec = sp; f.slot = 0; f.ram_held = 0;
        ns.in_flight.push_back(f);
        check(ns.in_flight[0].compiled, "s3b-compiled-flag");
        check(ns.in_flight[0].turns_total == 3, "s3b-compiled-duration");
        check(ns.in_flight[0].spec.damage == 4, "s3b-compiled-spec-carried");
        astra::NetInFlight d;            // default
        check(!d.compiled, "s3b-compiled-default-false");
        check(!d.launched, "s3b-launch-default");
    }
    {
        // Slice 4 — pipe payload travel model (statically decidable).
        // clamp_seg_len: bounds are [2,6] per net_pipe_path.h.
        check(astra::clamp_seg_len(1) == 2, "s4-seg-clamp");   // below low → clamp to 2
        check(astra::clamp_seg_len(9) == 6, "s4-seg-clamp");   // above high → clamp to 6
        check(astra::clamp_seg_len(4) == 4, "s4-seg-clamp");   // in-range → identity
        // NetInFlight with a 3-cell pipe_path: seg_len == clamp_seg_len(3) == 3.
        astra::NetInFlight f;
        f.pipe_path = {{1,1},{2,1},{3,1}};
        f.seg_len   = astra::clamp_seg_len((int)f.pipe_path.size());
        check(f.seg_len        == 3, "s4-inflight-defaults");   // clamp(3,2,6)==3
        check(f.payloads.empty(),    "s4-inflight-defaults");   // default vector empty
        check(f.iters_launched == 0, "s4-inflight-defaults");   // in-class default
        // NetSession slice-4 fields: armed_slot==-1, active_pipe==0.
        astra::NetSession ns;
        check(ns.armed_slot == -1 && ns.active_pipe == 0, "s4-arm-defaults");
    }
    // Combat-arena bench invariants (Game-free).
    {
        astra::TargetDescriptor d;
        d.kind = astra::NetspaceTargetKind::CombatArena;
        d.tier = 1; d.seed = 1;
        astra::Netspace a  = astra::gen_for_target(d);
        astra::Netspace a2 = astra::gen_for_target(d);
        check(a.tiles == a2.tiles
              && a.initial_ice.size() == a2.initial_ice.size(),
              "s4ar-deterministic");
        check(a.pipes.size() >= 4, "s4ar-pipes");
        auto conn = astra::connected_pipe_indices(a, a.jack_in_x, a.jack_in_y);
        check(conn.size() >= 4, "s4ar-hub-fanout");
        int smin = 99, smax = 0;
        bool wall_ok = false;
        for (int idx : conn) {
            auto path = astra::pipe_path_cells(a, idx,
                                               a.jack_in_x, a.jack_in_y);
            int n = astra::clamp_seg_len(static_cast<int>(path.size()));
            if (n < smin) smin = n;
            if (n > smax) smax = n;
            if (!path.empty() && a.breakwall_lookup.count(path.back()))
                wall_ok = true;
        }
        check(smin == 2 && smax == 6, "s4ar-seg-span");
        int w = 0, g = 0, bl = 0;
        for (auto& ic : a.initial_ice) {
            if      (ic.color == astra::IceColor::White) ++w;
            else if (ic.color == astra::IceColor::Gray)  ++g;
            else                                         ++bl;
        }
        check(w == 1 && g == 2 && bl == 1 && a.initial_ice.size() == 4,
              "s4ar-roster-t1");
        check(!a.breakwall_lookup.empty(), "s4ar-breakwall");
        check(wall_ok, "s4ar-wall-impact-cell");
        // Since S4 (node-scoped Impact) a station pipe only needs a live
        // ICE somewhere IN the terminus room, not on the exact cell.
        int station_pipes = 0, station_in_node = 0;
        for (int idx : conn) {
            auto path = astra::pipe_path_cells(a, idx,
                                               a.jack_in_x, a.jack_in_y);
            if (path.empty() || a.breakwall_lookup.count(path.back()))
                continue;
            ++station_pipes;
            int ri = astra::room_index_at(a, path.back().first,
                                          path.back().second);
            if (ri < 0 || ri >= static_cast<int>(a.rooms.size())) continue;
            const astra::NetRoom& rm = a.rooms[static_cast<size_t>(ri)];
            for (auto& ic : a.initial_ice)
                if (ic.x >= rm.x && ic.x < rm.x + rm.w &&
                    ic.y >= rm.y && ic.y < rm.y + rm.h) {
                    ++station_in_node; break;
                }
        }
        check(station_pipes == 3 && station_in_node == 3,
              "s4ar-ice-in-node");
    }
    // Slice-1 tactical-combat lock predicate (Game-free).
    {
        astra::NetSession ns;
        ns.netspace.rooms.clear();
        astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
        astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
        ns.netspace.rooms.push_back(a);
        ns.netspace.rooms.push_back(b);
        astra::NetPipe p;
        p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
        for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
        ns.netspace.pipes.push_back(p);
        ns.avatar_x = 1; ns.avatar_y = 1;
        check(!astra::combat_should_lock(ns), "s5tc-no-ice");
        astra::Ice g; g.x = 11; g.y = 1;
        g.color = astra::IceColor::Gray; g.hp = 2;
        ns.ice.push_back(g);
        check(astra::combat_should_lock(ns), "s5tc-gray-locks");
        ns.ice[0].color = astra::IceColor::White;
        check(!astra::combat_should_lock(ns), "s5tc-white-ambient");
        ns.ice[0].color = astra::IceColor::Gray;
        ns.ice[0].charmed_turns_left = 3;
        check(!astra::combat_should_lock(ns), "s5tc-charmed-safe");
        ns.ice[0].charmed_turns_left = 0;
        check(astra::update_combat_lock(ns), "s5tc-transition");
        check(ns.combat_mode == astra::NetSession::NetCombatMode::Combat,
              "s5tc-mode-combat");
    }
    // Slice-2 CORE action registry + deck-def mapping (Game-free).
    {
        auto t1 = astra::cyberdeck_stats_tier1();
        auto t2 = astra::cyberdeck_stats_tier2();
        check(t1.core_actions[0] == astra::NetCoreAction::Sniff
           && t1.core_actions[1] == astra::NetCoreAction::Channel
           && t1.core_actions[2] == astra::NetCoreAction::None
           && t1.core_actions[3] == astra::NetCoreAction::None,
           "s5tc2-deck-tier1");
        check(t2.core_actions[0] == astra::NetCoreAction::Sniff
           && t2.core_actions[3] == astra::NetCoreAction::Run,
           "s5tc2-deck-tier2");
        astra::NetSession ns;
        ns.core_actions = t1.core_actions;
        ns.ram = 0; ns.ram_max = 8;
        astra::core_action_perform(ns, 1);
        check(ns.ram == 2, "s5tc2-channel");
        ns.brace_turns = 0;
        astra::core_action_perform(ns, 0);
        check(ns.brace_turns == 0, "s5tc2-sniff-noop-state");
        ns.core_actions[2] = astra::NetCoreAction::Brace;
        astra::core_action_perform(ns, 2);
        check(ns.brace_turns == 1, "s5tc2-brace");
        check(std::string(astra::core_action_label(
            astra::NetCoreAction::None)).empty(), "s5tc2-none-label");
    }
    // Slice-3 ranged-caster engagement + enqueue + cadence (Game-free).
    {
        astra::NetInFlight df;
        check(!df.hostile, "s5tc3-inflight-hostile-default");
        astra::Ice di;
        check(di.cast_cooldown == 0, "s5tc3-ice-cooldown-default");

        astra::NetSession ns;
        ns.netspace.rooms.clear();
        astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
        astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
        ns.netspace.rooms.push_back(a);
        ns.netspace.rooms.push_back(b);
        astra::NetPipe p;
        p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
        for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
        ns.netspace.pipes.push_back(p);
        ns.avatar_x = 1; ns.avatar_y = 1;            // room a (near node)
        astra::Ice g; g.x = 11; g.y = 1;             // room b (far node)
        g.color = astra::IceColor::Gray; g.hp = 2;
        ns.ice.push_back(g);

        // Not engaged until the S1 lock says COMBAT.
        astra::ice_cast_tick(ns);
        check(ns.in_flight.empty(), "s5tc3-no-cast-in-normal");

        astra::update_combat_lock(ns);               // topology -> COMBAT
        check(ns.combat_mode == astra::NetSession::NetCombatMode::Combat,
              "s5tc3-locked");
        astra::ice_cast_tick(ns);
        check(ns.in_flight.size() == 1, "s5tc3-cast-enqueued");
        check(ns.in_flight[0].hostile, "s5tc3-payload-hostile");
        check(ns.in_flight[0].seg_len >= 2
              && ns.in_flight[0].seg_len <= 6, "s5tc3-seg-clamped");
        // Reversed path: payload targets the avatar-end cell.
        check(ns.in_flight[0].target_x == 1
              && ns.in_flight[0].target_y == 1, "s5tc3-target-avatar-end");
        check(ns.ice[0].cast_cooldown == astra::kIceCastCadence,
              "s5tc3-cooldown-armed");

        // Cadence: no second cast while cooling down (just decrements).
        astra::ice_cast_tick(ns);
        check(ns.in_flight.size() == 1, "s5tc3-cadence-gated");
        check(ns.ice[0].cast_cooldown == astra::kIceCastCadence - 1,
              "s5tc3-cooldown-decrements");

        // White never casts; charmed Gray never casts.
        ns.in_flight.clear();
        ns.ice[0].cast_cooldown = 0;
        ns.ice[0].color = astra::IceColor::White;
        astra::ice_cast_tick(ns);
        check(ns.in_flight.empty(), "s5tc3-white-ambient");
        ns.ice[0].color = astra::IceColor::Gray;
        ns.ice[0].charmed_turns_left = 2;
        astra::ice_cast_tick(ns);
        check(ns.in_flight.empty(), "s5tc3-charmed-no-cast");
        // Black is the walker (S5) — never a caster.
        ns.ice[0].charmed_turns_left = 0;
        ns.ice[0].color = astra::IceColor::Black;
        astra::ice_cast_tick(ns);
        check(ns.in_flight.empty(), "s5tc3-black-not-caster");
    }
    // Slice-4 pipe collision + node-scoped Impact selection (Game-free).
    {
        astra::NetInFlight df;
        check(df.pipe_index == -1, "s5tc4-pipe-index-default");

        auto mk = [](bool hostile, int pipe, int seg_len,
                     int dmg, int seg) {
            astra::NetInFlight f;
            f.hostile    = hostile;
            f.pipe_index = pipe;
            f.seg_len    = seg_len;
            f.spec.damage = dmg;
            f.pipe_path  = {{0,0},{1,0}};      // non-empty = travel entry
            f.payloads   = { seg };
            return f;
        };

        // Same cell (uP=3, uI=5-2=3), equal damage -> annihilate both.
        {
            astra::NetSession ns;
            ns.in_flight.push_back(mk(false, 0, 5, 3, 3));   // player uP=3
            ns.in_flight.push_back(mk(true,  0, 5, 3, 2));   // ICE    uI=3
            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.empty()
               && ns.in_flight[1].payloads.empty(),
               "s5tc4-collide-annihilate");
        }
        // Unequal -> loser destroyed, winner carries the difference.
        {
            astra::NetSession ns;
            ns.in_flight.push_back(mk(false, 0, 5, 5, 3));   // player uP=3
            ns.in_flight.push_back(mk(true,  0, 5, 2, 2));   // ICE    uI=3
            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.size() == 1
               && ns.in_flight[1].payloads.empty()
               && ns.in_flight[0].spec.damage == 3,
               "s5tc4-collide-carry");
        }
        // Adjacent swap (uP=4, uI=5-2=3 -> diff==1) collides.
        {
            astra::NetSession ns;
            ns.in_flight.push_back(mk(false, 0, 5, 1, 4));   // uP=4
            ns.in_flight.push_back(mk(true,  0, 5, 1, 2));   // uI=3
            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.empty()
               && ns.in_flight[1].payloads.empty(),
               "s5tc4-collide-swap");
        }
        // Same direction never collides; different pipe never collides.
        {
            astra::NetSession ns;
            ns.in_flight.push_back(mk(false, 0, 5, 1, 3));
            ns.in_flight.push_back(mk(false, 0, 5, 1, 3));   // both player
            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.size() == 1
               && ns.in_flight[1].payloads.size() == 1,
               "s5tc4-same-dir-no-collide");
        }
        {
            astra::NetSession ns;
            ns.in_flight.push_back(mk(false, 0, 5, 1, 3));
            ns.in_flight.push_back(mk(true,  1, 5, 1, 2));   // pipe 1
            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.size() == 1
               && ns.in_flight[1].payloads.size() == 1,
               "s5tc4-diff-pipe-no-collide");
        }

        // net_node_targets: room-scoped selection (the radius-0 fix).
        {
            astra::NetSession ns;
            astra::NetRoom rm; rm.x = 0; rm.y = 0; rm.w = 5; rm.h = 5;
            ns.netspace.rooms.push_back(rm);
            astra::Ice i0; i0.x = 1; i0.y = 1; i0.hp = 2;   // in room
            astra::Ice i1; i1.x = 3; i1.y = 3; i1.hp = 2;   // in room
            astra::Ice i2; i2.x = 50; i2.y = 50; i2.hp = 2; // outside
            ns.ice = { i0, i1, i2 };
            astra::EffectSpec sp; sp.damage = 1; sp.radius = 0;
            auto single = astra::net_node_targets(ns, sp, 4, 2);
            // closest in-room to (4,2): i1 Cheb=1 < i0 Cheb=3.
            check(single.size() == 1 && single[0] == 1,
                  "s5tc4-node-single-closest");
            sp.radius = 1;                                  // AOE
            auto aoe = astra::net_node_targets(ns, sp, 4, 2);
            check(aoe.size() == 2, "s5tc4-node-aoe-all");   // i2 excluded
            ns.ice.clear();
            check(astra::net_node_targets(ns, sp, 4, 2).empty(),
                  "s5tc4-node-none");
            astra::EffectSpec z; z.damage = 0;
            check(astra::net_node_targets(ns, z, 4, 2).empty(),
                  "s5tc4-node-nodmg");
        }
    }
    // Slice-5 Black pipe-graph walker + payload<->Black contact (Game-free).
    {
        astra::Ice di;
        check(di.walk_pipe_index == -1 && di.walk_seg == 0
              && di.walk_seg_len == 0 && !di.killed,
              "s5tc5-ice-walker-defaults");

        // pipe_graph_next_hop: 2-room linear graph.
        {
            astra::Netspace ns;
            astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
            astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
            ns.rooms = { a, b };
            astra::NetPipe p;
            p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
            for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
            ns.pipes.push_back(p);
            check(astra::pipe_graph_next_hop(ns, 0, 1) == 0,
                  "s5tc5-bfs-direct");
            check(astra::pipe_graph_next_hop(ns, 1, 0) == 0,
                  "s5tc5-bfs-direct-reverse");
            check(astra::pipe_graph_next_hop(ns, 0, 0) == -1,
                  "s5tc5-bfs-same-room");
            check(astra::pipe_graph_next_hop(ns, 0, 5) == -1,
                  "s5tc5-bfs-out-of-range");
        }

        // 3-room linear: A -- pipe0 -- B -- pipe1 -- C.
        {
            astra::Netspace ns;
            astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
            astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
            astra::NetRoom c; c.x = 20; c.y = 0; c.w = 3; c.h = 3;
            ns.rooms = { a, b, c };
            astra::NetPipe p0; p0.x0 = 1; p0.y0 = 1;
            p0.x1 = 11; p0.y1 = 1;
            for (int x = 1; x <= 11; ++x) p0.cells.emplace_back(x, 1);
            astra::NetPipe p1; p1.x0 = 11; p1.y0 = 1;
            p1.x1 = 21; p1.y1 = 1;
            for (int x = 11; x <= 21; ++x) p1.cells.emplace_back(x, 1);
            ns.pipes = { p0, p1 };
            check(astra::pipe_graph_next_hop(ns, 0, 2) == 0,
                  "s5tc5-bfs-two-hop-first-hop");
            check(astra::pipe_graph_next_hop(ns, 2, 0) == 1,
                  "s5tc5-bfs-two-hop-reverse");
        }

        // black_walker_tick: avatar in room a, Black in room b, one
        // pipe. Walker picks the pipe and starts stepping immediately.
        {
            astra::NetSession ns;
            astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
            astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
            ns.netspace.rooms = { a, b };
            astra::NetPipe p;
            p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
            for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
            ns.netspace.pipes.push_back(p);
            ns.avatar_x = 1; ns.avatar_y = 1;            // room a
            astra::Ice blk;
            blk.x = 11; blk.y = 1;                        // room b
            blk.color = astra::IceColor::Black; blk.hp = 4;
            ns.ice.push_back(blk);

            astra::black_walker_tick(ns);
            check(ns.ice[0].walk_pipe_index == 0
                  && ns.ice[0].walk_seg == 1
                  && ns.ice[0].walk_seg_len >= 2
                  && ns.ice[0].walk_seg_len <= 6,
                  "s5tc5-walker-launch");
            check(!ns.black_reached_player_node,
                  "s5tc5-not-reached-yet");
            // Drive enough beats for arrival (worst-case seg_len = 6).
            for (int i = 0; i < 12; ++i) {
                if (ns.black_reached_player_node) break;
                astra::black_walker_tick(ns);
            }
            check(ns.black_reached_player_node, "s5tc5-reached-avatar");
        }

        // payload<->Black contact: player payload meets a Black at the
        // same physical pipe segment -> Black takes damage, payload gone.
        {
            astra::NetSession ns;
            // Single payload entry on pipe 0, player-side (hostile=false).
            astra::NetInFlight f;
            f.hostile = false; f.pipe_index = 0; f.seg_len = 5;
            f.spec.damage = 3;
            f.pipe_path = {{0,0},{1,0}};      // non-empty = travel entry
            f.payloads = { 3 };               // uP = 3
            ns.in_flight.push_back(f);
            // Black in transit on the same pipe with walk_seg_len=5
            // and walk_seg=2 -> u_black = 5 - 2 = 3. Same cell.
            astra::Ice blk;
            blk.color = astra::IceColor::Black; blk.hp = 5;
            blk.walk_pipe_index = 0;
            blk.walk_seg        = 2;
            blk.walk_seg_len    = 5;
            blk.walk_path       = {{1,0},{2,0},{3,0},{4,0},{5,0}};
            ns.ice.push_back(blk);

            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.empty(),
                  "s5tc5-payload-vs-black-consumes-payload");
            check(ns.ice[0].hp == 2,    // 5 - 3
                  "s5tc5-payload-vs-black-damages-black");
        }

        // I1 regression: long pipe (clamped seg_len < walk_path.size()).
        {
            astra::NetSession ns;
            astra::NetInFlight f;
            f.hostile = false; f.pipe_index = 0; f.seg_len = 6;
            f.spec.damage = 4;
            f.pipe_path = {{0,0},{1,0}};       // non-empty
            f.payloads  = { 3 };               // uP = 3
            ns.in_flight.push_back(f);
            astra::Ice blk;
            blk.color = astra::IceColor::Black; blk.hp = 5;
            blk.walk_pipe_index = 0;
            blk.walk_seg        = 4;
            blk.walk_seg_len    = 6;
            // 11-cell physical path simulating the bench's JACK->BLACK.
            blk.walk_path = {{0,0},{1,0},{2,0},{3,0},{4,0},
                              {5,0},{6,0},{7,0},{8,0},{9,0},{10,0}};
            ns.ice.push_back(blk);
            astra::resolve_pipe_collisions(ns);
            check(ns.in_flight[0].payloads.empty(),
                  "s5tc5-long-pipe-payload-consumed");
            check(ns.ice[0].hp == 1,                 // 5 - 4
                  "s5tc5-long-pipe-black-damaged");
        }

        // C1 regression: a Black somehow co-located with the avatar is
        // the same-room detection's responsibility. (The place_ice_far
        // fix prevents the configuration arising; this pins the
        // walker's behaviour as the safety net.)
        {
            astra::NetSession ns;
            astra::NetRoom a; a.x = 0; a.y = 0; a.w = 3; a.h = 3;
            ns.netspace.rooms = { a };
            ns.avatar_x = 1; ns.avatar_y = 1;
            astra::Ice blk;
            blk.x = 1; blk.y = 1;
            blk.color = astra::IceColor::Black; blk.hp = 4;
            ns.ice.push_back(blk);
            astra::black_walker_tick(ns);
            check(ns.black_reached_player_node,
                  "s5tc5-same-room-instant-reach");
        }
    }

    // Slice-6 telegraph + sniff_show + windup + RUN-adjacent (Game-free).
    {
        astra::Ice di;
        check(di.telegraph_tier == astra::IceTelegraphTier::Watchdog
              && di.cast_windup_left == 0 && di.cast_windup_total == 0,
              "s5tc6-defaults");

        astra::NetSession s0;
        check(s0.sniff_level == 0 && !s0.run_active,
              "s5tc6-sniff-default");

        // sniff-clamp: N>kSniffMax presses cap at kSniffMax.
        s0.core_actions[0] = astra::NetCoreAction::Sniff;
        for (int i = 0; i < 10; ++i) astra::core_action_perform(s0, 0);
        check(s0.sniff_level == astra::kSniffMax,
              "s5tc6-sniff-clamp");

        // sniff_show table: Watchdog/T0 hides all.
        using astra::sniff_show;
        using astra::IceTelegraphTier;
        using astra::RevealKind;
        check(!sniff_show(IceTelegraphTier::Watchdog, 0, RevealKind::PayloadDmg)
              && !sniff_show(IceTelegraphTier::Watchdog, 0, RevealKind::IceCastBar)
              && !sniff_show(IceTelegraphTier::Watchdog, 0, RevealKind::IceCastName)
              && !sniff_show(IceTelegraphTier::Watchdog, 0, RevealKind::IceHp)
              && !sniff_show(IceTelegraphTier::Watchdog, 0, RevealKind::BlackEtaCoarse)
              && !sniff_show(IceTelegraphTier::Watchdog, 0, RevealKind::BlackEtaPrecise),
              "s5tc6-sniff-show-watchdog-t0");

        // Watchdog/T1: PayloadDmg + IceCastBar + BlackEtaCoarse open.
        check(sniff_show(IceTelegraphTier::Watchdog, 1, RevealKind::PayloadDmg)
              && sniff_show(IceTelegraphTier::Watchdog, 1, RevealKind::IceCastBar)
              && sniff_show(IceTelegraphTier::Watchdog, 1, RevealKind::BlackEtaCoarse)
              && !sniff_show(IceTelegraphTier::Watchdog, 1, RevealKind::IceCastName)
              && !sniff_show(IceTelegraphTier::Watchdog, 1, RevealKind::IceHp)
              && !sniff_show(IceTelegraphTier::Watchdog, 1, RevealKind::BlackEtaPrecise),
              "s5tc6-sniff-show-watchdog-t1");

        // Watchdog/T2: everything open.
        check(sniff_show(IceTelegraphTier::Watchdog, 2, RevealKind::PayloadDmg)
              && sniff_show(IceTelegraphTier::Watchdog, 2, RevealKind::IceCastBar)
              && sniff_show(IceTelegraphTier::Watchdog, 2, RevealKind::IceCastName)
              && sniff_show(IceTelegraphTier::Watchdog, 2, RevealKind::IceHp)
              && sniff_show(IceTelegraphTier::Watchdog, 2, RevealKind::BlackEtaCoarse)
              && sniff_show(IceTelegraphTier::Watchdog, 2, RevealKind::BlackEtaPrecise),
              "s5tc6-sniff-show-watchdog-t2");

        // Non-Watchdog tiers hide everything (future-content seam).
        check(!sniff_show(IceTelegraphTier::Elite, 2, RevealKind::PayloadDmg)
              && !sniff_show(IceTelegraphTier::Boss, 2, RevealKind::IceCastBar)
              && !sniff_show(IceTelegraphTier::Blackwall, 2, RevealKind::IceCastName),
              "s5tc6-sniff-show-future-tiers-hidden");

        // Windup pipeline: Gray engaged with cooldown=0 enters windup,
        // does NOT spawn until windup ticks to 0.
        {
            astra::NetSession ns;
            astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
            astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
            ns.netspace.rooms = { a, b };
            astra::NetPipe p;
            p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
            for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
            ns.netspace.pipes.push_back(p);
            ns.avatar_x = 1; ns.avatar_y = 1;
            ns.combat_mode = astra::NetSession::NetCombatMode::Combat;
            astra::Ice gr;
            gr.x = 11; gr.y = 1;
            gr.color = astra::IceColor::Gray; gr.hp = 2;
            gr.cast_cooldown = 0;     // ready
            ns.ice.push_back(gr);

            astra::ice_cast_tick(ns);
            check(ns.ice[0].cast_windup_left == astra::kIceGrayWindupBeats
                  && ns.ice[0].cast_windup_total == astra::kIceGrayWindupBeats,
                  "s5tc6-windup-enters");
            check(ns.in_flight.empty(),
                  "s5tc6-windup-no-launch");
            // Tick down to zero; payload should spawn on the very beat
            // windup_left transitions from 1 to 0.
            for (int i = 0; i < astra::kIceGrayWindupBeats - 1; ++i)
                astra::ice_cast_tick(ns);
            check(ns.ice[0].cast_windup_left == 1 && ns.in_flight.empty(),
                  "s5tc6-windup-pre-fire");
            astra::ice_cast_tick(ns);
            check(ns.ice[0].cast_windup_left == 0
                  && ns.in_flight.size() == 1
                  && ns.in_flight[0].hostile
                  && ns.in_flight[0].spec.damage == astra::kIceGrayCastDamage,
                  "s5tc6-windup-fires-at-zero");
            check(ns.ice[0].cast_cooldown == astra::kIceCastCadence,
                  "s5tc6-cadence-reset");
            // S6.2: spawn-time linkage from payload back to source ICE.
            // Single ICE in this test scene -> &ice - s.ice.data() == 0.
            check(!ns.in_flight.empty()
                  && ns.in_flight[0].source_ice_idx == 0,
                  "s5tc6-source-ice-idx");
        }

        // black_eta_beats: 2-room linear, Black at far room, avatar at
        // near room. ETA = cells of the connecting pipe (= 11 here).
        {
            astra::NetSession ns;
            astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
            astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
            ns.netspace.rooms = { a, b };
            astra::NetPipe p;
            p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
            for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
            ns.netspace.pipes.push_back(p);
            ns.avatar_x = 1; ns.avatar_y = 1;
            astra::Ice blk;
            blk.x = 11; blk.y = 1;
            blk.color = astra::IceColor::Black; blk.hp = 4;
            ns.ice.push_back(blk);
            int eta = astra::black_eta_beats(ns, ns.ice[0]);
            check(eta == 11, "s5tc6-black-eta-far-room");
        }

        // (RUN halt-on-Black-adjacent is exercised in-game; the helper
        // any_black_one_hop is file-local in net_combat.cpp so we test
        // the user-visible contract via Black walker placement: a Black
        // already in the avatar's room is "adjacent" per the helper.
        // Detection logic is covered by the precision selftest above +
        // S5's s5tc5-reached-avatar.)
    }

    // Slice-7c.1 daemon kind table + door grammar daemon seeding.
    {
        // Daemon table sanity.
        const astra::DaemonDef& d_watch =
            astra::daemon_def(astra::DaemonKind::Watchdog);
        check(d_watch.archetype == astra::IceColor::Gray
              && d_watch.windup_beats == 4
              && d_watch.cast_damage == 1
              && d_watch.cast_radius == 0
              && d_watch.render_style == astra::DaemonRenderStyle::Glyph,
              "s5tc7c1-watchdog-baseline");

        const astra::DaemonDef& d_lock =
            astra::daemon_def(astra::DaemonKind::Lock);
        check(d_lock.archetype == astra::IceColor::Gray
              && std::string(d_lock.cast_prog_name) == "LOCK.fw"
              && d_lock.render_style == astra::DaemonRenderStyle::RoomFill
              && d_lock.base_hp == 4
              && d_lock.windup_beats == 5,
              "s5tc7c1-lock-def");

        const astra::DaemonDef& d_bolt =
            astra::daemon_def(astra::DaemonKind::Bolt);
        check(d_bolt.archetype == astra::IceColor::Gray
              && std::string(d_bolt.cast_prog_name) == "BOLT.T9"
              && d_bolt.render_style == astra::DaemonRenderStyle::Glyph
              && d_bolt.color == astra::Color::Yellow
              && d_bolt.is_boss == true
              && d_bolt.base_hp == 12
              && d_bolt.windup_beats == 6
              && d_bolt.cast_damage == 2
              && std::string(d_bolt.glyph) == "\xe2\x96\xa3",   // ▣
              "s5tc7c1-bolt-def");

        // Default Ice has Watchdog kind (back-compat with S6 selftests).
        astra::Ice di;
        check(di.kind == astra::DaemonKind::Watchdog
              && di.windup_override == 0
              && di.cast_damage_override == 0,
              "s5tc7c1-ice-defaults");

        // ice_cast_tick honors windup_override.
        {
            astra::NetSession ns;
            astra::NetRoom a; a.x = 0;  a.y = 0; a.w = 3; a.h = 3;
            astra::NetRoom b; b.x = 10; b.y = 0; b.w = 3; b.h = 3;
            ns.netspace.rooms = { a, b };
            astra::NetPipe p;
            p.x0 = 1; p.y0 = 1; p.x1 = 11; p.y1 = 1;
            for (int x = 1; x <= 11; ++x) p.cells.emplace_back(x, 1);
            ns.netspace.pipes.push_back(p);
            ns.avatar_x = 1; ns.avatar_y = 1;
            ns.combat_mode = astra::NetSession::NetCombatMode::Combat;
            astra::Ice gr;
            gr.x = 11; gr.y = 1;
            gr.color = astra::IceColor::Gray; gr.hp = 5; gr.hp_max = 5;
            gr.kind = astra::DaemonKind::Lock;
            gr.windup_override = 5;
            gr.cast_damage_override = 0;     // use def baseline (1)
            gr.cast_cooldown = 0;
            ns.ice.push_back(gr);

            astra::ice_cast_tick(ns);
            check(ns.ice[0].cast_windup_left == 5
                  && ns.ice[0].cast_windup_total == 5,
                  "s5tc7c1-lock-windup-override-applied");
            // 5 beats later the payload spawns with def's cast_damage=1.
            for (int i = 0; i < 5; ++i) astra::ice_cast_tick(ns);
            check(ns.in_flight.size() == 1
                  && ns.in_flight[0].spec.damage == 1
                  && ns.in_flight[0].prog_name == "LOCK.fw",
                  "s5tc7c1-lock-fires-with-correct-name-and-dmg");
        }
    }

    con.log(fails == 0 ? "net selftest: PASS" : ("net selftest: " + std::to_string(fails) + " FAIL"));
}

void DevConsole::execute_command(const std::string& cmd, Game& game) {
    std::vector<std::string> args;
    std::string token;
    for (char c : cmd) {
        if (c == ' ') {
            if (!token.empty()) { args.push_back(token); token.clear(); }
        } else {
            token += c;
        }
    }
    if (!token.empty()) args.push_back(token);
    if (args.empty()) return;

    auto& player = game.player();
    const auto& verb = args[0];

    if (verb == "help") {
        log("Commands:");
        log("  warp random        - warp to random map");
        log("  warp stamp <type>  - POI stamp test (ruins, ship, outpost, cave, settlement, landing)");
        log("  budget             - dump current planet's PoiBudget");
        log("  discoveries        - list Discovery-category journal entries");
        log("  give hp <n>        - set HP");
        log("  give xp <n>        - set XP");
        log("  give money <n>     - set credits");
        log("  give sp <n>        - set skill points");
        log("  give ap <n>        - set attribute points");
        log("  give rep <faction> <n> - set faction reputation");
        log("  give ship <component>         - install ship component (engine [or engine_coil_mk1], hull_plate, shield_generator, navi_computer_mk2)");
        log("  give item                     - list all items (identifier, category, rarity range)");
        log("  give item <identifier>        - spawn item at Common, level 1");
        log("  give item <id> <rarity>       - spawn at given rarity, level 1");
        log("  give item <id> <rarity> <lvl> - fully specified (rarity: c/u/r/e/l)");
        log("  set invuln         - toggle invulnerability");
        log("  set level <n>      - set player level");
        log("  effect burn <dur>  - apply burn effect");
        log("  effect regen <dur> - apply regen effect");
        log("  effect clear       - remove all effects");
        log("  kill all           - kill all hostile NPCs");
        log("  quest kill         - random kill quest");
        log("  quest fetch        - random fetch quest");
        log("  quest deliver      - random deliver quest");
        log("  quest scout        - random scout quest");
        log("  quest story        - The Missing Hauler");
        log("  quest begin <id>   - force-start a story quest by id (bypass prereqs)");
        log("  quest complete <id> - fill objectives so quest is ready for hand-in");
        log("  quest finish <id>  - force-complete active quest by id (fires cascade)");
        log("  heal               - full heal");
        log("  reveal_traps       - toggle render of all hidden traps");
        log("  spawn-trap <prox|emp|incendiary|decoy|caltrops|dungeon> - spawn a dungeon trap at player feet");
        log("  learn-schem <name|all> - learn a schematic by output name (prox, emp, caltrops, healing_stim, frag_grenade, ...) or a fragment (volt, pyre, drain, warp, decay, jitter, slag, relay, broadcast, amplify, tick, loop)");
        log("  bearings           - regain bearings if lost");
        log("  lore list           - list lore-annotated systems");
        log("  lore warp <feature> - warp to system (beacon/megastructure/terraformed/scarred/battle/weapon/plague/tier1-3)");
        log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock|neutron|derelict)");
        log("  chart reveal <name> - reveal system by name substring");
        log("  chart hide <name>   - hide system by name substring");
        log("  spawn <name>                  - NPC role or FixtureType (auto-detect)");
        log("  spawn npc <role>              - explicit NPC");
        log("    roles: archon_remnant, void_reaver, archon_sentinel, conclave_sentry,");
        log("           heavy_conclave_sentry, rust_hound, sentry_drone, archon_automaton");
        log("  spawn fixture <FixtureType>   - fixture (auto-Hackable if Electronic; resets LAN)");
        log("  spawn ice <color>             - ICE in mid-jack-in sector");
        log("  spawn trap <kind>             - trap");
        log("  fixtures     - list quest fixtures (id, location key, tile)");
        log("  tp <x> <y>   - teleport to tile (x, y) on current map");
        log("  tp <fixture_id> - teleport to that quest fixture if it's on the current map");
        log("  history             - show world lore history");
        log("  biome_test <biome> [settlement [frontier|advanced|ruined]]");
        log("                     [ruins [monolithic|baroque|crystal|industrial] [connected]]");
        log("                     [outpost]");
        log("                     [ship [pod|freighter|corvette]]");
        log("                     [cave [natural|mine|excavation]]");
        log("    biomes: grassland forest jungle sandy rocky volcanic marsh ice");
        log("    fungal crystal corroded aquatic alien_crystalline alien_organic");
        log("    alien_geometric alien_void alien_light scarred_scorched scarred_glassed");
        log("    settlement styles: frontier, advanced, ruined (default: frontier)");
        log("    ruins: generates ruin POI; civ style optional; 'connected' sets all 4 edges");
        log("    outpost: fenced fort with main building, tents, campfires");
        log("    ship: crashed wreck; class optional (auto = lore-weighted)");
        log("    cave: dungeon entrance; variant optional (natural/mine/excavation)");
        log("  dungen <style> [civ] - generate a pipeline dungeon (style: simple_rooms)");
        log("  editor             - open map editor");
        log("  mono [on|off]      - toggle monochrome render filter (visual debug)");
        log("  clear              - clear console");
        log("  give skill <id|name>          - learn a skill");
        log("  unequip-implant <0|1>         - remove implant from slot, return to inventory");
        log("  detection <n>                 - set zone detection counter");
        log("  sync-soul                     - force Sync Soul on nearest Precursor console");
        log("  unlock-anchor                 - grant ConsciousnessAnchor + seed deep-Grid base");
        log("  rebirth                       - open Sgr A* rebirth modal");
        log("  rebirth-reset                 - delete consciousness.dat (clean slate)");
        log("  netprog                       - load test Loop(3){Volt} chain into deck slot 1 (slice-3b AST bridge)");
        log("  jack combat [tier] [seed]     - jack into the combat test bench (hub + 4 pipes: short/mid/long/wall)");
    }
    else if (verb == "clear") {
        clear();
    }
    else if (verb == "budget") {
        const TileMap& owm = game.world().map();
        if (owm.map_type() != MapType::Overworld) {
            output_.push_back("(not on an overworld — fly to a planet)");
        } else {
            const PoiBudget& b = owm.poi_budget();
            std::string report = format_poi_budget(b);
            size_t start = 0;
            while (start < report.size()) {
                size_t nl = report.find('\n', start);
                std::string line = report.substr(start, nl - start);
                if (!line.empty()) output_.push_back(line);
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            output_.push_back("Hidden POIs: " +
                std::to_string(owm.hidden_pois().size()));
            output_.push_back("Anchor hints: " +
                std::to_string(owm.anchor_hints().size()));
        }
    }
    else if (verb == "discoveries") {
        const auto& journal = game.player().journal;
        int count = 0;
        for (const auto& e : journal) {
            if (e.category == JournalCategory::Discovery) {
                output_.push_back(e.title);
                ++count;
            }
        }
        if (count == 0) output_.push_back("(no discoveries)");
    }
    else if (verb == "heal") {
        player.hp = player.effective_max_hp();
        log("HP restored to " + std::to_string(player.hp));
    }
    else if (verb == "reveal_traps") {
        game.toggle_reveal_traps();
        log(game.reveal_traps_debug() ? "Trap debug ON" : "Trap debug OFF");
    }
    else if (verb == "spawn-trap") {
        if (args.size() < 2) { log("usage: spawn-trap <kind>"); return; }
        const std::string& s = args[1];
        TrapKind k;
        if      (s == "prox")       k = TrapKind::ProximityMine;
        else if (s == "emp")        k = TrapKind::EmpMine;
        else if (s == "incendiary") k = TrapKind::IncendiaryMine;
        else if (s == "decoy")      k = TrapKind::DecoyMine;
        else if (s == "caltrops")   k = TrapKind::Caltrops;
        else if (s == "dungeon")    k = TrapKind::DungeonGeneric;
        else { log("unknown trap kind: " + s); return; }
        place_dungeon_trap(game.world(), game.player().x, game.player().y, k);
        log("Spawned " + display_name(k));
    }
    else if (verb == "learn-schem") {
        if (args.size() < 2) {
            log("usage: learn-schem <name|all>");
            log("  Try: prox emp incendiary decoy caltrops");
            log("       healing_stim adrenaline_stim endure_stim focus_stim berserker_stim medkit");
            log("       frag_grenade emp_grenade cryo_grenade incendiary_grenade smoke_grenade flashbang");
            return;
        }
        // Map common short names to schematic output_name substrings.
        auto match = [](const std::string& s, const SchematicRecipe& r) {
            std::string out = r.output_name ? r.output_name : "";
            // case-insensitive substring or short alias
            auto lower = [](std::string v) { for (auto& c : v) c = std::tolower(c); return v; };
            std::string sl = lower(s), nl = lower(out);
            if (s == "prox")       return out == std::string("Proximity Mine");
            if (s == "emp")        return out == std::string("EMP Mine");
            if (s == "incendiary") return out == std::string("Incendiary Mine");
            if (s == "decoy")      return out == std::string("Decoy Mine");
            if (s == "caltrops")   return out == std::string("Caltrops");
            // generic substring match: try the input as-is first (so
            // underscored output names like "pulse_hammer.exe" match
            // "pulse_hammer"), then fall back to underscores-as-spaces
            // (so "frag_grenade" matches "Frag Grenade").
            if (nl.find(sl) != std::string::npos) return true;
            std::string alias = sl;
            for (auto& c : alias) if (c == '_') c = ' ';
            return nl.find(alias) != std::string::npos;
        };

        const auto& recipes = schematic_recipes();
        int learned = 0;
        for (const auto& r : recipes) {
            if (args[1] == "all" || match(args[1], r)) {
                auto res = learn_schematic(player, r.schematic_id,
                                           r.output_name, r.output_desc);
                if (res.success) { ++learned; log(res.message); }
            }
        }

        // Fragments — granted to Player.learned_fragments by name.
        if (args[1] != "all") {
            const FragmentDef* def = find_fragment_by_name(args[1].c_str());
            if (def) {
                bool already = false;
                for (auto fid : player.learned_fragments) {
                    if (fid == def->id) { already = true; break; }
                }
                if (!already) {
                    player.learned_fragments.push_back(def->id);
                    log("Learned fragment: " + std::string(def->display));
                    ++learned;
                }
            }
        } else {
            // "learn all" grants every fragment too.
            int frag_added = 0;
            for (const auto& def : fragment_catalog()) {
                if (def.id == FragmentId::None) continue;
                bool already = false;
                for (auto fid : player.learned_fragments) {
                    if (fid == def.id) { already = true; break; }
                }
                if (!already) {
                    player.learned_fragments.push_back(def.id);
                    ++learned;
                    ++frag_added;
                }
            }
            if (frag_added > 0) log("Granted " + std::to_string(frag_added) + " fragments.");
        }

        if (learned == 0) log("No matching schematic or fragment for '" + args[1] + "'.");
    }
    else if (verb == "solve") {
        auto& map = game.world().map();
        int solved = 0;
        for (int i = 0; i < map.puzzle_count(); ++i) {
            auto& ps = map.puzzle_mut(i);
            if (ps.solved) continue;
            astra::dungeon::on_button_pressed(game, ps.id);
            ++solved;
        }
        log("Solved " + std::to_string(solved) + " puzzle(s).");
    }
    else if (verb == "flash") {
        game.animations().spawn_effect(anim_damage_flash, player.x, player.y);
        log("Spawned damage flash at player (" + std::to_string(player.x) + "," + std::to_string(player.y) +
            "). Active effects: " + std::to_string(game.animations().has_active_effects() ? 1 : 0));
    }
    else if (verb == "editor") {
        game.map_editor().open(game);
        if (game.map_editor().is_open()) {
            log("Map editor opened.");
        }
    }
    else if (verb == "mono") {
        bool on = !game.renderer()->monochrome();
        if (args.size() >= 2) {
            on = (args[1] == "on" || args[1] == "1" || args[1] == "true");
        }
        game.renderer()->set_monochrome(on);
        log(std::string("Monochrome filter: ") + (on ? "ON" : "OFF"));
    }
    else if (verb == "bearings") {
        if (game.lost()) {
            game.set_lost(false);
            log("Bearings regained.");
        } else {
            log("You're not lost.");
        }
    }
    else if (verb == "warp" && args.size() >= 2) {
        if (args[1] == "random") {
            game.dev_command_warp_random();
            log("Warped to random map.");
        } else if (args[1] == "stamp" && args.size() >= 3) {
            static const std::pair<const char*, Tile> stamps[] = {
                {"ruins", Tile::OW_Ruins}, {"ship", Tile::OW_CrashedShip},
                {"outpost", Tile::OW_Outpost}, {"cave", Tile::OW_CaveEntrance},
                {"settlement", Tile::OW_Settlement},
            };
            bool found = false;
            for (const auto& [name, tile] : stamps) {
                if (args[2] == name) {
                    game.dev_command_warp_stamp(tile);
                    log("Warped to " + std::string(name) + " stamp.");
                    found = true;
                    break;
                }
            }
            if (!found) log("Unknown stamp: " + args[2]);
        } else {
            log("Usage: warp random | warp stamp <type>");
        }
    }
    else if (verb == "biome_test" && args.size() < 2) {
        log("Usage: biome_test <biome>");
        log("Type 'help' for biome list.");
    }
    else if (verb == "biome_test" && args.size() >= 2) {
        Biome biome;
        if (!parse_biome(args[1], biome)) {
            log("Unknown biome: " + args[1]);
            log("Options: grassland, forest, jungle, sandy, rocky, volcanic,");
            log("  aquatic, ice, fungal, crystal, corroded,");
            log("  alien_crystalline, alien_organic, alien_geometric,");
            log("  alien_void, alien_light, scarred_scorched, scarred_glassed");
            return;
        }
        int layer = 0;
        std::string poi_type;
        std::string poi_style;
        bool connected = false;
        float ruin_decay_override = -1.0f;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "settlement") {
                poi_type = "settlement";
            } else if (args[i] == "ruins") {
                poi_type = "ruins";
            } else if (args[i] == "outpost") {
                poi_type = "outpost";
            } else if (args[i] == "ship" || args[i] == "crashed_ship") {
                poi_type = "ship";
            } else if (args[i] == "pod" || args[i] == "freighter" || args[i] == "corvette") {
                if (poi_type.empty()) poi_type = "ship";
                poi_style = args[i];
            } else if (args[i] == "cave" || args[i] == "cave_entrance") {
                poi_type = "cave";
            } else if (args[i] == "natural" || args[i] == "mine" || args[i] == "excavation") {
                if (poi_type.empty()) poi_type = "cave";
                poi_style = args[i];
            } else if (args[i] == "connected") {
                connected = true;
            } else if (args[i] == "frontier") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "frontier";
            } else if (args[i] == "advanced") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "advanced";
            } else if (args[i] == "ruined") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "ruined";
            } else if (args[i] == "monolithic" || args[i] == "baroque" ||
                       args[i] == "crystal" || args[i] == "industrial") {
                if (poi_type.empty()) poi_type = "ruins";
                poi_style = args[i];
            } else {
                // Try as float first (for ruin decay), then int (for layer)
                try {
                    float f = std::stof(args[i]);
                    if (args[i].find('.') != std::string::npos) {
                        ruin_decay_override = f;
                    } else {
                        layer = static_cast<int>(f);
                    }
                } catch (...) {
                    log("Invalid arg: " + args[i]);
                    return;
                }
            }
        }
        std::string civ_name;
        if (poi_type == "ruins") civ_name = poi_style;
        game.dev_command_biome_test(biome, layer, poi_type,
                                    poi_type == "ruins" ? "" : poi_style,
                                    connected, civ_name, ruin_decay_override);
        std::string msg = "Biome test: " + args[1] + " (360x150)";
        if (poi_type == "settlement") {
            std::string style_display = poi_style.empty() ? "frontier" : poi_style;
            msg += " + settlement (" + style_display + ")";
        } else if (poi_type == "ruins") {
            msg += " + ruins";
            if (!civ_name.empty()) msg += " (" + civ_name + ")";
            if (ruin_decay_override >= 0.0f)
                msg += " decay=" + std::to_string(ruin_decay_override).substr(0, 4);
            if (connected) msg += " (connected)";
        } else if (poi_type == "outpost") {
            msg += " + outpost";
        } else if (poi_type == "ship") {
            msg += " + crashed ship";
            if (!poi_style.empty()) msg += " (" + poi_style + ")";
        } else if (poi_type == "cave") {
            msg += " + cave entrance";
            if (!poi_style.empty()) msg += " (" + poi_style + ")";
        }
        log(msg);
    }
    else if (verb == "dungen") {
        if (args.size() < 2) {
            log("usage: dungen <style_id> [civ_name]");
            log("  styles: simple_rooms");
            return;
        }

        dungeon::StyleId sid;
        if (!dungeon::parse_style_id(args[1], sid)) {
            log("unknown style: " + args[1]);
            return;
        }
        const std::string civ_name = args.size() >= 3 ? args[2] : "Natural";

        uint32_t seed = static_cast<uint32_t>(std::time(nullptr));
        game.dev_command_dungen(sid, civ_name, seed);

        log("generated dungeon: " + args[1] + " / " + civ_name);
    }
    else if (verb == "dumpmap") {
        std::string path = args.size() >= 2 ? args[1] : std::string{};
        std::string written = game.dev_command_dumpmap(path);
        if (written.empty()) log("dumpmap: failed to write file");
        else                 log("dumpmap: wrote " + written);
    }
    else if (verb == "give" && args.size() >= 3 && args[1] == "ship") {
        Item item;
        if (args[2] == "engine" || args[2] == "engine_coil_mk1") {
            item = build_engine_coil_mk1();
        } else {
            const LootEntry* entry = find_entry_by_identifier(args[2]);
            if (entry == nullptr || entry->category != Category::ShipComponent) {
                log("give ship: unknown component '" + args[2]
                    + "' (try: engine, hull_plate, shield_generator, navi_computer_mk2)");
                return;
            }
            item = build_by_def_id(entry->item_def_id);
        }
        log("Added " + item.name + " to ship cargo.");
        player.ship.cargo.push_back(std::move(item));
    }
    else if (verb == "give" && args.size() >= 2 && args[1] == "item") {
        // Subcommand: list mode (no identifier given)
        if (args.size() == 2) {
            const auto& entries = loot_table_all_entries();

            // Group by category for readability.
            std::map<Category, std::vector<const LootEntry*>> by_cat;
            for (const auto& e : entries) by_cat[e.category].push_back(&e);

            log("Loot table — " + std::to_string(entries.size()) + " items");
            for (const auto& [cat, list] : by_cat) {
                log("  [" + std::string(category_name(cat)) + "]");
                for (const auto* e : list) {
                    Item probe = build_by_def_id(e->item_def_id);
                    std::string line = "    " + e->identifier + "  " + probe.name
                                     + "  (" + std::string(rarity_short_name(e->min_rarity))
                                     + ".." + std::string(rarity_short_name(e->max_rarity))
                                     + ")";
                    log(line);
                }
            }
            return;
        }

        // Subcommand: spawn an item
        std::string_view identifier = args[2];
        Rarity rarity = Rarity::Common;
        int level = 1;

        if (args.size() >= 4) {
            auto parsed = parse_rarity_arg(args[3]);
            if (!parsed.has_value()) {
                log("give item: unknown rarity '" + args[3]
                    + "' (expected c/u/r/e/l or full name)");
                return;
            }
            rarity = *parsed;
        }
        if (args.size() >= 5) {
            try {
                level = std::stoi(args[4]);
            } catch (...) {
                log("give item: level must be an integer");
                return;
            }
            if (level < 1) level = 1;
        }

        const LootEntry* entry = find_entry_by_identifier(identifier);
        if (entry == nullptr) {
            log("give item: unknown identifier '" + std::string(identifier)
                + "' (try `give item` with no args to list)");
            return;
        }
        if (rarity < entry->min_rarity || rarity > entry->max_rarity) {
            log("give item: '" + entry->identifier + "' rarity out of range ("
                + std::string(rarity_short_name(entry->min_rarity)) + ".."
                + std::string(rarity_short_name(entry->max_rarity)) + ")");
            return;
        }

        Item item = build_by_def_id(entry->item_def_id);
        scale_item_to_rarity(item, rarity);
        scale_item_to_level(item, level);
        // No affixes — dev command stays deterministic.

        log("Added " + entry->identifier + " ("
            + std::string(rarity_short_name(rarity)) + ", lvl "
            + std::to_string(level) + ") to inventory.");
        add_to_inventory_stacked(player.inventory, std::move(item));
    }
    else if (verb == "give" && args.size() >= 3 && args[1] == "skill") {
        std::string name = args[2];
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        SkillId target = SkillId{};
        std::string display_name;
        bool found = false;
        for (const auto& cat : skill_catalog()) {
            std::string cn = cat.name;
            for (auto& c : cn) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string cn_snake = cn;
            for (auto& c : cn_snake) if (c == ' ' || c == '-') c = '_';
            if (cn == name || cn_snake == name ||
                ("cat_" + cn_snake) == name) {
                target = cat.unlock_id;
                display_name = "Cat_" + cat.name;
                found = true;
                break;
            }
            for (const auto& sk : cat.skills) {
                std::string sn = sk.name;
                for (auto& c : sn) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                std::string sn_snake = sn;
                for (auto& c : sn_snake) if (c == ' ' || c == '-') c = '_';
                if (sn == name || sn_snake == name) {
                    target = sk.id;
                    display_name = sk.name;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) { log("unknown skill: " + args[2]); return; }
        if (std::find(player.learned_skills.begin(), player.learned_skills.end(), target) ==
                player.learned_skills.end()) {
            player.learned_skills.push_back(target);
            log("Granted skill: " + display_name);
        } else {
            log("Already learned.");
        }
    }
    else if (verb == "give" && args.size() >= 3) {
        int val = 0;
        try { val = std::stoi(args[2]); } catch (...) {
            log("Invalid number: " + args[2]);
            return;
        }
        if (args[1] == "hp") {
            player.hp = val;
            if (player.hp > player.effective_max_hp()) player.hp = player.effective_max_hp();
            log("HP set to " + std::to_string(player.hp));
        } else if (args[1] == "xp") {
            player.xp = val;
            log("XP set to " + std::to_string(player.xp));
            game.dev_command_level_up();
        } else if (args[1] == "money") {
            player.money = val;
            log("Credits set to " + std::to_string(player.money));
        } else if (args[1] == "sp") {
            player.skill_points = val;
            log("SP set to " + std::to_string(player.skill_points));
        } else if (args[1] == "ap") {
            player.attribute_points = val;
            log("AP set to " + std::to_string(player.attribute_points));
        } else if (args[1] == "rep" && args.size() >= 4) {
            // give rep <faction words...> <value>
            // Last arg is the value, everything between is the faction name
            int rep_val = 0;
            try { rep_val = std::stoi(args.back()); } catch (...) {
                log("Invalid number: " + args.back());
                return;
            }
            std::string faction;
            for (size_t i = 2; i < args.size() - 1; ++i) {
                if (!faction.empty()) faction += " ";
                faction += args[i];
            }
            bool found = false;
            for (auto& fs : player.reputation) {
                if (fs.faction_name == faction) {
                    fs.reputation = rep_val;
                    found = true;
                    break;
                }
            }
            if (found)
                log("Reputation with " + faction + " set to " + std::to_string(rep_val));
            else
                log("Unknown faction: " + faction);
        } else {
            log("Unknown: give " + args[1]);
        }
    }
    else if (verb == "set" && args.size() >= 2) {
        if (args[1] == "invuln") {
            if (has_effect(player.effects, EffectId::Invulnerable)) {
                remove_effect(player.effects, EffectId::Invulnerable);
                log("Invulnerability OFF");
            } else {
                add_effect(player.effects, make_invulnerable_ge());
                log("Invulnerability ON");
            }
            rebuild_auras_from_sources(player);
        } else if (args[1] == "level" && args.size() >= 3) {
            int lvl = 0;
            try { lvl = std::stoi(args[2]); } catch (...) {
                log("Invalid number: " + args[2]);
                return;
            }
            while (player.level < lvl) {
                game.dev_command_level_up();
            }
            log("Level set to " + std::to_string(player.level));
        } else {
            log("Unknown: set " + args[1]);
        }
    }
    else if (verb == "effect" && args.size() >= 2) {
        if (args[1] == "clear") {
            player.effects.clear();
            log("All effects cleared.");
            rebuild_auras_from_sources(player);
        } else if (args[1] == "burn") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_burn_ge(dur, 1));
            rebuild_auras_from_sources(player);
            log("Burn applied for " + std::to_string(dur) + " ticks.");
        } else if (args[1] == "regen") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_regen_ge(dur, 1));
            rebuild_auras_from_sources(player);
            log("Regen applied for " + std::to_string(dur) + " ticks.");
        } else if (args[1] == "poison") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_poison_ge(dur, 1));
            rebuild_auras_from_sources(player);
            log("Poison applied for " + std::to_string(dur) + " ticks.");
        } else {
            log("Unknown effect: " + args[1]);
        }
    }
    else if (verb == "kill" && args.size() >= 2 && args[1] == "all") {
        int count = 0;
        for (auto& npc : game.npcs()) {
            if (npc.alive() && is_hostile_to_player(npc.faction, game.player())) {
                npc.hp = 0;
                ++count;
            }
        }
        game.dev_command_kill_hostiles();
        log("Killed " + std::to_string(count) + " hostile NPCs.");
    }
    else if (verb == "quest") {
        if (args.size() >= 2 && args[1] == "kill") {
            auto q = game.quests().generate_kill_quest(game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "fetch") {
            auto q = game.quests().generate_fetch_quest(game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "deliver") {
            auto q = game.quests().generate_deliver_quest("Merchant", game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "scout") {
            // Pick a random landable body from the current system
            auto& nav = game.world().navigation();
            std::string body;
            int body_idx = -1;
            uint32_t sys_id = 0;
            for (auto& sys : nav.systems) {
                if (sys.id == nav.current_system_id) {
                    generate_system_bodies(sys);
                    sys_id = sys.id;
                    std::vector<int> landable;
                    for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
                        if (sys.bodies[i].landable) landable.push_back(i);
                    }
                    if (!landable.empty()) {
                        int idx = std::uniform_int_distribution<int>(
                            0, static_cast<int>(landable.size()) - 1)(game.world().rng());
                        body_idx = landable[idx];
                        body = sys.bodies[body_idx].name;
                    }
                    break;
                }
            }
            if (body.empty()) body = "Unknown Body";
            auto q = game.quests().generate_scout_quest(body, game.world().rng());
            // Register map marker
            if (sys_id != 0 && body_idx >= 0) {
                q.target_system_id = sys_id;
                q.target_body_index = body_idx;
                LocationKey mk = {sys_id, body_idx, -1, false, -1, -1, 0};
                QuestLocationMeta meta;
                meta.quest_id = q.id;
                meta.quest_title = q.title;
                meta.target_system_id = sys_id;
                meta.target_body_index = body_idx;
                meta.remove_on_completion = true;
                game.world().quest_locations()[mk] = std::move(meta);
            }
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "story") {
            auto* sq = find_story_quest("story_missing_hauler");
            if (sq && !game.quests().has_active_quest("story_missing_hauler")) {
                auto q = sq->create_quest();
                q.arc_id = sq->arc_id();
                q.prerequisite_ids = sq->prerequisite_ids();
                q.reveal = sq->reveal_policy();
                log("Quest: " + q.title);
                log("  " + q.description);
                game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
                sq->on_accepted(game);
                log("Quest markers placed on star chart.");
            } else if (game.quests().has_active_quest("story_missing_hauler")) {
                log("Quest already active.");
            } else {
                log("Story quest not found.");
            }
        } else if (args.size() >= 2 && args[1] == "fixture") {
            // Register a debug def and plant it adjacent to the player.
            QuestFixtureDef def;
            def.id = "dev_smoke_fixture";
            def.glyph = '*';
            def.color = 135;
            def.prompt = "Play debug transmission";
            def.log_message = "You nudge the debug fixture. It beeps.";
            def.log_title = "DEV SMOKE TRANSMISSION";
            def.log_lines = {
                "This is line one of the debug transmission.",
                "",
                "Line two. The reveal should advance at thirty chars per second.",
                "Press Space to skip; press Esc to close; re-interact to replay.",
            };
            register_quest_fixture(def);

            FixtureData fd;
            fd.type = FixtureType::QuestFixture;
            fd.interactable = true;
            fd.passable = true;
            fd.quest_fixture_id = def.id;

            int fx = game.player().x + 1;
            int fy = game.player().y;
            if (fx >= 0 && fx < game.world().map().width() &&
                fy >= 0 && fy < game.world().map().height() &&
                game.world().map().passable(fx, fy) &&
                game.world().map().fixture_id(fx, fy) < 0) {
                game.world().map().add_fixture(fx, fy, fd);
                log("Planted dev_smoke_fixture at (" +
                    std::to_string(fx) + "," + std::to_string(fy) + ")");
            } else {
                log("No open tile adjacent to player for fixture.");
            }
        } else if (args.size() >= 3 && args[1] == "begin") {
            // Force-start a story quest by id, bypassing prereqs and dialog.
            const std::string& qid = args[2];
            if (game.quests().has_active_quest(qid)) {
                log("quest begin: already active");
                return;
            }
            auto* sq = find_story_quest(qid);
            if (!sq) {
                log("quest begin: no story quest with id '" + qid + "'");
                return;
            }
            auto q = sq->create_quest();
            // Mirror init_from_catalog: carry arc/prereq/reveal metadata so
            // journal rendering places the quest under its arc when it later
            // completes (instead of as a loose entry).
            q.arc_id = sq->arc_id();
            q.prerequisite_ids = sq->prerequisite_ids();
            q.reveal = sq->reveal_policy();
            game.quests().accept_quest(std::move(q), game.world().world_tick(),
                                       game.player());
            sq->on_accepted(game);
            log("Force-started quest: " + qid);
        } else if (args.size() >= 3 && args[1] == "complete") {
            // Tick every objective to its target so the quest is ready for
            // hand-in (e.g. talking to the giver NPC). Does NOT call
            // complete_quest — use "quest finish" for that.
            const std::string& qid = args[2];
            Quest* q = game.quests().find_active(qid);
            if (!q) {
                log("quest complete: '" + qid + "' is not active");
                return;
            }
            for (auto& obj : q->objectives) obj.current_count = obj.target_count;
            log("Objectives filled for: " + qid + " (hand in to complete)");
        } else if (args.size() >= 3 && args[1] == "finish") {
            // Force-complete an active quest by id (fires on_completed + DAG).
            const std::string& qid = args[2];
            if (!game.quests().has_active_quest(qid)) {
                log("quest finish: '" + qid + "' is not active");
                return;
            }
            // Tick every objective to its target so complete_quest sees it done
            // and its reward / journal paths run normally.
            if (Quest* q = game.quests().find_active(qid)) {
                for (auto& obj : q->objectives) obj.current_count = obj.target_count;
            }
            game.quests().complete_quest(qid, game, game.world().world_tick());
            log("Force-finished quest: " + qid);
        } else {
            log("Usage: quest kill|fetch|deliver|scout|story|fixture");
            log("       quest begin <id> | quest complete <id> | quest finish <id>");
        }
    }
    else if (verb == "history") {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        // Close console and open the lore viewer
        open_ = false;
        game.open_lore_viewer();
    }
    else if (verb == "lore" && args.size() >= 2) {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        auto& nav = game.world().navigation();

        // Match a lore feature name to a system predicate
        auto match_system = [](const std::string& feature, const LoreAnnotation& la) -> bool {
            if (feature == "beacon")         return la.beacon;
            if (feature == "megastructure")  return la.has_megastructure;
            if (feature == "terraformed")    return la.terraformed;
            if (feature == "scarred" || feature == "scar")
                return la.battle_site || la.weapon_test_site;
            if (feature == "battle")         return la.battle_site;
            if (feature == "weapon")         return la.weapon_test_site;
            if (feature == "plague")         return la.plague_origin;
            if (feature == "tier3")          return la.lore_tier >= 3;
            if (feature == "tier2")          return la.lore_tier >= 2;
            if (feature == "tier1")          return la.lore_tier >= 1;
            return false;
        };

        if (args[1] == "list") {
            // List all systems with lore features
            int count = 0;
            for (const auto& sys : nav.systems) {
                const auto& la = sys.lore;
                if (la.lore_tier == 0) continue;
                std::string flags;
                if (la.beacon)          flags += " [beacon]";
                if (la.has_megastructure) flags += " [mega]";
                if (la.terraformed)     flags += " [terraform]";
                if (la.battle_site)     flags += " [battle]";
                if (la.weapon_test_site) flags += " [weapon]";
                if (la.plague_origin)   flags += " [plague]";
                if (la.scar_count > 0)  flags += " [scars:" + std::to_string(la.scar_count) + "]";
                log("  #" + std::to_string(sys.id) + " " + sys.name +
                    " (tier " + std::to_string(la.lore_tier) + ")" + flags);
                if (++count >= 30) { log("  ... (truncated)"); break; }
            }
            if (count == 0) log("No lore-annotated systems found.");
            else log(std::to_string(count) + " systems shown.");
        }
        else if (args[1] == "warp" && args.size() >= 3) {
            const std::string& feature = args[2];
            // Find first matching system
            for (const auto& sys : nav.systems) {
                if (match_system(feature, sys.lore)) {
                    game.dev_command_warp_to_system(sys.id);

                    std::string flags;
                    if (sys.lore.beacon) flags += " beacon";
                    if (sys.lore.has_megastructure) flags += " mega";
                    if (sys.lore.terraformed) flags += " terraform";
                    if (sys.lore.battle_site) flags += " battle";
                    if (sys.lore.weapon_test_site) flags += " weapon";
                    if (sys.lore.scar_count > 0) flags += " scars:" + std::to_string(sys.lore.scar_count);
                    log("Warped to " + sys.name + " (#" + std::to_string(sys.id) +
                        ", tier " + std::to_string(sys.lore.lore_tier) + ")" + flags);
                    return;
                }
            }
            log("No system found with feature: " + feature);
        }
        else {
            log("Usage: lore list | lore warp <feature>");
            log("Features: beacon, megastructure, terraformed, scarred, battle, weapon, plague, tier1, tier2, tier3");
        }
    }
    else if (verb == "chart") {
        auto& nav = game.world().navigation();
        if (args.size() >= 2 && args[1] == "create") {
            std::string kind = "asteroid";
            std::string name = "Custom";
            if (args.size() == 3) {
                // Single extra arg: name only (back-compat, `chart create Foo`).
                name = args[2];
            } else if (args.size() >= 4) {
                // Two extra args: <kind> <name>. Kind must be known.
                std::string a2 = args[2];
                if (a2 != "asteroid" && a2 != "scar" &&
                    a2 != "rock" && a2 != "neutron" && a2 != "derelict") {
                    log("chart create: unknown kind '" + a2 +
                        "' (expected asteroid|scar|rock|neutron|derelict)");
                    return;
                }
                kind = a2;
                name = args[3];
            }

            auto coords = pick_coords_near(nav, nav.current_system_id,
                                           2.0f, 5.0f, game.world().rng());
            if (!coords) {
                log("chart create: couldn't find a spot near current system");
                return;
            }

            CustomSystemSpec spec;
            spec.name = name;
            spec.gx = coords->first;
            spec.gy = coords->second;
            spec.star_class = StarClass::ClassM;
            spec.discovered = true;

            if (kind == "asteroid") {
                spec.bodies = { make_landable_asteroid(name + " Rock") };
            } else if (kind == "scar") {
                spec.bodies = { make_scar_planet(name + " Prime") };
            } else if (kind == "neutron") {
                spec.star_class = StarClass::Neutron;
                spec.bodies = { make_landable_asteroid(name + " Fragment") };
            } else if (kind == "derelict") {
                spec.has_station = true;
                spec.station.type = StationType::Abandoned;
                spec.station.specialty = StationSpecialty::Generic;
                spec.station.name = name + " Outpost";
                spec.star_class = StarClass::ClassG;
                spec.bodies = {};
            } else { // "rock"
                CelestialBody b;
                b.name = name + " Rock";
                b.type = BodyType::Rocky;
                b.atmosphere = Atmosphere::None;
                b.temperature = Temperature::Cold;
                b.size = 2;
                b.landable = true;
                b.danger_level = 1;
                b.day_length = 200;
                spec.bodies = { std::move(b) };
            }

            uint32_t id = add_custom_system(nav, std::move(spec));
            log("Created custom " + kind + " system '" + name + "' id=" +
                std::to_string(id) + " at (" + std::to_string(coords->first) +
                ", " + std::to_string(coords->second) + ")");
        } else if (args.size() >= 2 && args[1] == "reveal") {
            if (args.size() < 3) { log("Usage: chart reveal <name-substring>"); return; }
            const std::string& needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (reveal_system(nav, s.id)) {
                        log("Revealed '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else if (args.size() >= 2 && args[1] == "hide") {
            if (args.size() < 3) { log("Usage: chart hide <name-substring>"); return; }
            const std::string& needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (hide_system(nav, s.id)) {
                        log("Hid '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else {
            log("Usage: chart create [kind] [name]|reveal <name>|hide <name>");
        }
    }
    else if (verb == "spawn") {
        // Plan 5 Tasks 16-17 — unified dispatcher. Subkind-explicit forms
        // (`spawn npc <role>`, `spawn fixture <Type>`, etc.) win first; the
        // bare `spawn <name>` falls back to auto-detect (NPC role first,
        // then FixtureType).
        if (args.size() < 2) {
            log("usage: spawn <name>");
            log("       spawn npc <role>");
            log("       spawn fixture <FixtureType>");
            log("       spawn ice <white|gray|black>");
            log("       spawn trap <kind>");
            log("note: 'spawn fixture' on a live LAN performs a destructive reset of");
            log("      that LAN's persistence (cracked firewalls, looted nodes).");
            return;
        }
        if (args[1] == "npc" && args.size() >= 3)     { cmd_spawn_npc(*this, game, args[2]);     return; }
        if (args[1] == "fixture" && args.size() >= 3) { cmd_spawn_fixture(*this, game, args[2]); return; }
        if (args[1] == "ice" && args.size() >= 3)     { cmd_spawn_ice(*this, game, args[2]);     return; }
        if (args[1] == "trap" && args.size() >= 3)    { cmd_spawn_trap(*this, game, args[2]);    return; }

        // Auto-detect: NPC role first, then FixtureType.
        if (npc_role_exists(args[1])) { cmd_spawn_npc(*this, game, args[1]); return; }
        if (auto ft = fixture_type_from_name(args[1])) {
            cmd_spawn_fixture_with_type(*this, game, *ft);
            return;
        }
        log("spawn: unknown name '" + args[1] + "' (not an NPC role or FixtureType)");
    }
    else if (verb == "fixtures") {
        // Dump quest-fixture placements across all quest_locations PLUS
        // notable navigation fixtures (stairs, hatches, anything with a
        // quest_fixture_id) on the current map, so the dev can see
        // what's been stamped where.
        const auto& qlocs = game.world().quest_locations();
        int total = 0;
        if (!qlocs.empty()) {
            log("-- quest_locations metadata --");
            for (const auto& [key, meta] : qlocs) {
                auto [sys, b, m, stn, ow_x, ow_y, d] = key;
                for (const auto& p : meta.fixtures) {
                    ++total;
                    std::string loc = "sys=" + std::to_string(sys) +
                                      " body=" + std::to_string(b) +
                                      (m >= 0 ? " moon=" + std::to_string(m) : "") +
                                      (stn ? " [station]" : "") +
                                      (d > 0 ? " depth=" + std::to_string(d) : "");
                    if (p.x < 0 || p.y < 0) {
                        log(p.fixture_id + " — " + loc + " — (unplaced)");
                    } else {
                        log(p.fixture_id + " — " + loc + " — tile (" +
                            std::to_string(p.x) + "," + std::to_string(p.y) + ")");
                    }
                }
            }
        }

        // Current map scan for navigation fixtures and any quest-tagged
        // fixture that doesn't appear in quest_locations (e.g. the
        // Conclave Archive hatch, programmatically placed by poi_phase).
        const auto& m = game.world().map();
        int map_shown = 0;
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& f = m.fixture(fidx);
                const char* tname = nullptr;
                switch (f.type) {
                    case FixtureType::StairsUp:     tname = "stairs_up";     break;
                    case FixtureType::StairsDown:   tname = "stairs_down";   break;
                    case FixtureType::StairsDownPrecursor: tname = "stairs_down_precursor"; break;
                    case FixtureType::DungeonHatch: tname = "hatch";         break;
                    case FixtureType::QuestFixture: tname = "quest_fixture"; break;
                    default: break;
                }
                if (!tname) continue;
                if (map_shown == 0) log("-- current map fixtures --");
                std::string line = std::string(tname) + " (" +
                                   std::to_string(x) + "," +
                                   std::to_string(y) + ")";
                if (!f.quest_fixture_id.empty())
                    line += " id=" + f.quest_fixture_id;
                log(line);
                ++map_shown;
            }
        }

        if (total == 0 && map_shown == 0)
            log("No quest fixtures declared and no navigation fixtures on current map.");
    }
    else if (verb == "mapfix" && args.size() == 1) {
        // List every placed fixture on the current map.
        const auto& m = game.world().map();
        int shown = 0;
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& f = m.fixture(fidx);
                std::string line = "(" + std::to_string(x) + "," +
                                   std::to_string(y) + ") type=" +
                                   std::to_string(static_cast<int>(f.type));
                if (!f.quest_fixture_id.empty())
                    line += " id=" + f.quest_fixture_id;
                log(line);
                ++shown;
            }
        }
        if (shown == 0) log("No fixtures on this map.");
        else log("Total: " + std::to_string(shown));
    }
    else if (verb == "regen" && args.size() == 1) {
        // Purge the location cache so the current map and all dungeon
        // levels regenerate on next entry. Useful when generator logic
        // has changed and cached maps are stale. Does NOT affect the
        // current map you're standing on until you leave and come back.
        size_t n = game.world().location_cache().size();
        game.world().location_cache().clear();
        log("Purged " + std::to_string(n) + " cached locations. Leave and re-enter any map to regenerate.");
    }
    else if (verb == "tp" && args.size() >= 3) {
        // Teleport player to (x, y) on the current map.
        int tx = std::atoi(args[1].c_str());
        int ty = std::atoi(args[2].c_str());
        if (tx < 0 || tx >= game.world().map().width() ||
            ty < 0 || ty >= game.world().map().height()) {
            log("tp: out of bounds");
            return;
        }
        game.player().x = tx;
        game.player().y = ty;
        log("Teleported to (" + std::to_string(tx) + "," + std::to_string(ty) + ")");
    }
    else if (verb == "tp" && args.size() == 2) {
        // Teleport targets, in priority order:
        //   (a) short keyword for FixtureType — "stairs_down", "stairs_up",
        //       "hatch" — jumps to the first matching fixture on the map.
        //   (b) quest fixture id in QuestLocationMeta.fixtures.
        //   (c) quest_fixture_id set on an actual fixture on the current
        //       map (for programmatically placed fixtures).
        const std::string& fid = args[1];
        FixtureType ft_target = FixtureType::Table;
        bool use_type = true;
        if      (fid == "stairs_down") ft_target = FixtureType::StairsDown;
        else if (fid == "stairs_down_precursor") ft_target = FixtureType::StairsDownPrecursor;
        else if (fid == "stairs_up")   ft_target = FixtureType::StairsUp;
        else if (fid == "hatch")       ft_target = FixtureType::DungeonHatch;
        else use_type = false;

        if (use_type) {
            const auto& m = game.world().map();
            for (int y = 0; y < m.height(); ++y) {
                for (int x = 0; x < m.width(); ++x) {
                    int fidx = m.fixture_id(x, y);
                    if (fidx < 0) continue;
                    if (m.fixture(fidx).type != ft_target) continue;
                    game.player().x = x;
                    game.player().y = y;
                    log("Teleported to " + fid + " at (" +
                        std::to_string(x) + "," + std::to_string(y) + ")");
                    return;
                }
            }
            log("tp: no " + fid + " on this map");
            return;
        }
        for (const auto& [key, meta] : game.world().quest_locations()) {
            for (const auto& p : meta.fixtures) {
                if (p.fixture_id != fid) continue;
                if (p.x < 0 || p.y < 0) {
                    log("tp: fixture '" + fid + "' hasn't been placed yet (enter its map first)");
                    return;
                }
                if (p.x >= game.world().map().width() ||
                    p.y >= game.world().map().height()) {
                    log("tp: fixture '" + fid + "' is on a different map");
                    return;
                }
                game.player().x = p.x;
                game.player().y = p.y;
                log("Teleported to '" + fid + "' at (" +
                    std::to_string(p.x) + "," + std::to_string(p.y) + ")");
                return;
            }
        }
        // Pass 2: scan the current map's placed fixtures.
        const auto& m = game.world().map();
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& f = m.fixture(fidx);
                if (f.quest_fixture_id != fid) continue;
                game.player().x = x;
                game.player().y = y;
                log("Teleported to '" + fid + "' at (" +
                    std::to_string(x) + "," + std::to_string(y) + ")");
                return;
            }
        }
        log("tp: fixture '" + fid + "' not found");
    }
    else if (verb == "lan-info") {
        const auto& m = game.world().map();
        const auto& meta = game.world().lan_metadata();
        log("=== LAN diagnostic ===");
        log("active map: '" + m.location_name() + "' (type " +
            std::to_string(static_cast<int>(m.map_type())) + ")");
        log("region_label='" + meta.region_label + "'");
        log("display_name='" + meta.display_name + "'");
        log("flavour=" + std::to_string(static_cast<int>(meta.flavour)) +
            " connected=" + std::string(meta.connected ? "yes" : "no"));
    }
    else if (verb == "detection") {
        if (args.size() < 2) {
            log("usage: detection <0..100>  (sets active zone Detection counter)");
            return;
        }
        int n = 0;
        try { n = std::stoi(args[1]); } catch (...) {
            log("invalid number");
            return;
        }
        int cur = game.hacking().detection();
        game.hacking().add_detection(n - cur);
        log("Detection = " + std::to_string(game.hacking().detection()));
    }
    else if (verb == "jack-out") {
        if (!game.hacking().jacked_in()) {
            log("Not jacked in.");
            return;
        }
        game.hacking().jack_out(game, JackOutKind::Voluntary);
        log("Jacked out.");
    }
    else if (verb == "jack") {
        // Usage: jack [kind] [tier] [seed]
        //   kind: empty | door | vending | camera | atm | turret |
        //         elevator | traffic | corpse | npc | mainframe | blackwall | combat
        //   tier: 1..5 (default 1)
        //   seed: uint32 (default world_tick)
        TargetDescriptor d;
        d.kind = NetspaceTargetKind::Empty;
        d.tier = 1;
        d.seed = static_cast<uint32_t>(game.world().world_tick());

        if (args.size() >= 2) {
            const std::string& k = args[1];
            if      (k == "empty")     d.kind = NetspaceTargetKind::Empty;
            else if (k == "door")      d.kind = NetspaceTargetKind::Door;
            else if (k == "vending")   d.kind = NetspaceTargetKind::VendingMachine;
            else if (k == "camera")    d.kind = NetspaceTargetKind::Camera;
            else if (k == "atm")       d.kind = NetspaceTargetKind::Atm;
            else if (k == "turret")    d.kind = NetspaceTargetKind::Turret;
            else if (k == "elevator")  d.kind = NetspaceTargetKind::Elevator;
            else if (k == "traffic")   d.kind = NetspaceTargetKind::TrafficLight;
            else if (k == "corpse")    d.kind = NetspaceTargetKind::Corpse;
            else if (k == "npc")       d.kind = NetspaceTargetKind::NpcHead;
            else if (k == "mainframe") d.kind = NetspaceTargetKind::Mainframe;
            else if (k == "blackwall") d.kind = NetspaceTargetKind::BlackwallTear;
            else if (k == "combat")    d.kind = NetspaceTargetKind::CombatArena;
            else { log("unknown netspace kind: " + k); return; }
        }
        if (args.size() >= 3) {
            try { d.tier = std::clamp(std::stoi(args[2]), 1, 5); }
            catch (...) {}
        }
        if (args.size() >= 4) {
            try { d.seed = static_cast<uint32_t>(std::stoul(args[3])); }
            catch (...) {}
        }
        game.hacking().jack_in(game, d);
    }
    else if (verb == "trace") {
        if (args.size() < 2) {
            log("usage: trace <0..100>  (sets Grid Trace counter)");
            return;
        }
        if (!game.hacking().jacked_in()) {
            log("Not jacked in.");
            return;
        }
        int n = 0;
        try { n = std::stoi(args[1]); } catch (...) { log("invalid number"); return; }
        n = std::clamp(n, 0, 100);
        game.hacking().session()->trace = n;
        log("Trace = " + std::to_string(n));
    }
    else if (verb == "net") {
        if (!game.hacking().jacked_in()) { log("Not jacked in."); return; }
        auto* s = game.hacking().session_mut();
        if (args.size() >= 2 && args[1] == "state") {
            const char* n[] = {"Opening","Stable","Stressed","Hunted",
                               "Critical","BlackIceTakeover","Blackwall","Closing"};
            log("window_state=" + std::string(n[(int)s->netspace.window_state])
                + " trace=" + std::to_string(s->trace)
                + " seq=" + std::to_string((int)s->window_seq.kind));
        }
        else if (args.size() >= 3 && args[1] == "force") {
            const std::string& w = args[2];
            WindowState ws = s->netspace.window_state;
            if      (w=="stable")   ws=WindowState::Stable;
            else if (w=="stressed") ws=WindowState::Stressed;
            else if (w=="hunted")   ws=WindowState::Hunted;
            else if (w=="critical") ws=WindowState::Critical;
            else if (w=="blackwall")ws=WindowState::Blackwall;
            else { log("usage: net force stable|stressed|hunted|critical|blackwall"); return; }
            s->netspace.window_state = ws;
            log("forced window_state=" + w + " (will re-derive on next world tick unless held)");
        }
        else if (args.size() >= 2 && args[1] == "selftest") {
            run_net_selftest(*this);
        }
        else if (args.size() >= 2 && args[1] == "takeover") {
            game.hacking().request_takeover();
        }
        else if (args.size() >= 2 && args[1] == "combat") {
            bool on = !(args.size() >= 3 && args[2] == "off");
            s->combat_manual = on;
            astra::update_combat_lock(*s);
            log(std::string("combat ") + (on ? "ON (manual)" : "OFF"));
        }
        else {
            log("usage: net state | net force <s> | net selftest | net takeover | net combat on|off");
        }
    }
    else if (verb == "spawn-ice") {
        if (args.size() < 2) {
            log("usage: spawn-ice <white|gray|black>");
            return;
        }
        if (!game.hacking().jacked_in()) {
            log("Not jacked in.");
            return;
        }
        IceColor color;
        int hp;
        if      (args[1] == "white") { color = IceColor::White; hp = 1; }
        else if (args[1] == "gray")  { color = IceColor::Gray;  hp = 2; }
        else if (args[1] == "black") { color = IceColor::Black; hp = 4; }
        else { log("unknown color: " + args[1]); return; }

        auto* sess = game.hacking().session();
        // Place adjacent to avatar in any passable, unoccupied tile.
        static const int dxs[4] = { 0, 0, -1, 1 };
        static const int dys[4] = { -1, 1, 0, 0 };
        for (int d = 0; d < 4; ++d) {
            int nx = sess->avatar_x + dxs[d];
            int ny = sess->avatar_y + dys[d];
            if (!sess->netspace.passable(nx, ny)) continue;
            bool occupied = false;
            for (auto& i : sess->ice) if (i.x == nx && i.y == ny) { occupied = true; break; }
            if (occupied) continue;
            Ice ice;
            ice.x = nx; ice.y = ny; ice.color = color; ice.hp = hp;
            sess->ice.push_back(ice);
            log("Spawned " + args[1] + " ICE.");
            return;
        }
        log("No adjacent passable tile to spawn ICE.");
    }
    else if (verb == "dump-precursor") {
        // Find the nearest Precursor console hackable on the current map.
        const auto& m  = game.world().map();
        const auto& px = game.player().x;
        const auto& py = game.player().y;
        const Hackable* nearest = nullptr;
        int best_dist = INT_MAX;
        int w = m.width();
        int h = m.height();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& fd = m.fixture(fidx);
                if (!fd.cyber) continue;
                if (!has_tag(fd.cyber->tags, HackTag::AlienTech)) continue;
                int dist = std::abs(x - px) + std::abs(y - py);
                if (dist < best_dist) { best_dist = dist; nearest = &*fd.cyber; }
            }
        }
        if (!nearest) {
            log("no Precursor console on this map");
            return;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "fragments: %zu / progress: %d",
                      nearest->lore_fragments.size(), nearest->soul_mirror_progress);
        log(buf);
        for (const auto& f : nearest->lore_fragments) {
            log("  - " + f.archive_id + (f.committed ? " [done]" : ""));
        }
    }
    else if (verb == "sync-soul") {
        // Force-start a manual Soul Mirror channel on the nearest Precursor console.
        const auto& m  = game.world().map();
        const int   px = game.player().x;
        const int   py = game.player().y;
        Hackable* console = nullptr;
        int best_dist = INT_MAX;
        int w = m.width();
        int h = m.height();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& fd = m.fixture(fidx);
                if (!fd.cyber) continue;
                if (!has_tag(fd.cyber->tags, HackTag::AlienTech)) continue;
                int dist = std::abs(x - px) + std::abs(y - py);
                if (dist < best_dist) {
                    best_dist = dist;
                    console = &*game.world().map().fixture_mut(fidx).cyber;
                }
            }
        }
        if (!console) { log("no Precursor console nearby"); return; }
        soul_mirror::begin_active(game, *console);
        log("Forced Sync Soul start.");
    }
    else if (verb == "unequip-implant") {
        if (args.size() != 1) { log("usage: unequip-implant <0|1>"); return; }
        int slot = std::atoi(args[0].c_str());
        if (slot < 0 || slot >= astra::Player::IMPLANT_SLOTS) {
            log("slot out of range");
            return;
        }
        auto& s = game.player().implants[slot];
        if (!s) { log("slot is empty"); return; }
        add_to_inventory_stacked(game.player().inventory, *s);
        s = std::nullopt;
        log("Unequipped implant from slot " + args[0]);
    }
    else if (verb == "unlock-anchor") {
        grant_skill(game.player(), SkillId::ConsciousnessAnchor);
        apply_skill_side_effects(game, SkillId::ConsciousnessAnchor);
        log("ConsciousnessAnchor unlocked. Base seeded.");
    }
    else if (verb == "rebirth") {
        game.rebirth().begin();
        log("Sgr A* rebirth modal opened.");
    }
    else if (verb == "rebirth-reset") {
        delete_consciousness();
        log("consciousness.dat cleared.");
    }
    // :netprog — load a test authored chain (Loop x3 { Volt }) into deck slot 0
    // so the slice-3b AST bridge can be exercised in-game:
    // jack in, press 1 near ICE -> iter 1/3..3/3, ICE takes damage.
    else if (verb == "netprog") {
        auto* ds = game.player().equipment.equipped_cyberdeck();
        if (!ds || !*ds || !(*ds)->deck) {
            log("netprog: no cyberdeck equipped");
            return;
        }
        auto& dk = *(*ds)->deck;
        astra::ProgramNode prod;
        prod.fragment = astra::FragmentId::Volt;
        astra::ProgramNode loop_node;
        loop_node.fragment = astra::FragmentId::Loop;
        loop_node.param = 3;
        loop_node.body.push_back(prod);
        std::vector<astra::ProgramNode> chain;
        chain.push_back(loop_node);
        astra::CompiledProgram cp = astra::compile_program(chain, "test-loop3");
        dk.loaded[0].program_def_id = 0;
        dk.loaded[0].compiled = cp;
        log("netprog: loaded test-loop3 (Loop x3 { Volt }) into slot 1; jack in and press 1 near ICE.");
    }
    else {
        log("Unknown command: " + verb + ". Type 'help' for commands.");
    }
}

void DevConsole::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_) return;

    int con_h = std::min(20, screen_h / 2);
    if (con_h < 10) con_h = 10;
    Rect bounds{0, screen_h - con_h, screen_w, con_h};
    UIContext outer(renderer, bounds);
    auto ctx = outer.panel({
        .title = "Console",
        .footer = "[Esc] Close  [Enter] Execute  [Up/Down] History  [PgUp/PgDn] Scroll",
    });

    int content_h = ctx.height();
    int input_row = content_h - 1;

    // Input prompt with in-place cursor
    std::string display = input_;
    if (cursor_ >= display.size()) display += ' ';
    ctx.styled_text({.x = 0, .y = input_row, .segments = {
        {"> ", UITag::TextAccent},
        {display, UITag::TextBright},
    }});
    char cursor_ch = (cursor_ < input_.size()) ? input_[cursor_] : ' ';
    ctx.put(2 + static_cast<int>(cursor_), input_row, cursor_ch,
            Color::Black, Color::White);

    // Scrollable output
    int out_rows = input_row;
    int total = static_cast<int>(output_.size());

    int max_scroll = total - out_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_ > max_scroll) scroll_ = max_scroll;

    int end = total - scroll_;
    int start = end - out_rows;
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < end && row < out_rows; ++i, ++row) {
        const auto& line = output_[i];
        // Echo lines (starting with >) show bright, output shows dim
        UITag tag = (line.size() >= 2 && line[0] == '>') ? UITag::TextBright : UITag::TextDim;
        ctx.text({.x = 0, .y = row, .content = line, .tag = tag});
    }
}

} // namespace astra
