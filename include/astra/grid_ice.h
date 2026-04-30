#pragma once

#include <cstdint>

namespace astra {

enum class IceColor : uint8_t { White, Gray, Black };

struct GridIce {
    int       x = 0;
    int       y = 0;
    int       hp = 1;
    IceColor  color = IceColor::White;
    int       patrol_dir = 0;          // 0..3 (white only)
    bool      sees_avatar = false;     // refreshed each turn for all colors
};

struct GridSession; // fwd
class Game;         // fwd

namespace grid_ice {

void spawn_for_sector(GridSession& s, uint32_t seed, int security_tier);
void tick_all(GridSession& s, Game& game);

// Damage hooks. Called from program effects.
void damage(GridSession& s, GridIce& ice, int dmg);
bool kill_if_dead(GridSession& s, GridIce& ice);

} // namespace grid_ice

} // namespace astra
