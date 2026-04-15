# New Body Types & Biome Override — Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `docs/plans/nova-stellar-signal-gap-analysis.md` — unlocks Nova arc Stage 2 Echo 1 (the scar system) and the Stage 3 beacon asteroid.

## Summary

Add two primitives that quests (and future procedural generators) can compose:

1. **`BodyType::LandableAsteroid`** — a new celestial body type distinct from `AsteroidBelt`. Renders as `*` on the star chart. Intended for single-rock landable bodies (beacon orbits, mining claims, derelict anchor points).
2. **`CelestialBody::biome_override`** — an optional `Biome` on a celestial body that bypasses `determine_biome(type, atmo, temp, seed)` and forces a specific biome when the player enters the body. Lets quests place a scar planet (using the existing `ScarredScorched` / `ScarredGlassed` biomes) without polluting `determine_biome` with quest-specific mappings.

Paired with the body-override, a new chart-color helper gives scar planets a distinct color on the local system map (rust/burnt-orange for scorched, pale cyan for glassed) while keeping their glyph as an ordinary Terrestrial `o`.

Two new factory presets land in `body_presets.h` alongside the existing `make_asteroid_orbit`:

- `make_landable_asteroid(std::string name)` — supersedes `make_asteroid_orbit` (distinct glyph).
- `make_scar_planet(std::string name, Biome scar_biome = Biome::ScarredGlassed)`.

---

## Goals

- One-line construction of a landable asteroid or a scar planet via `body_presets.h`.
- No changes to procedural generation logic — `determine_biome` remains untouched for procedural bodies.
- Distinct color (not glyph) on the local system map for biome-overridden bodies, so the narrative isn't spoiled by a conspicuous glyph but attentive players notice the anomaly.
- Save/load round-trip for `biome_override`.
- Smoke-testable via dev-console.

## Non-goals

- New biomes — `ScarredScorched` / `ScarredGlassed` already exist in `biome_profiles.cpp`.
- New NPCs — Archon Remnants / Void Reavers / Archon Sentinel land in a separate spec.
- Neutron-star systems — separate spec.
- Quest wiring (Nova or otherwise) — each quest is its own spec/plan.
- Generator support for procedural scar planets — quests are the only producer today.

---

## Data Model

### `BodyType` extension

File: `include/astra/celestial_body.h`.

```cpp
enum class BodyType : uint8_t {
    Rocky,
    GasGiant,
    IceGiant,
    Terrestrial,
    DwarfPlanet,
    AsteroidBelt,
    LandableAsteroid,   // NEW — a single landable rock, not a belt
};
```

### `CelestialBody` extension

```cpp
struct CelestialBody {
    // ... existing fields ...
    std::optional<Biome> biome_override;   // NEW — when set, forces Biome on entry
};
```

Needs `#include <optional>` at the top of the header.

### `determine_biome` overload

Add a single-arg-body overload in `src/celestial_body.cpp`:

```cpp
Biome determine_biome(const CelestialBody& body, unsigned seed) {
    if (body.biome_override.has_value()) return *body.biome_override;
    return determine_biome(body.type, body.atmosphere, body.temperature, seed);
}
```

Declared in `celestial_body.h`. Existing `determine_biome(BodyType, Atmosphere, Temperature, unsigned)` stays — it's still the producer for procedural bodies.

Call sites that currently call the four-arg form with fields of a body should migrate to the body-overload. The plan enumerates them precisely.

### Chart color helper

File: `include/astra/celestial_body.h`, `src/celestial_body.cpp`.

```cpp
Color body_chart_color(const CelestialBody& body);
```

Body:

```cpp
Color body_chart_color(const CelestialBody& body) {
    if (body.biome_override) {
        switch (*body.biome_override) {
            case Biome::ScarredScorched: return static_cast<Color>(202);  // rust-orange
            case Biome::ScarredGlassed:  return static_cast<Color>(231);  // pale glass-white
            default: break;   // any other forced biome: fall through to type color
        }
    }
    return body_type_color(body.type);
}
```

Render sites migrate from `body_type_color(body.type)` to `body_chart_color(body)` — three call sites:

- `src/terminal_renderer_galaxy.cpp:457`
- `src/star_chart_viewer.cpp:915`
- `src/star_chart_viewer.cpp:940`

### `body_type_glyph` / `body_type_color` additions

Add the new `LandableAsteroid` case to both switches in `src/celestial_body.cpp`:

```cpp
char body_type_glyph(BodyType type) {
    switch (type) {
        // ... existing cases ...
        case BodyType::LandableAsteroid: return '*';
    }
    return '?';
}

Color body_type_color(BodyType type) {
    switch (type) {
        // ... existing cases ...
        case BodyType::LandableAsteroid: return Color::White;
    }
    return Color::DarkGray;
}
```

### Detail-map type mapping

`src/game_world.cpp:1250` currently maps `BodyType::AsteroidBelt` to `MapType::Asteroid`. Extend:

```cpp
case BodyType::DwarfPlanet:
case BodyType::AsteroidBelt:
case BodyType::LandableAsteroid:
    dest_type = MapType::Asteroid;
    break;
```

The detail-map generator's biome selection then flows through `determine_biome(body, seed)` and respects the override.

---

## Presets

File: `include/astra/body_presets.h`.

```cpp
// A single landable asteroid. Intended as the anchor body for a quest
// that places a fixture on its detail map (e.g. Nova's beacon, a signal
// node). Distinct chart glyph ('*') from AsteroidBelt ('~').
CelestialBody make_landable_asteroid(std::string name);

// A terrestrial planet with a forced scarred biome (scorched by default,
// or glassed). Used by quests that place content on a war-ravaged world.
CelestialBody make_scar_planet(std::string name,
                               Biome scar_biome = Biome::ScarredGlassed);
```

Implementations in `src/body_presets.cpp`:

```cpp
CelestialBody make_landable_asteroid(std::string name) {
    CelestialBody b;
    b.name = std::move(name);
    b.type = BodyType::LandableAsteroid;
    b.atmosphere = Atmosphere::None;
    b.temperature = Temperature::Cold;
    b.size = 1;
    b.moons = 0;
    b.orbital_distance = 1.0f;
    b.landable = true;
    b.explored = false;
    b.has_dungeon = false;
    b.danger_level = 1;
    b.day_length = 200;
    return b;
}

CelestialBody make_scar_planet(std::string name, Biome scar_biome) {
    CelestialBody b;
    b.name = std::move(name);
    b.type = BodyType::Terrestrial;
    b.atmosphere = Atmosphere::Thin;        // breathable-ish; glassed surface
    b.temperature = Temperature::Hot;       // sun-baked; overridden by biome
    b.size = 3;
    b.moons = 0;
    b.orbital_distance = 1.0f;
    b.landable = true;
    b.explored = false;
    b.has_dungeon = false;
    b.danger_level = 4;
    b.day_length = 200;
    b.biome_override = scar_biome;
    return b;
}
```

**Deprecate `make_asteroid_orbit`:** it was the first pass before `LandableAsteroid` existed. Replace with `make_landable_asteroid` at its single call site (`src/dev_console.cpp::chart create`). Remove the old function to avoid two near-identical presets.

---

## Save / Load

Bump save version 31 → 32. Gate the read on v32. Write is unconditional:

- In `write_navigation_section` (the per-body loop), append:

```cpp
w.write_u8(body.biome_override.has_value() ? 1 : 0);
if (body.biome_override) {
    w.write_u8(static_cast<uint8_t>(*body.biome_override));
}
```

- In `read_navigation_section` (the per-body loop), append:

```cpp
if (version >= 32) {
    if (r.read_u8() != 0) {
        body.biome_override = static_cast<Biome>(r.read_u8());
    }
    // else: leaves optional as nullopt
}
```

Older v31 saves load with `biome_override` defaulting to `std::nullopt` — correct, as no procedural body uses the override.

`BodyType::LandableAsteroid` rides the existing `u8` type field; its numeric value is simply `6` (the next after `AsteroidBelt=5`), so pre-v32 saves never contain it.

---

## Dev-Console Smoke Test

Extend `chart create` to take an optional kind:

```
chart create [asteroid|scar|rock] [name]
```

- `asteroid` (default) — uses `make_landable_asteroid(name)`.
- `scar` — uses `make_scar_planet(name, Biome::ScarredGlassed)`.
- `rock` — explicit use of procedural Rocky body via an ad-hoc `CelestialBody` (no preset) to exercise the non-override path.

If no kind is given, default to `asteroid` (backwards compatible with current behavior).

Usage log updates accordingly.

---

## File Map

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/celestial_body.h` | MODIFY | `LandableAsteroid` enum value; `biome_override` field; body-overload of `determine_biome`; `body_chart_color` decl; `<optional>` include |
| `src/celestial_body.cpp` | MODIFY | Glyph + color cases; biome-overload impl; chart-color impl |
| `include/astra/body_presets.h` | MODIFY | Decls for `make_landable_asteroid` + `make_scar_planet`; drop `make_asteroid_orbit` |
| `src/body_presets.cpp` | MODIFY | Impls for the two new presets; drop `make_asteroid_orbit` |
| `src/game_world.cpp` | MODIFY | Detail-map type mapping for `LandableAsteroid`; callers of `determine_biome(type,atmo,temp,seed)` that have a body switch to body overload (biome-override respected) |
| `src/terminal_renderer_galaxy.cpp` | MODIFY | Line 457: `body_type_color(body.type)` → `body_chart_color(body)` |
| `src/star_chart_viewer.cpp` | MODIFY | Lines 915 & 940: same swap |
| `src/save_file.cpp` | MODIFY | Write `biome_override`; read gated on v32 |
| `include/astra/save_file.h` | MODIFY | Version bump to 32 |
| `src/dev_console.cpp` | MODIFY | `chart create` takes an optional kind argument |

---

## Implementation Checklist (for the forthcoming plan)

1. Add `BodyType::LandableAsteroid` + glyph/color cases. Verify build.
2. Add `biome_override` field to `CelestialBody` + `<optional>` include.
3. Add body-overload of `determine_biome` in `celestial_body.h/.cpp`.
4. Add `body_chart_color` helper.
5. Migrate the three chart render call sites to `body_chart_color`.
6. Migrate `determine_biome` call sites that have a body in scope to the body overload (so the override actually takes effect on entry).
7. Extend the detail-map type-mapping switch for `LandableAsteroid`.
8. Replace `make_asteroid_orbit` with `make_landable_asteroid` in `body_presets.h/.cpp`; add `make_scar_planet`.
9. Update the one existing caller (dev_console) to use `make_landable_asteroid`.
10. Bump save version to 32; write + read `biome_override` gated.
11. Extend dev-console `chart create` with kind argument; add `scar` and default `asteroid`.
12. Smoke test: `chart create scar Fireworn` → land → detail map shows glassed-crater biome; chart shows rust-colored `o`.

---

## Out of scope — explicitly deferred

- New biomes beyond what already exists.
- Procedural production of scar planets (only quest-forced).
- Chart-level narrative reveals (e.g., tooltips naming "scar planet").
- Archon Sentinel miniboss and other NPCs.
- Neutron-star systems.
- Nova quest wiring.
