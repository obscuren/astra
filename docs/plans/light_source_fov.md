# Plan: Light Source FOV Extension (Implemented)

## Context

Torches and other light-emitting fixtures extend the player's view distance in their direction. When the player's FOV ray passes through a torch, the ray continues further (by the torch's `light_radius`), letting the player see deeper in that direction. Walls still block — no seeing around corners.

## Approach: Two-Pass FOV

**Pass 1**: Normal `compute_fov()` with player's radius. Reveals which light sources are visible.

**Pass 2**: `compute_fov_lit()` — shadowcasting from the player with extended radius per light source, but only marks tiles visible if within `light_radius` of a light source.

## Light Radii

| Fixture  | light_radius |
|----------|-------------|
| Torch    | 8           |
| Console  | 2           |
| Viewport | 1           |

## Files

- `include/astra/fov.h` — LightSource struct + compute_fov_lit()
- `src/fov.cpp` — cast_light_lit() filtered shadowcast variant
- `include/astra/tilemap.h` — light_radius field on FixtureData
- `src/tilemap.cpp` — light_radius values in make_fixture()
- `src/game_world.cpp` — light collection + extended pass in recompute_fov()
