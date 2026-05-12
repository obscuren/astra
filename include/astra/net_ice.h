#pragma once

#include <cstdint>

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
