#pragma once

namespace astra {
class Game;

// Called once per session after world is loaded / new game is created.
// Each scenario subscribes its own event handlers. Idempotent: on load,
// the bus was cleared, so calling this again is safe.
void register_all_scenarios(Game& game);

// Individual registrations — expose for tests / debug console.
void register_stage4_hostility_scenario(Game& game);

} // namespace astra
