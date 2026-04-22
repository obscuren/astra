#include "astra/dungeon/pipeline.h"

#include "astra/dungeon/backdrop.h"
#include "astra/dungeon/connectivity.h"
#include "astra/dungeon/decoration.h"
#include "astra/dungeon/fixtures.h"
#include "astra/dungeon/layout.h"
#include "astra/dungeon/overlay.h"

namespace astra::dungeon {

namespace {
std::mt19937 sub(uint32_t seed, uint32_t mix) { return std::mt19937(seed ^ mix); }
}

void run(TileMap& map, const DungeonStyle& style, const CivConfig& civ,
         const DungeonLevelSpec& spec, LevelContext& ctx) {
    auto rng_back  = sub(ctx.seed, 0xBDBDBDBDu);
    auto rng_lay   = sub(ctx.seed, 0x1A1A1A1Au);
    auto rng_con   = sub(ctx.seed, 0xC0FFEE00u);
    auto rng_ovl   = sub(ctx.seed, 0x0FEB0FEBu);
    auto rng_dec   = sub(ctx.seed, 0xDEC02011u);
    auto rng_fix   = sub(ctx.seed, 0xF12F12F1u);

    apply_backdrop    (map, style, civ,            rng_back);
    apply_layout      (map, style, civ, ctx,       rng_lay);
    apply_connectivity(map, style,      ctx,       rng_con);
    apply_overlays    (map, style, spec,           rng_ovl);
    apply_decoration  (map, style, civ, spec,      rng_dec);
    apply_fixtures    (map, style, civ, spec, ctx, rng_fix);
}

} // namespace astra::dungeon
