#pragma once

#include <cstdint>

namespace astra {

class Game;

enum class GrenadeKind : uint8_t {
    Frag = 0,
    Emp,
    Cryo,
    Incendiary,
    Smoke,
    Flashbang,
};

// Per-kind grenade stats. Throw range is fixed across kinds (see
// grenade_throw_range); `status` of 0 means "no status" (EffectId::Invulnerable
// sentinel — same convention as TrapDef).
struct GrenadeDef {
    int damage = 0;
    int burst_radius = 1;            // half-width; 1 = 3x3, 2 = 5x5
    int status = 0;                  // EffectId cast — 0 means "no status"
    int status_duration = 0;
    int status_tick_damage = 0;
};

const GrenadeDef& grenade_def_for(GrenadeKind k);
int  grenade_throw_range(GrenadeKind k);
int  grenade_throw_burst_width(GrenadeKind k);

GrenadeKind grenade_kind_for_item_id(uint16_t item_def_id);

// Display helpers (mirrors trap_*).
const char* grenade_kind_name(GrenadeKind k);
char        grenade_glyph(GrenadeKind k);
int         grenade_color(GrenadeKind k);

// Detonate immediately at (x, y). Applies damage + status to all alive
// entities within burst_radius (Chebyshev). Placer is splash-immune.
// Logs a headline + per-victim damage line, identical shape to traps.
void detonate_grenade(Game& game, GrenadeKind kind, int x, int y,
                      bool placer_is_player);

} // namespace astra
