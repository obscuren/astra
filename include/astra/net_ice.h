#pragma once

#include "astra/net_ice_telegraph.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace astra {

enum class IceColor : uint8_t { White, Gray, Black };

// Phase 5 S7c.1: typed daemon kinds layered on top of IceColor archetype.
// Defined here (rather than in daemon.h) because Ice::kind needs the
// complete enum for its default initializer, and daemon.h includes
// net_ice.h to read IceColor. The DaemonDef struct + daemon_def() lookup
// live in daemon.h.
enum class DaemonKind : std::uint16_t {
    Watchdog = 0,   // legacy default: a generic Gray-archetype caster
    Lock,           // door: room-fill defender
    Bolt,           // door: micro-boss
    // Phase 5 S7c.2 — grammar daemon sweep
    VaultFw,        // ATM: VAULT breakwall (RoomFill)
    TellrK9,        // ATM: vault enforcer (Glyph, boss)
    FraudExe,       // ATM: FRAUD trigger spawn
    PktDat,         // ATM: PACKETS trigger swarm
    LensCam,        // CAMERA: per-lens scanner
    ArchiveK9,      // CAMERA: archive enforcer (boss)
    MemryKex,       // CORPSE: MEMORY corruption (RoomFill)
    FloorK9,        // ELEVATOR: per-floor patrol
    ScrtyFw,        // ELEVATOR: SECURITY enforcer above spine gate (Glyph, S7d)
    HouseK9,        // ELEVATOR: penthouse enforcer (boss)
    TrrtDat,        // TURRET: corridor mook (fast)
    LolBin,         // VENDING: ultra-rare easter egg
};

struct Ice {
    int       x = 0;
    int       y = 0;
    int       hp = 1;
    int       hp_max = 1;              // S6.2: cached starting hp for HP bar render
    IceColor  color = IceColor::White;
    int       patrol_dir = 0;          // 0..3 (white only)
    bool      sees_avatar = false;     // refreshed each turn for all colors
    int       charmed_turns_left = 0;  // Plan 4: DaemonHijack — skips enemy AI while > 0
    int       cast_cooldown = 0;       // Phase 5 S3: beats until this ICE may ranged-cast again (0 = ready)

    // Phase 5 S5: pipe-graph walker state (Black only — other colors
    // leave these at defaults). walk_pipe_index = -1 means "at a node"
    // (use ice.x/ice.y to determine which room); walk_pipe_index >= 0
    // means "in transit on netspace.pipes[walk_pipe_index]" — Black
    // occupies walk_path[walker_cell_idx(walk_seg, walk_seg_len, N)]
    // this beat (linear-interpolated when the raw path is longer than
    // the clamped seg_len so Black covers the full physical pipe in
    // exactly walk_seg_len beats — symmetric with player payloads on
    // clamped pipes). walk_seg advances +1/beat; arrival when
    // walk_seg > walk_seg_len. walk_seg_len caches
    // clamp_seg_len(walk_path.size()) for the collision predicate.
    int       walk_pipe_index = -1;
    int       walk_seg        = 0;
    int       walk_seg_len    = 0;
    std::vector<std::pair<int,int>> walk_path;

    // Phase 5 S5: idempotency flag for net_ice::kill_if_dead so a
    // single dead ICE is processed exactly once across both
    // apply_effect_in_net's Pass-2 and resolve_inflight_impacts's new
    // end-sweep (which catches collision-killed ICE). Set in
    // kill_if_dead; reset only when a new ICE is constructed.
    bool      killed = false;

    // Phase 5 S6: ICE-intrinsic obfuscation tier (combat.md's
    // gradient). All S6 ICE default to Watchdog; Elite/Boss/Blackwall
    // are seams for future content. Drives sniff_show() gating.
    IceTelegraphTier telegraph_tier = IceTelegraphTier::Watchdog;

    // Phase 5 S6: cast windup state. cast_windup_left > 0 means ICE is
    // pre-spawning a payload (telegraph visible to player). When it
    // ticks from 1 to 0, payload spawns + cast_cooldown resets to
    // kIceCastCadence. cast_windup_total caches the starting beats for
    // X/N rendering. Both default 0 (= not winding).
    int cast_windup_left  = 0;
    int cast_windup_total = 0;

    // Phase 5 S6.3: room index this ICE was spawned in (-1 = unset /
    // pre-S6.3, no constraint). Used by tick_all's White patrol case to
    // keep White ICE bounded to its room rather than wandering into
    // other rooms (which would collide other ICE's room-anchored labels
    // in the renderer). Black walker ignores this -- it leaves its
    // home room by design.
    int home_room_idx = -1;

    // Phase 5 S7c.1: typed daemon kind. Drives cosmetic + statistical
    // specialization (name, glyph, color, HP, windup/cast stats, render
    // style) on top of the behavior archetype selected by IceColor.
    // Default Watchdog matches the S6 hardcoded Gray behavior, so all
    // existing call sites that construct Ice without setting kind keep
    // working unchanged.
    DaemonKind kind = DaemonKind::Watchdog;

    // Phase 5 S7c.1: per-spawn tier-scaling overrides. 0 = use the
    // DaemonDef baseline (so combat-bench / place_ice_far ICE that
    // don't set these read def.windup_beats / def.cast_damage,
    // preserving S6 behavior). Door grammar's seed_daemon writes
    // tier-scaled values from kLockTiers[] / kBoltTiers[] here.
    int windup_override = 0;
    int cast_damage_override = 0;

    // Phase 5 S7d: optional gate-tile coords. If set (>= 0), a tick_grid
    // post-impacts hook flips this cell to NetTile::Floor when this ICE
    // dies (hp <= 0). Used by ELEVATOR's SCRTY.fw -- the daemon IS the
    // security gate; killing it opens the spine. Defaults to -1/-1 =
    // "no gate" so existing call sites stay unchanged.
    int gate_tile_x = -1;
    int gate_tile_y = -1;
};

struct NetSession; // fwd
class Game;         // fwd

namespace net_ice {

void spawn_for_sector(NetSession& s, uint32_t seed, int security_tier);
void spawn_from_seeds(NetSession& s);       // Plan 8 v2 path: seeds with min_trace ≤ current trace
void promote_pending_seeds(NetSession& s);  // Plan 8: per-tick — materializes newly-eligible seeds
void tick_all(NetSession& s, Game& game);

// Damage hooks. Called from program effects.
void damage(NetSession& s, Ice& ice, int dmg);
bool kill_if_dead(NetSession& s, Ice& ice);

} // namespace net_ice

} // namespace astra
