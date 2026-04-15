# Neutron Star Class — Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `docs/plans/nova-stellar-signal-gap-analysis.md` (unblocks Nova Stage 2 Echo 3 "The Edge").

## Summary

Add `StarClass::Neutron` — a dying pulsar remnant that serves as the "star" of a system with no habitable zone and no procedural bodies. Quest-created via the existing `CustomSystemSpec`. Renders as `+` in BrightWhite on the star chart. Procedural generation never produces neutron systems; they exist only when a quest creates one.

---

## Goals

- One new `StarClass` enum value reachable only through `CustomSystemSpec`.
- Distinct visual identity (`+` glyph, BrightWhite color).
- No habitable zone, no procedural bodies.
- Dev-console smoke test via a new `chart create neutron <name>` kind.

## Non-goals

- Pulsar animation (static glyph).
- Gravitational-wave / ship-travel effects.
- Neutron-specific lore generation.
- Binary neutron systems.
- Procedural spawn weighting.

---

## Data Model

```cpp
enum class StarClass : uint8_t {
    ClassM, ClassK, ClassG, ClassF, ClassA, ClassB, ClassO,
    Neutron,   // NEW — pulsar remnant; only via custom systems
};
```

Switch-case additions:

| Function | Case |
|---|---|
| `star_class_name(StarClass)` | `"Neutron (Pulsar Remnant)"` |
| `star_class_glyph(StarClass)` | `'+'` |
| `star_class_color(StarClass)` | `Color::BrightWhite` |
| habitable-zone table inside `generate_system_bodies` | `hz_inner = 0.0f; hz_outer = 0.0f` |

`generate_system_bodies(StarSystem&)` gets an early return when `sys.star_class == StarClass::Neutron`: sets `bodies_generated = true` and returns without populating planets. Custom systems that pre-fill `sys.bodies` still work because `add_custom_system` already sets `bodies_generated = true` for non-empty input; the generator's early return never runs in that path.

---

## Integration

### Procedural generation

The existing probability cascade in `src/star_chart.cpp:118-124` selects only M-through-O. No change required — Neutron is simply unreachable from the percentile roll.

### Custom systems

No API change. Callers already pass any `StarClass` via `CustomSystemSpec.star_class`. Example (Nova Stage 2 Echo 3):

```cpp
uint32_t edge_id = add_custom_system(nav, {
    .name = "The Edge — Unmapped",
    .gx = coords.first,
    .gy = coords.second,
    .star_class = StarClass::Neutron,
    .discovered = false,
    .bodies = { make_landable_asteroid("Crystal Fragment") },
});
```

### `-Wswitch` coverage

Any other switch over `StarClass` needs a `Neutron` case. Known candidates to verify during implementation:

- `src/star_chart.cpp` — star-class info panels, filters.
- `src/star_chart_viewer.cpp` — display logic.
- `src/terminal_renderer_galaxy.cpp` — chart rendering (though `star_class_color` / `star_class_glyph` centralize this; a direct switch is unlikely).
- `src/dev_console.cpp` — `lore list` / `warp` string matching.

Each site that's missing the value becomes a compile warning or error; plan patches them as the build surfaces them.

### Dev-console

Extend the `chart create` kind argument (`asteroid|scar|rock|neutron`). The `neutron` kind:

- Sets `star_class = StarClass::Neutron`.
- Places one `make_landable_asteroid("<name> Fragment")` body.
- Uses the same `pick_coords_near` placement as other kinds.

---

## Save / Load

No schema change. `StarClass` is serialized as `u8` (in the per-system save loop around `src/save_file.cpp:836`). Appending a value keeps older saves valid; new saves encode Neutron as `7`.

---

## File Map

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/star_chart.h` | MODIFY | `Neutron` enum value |
| `src/star_chart.cpp` | MODIFY | Name / glyph / color cases; habitable-zone case; skip-bodies branch |
| `src/dev_console.cpp` | MODIFY | `neutron` kind in `chart create` |
| Any other `StarClass` switch | MODIFY | Add case per site |

---

## Implementation Checklist

1. Append `Neutron` to the `StarClass` enum.
2. Extend `star_class_name` / `star_class_glyph` / `star_class_color`.
3. Extend the habitable-zone table in `generate_system_bodies`.
4. Add an early-return body-generation branch for Neutron.
5. Patch any other `StarClass` switches the build flags.
6. Extend `chart create` with the `neutron` kind.
7. Smoke test: `chart create neutron Pulsara` → chart shows a `+` in BrightWhite; landing on the `Fragment` body works; save/reload preserves the class.

---

## Out of scope — explicitly deferred

- Pulsar animation.
- Neutron-binary systems (no combination with `binary = true`; treat as single-star for now).
- Gravitational effects on ship travel.
- Nova arc wiring.
