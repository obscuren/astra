#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace astra {

enum class IceColor : uint8_t { White, Gray, Black };

struct Ice {
    int       x = 0;
    int       y = 0;
    int       hp = 1;
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
