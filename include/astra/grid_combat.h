#pragma once

namespace astra {

class Game; // forward declare

// Bump-attack melee tunables.
inline constexpr int kGridMeleeDamage   = 3;
inline constexpr int kGridMeleeRamCost  = 0;
inline constexpr int kGridMeleeHeatCost = 0;
inline constexpr int kGridMeleeRange    = 1;

// XP grants per ICE kill into the player's main XP pool.
inline constexpr int kXpIceWhite = 4;
inline constexpr int kXpIceGray  = 8;
inline constexpr int kXpIceBlack = 16;

// Grant netspace-kill XP into the main player pool and check for level-up.
// No-op if amount <= 0.
void grant_grid_xp(Game& game, int amount);

}  // namespace astra
