#pragma once

#include <string>

namespace astra {

class Game; // forward declare
struct EffectSpec;
struct NetInFlight;
struct NetSession;

// Bump-attack melee tunables.
inline constexpr int kNetMeleeDamage   = 3;
inline constexpr int kNetMeleeRamCost  = 0;
inline constexpr int kNetMeleeHeatCost = 0;
inline constexpr int kNetMeleeRange    = 1;

// XP grants per ICE kill into the player's main XP pool.
inline constexpr int kXpIceWhite = 4;
inline constexpr int kXpIceGray  = 8;
inline constexpr int kXpIceBlack = 16;

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

}  // namespace astra
