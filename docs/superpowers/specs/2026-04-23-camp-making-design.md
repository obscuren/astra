# Camp Making — Design

Status: approved design, ready for implementation planning.
Related skill: `SkillId::CampMaking` (Wayfinding tree, already in `skill_defs.cpp`).

## Summary

Camp Making becomes an active ability: the player places a short-lived
campfire fixture on an adjacent tile. While the player stands within 6 tiles
of one of their own campfires, they receive a `Cozy` gameplay effect that
halves their natural-regen interval. Campfires also lay the groundwork for a
future cooking system via a generic fixture-tag mechanism.

## Player-facing behaviour

- Skill already defined: 50 SP, Int 12 req, non-passive.
- Triggered from an ability-bar slot once learned (same path as Jab / Cleave).
- Auto-places a `Campfire` fixture on the first passable, fixture-free
  adjacent tile (8-neighbour scan in a fixed order). Fails with a log line
  "No space to build a camp" if none available.
- `action_cost = 100` (twice a normal swing; you're stopping to build).
- `cooldown_ticks = 300`, tracked via a new `EffectId::CooldownCampMaking`.
- Works anywhere the player stands. No indoor/outdoor restrictions for v1.
- Campfire glyph: `^` (red/orange/yellow), animates per-tile by hashing
  `(x, y, world_tick)` so multiple fires don't flicker in lockstep.
- Campfire lifetime: `kCampfireLifetimeTicks = 150` (tweakable constant).
  On expiry the fixture is removed cleanly; tile reverts to its underlying
  terrain. No ashes / no decal in v1.

## Cozy effect

- New `EffectId::Cozy`, orange tint, shown in the effect bar for feedback.
- Applied only by the player's own campfires — the proximity scan looks for
  `FixtureType::Campfire` specifically, not the `HeatSource` tag. Other heat
  sources (settlement CampStoves, torches) do **not** grant Cozy.
- Chebyshev distance ≤ 6 from the player.
- Re-applied each world tick with `duration = 1` so it auto-expires the tick
  after the player steps out of range — no explicit removal logic.
- Benefit: halves the natural-regen interval. Implementation: the regen code
  in `game_world.cpp` checks `has_effect(player.effects, EffectId::Cozy)`
  and divides `regen_interval(hunger)` by 2 (minimum 1) when present. No
  new fields on the `Effect` struct.

## Fixture tagging system

Introduces a generic fixture-tag facility. Not required by Cozy itself but
added now because the player asked for it and it unblocks cooking later.

New enum (`include/astra/tilemap.h`):

```cpp
enum class FixtureTag : uint64_t {
    None           = 0,
    CookingSource  = 1ull << 0,
    HeatSource     = 1ull << 1,
    LightSource    = 1ull << 2,
};
```

New field on `FixtureData`:

```cpp
uint64_t tags = 0;
```

Helpers: `fixture_has_tag(const FixtureData&, FixtureTag)` and bitwise
`|`/`&` operators for `FixtureTag` in an `inline` header block.

`make_fixture()` is the single place tags are assigned. Initial mapping:

| Fixture     | Tags                                      |
|-------------|-------------------------------------------|
| Campfire    | CookingSource \| HeatSource \| LightSource |
| CampStove   | CookingSource \| HeatSource                |
| Kitchen     | CookingSource \| HeatSource                |
| Torch       | LightSource                                |
| Lamp        | LightSource                                |
| HoloLight   | LightSource                                |

All other fixtures get `0`. Future cooking can just scan adjacent fixtures
for `CookingSource`.

## Campfire expiry

Adds one field to `FixtureData`:

```cpp
int spawn_tick = -1;   // world_tick when placed; -1 = not time-limited
```

On placement, `spawn_tick = world_tick`. Each world tick, a sweep removes
any fixture whose `spawn_tick >= 0` and
`world_tick - spawn_tick >= kCampfireLifetimeTicks`. The sweep runs once
per tick at the same place as other per-tick world updates (proximity
scan for Cozy is the natural neighbour). Scans only the current map, not
all loaded maps; campfires on other maps resume ticking when the player
re-enters that map — acceptable for v1. Persisted via save file since
`FixtureData` already round-trips.

## Animation

Renderer-side only. In the fixture render path (`map_renderer`), when
`type == FixtureType::Campfire`:

```cpp
// pseudo
const Color palette[] = { Red, Orange, Yellow };
int idx = hash(x, y, world_tick / 2) % 3;
```

Glyph stays `^`. Color cycles. `world_tick / 2` keeps the flicker visible
but not frantic; tunable.

## Save / load

- `FixtureData::tags` — new field, add to save_file read/write.
- `FixtureData::spawn_tick` — new field, add to save_file read/write.
- Per the user's memory "No backcompat pre-ship", bump the save-schema
  version and reject older saves. No migration.

## Code touchpoints

- `include/astra/tilemap.h` — `FixtureType::Campfire`, `FixtureTag` enum,
  `FixtureData::tags`, `FixtureData::spawn_tick`, helpers.
- `src/tilemap.cpp` — `make_fixture` tag assignments, Campfire defaults.
- `include/astra/effect.h` + `src/effect.cpp` —
  `EffectId::Cozy`, `EffectId::CooldownCampMaking`, `make_cozy()` factory.
- `src/ability.cpp` — `CampMakingAbility` class, add to catalog.
- `src/game_world.cpp` — per-tick campfire-expiry sweep; Cozy proximity
  re-apply; regen interval halved when Cozy present.
- `src/map_renderer.cpp` (or wherever fixtures are drawn) — Campfire
  animated palette.
- `src/save_file.cpp` — new fields in FixtureData round-trip; schema bump.
- `docs/formulas.md` — document the Cozy regen rule.
- `docs/roadmap.md` — tick off Camp Making if it's listed.

## Non-goals (explicit YAGNI)

- Cooking UI / recipes — tag system is in place; actual cooking is a
  separate feature.
- Ashes, embers, or any post-expiry visual.
- Direction-prompt placement (we picked auto-place; direction-prompt can
  come later if the frictionless version feels wrong).
- Indoor/outdoor restrictions.
- Multiple simultaneous player campfires are allowed (no artificial cap);
  the 300-tick cooldown is the only throttle.
- Generalising Cozy to non-Campfire heat sources via a `CozySource` tag.
  Add only when a second source actually exists.

## Open questions

None — all design points resolved in brainstorming.
