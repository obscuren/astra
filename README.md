# ASTRA

```
        .              *                              .             |
   *         .                   .                              .  -o-
    █████╗ ███████╗████████╗██████╗  █████╗        *               |
   ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔══██╗            .
   ███████║███████╗   ██║   ██████╔╝███████║   .
 * ██╔══██║╚════██║   ██║   ██╔══██╗██╔══██║        *
   ██║  ██║███████║   ██║   ██║  ██║██║  ██║  .
   ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝
         .          *       .          .
   .            .                 *
```

[![Build](https://github.com/obscuren/astra/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/obscuren/astra/actions/workflows/cmake-multi-platform.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/obscuren/astra?include_prereleases&label=release)](https://github.com/obscuren/astra/releases)
[![Website](https://img.shields.io/badge/web-jeff.lookingforteam.com%2Fastra-blue)](https://jeff.lookingforteam.com/astra)

A sci-fi roguelike set in the far future. Wake aboard a derelict starship above Jupiter, repair her systems, and chart a course through a procedurally generated Milky Way toward **Sagittarius A\*** — the supermassive black hole at the galactic core. Every system has its own history; every asteroid, its own dungeon.

Astra is written in C++20 with a pluggable renderer backend. The terminal is the primary target; an optional SDL3 frontend is available for a windowed grid.

---

## Getting started

### Build — terminal (default)

```bash
cmake -B build && cmake --build build
./build/astra
```

### Build — with the SDL3 frontend

Requires `sdl3` and `sdl3-ttf` via `pkg-config`.

```bash
cmake -B build -DSDL=ON && cmake --build build
./build/astra-sdl --sdl
```

### Build — developer mode

Enables the in-game dev console (`` ` `` / backtick) and extra debug tooling. The binary is renamed to `astra-dev` so it won't collide with a release build.

```bash
cmake -B build -DDEV=ON && cmake --build build
./build/astra-dev
```

### Command-line flags

| Flag          | Description                           |
|---------------|---------------------------------------|
| `--term`      | Use the terminal renderer (default)   |
| `--sdl`       | Use the SDL3 renderer (if compiled)   |
| `-h`, `--help`| Print usage and exit                  |

### Platforms

Linux, macOS, Windows (MSVC and MinGW — the latter produces a statically linked `.exe`). CI builds Ubuntu/gcc, Ubuntu/clang, and Windows/MSVC on every push and PR.

---

## How to play

### Character creation

A new game walks you through a seven-step wizard: **Type → Race → Class → Attributes → Name → Location → Summary**.

- **Races** — Human, Veldrani, Kreth, Sylphari, Xytomorph, Stellari. Each has attribute modifiers, an overworld glyph, and elemental resistance biases.
- **Classes** — Voidwalker (melee tank), Gunslinger (ranged), Technomancer (tinkering/intellect), Operative (stealth/social), Marauder (survivalist).
- **Attributes** — 10 points distributed across STR, AGI, TOU, INT, WIL, LUC.
- **Presets** run the full guided flow; **Random** rolls the whole character in one shot.

Every journey begins aboard **The Heavens Above**, a hub station orbiting Jupiter. Your first quest — *Getting Airborne* — has you salvaging three starship components so you can leave dock.

### Controls

Press **`?`** in-game for a full, tabbed reference.

#### General

| Key          | Action                                          |
|--------------|-------------------------------------------------|
| `Space`      | Interact with adjacent NPC, fixture, or object  |
| `Tab`        | Open character screen (remembers last tab)      |
| `?`          | Help overlay                                    |
| `Esc`        | Pause / game menu (save, load, options, quit)   |
| `` ` ``      | Dev console *(developer builds only)*           |

#### Movement

Arrow keys **or** `hjkl`. The world only advances when you do.

| Key     | Action                                           |
|---------|--------------------------------------------------|
| Arrows / `hjkl` | Move one tile                            |
| `.`     | Wait one turn                                    |
| `w`     | Auto-walk (follow with direction) — `ww` = BFS auto-explore |
| `>`     | Descend / enter stairs, portal, or detail map    |
| `<`     | Ascend / exit to previous map                    |
| `l`     | Enter look mode (examine any tile)               |

#### Combat

| Key     | Action                                  |
|---------|-----------------------------------------|
| `t`     | Enter targeting mode                    |
| `s`     | Shoot current target                    |
| `r`     | Reload ranged weapon                    |
| `1`–`6` | Activate learned ability from the bar   |

Melee is bump-to-attack. In targeting mode, a green line means in-range, red means out-of-range.

#### Items

| Key | Action                                    |
|-----|-------------------------------------------|
| `g` | Pick up the item underfoot                |
| `i` | Inspect item / inventory context          |
| `d` | Drop selected item                        |

#### Side panel widgets

The right-hand pane is built from toggleable widgets.

| Key         | Action                                   |
|-------------|------------------------------------------|
| `F1`        | Toggle **Messages**                      |
| `F2`        | Toggle **Wait** (long-rest controller)   |
| `F3`        | Toggle **Minimap**                       |
| `F4`        | Toggle **Interactables** (NPCs/fixtures/items nearby) |
| `Tab` / `Shift+Tab` | Cycle focus between active widgets |
| `Ctrl+H`    | Hide / show the entire panel             |
| `+` / `-`   | Scroll or adjust the focused widget      |

#### Character screen tabs

Cycle with `q` / `e` once the screen is open: **Skills, Attributes, Equipment, Tinkering, Journal, Quests, Reputation, Ship**.

---

## What's in the game

### Exploration

- **Overworld** — noise-driven biome maps with rivers, lakes, mountains, and forests.
- **Detail maps** — settlements, outposts, ruins, crashed ships, cave entrances, and fenced camps layered over each asteroid or planet surface via a deterministic POI budget.
- **Dungeons** — multi-level BSP rooms-and-corridors, open caves, and tunnel caves. Some POIs (crashed ships, outposts, cave mouths) gate a rare dungeon portal.
- **Hidden ruins** — a slice of every planet's ruin budget is rolled invisible. Stepping on one reveals it and logs a Discovery entry in your Journal; the count feeds your scanner report.
- **Day / night cycle** — dawn, day, dusk, night phases that throttle visibility.

### Galactic scale

- **Procedural Milky Way** — hundreds of systems, 80% of them carrying an ancient station.
- **Star chart viewer** — galaxy, region, local cluster, and system zoom levels with quest markers rendered at every scale.
- **Space station types** — Normal, Scav, Pirate, Abandoned, Infested — each with its own keeper NPC, specialty rooms, and loot profile.
- **Hyperspace travel** — plot jumps between systems once your ship has a working navi computer and hyperspace engine.

### World lore engine

Before the first turn, Astra simulates billions of years of galactic history: 2–5 precursor civilizations, their interactions, cataclysms, and a beacon network pointing toward Sgr A*. Lore annotates system tiers, seeds megastructures and scarred terrain, and colours every ruin the player finds. The `history` dev command dumps the full timeline.

### Starship

- **Ship interior** — every starship is a walkable dungeon with its own layout.
- **Component slots** — six equipment slots (hyperspace engine, navi computer, scanner, reactor, shields, cargo). Components are tiered and installable from the Ship tab.
- **ARIA** — a shipboard AI whose command terminal gates access to ship systems with context-aware dialog.
- **Tutorial quest** — *Getting Airborne* walks new commanders through repairing the ship before launch.
- **Observatory** — a view-only star chart for planning jumps before the ship is ready.

### Quests

- Full quest system with objectives, rewards, and a Quests tab split into **Main Missions / Contracts / Bounties / Completed**.
- **Story arcs** — hand-authored multi-quest DAGs (the Missing Hauler arc and the Nova / Stellar Signal arc ship today) with prerequisites, fan-out branches, reveal policies, and failure cascades.
- **Random quests** — kill / fetch / deliver / scout templates offered by NPCs based on role.
- **Quest markers** — `!` and `?` glyphs rendered on the overworld *and* at every star-chart zoom level.
- Full save/load of active and completed quests.

### Combat & progression

- Bump-to-attack melee plus targeted ranged combat with reload and ammo.
- Player dodge (AGI-scaled, capped 50%), crit chance (LUC-scaled, capped 30%).
- Status effects, an ability bar (keys `1`–`6`), and status readouts in the bottom bar.
- **Skills** — eight categories: Acrobatics, Short Blade, Long Blade, Pistol, Rifle, Tinkering, Endurance, Persuasion — each with passive bonuses and active abilities gated by attribute requirements. Archaeology skills are in the pipeline.

### Items, gear, tinkering

- Rarity tiers **Common → Uncommon → Rare → Epic → Legendary** with random affixes.
- Four elemental resistances: **acid, electrical, cold, heat**.
- **Tinkering workbench** — Repair, Disassemble, Analyze (learn blueprints), Enhance (slot materials), Synthesize (combine blueprints into new items).
- **Repair bench fixture** — no-skill credits-for-durability station placed in stations.
- Journal codex of every blueprint, discovery, and narrative event.

### NPCs & trading

- Branching dialog, shops, and reputation-sensitive pricing.
- Faction reputation gates dialog options, quest offers, and tiered shop stock.
- Fullscreen **trade window**: merchant stock on the left, your inventory on the right, `Space` to buy/sell, `Tab` to swap sides.
- Named NPC roles: keepers, merchants, drifters, scavengers, prospectors, civilians, pirate captains & grunts, black-market vendors, void reavers, and the archon remnants.

### Persistence

- Full world save/load including quest state, world lore, and galactic position.
- Hall of fame for fallen commanders.

---

## Architecture

Game logic is fully decoupled from platform code via an abstract `Renderer` interface (`include/astra/renderer.h`). Two implementations currently exist:

- **TerminalRenderer** — POSIX raw terminal with ANSI escape sequences and a dirty character buffer. A separate `terminal_renderer_win.cpp` uses the Windows Console API.
- **SdlRenderer** — SDL3 + SDL3\_ttf graphical window with a monospace font grid.

`Game` coordinates a 60 Hz loop — `poll_input → handle_input → update → render` — but all gameplay systems live in focused classes (`WorldManager`, `CombatSystem`, `DialogManager`, `SaveSystem`, `InputManager`, `HelpScreen`, `CharacterScreen`, `Tinkering`, …) rather than a single monolithic `Game` object. Generators (overworld, dungeon, ruin, settlement, outpost, cave, crashed-ship) live under `src/generators/`; NPC behaviour lives under `src/npcs/`; quests live under `src/quests/`.

Roadmap lives in [`docs/roadmap.md`](docs/roadmap.md). Formulas live in [`docs/formulas.md`](docs/formulas.md). Design specs and plans live in [`docs/plans/`](docs/plans/) and `docs/superpowers/`.

---

## License

[MIT](LICENSE)
