#pragma once

namespace astra {

class Game; // forward declare

// Bump-attack melee tunables (Spec 1 §6.2).
inline constexpr int kGridMeleeDamage   = 3;
inline constexpr int kGridMeleeRamCost  = 0;   // RAM cost for melee
inline constexpr int kGridMeleeHeatCost = 0;   // Heat cost for melee
inline constexpr int kGridMeleeRange    = 1;

// Tether ability tunables (Spec 1 §4.2).
inline constexpr int kTetherRamCost  = 2;
inline constexpr int kTetherHeatCost = 4;

// XP grants (Spec 1 §8.2). Tier scalars to be tuned in playtest.
inline constexpr int kXpIceWhite           = 4;
inline constexpr int kXpIceGray            = 8;
inline constexpr int kXpIceBlack           = 16;
inline constexpr int kXpImprintSeverPerTier = 12;   // multiplied by npc_threat_tier

// Anchor HP curve (placeholder linear scale).
// Tier 1 NPC -> 10 HP, tier 5 -> 98 HP (~100). Tune in playtest.
inline int anchor_max_hp(int npc_threat_tier) {
    int t = npc_threat_tier <= 0 ? 1 : npc_threat_tier;
    return 10 + (t - 1) * 22;
}

// Grant Grid-kill XP into the main player pool and check for level-up.
// No-op if amount <= 0.
void grant_grid_xp(Game& game, int amount);

}  // namespace astra
