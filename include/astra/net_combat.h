#pragma once

#include "astra/net_ice.h"
#include "astra/net_ice_telegraph.h"

#include <cstddef>
#include <string>
#include <vector>

namespace astra {

class Game; // forward declare
struct EffectSpec;
struct NetInFlight;
struct NetSession;
struct Netspace;

// Bump-attack melee tunables.
inline constexpr int kNetMeleeDamage   = 3;
inline constexpr int kNetMeleeRamCost  = 0;
inline constexpr int kNetMeleeHeatCost = 0;
inline constexpr int kNetMeleeRange    = 1;

// XP grants per ICE kill into the player's main XP pool.
inline constexpr int kXpIceWhite = 4;
inline constexpr int kXpIceGray  = 8;
inline constexpr int kXpIceBlack = 16;

// Phase 5 S3: non-Black ICE ranged-cast tuning — THE S3 balance knob.
// Default = the "Conservative" option (1 dmg / single-target / 3-beat
// cooldown). Combined with pipe travel (seg_len beats) this is ~1 hit
// per 4-7 beats on the 3-HP avatar pool. Bump kIceGrayCastDamage to >=2
// to make BRACE's halve-min-1 actually mitigate (see mechanics.md).
inline constexpr int kIceGrayCastDamage = 1;   // single-target damage per cast
inline constexpr int kIceGrayCastRadius = 0;   // 0 = single target
inline constexpr int kIceCastCadence    = 3;   // cooldown beats set after a cast

// Grant netspace-kill XP into the main player pool and check for level-up.
// No-op if amount <= 0.
void grant_net_xp(Game& game, int amount);

// Phase 5 slice 3b: apply a compiled program's EffectSpec to the
// netspace. Honors damage + radius (ICE within the radius box of
// (tx,ty) take spec.damage via net_ice::damage + kill/XP). Statuses
// (jitter/slag/warp), DRAIN (returns_hp_pct) and relay_hops have NO
// ICE representation and are an explicit documented no-op in 3b.
// Returns a one-line summary for the log.
std::string apply_effect_in_net(Game& game, NetSession& s,
                                const EffectSpec& spec, int tx, int ty);

// Phase 5 slice 4: resolve a payload that has reached the far node of
// its pipe. If the target cell is a breakwall, demotes it by one density
// step; otherwise applies the program's compiled EffectSpec or ProgramDef
// effect at (f.target_x, f.target_y). Logs result into s.
void impact_resolve(Game& game, NetSession& s, NetInFlight& f);

// Phase 5 S3: apply a hostile payload's EffectSpec to the AVATAR (not
// ICE). Honors the Invulnerable dev-cheat and BRACE (s.brace_turns>0 =>
// halve, min 1, consumed). Sets last_killer_color (Gray-attributed in
// S3) unless already Black, so tick_grid's HP<=0 check routes
// NonBlackDeath. Returns a one-line log summary ("" = no log).
std::string apply_effect_at_avatar(Game& game, NetSession& s,
                                   const EffectSpec& spec, IceColor by);

// Phase 5 S3: per-world-turn ranged-cast pass for non-Black caster ICE
// (S3: Gray only). No-op unless s.combat_mode==Combat. For each live,
// un-charmed Gray sitting in a node directly pipe-connected to the
// avatar (the S1 lock topology), enqueues a hostile single-target
// NetInFlight down that pipe (ICE-end -> avatar-end) and arms its
// kIceCastCadence cooldown. Game-free (enqueue/topology only). Called
// from HackingSystem::tick_grid right after net_ice::tick_all.
void ice_cast_tick(NetSession& s);

// Phase 5 S4: which s.ice indices an Impact of `spec` at (tx,ty)
// damages — node-scoped on the room containing (tx,ty): AOE
// (radius>=1) => all live ICE in that room; single (radius==0) => the
// closest live ICE in it (Chebyshev to (tx,ty), tiebreak by index);
// {} if none / spec.damage<=0. Defensive legacy Chebyshev-box
// fallback only if room_index_at fails. Game-free.
std::vector<std::size_t> net_node_targets(const NetSession& s,
                                          const EffectSpec& spec,
                                          int tx, int ty);

// Phase 5 S4: the in-flight beat, decomposed (called in this order
// from tick_grid). advance = launch-beat + spawn + ++seg for travel
// payloads (legacy/self skipped). resolve_pipe_collisions = opposing
// same-pipe payloads that met-or-crossed annihilate / winner carries
// the damage difference. Both Game-free. resolve_inflight_impacts =
// the exact legacy/self resolve (unchanged) + travel Impact at
// seg>=seg_len + RAM-return/erase.
void net_inflight_advance(NetSession& s);
void resolve_pipe_collisions(NetSession& s);
void resolve_inflight_impacts(Game& game, NetSession& s);

// Phase 5 S5: pipe-graph BFS — returns the netspace.pipes index of the
// first-hop pipe on a shortest path from from_room to to_room, or -1
// if no path / from_room == to_room / out-of-range. Neighbor order is
// pipes-index ascending => deterministic shortest-path tiebreak.
// Game-free.
int pipe_graph_next_hop(const Netspace& ns, int from_room, int to_room);

// Phase 5 S5: per-world-turn Black-walker step. For each live, un-
// charmed Black ICE: at a node, BFS-picks the next pipe toward the
// avatar's room (pipe_graph_next_hop) and steps into its first cell
// this same beat (no idle launch beat); in-pipe, advances one cell.
// On arrival at an intermediate room, immediately picks the next hop
// (still one cell of progress per beat). On arrival in the avatar's
// room, sets s.black_reached_player_node = true (consumed by tick_grid
// to route jack_out(BlackIceDeath) -> unconditional GameOver). Game-
// free; tick_grid is the only Game-touching caller.
void black_walker_tick(NetSession& s);

// Phase 5 S6: RUN core-action's Game-touching autopilot loop. Ticks
// game.hacking().tick_grid(game) up to kRunAutopilotCap beats, halting
// the first beat a live Black walker is one pipe-hop away from the
// avatar's room (edge-trigger: was-not-adjacent last beat, IS-adjacent
// now). Also halts on game-over (jack-out, GameOver) or if the cap is
// reached (safety; never reached in practice). Single keypress = one
// committed turn from the outer caller's perspective.
void core_action_run(Game& game);

}  // namespace astra
