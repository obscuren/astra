#pragma once

#include <string>

namespace astra {

class Game;

// A short-lived "loud noise" emitted on the active map. NPCs in idle/
// wandering states retarget toward the noise location for the event's
// TTL when the emitter is hostile to them. Currently produced by
// Decoy Mines; future grenades / thrown rocks will reuse this.
struct NoiseEvent {
    int x = 0;
    int y = 0;
    int radius = 5;          // Chebyshev — 10-tile diameter for Decoy
    int ttl_ticks = 5;
    std::string emitter_owner_faction;  // empty if player-emitted
    bool emitter_is_player = false;
};

// Append a noise event to the active map's live registry.
void emit_noise_event(Game& game, NoiseEvent ev);

// Run once per world tick: decrement ttl, erase expired events.
void tick_noise_events(Game& game);

} // namespace astra
