# Plan: Time of Day, Waiting, and Dynamic View Range

**Status: Implemented**

## Summary

Added a time-of-day system tied to celestial body properties that affects visibility and includes a "Wait" tab menu.

## What was implemented

1. **Time System** (`include/astra/time_of_day.h`) — `DayClock` struct with Dawn/Day/Dusk/Night phases, global calendar (Cycle/Day from `world_tick_`), and per-body day lengths
2. **Dynamic View Range** — Surface maps use time-of-day to lerp between `view_radius` (day) and `light_radius` (night). Dungeons always use `light_radius`. Ships/stations/overworld unchanged.
3. **Stats Bar** — Dynamic `"C1 D3 ☀ 14:30"` display with phase-colored icons replacing hardcoded "Cycle 1, Day 1"
4. **Wait Tab** — 5th panel tab with 6 options: wait 1/10/50/100 turns, wait until healed, wait until morning. Interrupts on damage.
5. **Celestial Body Integration** — `day_length` field derived from body type/size during generation. Moons default to 400 ticks (tidally locked).
6. **Persistence** — Save version bumped to 10 with `local_tick`, `local_ticks_per_day`, `light_radius`, and body `day_length` fields.

## Files modified

- `include/astra/time_of_day.h` (new)
- `include/astra/celestial_body.h`
- `include/astra/player.h`
- `include/astra/game.h`
- `include/astra/save_file.h`
- `src/game.cpp`
- `src/save_file.cpp`
- `src/celestial_body.cpp`
- `src/star_chart.cpp`
