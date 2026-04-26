# ASTRA

```
        .              *                     .           |
   *         .                   .                      -o-
    █████╗ ███████╗████████╗██████╗  █████╗        *     |
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

A sci-fi roguelike set in the far future. Travel the Milky Way, crawl ancient ruins, upgrade your starship, and make your way toward Sagittarius A* — the supermassive black hole at the heart of the galaxy.

## The Game

You wake aboard **The Heavens Above**, a weathered space station orbiting Jupiter. Your ship is grounded, your credits are thin, and something is broadcasting from deep inside Conclave space. Trade with merchants who don't all speak your language, talk your way into work, repair your hyperspace drive, and plot a course through a procedurally generated galaxy that remembers everything that ever lived in it.

The world is turn-based — nothing moves until you do. Death is permanent. Reaching Sgr A\* and falling in begins a new cycle: a fresh galaxy, an intact ship, and whatever knowledge you've carried across the horizon.

### Character Creation

Build your character through a guided wizard. The roster below is **work in progress** — more races and classes will be added over time.

**Races** — each has unique attribute modifiers and elemental resistances:

| Race | Flavour | Strengths | Weaknesses |
|---|---|---|---|
| **Human** | The galaxy's generalists — adaptable and resourceful | +2 LUC, no penalties | No standout strengths |
| **Veldrani** | Tall, blue-skinned diplomats and traders | +2 INT, +1 WIL, +3 cold res | −1 STR, −1 TOU |
| **Kreth** | Stocky, mineral-skinned engineers | +3 STR, +2 TOU, +5 acid res, +3 heat res | −2 AGI, −1 LUC |
| **Sylphari** | Wispy, luminescent wanderers | +3 AGI, +2 WIL, +5 electrical res | −2 STR, −2 TOU |
| **Stellari** | Luminous, ancient stellar engineers | +3 INT, +2 WIL, +5 heat res, +3 electrical res | −1 STR, −1 AGI, −1 LUC |

(The **Xytomorph** exists as a chitinous predator race — NPC-only, not playable.)

**Classes** — each with a starting attribute profile, HP/carry bonuses, and a handful of skills already learned:

| Class | Role | Starting skills |
|---|---|---|
| **Voidwalker** | Frontline melee juggernaut. Heavy armour and long-blade mastery, +4 HP | Long Blade Expertise, Thick Skin |
| **Gunslinger** | Ranged specialist. Lightning reflexes, quick-draw pistols, acrobatic evasion | Steady Hand, Quickdraw, Swiftness |
| **Technomancer** | Engineer and hacker. Weak in direct combat, unmatched at the workbench | Basic Repair, Disassemble |
| **Operative** | Stealth agent and smooth talker. Short blades in the dark, silver tongue in the light | Short Blade Expertise, Jab |
| **Marauder** | Survivalist berserker. Shrugs off damage, relies on instinct and toughness | Thick Skin, Iron Will |

Finally, distribute **10 attribute points** across STR / AGI / TOU / INT / WIL / LUC, name your character, pick a starting location — or roll everything random.

### Exploration

Move through the world one step at a time — everything advances when you do. Land on asteroids and planets to walk their overworld surface. Every biome is hand-shaped by noise and lore — rivers, lakes, mountains, dead forests, ice shelves, ruins from civilizations that rose and fell long before you were born. Detail zones contain settlements, outposts, crashed ships, cave entrances, and hidden ruins that only reveal themselves when you step on them. Cave entrances and dungeon portals drop you into multi-level crawls full of loot, enemies, and locked doors.

A day/night cycle (dawn, day, dusk, night) affects visibility and what's moving around out there.

### Starship & Travel

Your ship is your home between systems. Walk its interior, talk to **ARIA** at the command terminal, install components into six equipment slots, and manage cargo. Systems are reached by plotting hyperspace jumps through black holes — the further you want to go, the better your **Hyperspace Engine** and **Navi Computer** need to be.

The **star chart** shows the galaxy at four zoom levels: galaxy, region, local cluster, and individual system. Press `F` to toggle faction ownership bands. Quest markers appear at every zoom level.

### Combat & Gear

Fight hostile creatures in dungeons with melee and ranged weapons. Equip armor, helmets, shields, and accessories. Items come in five rarity tiers (Common → Legendary) with random affixes that add stat bonuses. Level up to earn attribute and skill points.

#### How Combat Works

Combat is d20-based. Every attack rolls against the target's **Dodge Value**; on a hit, a separate **penetration roll** decides whether the blow punches through armor. Shields (if present) soak damage first, and affinity bonuses make certain shields or armor more effective against specific damage types.

**Attack roll** — `1d20 + modifier + weapon_skill_bonus ≥ target_DV`
- Player modifier: `(AGI − 10) / 2`
- NPC modifier: `level / 2`
- +2 if you're trained in the matching weapon class
- Nat 20 always hits, nat 1 always misses

**Penetration roll** — `1d10 + (STR − 10)/2` (player) vs. `target_AV + type_affinity`
- Each +4 over AV rolls weapon damage dice an extra time (heavy armor-pierce = multi-hits)
- Nat 10 always penetrates, nat 1 deals 0 damage

**Criticals (player only)** — `clamp((LUC − 8) × 2 + 3, 0, 30)%` chance to skip the penetration roll and roll damage dice twice. LUC is the direct counter to high-AV enemies.

**Damage types** — Kinetic, Plasma, Electrical, Cryo, Acid. Each has a resistance stat applied as a percentage after penetration, then any active status effects (burn, poison, invulnerable, etc.) modify the final number.

**Weapon dice** scale by rarity (Common `1d4` melee → Legendary `3d6+3` melee; ranged dice are one tier higher). See [`docs/items.md`](docs/items.md) for the full item catalog and [`docs/mechanics.md`](docs/mechanics.md) for the rules — HP regen, NPC scaling, action costs, day/night view radius, haggle math, and more.

### Skills

Spend skill points to unlock passive bonuses and active abilities. The tree is **work in progress** — more skills and categories will land over time. Current categories:

| Category | Description |
|---|---|
| **Acrobatics** | Agile movement and evasion. *Swiftness* (+5 DV vs. missile weapons), *Tumble* (reactive dodge on melee hit). |
| **Short Blade** | Knives and daggers — fast and precise. *Short Blade Expertise* (+1 hit, −25% primary-hand action cost), *Jab* (off-hand quick strike). |
| **Long Blade** | Swords and cleavers — heavy strikes. *Long Blade Expertise* (+1 hit, better parry), *Cleave* (hit all adjacent enemies). |
| **Pistol** | Sidearms and close-quarters fire. *Steady Hand* (+1 accuracy), *Quickdraw* (draw + fire in one cheap action). |
| **Rifle** | Long-range firearms. *Marksman* (+2 range, better long-range accuracy), *Suppressing Fire* (cone pin). |
| **Tinkering** | Repair, modify, and break down technology. *Basic Repair*, *Disassemble*, *Synthesize* (craft new items from learned blueprints). |
| **Endurance** | Physical and mental resilience. *Thick Skin* (+1 AV), *Iron Will* (+5 vs. psionic effects). |
| **Persuasion** | Social influence. *Haggle* (−10% buy / +10% sell), *Intimidate* (frighten hostile creatures into fleeing). |
| **Wayfinding** | Navigation and overland travel. *Compass Sense* (faster recovery from being lost), five *Terrain Lore* skills (50% less lost, halved travel time on matching biome), *Scout's Eye* (NPCs on minimap), *Cartographer* (items and POIs on minimap). |
| **Archaeology** | Study of precursor civilizations. *Ruin Reader* (full lore-fragment text), *Artifact Identification* (auto-ID on pickup), *Excavation* (search ruins for caches), *Cultural Attunement* (bonuses with studied civs), *Precursor Linguist* (unlock sealed doors), *Beacon Sense* (Sgr A* beacons on star chart). |

Active abilities bind to keys `1`–`5` when learned. Most skills gate behind an attribute requirement (usually INT, AGI, or WIL) and a skill-point cost.

### Tinkering

A workbench (character screen → Tinkering tab) lets you work with equipment and materials. Each action is gated behind a skill in the **Tinkering** category — you start with none of them, and unlock *Basic Repair*, *Disassemble*, and *Synthesize* by spending skill points (most require Intelligence 15+).

- **Repair** damaged gear with scrap *(requires Basic Repair)*
- **Disassemble** items into Nano-Fiber / Power Core / Circuit Board / Alloy Ingot *(requires Disassemble)*
- **Analyze** items to learn blueprints (recorded in your Journal)
- **Enhance** gear by slotting materials for permanent stat boosts
- **Synthesize** new items from two learned blueprints + materials *(requires Synthesize)*

### Quests & Factions

Random contracts (kill / fetch / deliver / scout) from NPCs based on their role, plus hand-authored story arcs — the current main arc is **The Stellar Signal**, a multi-stage mystery about a transmission coming from deep inside Conclave-controlled space. Reputation with factions affects prices, dialog gates, and quest availability.

### Controls

| Key | Action |
|-----|--------|
| Arrow keys / `hjkl` | Move |
| `<space>` | Interact with adjacent NPC or object |
| `<tab>` | Character screen |
| `g` | Pick up item from floor |
| `d` | Drop item from inventory |
| `t` / `s` / `r` | Target / shoot / reload |
| `.` | Wait one turn |
| `w` + dir / `ww` | Auto-walk / auto-explore |
| `>` / `<` | Use stairs / board ship |
| `c` / `i` | Character screen / inventory tab |
| `1`–`5` | Activate ability |
| `Ctrl+H` | Toggle side panel |
| `Esc` | Close window / Game menu |

## World Generation

Astra's galaxy is generated deterministically from a seed, in layers:

1. **Galaxy simulation** — before the player spawns, a state-driven civ sim runs across billions of years: **4–8 precursor civilizations** rise and fall with traits, interactions, and a beacon network pointing toward Sgr A*. Names come from six phoneme pools. The full timeline is dumpable with the `history` dev console command.
2. **Star systems** — spiral-arm placement using the real map of the Milky Way. Each system has 1–2 stars (binary), planets, moons, asteroid belts, and ~80% chance of a precursor space station. Systems are clustered into **faction ownership** bands.
3. **Planet overworlds** — noise-based biomes, rivers, lakes, mountains, shaped by the lore layer (ancient weapon tests scar terrain, terraforming shifts biomes, megastructures become orbital POIs).
4. **POI placement** — a deterministic per-planet `PoiBudget` scores candidate sites against terrain requirements and places settlements, outposts, ruins, crashed ships, cave entrances, and landing pads. A subset of ruins are rolled **hidden** — they render as underlying biome until you step on them, then log to your Journal.
5. **Detail zones** — each overworld tile expands into a detail map when entered, stamping the relevant POI (settlement, outpost, ruin, cave entrance, crashed ship…) with variant-specific layouts.
6. **Dungeons** — rooms + corridors + locked doors beneath cave entrances and portal POIs, with loot, enemies, and fixtures themed to the parent POI.

Everything persists: world state, lore timeline, quest progress, faction reputation, and hall-of-fame entries for fallen characters all save to disk.

---

## Build

Terminal renderer (default, primary target):

```bash
cmake -B build && cmake --build build
./build/astra
```

SDL3 renderer — **experimental and not really playable yet**. Only a handful of implementation details are wired up; use the terminal build for actual gameplay.

```bash
cmake -B build -DSDL=ON && cmake --build build
./build/astra --sdl
```

## Architecture

Astra is C++20 with **strict separation between game logic and platform code**. Everything the game does is expressed against an abstract `Renderer` interface (`include/astra/renderer.h`) — the game never touches a system call, an ioctl, a Win32 API, or an SDL function directly.

### Renderer backends

- **TerminalRenderer** — character-grid renderer with ANSI escape sequences and a cell buffer. Split backends: POSIX (`terminal_renderer.cpp`, raw termios) and Windows (`terminal_renderer_win.cpp`, Win32 console API + virtual terminal processing). This is the primary target.
- **SdlRenderer** — SDL3 + SDL3_ttf graphical window with a monospace font grid (deferred, experimental).

Key virtual keycodes (`KEY_UP`, `KEY_DOWN`, …) are defined in `renderer.h` so both backends translate native events into the same values before handing them to the game.

### The Game coordinator

`Game` is deliberately a thin orchestrator — it owns a `unique_ptr<Renderer>`, a `GameState` enum (MainMenu, Playing, …), and drives the main loop at 60 fps:

```
poll_input → handle_input → update → render
```

The world itself is **turn-based**: `update` only advances simulation after the player acts. Between keypresses, nothing moves — the render loop just keeps drawing the paused state and advances animations.

### Subsystems

Game behaviour is pulled out of `Game` and into focused classes, each owning one concern:

| Subsystem | Responsibility |
|---|---|
| `WorldManager` | World state container, map transitions, celestial bodies |
| `InputManager` | Input routing, look mode, target mode |
| `CombatSystem` | Attack/penetration rolls, damage pipeline, status effects |
| `DialogManager` | NPC dialogs, branching trees, quest hooks |
| `QuestManager` | Quest state, objectives, markers, persistence |
| `SaveSystem` | Tagged-section save/load of world, lore, quests, player |
| `DevConsole` | Dev-mode command prompt (`history`, spawn tools, etc.) |
| `HelpScreen` | Help overlay |
| `MapRenderer` | Reusable map-drawing primitive used by overworld, dungeons, ship, station |
| `CharacterScreen` | The `<tab>`-key screen with its eight tabs |
| `Tinkering` | Workbench actions (repair / disassemble / analyze / enhance / synthesize) |
| `EventBus` | In-process typed event bus used by the story-quest scenario graph |
| `GalaxySim` / `LoreGenerator` | Pre-play civilization simulation, names, lore influence maps |
| `PoiBudget` / `PoiPlacement` | Deterministic per-planet POI budgeting and site scoring |
| `OverworldGenerator` / `RuinGenerator` / `CaveEntranceGenerator` / ... | Template-method terrain and POI generators |

### Conventions

- Mechanics and constants live in `docs/mechanics.md` — if you touch a combat/economy number in code, update the doc in the same commit
- Item stats (weapons, armor, cells, mods, etc.) live in `docs/items.md` — update when items change

## Platforms

Linux, macOS, Windows. CI builds on Ubuntu (gcc, clang) and Windows (MSVC).

## License

[MIT](LICENSE)
