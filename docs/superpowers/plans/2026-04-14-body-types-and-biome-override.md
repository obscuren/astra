# Body Types & Biome Override Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `BodyType::LandableAsteroid` (distinct `*` chart glyph) and `CelestialBody::biome_override` (force a Biome on entry), plus two presets — `make_landable_asteroid` and `make_scar_planet` — so quests can compose distinct landable asteroid bodies and scar planets.

**Architecture:** `body_display_color` (already the chart-color helper) is extended to honor `biome_override`. `determine_biome` gets a body-taking overload that respects the override and delegates to the existing four-arg form. The lone non-moon caller in `game_world.cpp` migrates to the overload. `make_asteroid_orbit` is replaced by `make_landable_asteroid`.

**Tech Stack:** C++20; existing `CelestialBody`, `Biome`, save/load, `body_presets.h`. No new deps.

**Spec:** `docs/superpowers/specs/2026-04-14-body-types-and-biome-override-design.md`

**Save version:** bumps v31 → v32 (per-body `biome_override` appended).

**Worktree:** run this plan in `.worktrees/body-types` on branch `feat/body-types`, forked from `main`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/celestial_body.h` | MODIFY | `BodyType::LandableAsteroid`; `CelestialBody::biome_override`; body-overload `determine_biome` decl; `<optional>` |
| `src/celestial_body.cpp` | MODIFY | New cases in `body_type_glyph` / `_color` / `_name` / `determine_biome`; body-overload impl; extend `body_display_color` for scar biomes |
| `include/astra/body_presets.h` | MODIFY | Drop `make_asteroid_orbit`; add `make_landable_asteroid`, `make_scar_planet` |
| `src/body_presets.cpp` | MODIFY | Same |
| `src/game_world.cpp` | MODIFY | Add `LandableAsteroid` to MapType switch; switch line 1271 to body overload |
| `src/dev_console.cpp` | MODIFY | `chart create [kind] [name]` with `asteroid` (default) / `scar` / `rock`; replace `make_asteroid_orbit` with `make_landable_asteroid` |
| `include/astra/save_file.h` | MODIFY | Bump version to 32 |
| `src/save_file.cpp` | MODIFY | Write/read `biome_override` per-body in STAR section |

Build: `cmake --build build`. Run: `./build/astra-dev --term`. Commit prefixes: `feat(starchart):`, `feat(save):`, `feat(dev):`.

---

### Task 1: Add `BodyType::LandableAsteroid` and switch coverage

**Files:**
- Modify: `include/astra/celestial_body.h:12-19` (enum), `:76-79` (decl unchanged but new cases needed in defs)
- Modify: `src/celestial_body.cpp` — switches at lines 6, 18, 44 (name), 87 (determine_biome)

- [ ] **Step 1: Extend the enum**

In `include/astra/celestial_body.h`, change the `BodyType` enum:

```cpp
enum class BodyType : uint8_t {
    Rocky,
    GasGiant,
    IceGiant,
    Terrestrial,
    DwarfPlanet,
    AsteroidBelt,
    LandableAsteroid,   // single landable rock; chart glyph '*'
};
```

- [ ] **Step 2: Glyph case**

In `src/celestial_body.cpp::body_type_glyph` (line 6), append before `}`:

```cpp
        case BodyType::LandableAsteroid: return '*';
```

- [ ] **Step 3: Color case**

In `body_type_color` (line 18), append:

```cpp
        case BodyType::LandableAsteroid: return Color::White;
```

- [ ] **Step 4: Name case**

In `body_type_name` (line 44), append:

```cpp
        case BodyType::LandableAsteroid: return "Landable Asteroid";
```

- [ ] **Step 5: `determine_biome` case**

In the `determine_biome(BodyType type, Atmosphere atmo, Temperature temp, unsigned seed)` switch, find `case BodyType::AsteroidBelt:` (line 128) and add a parallel case immediately after the AsteroidBelt block (mirroring its pick):

```cpp
        case BodyType::LandableAsteroid:
            return pick({Biome::Rocky, Biome::Crystal});
```

- [ ] **Step 6: Build**

Run: `cmake --build build`
Expected: clean build. `-Wswitch` warnings on any other switch over `BodyType` will surface — fix each one with a `LandableAsteroid` case mirroring `AsteroidBelt`'s behavior. Common locations: `src/game_world.cpp:1244`. To find them all:

```
grep -rn "switch.*body.type\|switch.*BodyType\|case BodyType::AsteroidBelt" src/ include/
```

If you find a switch where AsteroidBelt has a specific case but no fallthrough, add an explicit `case BodyType::LandableAsteroid:` with the same body. Do **not** modify the `MapType::Asteroid` case in `game_world.cpp` here — that's Task 3.

- [ ] **Step 7: Commit**

```bash
git add include/astra/celestial_body.h src/celestial_body.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): BodyType::LandableAsteroid

A distinct landable rock body separate from AsteroidBelt. Chart
glyph is '*' (vs '~' for belts). Mirrors AsteroidBelt biome
selection (Rocky | Crystal pick).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Add `biome_override` + body-overload `determine_biome` + extend `body_display_color`

**Files:**
- Modify: `include/astra/celestial_body.h:58-73` (struct), `:84` (decl)
- Modify: `src/celestial_body.cpp:30-42` (body_display_color); append new function

- [ ] **Step 1: Add `<optional>` to header**

In `include/astra/celestial_body.h`, near the existing includes (top of file, with `<cstdint>`/`<string>`/`<vector>`), add:

```cpp
#include <optional>
```

- [ ] **Step 2: Add `biome_override` field**

In `struct CelestialBody`, append after `int day_length = 200;`:

```cpp
    std::optional<Biome> biome_override;   // when set, forces Biome on entry
```

- [ ] **Step 3: Declare body-overload**

Just below the existing `Biome determine_biome(BodyType, Atmosphere, Temperature, unsigned)` declaration (line 84), add:

```cpp
// Body-overload: respects body.biome_override; otherwise delegates above.
Biome determine_biome(const CelestialBody& body, unsigned seed);
```

- [ ] **Step 4: Implement body-overload**

In `src/celestial_body.cpp`, after the existing four-arg `determine_biome` body, add:

```cpp
Biome determine_biome(const CelestialBody& body, unsigned seed) {
    if (body.biome_override.has_value()) return *body.biome_override;
    return determine_biome(body.type, body.atmosphere, body.temperature, seed);
}
```

- [ ] **Step 5: Extend `body_display_color`**

Replace the current `body_display_color` (lines 30-42 in `src/celestial_body.cpp`) with:

```cpp
Color body_display_color(const CelestialBody& body) {
    // Quest-forced biomes get distinctive chart colors.
    if (body.biome_override) {
        switch (*body.biome_override) {
            case Biome::ScarredScorched: return static_cast<Color>(202);  // rust-orange
            case Biome::ScarredGlassed:  return static_cast<Color>(231);  // pale glass-white
            default: break;   // any other forced biome: fall through to type color logic
        }
    }
    // Mars-like: cold rocky with thin atmo = rust red
    if (body.type == BodyType::Rocky &&
        body.temperature == Temperature::Cold &&
        body.atmosphere == Atmosphere::Thin) {
        return static_cast<Color>(166);  // rust red
    }
    // Scorching rocky = reddish orange (Mercury/Venus-like)
    if (body.type == BodyType::Rocky && body.temperature == Temperature::Scorching) {
        return static_cast<Color>(208);  // orange
    }
    return body_type_color(body.type);
}
```

- [ ] **Step 6: Build**

Run: `cmake --build build`
Expected: clean build. No callers of the new overload exist yet (Task 3 wires them).

- [ ] **Step 7: Commit**

```bash
git add include/astra/celestial_body.h src/celestial_body.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): CelestialBody::biome_override + body_display_color extension

Optional Biome on the body that, when set, forces the entry biome
(bypasses determine_biome's attribute-driven selection). Chart color
honors the override for ScarredScorched (rust-orange) and
ScarredGlassed (pale glass-white).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Wire `game_world.cpp` — body overload + LandableAsteroid map type

**Files:**
- Modify: `src/game_world.cpp:1244-1256` (MapType switch), `:1271` (determine_biome call)

- [ ] **Step 1: Add LandableAsteroid to MapType switch**

In `src/game_world.cpp` around line 1244, the switch is:

```cpp
switch (body.type) {
    case BodyType::Rocky:
    case BodyType::Terrestrial:
        dest_type = MapType::Rocky;
        break;
    case BodyType::DwarfPlanet:
    case BodyType::AsteroidBelt:
        dest_type = MapType::Asteroid;
        break;
    default:
        dest_type = MapType::Rocky;
        break;
}
```

Add `LandableAsteroid` to the asteroid group:

```cpp
    case BodyType::DwarfPlanet:
    case BodyType::AsteroidBelt:
    case BodyType::LandableAsteroid:
        dest_type = MapType::Asteroid;
        break;
```

- [ ] **Step 2: Migrate `determine_biome` call to body overload**

Around line 1271, current code is:

```cpp
dest_biome = determine_biome(body.type, body.atmosphere, body.temperature, biome_seed);
```

Replace with:

```cpp
dest_biome = determine_biome(body, biome_seed);
```

Leave the moon-side call at line 1268 unchanged (`moon` is a synthetic body with no override; using either form is fine, but the four-arg call is what's there and it's correct).

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): wire LandableAsteroid to detail map; honor biome_override

Map-type switch routes LandableAsteroid into the Asteroid detail
generator (same as AsteroidBelt/DwarfPlanet). Body-side determine_biome
call uses the new body overload so biome_override on a custom body is
applied at entry.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Replace `make_asteroid_orbit` with `make_landable_asteroid` + add `make_scar_planet`

**Files:**
- Modify: `include/astra/body_presets.h` — drop `make_asteroid_orbit`, add two new factories
- Modify: `src/body_presets.cpp` — same
- Modify: `src/dev_console.cpp` — caller migration in the `chart create` block (single `make_asteroid_orbit` use)

- [ ] **Step 1: Replace the header**

Open `include/astra/body_presets.h`. Replace the existing `make_asteroid_orbit` declaration with:

```cpp
// A single landable asteroid. Distinct chart glyph ('*') from
// AsteroidBelt ('~'). Used as the anchor body for quests that place
// a fixture on the asteroid's detail map (e.g. Nova's beacon).
CelestialBody make_landable_asteroid(std::string name);

// A terrestrial planet with a forced scarred biome (glassed by default,
// or scorched). Used by quests that place content on a war-ravaged world.
CelestialBody make_scar_planet(std::string name,
                               Biome scar_biome = Biome::ScarredGlassed);
```

- [ ] **Step 2: Replace the impl**

In `src/body_presets.cpp`, replace `make_asteroid_orbit` with both presets:

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
    b.atmosphere = Atmosphere::Thin;
    b.temperature = Temperature::Hot;
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

- [ ] **Step 3: Migrate the dev_console caller**

In `src/dev_console.cpp`, find the line `spec.bodies = { make_asteroid_orbit(name + " Rock") };` inside the `chart create` block. Replace with:

```cpp
spec.bodies = { make_landable_asteroid(name + " Rock") };
```

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build. The single previous caller of `make_asteroid_orbit` now uses the renamed function; no other callers exist.

- [ ] **Step 5: Commit**

```bash
git add include/astra/body_presets.h src/body_presets.cpp src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): make_landable_asteroid + make_scar_planet presets

Replaces make_asteroid_orbit with make_landable_asteroid (uses the
new BodyType::LandableAsteroid). Adds make_scar_planet that builds
a Terrestrial body with biome_override forced to the chosen scar
biome. Dev-console chart create switches to the new asteroid preset.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Save v32 — serialize `biome_override`

**Files:**
- Modify: `include/astra/save_file.h:63` — version bump
- Modify: `src/save_file.cpp:824-875` (write_navigation_section), `:1623+` (read_navigation_section)

- [ ] **Step 1: Bump version**

In `include/astra/save_file.h` around line 63, change:

```cpp
uint32_t version = 32;   // v32: CelestialBody.biome_override
```

- [ ] **Step 2: Write per-body**

In `write_navigation_section` (line 824), inside the `for (const auto& body : sys.bodies)` loop, after `w.write_i32(body.day_length);` (the last existing body field), append:

```cpp
            // v32: biome override (optional Biome forced on entry)
            w.write_u8(body.biome_override.has_value() ? 1 : 0);
            if (body.biome_override) {
                w.write_u8(static_cast<uint8_t>(*body.biome_override));
            }
```

- [ ] **Step 3: Read per-body, gated**

In `read_navigation_section` (line 1623), find the corresponding body-read loop. After `body.day_length = r.read_i32();` (the last v10+ body field), append:

```cpp
                if (version >= 32) {
                    if (r.read_u8() != 0) {
                        body.biome_override = static_cast<Biome>(r.read_u8());
                    }
                }
```

- [ ] **Step 4: Build + smoke round-trip**

Run: `cmake --build build`
Expected: clean build.

Run: `./build/astra-dev --term`
- Start a new game; save; quit; load. Confirm no version error. Quit.

- [ ] **Step 5: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "$(cat <<'EOF'
feat(save): v32 — serialize CelestialBody.biome_override

One byte presence flag + one byte Biome value, appended after
day_length. Older saves load with biome_override default-nullopt.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Dev-console `chart create [kind] [name]`

**Files:**
- Modify: `src/dev_console.cpp` — `chart create` block (around lines 638-661)

- [ ] **Step 1: Update parsing**

The current handler interprets the second positional arg as the name. New shape: `chart create [kind] [name]`. `kind` is one of `asteroid` (default), `scar`, `rock`. If `args[2]` isn't a known kind, treat it as the name with `kind = "asteroid"` for backward compatibility.

Replace the body of the `if (args.size() >= 2 && args[1] == "create") { ... }` block with:

```cpp
        if (args.size() >= 2 && args[1] == "create") {
            std::string kind = "asteroid";
            std::string name = "Custom";
            if (args.size() >= 3) {
                std::string a2 = args[2];
                if (a2 == "asteroid" || a2 == "scar" || a2 == "rock") {
                    kind = a2;
                    if (args.size() >= 4) name = args[3];
                } else {
                    name = a2;
                }
            }

            auto coords = pick_coords_near(nav, nav.current_system_id,
                                           2.0f, 5.0f, game.world().rng());
            if (!coords) {
                log("chart create: couldn't find a spot near current system");
                return;
            }

            CustomSystemSpec spec;
            spec.name = name;
            spec.gx = coords->first;
            spec.gy = coords->second;
            spec.star_class = StarClass::ClassM;
            spec.discovered = true;

            if (kind == "asteroid") {
                spec.bodies = { make_landable_asteroid(name + " Rock") };
            } else if (kind == "scar") {
                spec.bodies = { make_scar_planet(name + " Prime") };
            } else { // "rock"
                CelestialBody b;
                b.name = name + " Rock";
                b.type = BodyType::Rocky;
                b.atmosphere = Atmosphere::None;
                b.temperature = Temperature::Cold;
                b.size = 2;
                b.landable = true;
                b.danger_level = 1;
                b.day_length = 200;
                spec.bodies = { std::move(b) };
            }

            uint32_t id = add_custom_system(nav, std::move(spec));
            log("Created custom " + kind + " system '" + name + "' id=" +
                std::to_string(id) + " at (" + std::to_string(coords->first) +
                ", " + std::to_string(coords->second) + ")");
        }
```

- [ ] **Step 2: Update help line**

Find the existing usage line (around line 134):

```cpp
log("  chart create [name] - create custom system near current");
```

Replace with:

```cpp
log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock)");
```

Also update the inline `Usage:` line at the end of the chart block:

```cpp
log("Usage: chart create [kind] [name]|reveal <name>|hide <name>");
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): chart create [kind] [name] for landable asteroid/scar/rock

Lets dev console exercise both new presets and the biome_override
path. Defaults to asteroid for back-compat.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: End-to-end smoke test

**Files:** none modified — manual verification.

- [ ] **Step 1: Launch**

```
./build/astra-dev --term
```

- [ ] **Step 2: Asteroid path (regression)**

Start a new game, dev console (backtick):

```
chart create asteroid Rockville
```

Open chart (`m`). Confirm `*` glyph in white near Sol. Land on body, enter detail map, confirm asteroid biome (Rocky or Crystal).

- [ ] **Step 3: Scar path (new)**

```
chart create scar Fireworn
```

Open chart. Confirm `o` glyph but in **pale cyan-white** (ScarredGlassed) — visually distinct from neighbouring green Terrestrials. Land on the body. Enter detail map.

Expected: detail biome generates from `ScarredGlassed` profile (no actual gameplay difference is in scope here — the biome just selects the right scatter/elevation profile).

- [ ] **Step 4: Save/load round-trip**

Save, quit to menu, load. Walk back to Fireworn. Color still pale cyan-white. Land again — same biome (override survived save/load).

- [ ] **Step 5: Optional rock kind**

```
chart create rock Plainville
```

Open chart. Glyph `o` in DarkGray (procedural Rocky). Confirms the non-override path still works.

- [ ] **Step 6: No commit**

Smoke test only. If any deviation, file as a follow-up issue (or fix in a separate task).

---

## Acceptance Criteria

- `cmake --build build` clean at every commit.
- `chart create asteroid Foo` plants a `*` (white) near current system, landable.
- `chart create scar Bar` plants an `o` (pale cyan-white) near current system; entering yields the ScarredGlassed biome.
- `biome_override` survives save → reload (verified by re-entering the body and getting the same biome).
- v31 saves load without complaint; their bodies have `biome_override = nullopt`.
- No `-Wswitch` warnings about `LandableAsteroid` after Task 1.
- No existing in-tree caller of `make_asteroid_orbit` remains.

---

## Out of Scope (explicitly deferred)

- New biomes (only existing scarred biomes are surfaced).
- Procedural production of scar planets.
- Tooltip / journal copy describing what the colored body is.
- New NPCs, Archon Sentinel miniboss.
- Neutron-star systems / star classes.
- Nova quest wiring (next plan).
