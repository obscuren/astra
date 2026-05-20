#pragma once
// Phase 5 S6: telegraph + info-tier ladder.
//
// IceTelegraphTier is the ICE-intrinsic obfuscation tier (combat.md's
// obfuscation gradient: Watchdog full -> Elite name-hidden -> Boss
// compiling-only -> Blackwall lying). All S6 ICE default to Watchdog;
// the other tiers are SEAMS for future content (Elite/Boss rosters in
// S7+; Blackwall in Phase 6).
//
// sniff_level is a per-session sticky counter (0..kSniffMax). Each
// SNIFF press: ++sniff_level (capped), AND consumes the beat. Reveals
// are unlocked additively by tier ladder. sniff_show(tier, level, kind)
// is the single Game-free predicate the renderer + HOSTILES band call
// to gate every individual reveal. Future tier branches (Elite/Boss/
// Blackwall) plug in here without touching the renderer.

#include <cstdint>

namespace astra {

class NetSession;
struct Ice;

enum class IceTelegraphTier : std::uint8_t {
    Watchdog,   // S6 default: full transparency at sniff_level high enough
    Elite,      // future content: progress visible, name hidden
    Boss,       // future content: only "compiling..."
    Blackwall,  // Phase 6: lying telegraph
};

enum class RevealKind : std::uint8_t {
    PayloadDmg,         // hostile in-pipe payload damage label
    IceCastBar,         // ICE windup [N/M] progress
    IceCastName,        // ICE program name ("gray ICE.exe")
    IceHp,              // ICE current HP
    BlackEtaCoarse,     // Black "near/far" badge
    BlackEtaPrecise,    // Black exact beats-to-avatar-room
};

// Game-free reveal predicate. v1 implements only Watchdog; future tiers
// return false (hide all) so they are defined-seams, not undefined-
// behaviour. Caller passes the ICE's intrinsic tier and the session's
// current sniff_level; gets a yes/no per reveal kind.
bool sniff_show(IceTelegraphTier tier, int sniff_level, RevealKind k);

// Game-free black-walker ETA in beats (sum of remaining cells on the
// current pipe + cells of all future pipes on the shortest path to the
// avatar's room). -1 = unreachable / dead / charmed / no avatar room.
int black_eta_beats(const NetSession& s, const Ice& blk);

inline constexpr int kSniffMax           = 2;    // 3-rung ladder: 0, 1, 2
inline constexpr int kIceGrayWindupBeats = 4;    // pre-launch telegraph window
inline constexpr int kRunAutopilotCap    = 64;   // safety bound; never reached
inline constexpr int kHostilesSidebarW   = 28;   // right-of-field sidebar width
inline constexpr int kInlineBarMaxCells  = 6;    // S6.2: max cells per inline bar (HP / cast)
inline constexpr int kInlineLabelMaxW    = 32;   // S6.2: soft cap on inline label width

}  // namespace astra
