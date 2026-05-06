#pragma once

namespace astra {

// Bump-attack melee tunables (Spec 1 §6.2).
inline constexpr int kGridMeleeDamage      = 3;
inline constexpr int kGridMeleeChannelCost = 0;   // RAM in code identifiers (deferred rename)
inline constexpr int kGridMeleeDriftCost   = 0;   // Heat in code identifiers (deferred rename)
inline constexpr int kGridMeleeRange       = 1;

// Bind ability tunables (Spec 1 §4.2).
inline constexpr int kBindChannelCost = 2;
inline constexpr int kBindDriftCost   = 4;

// XP grants (Spec 1 §8.2). Tier scalars to be tuned in playtest.
inline constexpr int kXpIceWhite           = 4;
inline constexpr int kXpIceGray            = 8;
inline constexpr int kXpIceBlack           = 16;
inline constexpr int kXpAnchorSeverPerTier = 12;   // multiplied by npc_threat_tier

// Anchor HP curve (placeholder linear scale).
// Tier 1 NPC -> 10 HP, tier 5 -> 98 HP (~100). Tune in playtest.
inline int anchor_max_hp(int npc_threat_tier) {
    int t = npc_threat_tier <= 0 ? 1 : npc_threat_tier;
    return 10 + (t - 1) * 22;
}

}  // namespace astra
