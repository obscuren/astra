#include "astra/grid_ice.h"
#include "astra/grid_session.h"

namespace astra {

namespace grid_ice {

void spawn_for_sector(GridSession& /*s*/, uint32_t /*seed*/, int /*security_tier*/) {}

void tick_all(GridSession& /*s*/, class Game& /*game*/) {}

void damage(GridSession& /*s*/, GridIce& ice, int dmg) {
    ice.hp -= dmg;
}

bool kill_if_dead(GridSession& /*s*/, GridIce& ice) {
    return ice.hp <= 0;
}

} // namespace grid_ice

} // namespace astra
