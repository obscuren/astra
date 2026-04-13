#include "astra/infested_station_generator.h"
#include "astra/map_generator.h"
#include "astra/station_type.h"

namespace astra {

// Forward declaration from derelict_station_generator.cpp
std::unique_ptr<MapGenerator> make_derelict_station_generator(const StationContext& ctx);

// Infested stations reuse the derelict layout.  The heavy xytomorph infestation
// is applied as a post-generation spawn step in game_world.cpp, gated on
// sctx.type == StationType::Infested (seed: keeper_seed ^ 0xF001).
std::unique_ptr<MapGenerator> make_infested_station_generator(const StationContext& ctx) {
    return make_derelict_station_generator(ctx);
}

} // namespace astra
