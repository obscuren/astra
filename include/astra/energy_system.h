#pragma once

namespace astra {
struct Player;
class WorldManager;

class EnergySystem {
public:
    // Deposit energy from active Solar Panels on the player's items
    // when the player is outdoors. `ticks` is the wall-tick count
    // elapsed during this advance_world() call.
    void tick(Player& player, const WorldManager& world, int ticks);
};

} // namespace astra
