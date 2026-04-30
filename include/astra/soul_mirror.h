#pragma once

#include <cstdint>

namespace astra {

class Game;
class Renderer;
struct Hackable;

struct SoulMirrorChannelState {
    bool      active    = false;
    bool      passive   = false;   // true = Neural Backup auto-sync (no EP cost)
    Hackable* console   = nullptr; // raw — channel lifetime is bounded by the turn
    int       console_x = 0;      // tile the player must remain on
    int       console_y = 0;
};

namespace soul_mirror {

inline constexpr int kProgressPerTurn      = 1;
inline constexpr int kEpCostPerTurn        = 2;    // active channel only
inline constexpr int kCommitThreshold      = 10;   // progress per fragment commit
inline constexpr int kDamageDetectionBurst = 5;

void begin_active (Game& game, Hackable& console);
void begin_passive(Game& game, Hackable& console);

// Called once per game turn.
void tick(Game& game);

// Called when the player takes damage during the current turn.
void on_player_damaged(Game& game);

bool is_active(const Game& game);

// Render a one-line strip in the play HUD if active. No-op otherwise.
void render_hud_strip(Game& game, Renderer& r);

} // namespace soul_mirror
} // namespace astra
