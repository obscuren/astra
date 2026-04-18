#include "astra/scenarios.h"
#include "astra/game.h"

namespace astra {

void register_stage4_hostility_scenario(Game& /*game*/) {
    // Implementation lands in Task 9.
}

void register_all_scenarios(Game& game) {
    register_stage4_hostility_scenario(game);
}

} // namespace astra
