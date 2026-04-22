#include "astra/dungeon/overlay.h"

#include "astra/dungeon_recipe.h"
#include "astra/rect.h"
#include "astra/ruin_types.h"   // apply_ruin_stamps, RuinPlan, RuinStampConfig, RuinStampType
#include "astra/tilemap.h"

#include <algorithm>

namespace astra::dungeon {

namespace {

bool overlay_allowed(const DungeonStyle& style, OverlayKind k) {
    return std::find(style.allowed_overlays.begin(),
                     style.allowed_overlays.end(), k)
           != style.allowed_overlays.end();
}

// Build a minimal whole-map RuinPlan shim so we can call apply_ruin_stamps
// without carrying an actual RuinPlan through the new pipeline.
RuinPlan whole_map_plan(const TileMap& map, RuinStampType stamp_type,
                         float intensity) {
    RuinPlan plan;
    plan.footprint = Rect{0, 0, map.width(), map.height()};
    plan.stamps.push_back(RuinStampConfig{stamp_type, intensity});
    return plan;
}

void apply_one(TileMap& map, OverlayKind k, std::mt19937& rng) {
    switch (k) {
    case OverlayKind::BattleScarred:
        // Path A: stamp_battle_scarred only needs (TileMap&, Rect, float, rng).
        // We route through apply_ruin_stamps with a whole-map shim plan.
        apply_ruin_stamps(map,
                          whole_map_plan(map, RuinStampType::BattleScarred, 0.5f),
                          Biome::Dungeon, rng);
        break;
    case OverlayKind::Infested:
        // Path A: stamp_infested only needs (TileMap&, Rect, float, rng).
        apply_ruin_stamps(map,
                          whole_map_plan(map, RuinStampType::Infested, 0.5f),
                          Biome::Dungeon, rng);
        break;
    case OverlayKind::Flooded:
        // Path A: stamp_flooded only needs (TileMap&, Rect, float, rng).
        apply_ruin_stamps(map,
                          whole_map_plan(map, RuinStampType::Flooded, 0.5f),
                          Biome::Dungeon, rng);
        break;
    case OverlayKind::Vacuum:
        // Reserved for station styles — no-op in slice 1.
        break;
    case OverlayKind::None:
        break;
    }
}

} // namespace

void apply_overlays(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, std::mt19937& rng) {
    for (auto k : spec.overlays) {
        if (!overlay_allowed(style, k)) continue;
        apply_one(map, k, rng);
    }
}

} // namespace astra::dungeon
