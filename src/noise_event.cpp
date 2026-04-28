#include "astra/noise_event.h"

#include "astra/game.h"
#include "astra/world_manager.h"

namespace astra {

void emit_noise_event(Game& game, NoiseEvent ev) {
    game.world().noise_events().push_back(std::move(ev));
}

void tick_noise_events(Game& game) {
    auto& events = game.world().noise_events();
    for (auto it = events.begin(); it != events.end(); ) {
        if (--it->ttl_ticks <= 0) {
            it = events.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace astra
