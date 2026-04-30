# Hacking & The Grid — Design Spec

**Date:** 2026-04-29
**Status:** Draft, awaiting user review
**Branch:** `feature/hacking`

---

## 1. Concept & pillars

Hacking is a niche playstyle in Astra centered on three loops:

1. **Quickhacks (B-layer)** — lightweight, menu-driven, in-the-world targeted hacks against electronic devices (turrets, doors, cameras, NPC implants). Available to *anyone with a deck and the right loaded program*; not gated by skill alone, but skill makes them better.
2. **The Grid (A-layer)** — jack into a console to upload consciousness. Body becomes phased-out and invulnerable for the duration. Avatar walks Tron-styled tile sectors connected by a node graph (subnet → regional darknet → deep-Grid). Combat is HP-light, Trace-heavy. Black ICE deals real HP that bleeds back to the body — die in the Grid, die IRL.
3. **The deep-Grid (D-layer)** — a persistent cyberspace metaverse layer. **The deep-Grid persists across Sgr A\* rebirth.** Your body is reborn naked in a fresh galaxy; your Grid identity, base, AI contacts, decrypted lore archives, and Grid currency carry over. This is the literal mechanical expression of "infinite knowledge of the universe."

### Pillars (load-bearing principles)

- **Identity through resource allocation, not class lock.** Anyone can dabble; only investment makes a real decker.
- **Trace > damage.** The Grid is a heist with a clock, not a slugfest.
- **The deep-Grid is the meta-layer.** Hacking gives the fastest/cheapest path to it; other builds reach it slower via cybernetics + Precursor consoles.
- **Integrate, don't add.** Programs are tinkering items. Decks are loot. Skills are a new `Cat_Hacking` category. Quickhacks reuse the targeting system. The PDA replaces the character screen as a tabbed device.

### Non-goals

- Complex command-line parsers / "real" shells. The terminal is bounded and small.
- Multiplayer / shared deep-Grid (single-player only).
- Replacement of physical combat. Hacking is parallel to, not above, fighting.
- Voice/audio. ASCII/text only.

### Gating model (the niche-playstyle line)

| Layer | Gate |
|---|---|
| Quickhacks (B) | Equipped cyberdeck + loaded `.qh` program. Any character. |
| Jacking into the Grid (A) | `Cat_Hacking` skill category unlocked (1 skill point). The line between dabbler and decker. |
| Deep-Grid traversal | `DeepGridNavigator` skill (gateway crack). |
| Cross-Sgr-A\* deep-Grid persistence | `ConsciousnessAnchor` capstone skill (full); or non-hacker path via cybernetic Neural Backup implant + Precursor console sync (partial — lore archive only). |

---

## 2. Architecture

This is a feature decomposition. Each unit has one clear job. Follows project rule: `Game` is a coordinator, not a container.

### New subsystems

| Unit | Header | Responsibility |
|---|---|---|
| `Hackable` (component on objects/NPCs) | `hackable.h` | Marks an entity as hackable. Carries: device kind, security tier, current state (Clean/Compromised/Alarmed), available quickhacks, owning network ID, optional jack-in target node. |
| `Cyberdeck` (item) | `cyberdeck.h` | Item subclass with stats: `ram_max`, `cpu`, `slots`, `stealth`, `cooling_rate`, `heat_cap`. Loaded programs in slot array. Equipped like armor. |
| `Program` (item) | `program.h` | Item subclass. Fields: `kind {ATK\|STL\|UTL\|QH}`, `tier`, `ram_cost`, `heat_cost`, `effect_id`, `target_filter` (for QH). Crafted via tinkering or looted. |
| `HackingSystem` | `hacking_system.h/cpp` | Coordinator. Resolves quickhack attempts, manages jack-in, owns active Grid session, talks to Renderer for terminal/Grid views. |
| `GridSession` | `grid_session.h/cpp` | Live state of a jacked-in run: avatar position, HP, RAM, Heat, Trace, current sector, current node, loaded programs runtime. Lifetime = jacked-in duration. |
| `Network` (graph) | `grid_network.h` | Per-galaxy graph of nodes (subnets/darknets) + the persistent deep-Grid graph. Edges encode gateway requirements. |
| `GridSector` | `grid_sector.h` | A single tile sector: tilemap, ICE actors, data nodes, exits. Generated procedurally per node-kind, or loaded from persistent state for deep-Grid anchor sectors. |
| `Pda` (UI screen) | `pda_screen.h/cpp` | Tabbed screen replacing `character_screen`. Existing 9 tabs preserved (Skills, Attributes, Equipment, Tinkering, Cooking, Journal, Quests, Reputation, Ship); new `Hacking` tab added — see §4. Each tab is a separate render module. |
| `PdaHackingTab` | `pda_hacking_tab.h/cpp` | Terminal-flavored Hacking tab. Owns the bounded command set, tab-completion, history. |
| `Trace` (resource on `GridSession`) | embedded | Counter 0–100, ticks per turn, raised by Heat & noisy actions, lowered by Stealth programs. Hits 100 → black ICE summon. |
| `ConsciousnessSave` | `consciousness_save.h/cpp` | Meta-save. Separate file scope from galaxy-save. Holds: deep-Grid base inventory, AI contact reputation, decrypted lore, Grid currency, persistent avatar identity. |

### Skill tree additions (`skill_defs.h`)

```cpp
Cat_Hacking         = 12,    // REQUIRED to jack in. Without it, quickhacks still work.
// Hacking unlocks
Intrusion           = 1200,  // -1 trace per noisy action
IceBreaking         = 1201,  // +1 dmg vs ICE
DaemonMastery       = 1202,  // +1 deck slot
GhostProtocol       = 1203,  // first program each Grid run is heatless
DeepGridNavigator   = 1204,  // gateway crack chance + map reveal
NeuralFortitude     = 1205,  // halve black-ICE bleed-through
CodeCraft           = 1206,  // unlock T3 program tinker recipes
ConsciousnessAnchor = 1207,  // (capstone) full deep-Grid persistence for this character
```

### Existing systems integrated

- **Tinkering** — Programs are a new schematic family. New material category: `code_fragment` (T1/T2/T3 fragments dropped by Grid loot). Recipes already work.
- **Loot tables** — New item categories `cyberdeck` and `program` with tier weights. Existing pipeline.
- **Interaction** — `Hackable` becomes a new `InteractionData` trait alongside `talk/shop/quest`. Pressing interact on a hackable shows quickhack menu plus "Jack In" if it has a jack-in node.
- **InputManager** — New keybind `H` opens quickhack target cursor. PDA opens via existing character-screen key, retabbed.
- **Renderer** — Grid sectors reuse the tilemap renderer with a new palette/glyph set. Graph view is a new render mode (similar pattern to the existing star chart's zoom layers).
- **Save format** — Galaxy save bumps `SAVE_FILE_VERSION`. New separate `consciousness.dat` save file (per character profile).
- **Energy system** — RAM regenerates between Grid runs; not tied to global Energy. Heat is local to Grid sessions only.

### Data flow — in-world quickhack

```
Player presses H
  → InputManager → HackingSystem::begin_quickhack_targeting()
  → Renderer cursor mode (existing)
  → Player selects target tile
  → HackingSystem checks Hackable component on target
    + queries equipped Cyberdeck for loaded QH programs
    + filters programs by target_filter
  → Popup menu of available quickhacks
  → On select: HackingSystem::execute_quickhack(program, target)
    - check ram, debit ram, add Detection (not Heat — quickhacks are out-of-Grid)
    - apply effect via target's Hackable handler
    - emit message + event
  → InputManager returns to play mode
```

### Data flow — jack in

```
Player on a jack-in capable Hackable, presses interact
  → preconditions: Cat_Hacking unlocked, deck equipped
  → menu includes "Jack In"
  → HackingSystem::jack_in(node_id)
    - mark body phased-out (invulnerable, frozen)
    - construct GridSession (avatar HP from skills+deck, ram from deck, heat=0, trace=0)
    - resolve initial sector for node from Network
  → Renderer enters Grid mode (graph view first, then sector)
  → Game loop now drives GridSession instead of overworld/dungeon
  → On jack_out (voluntary or forced): persist sector state if deep-Grid, restore body
```

### File-size discipline

Each file ≤ ~400 lines target. `HackingSystem` stays a thin coordinator; per-program effects live in a `program_effects.cpp` lookup table, not in the system itself. Same pattern as `combat_system` + `dice` + `effect`.

---

## 3. The Grid: tiers, sectors, ICE, Trace

### Grid tier definitions

| Tier | Source nodes | Sector size | ICE difficulty | Trace tick / turn | Loot quality | Persists? |
|---|---|---|---|---|---|---|
| **Subnet** | A single hackable device or local LAN | 1–3 small rooms (6×6 to 10×10) | T1, mostly white ICE | +1 | Code-fragments T1, credits, log fragments | No (regen on entry) |
| **Regional darknet** | Station-wide, asteroid-wide, or shared-corp networks | 4–8 rooms, full mini-dungeon | T1–T2, mixed ICE incl. occasional gray | +2 | Code-fragments T1–T2, programs (rare), schematics | Per-galaxy stable; resets on Sgr A\* |
| **Deep-Grid** | Anchor sectors only reachable via cracked gateways from regional darknets | Full dungeon depth (multi-floor) | T2–T3, gray + black ICE | +3 | Code-fragments T2–T3, exotic programs, lore archives, Grid currency, AI faction rep | **Persists across Sgr A\* rebirth** |

### ICE color tiers

- **White ICE** — patrols, harmless individually, contributes to Trace if it sees you.
- **Gray ICE** — engages your avatar; deals **avatar HP** damage only; bleed stays virtual.
- **Black ICE** — deals **real HP damage** that bleeds back to body. Spawns when Trace = 100, or pre-placed in deep-Grid critical sectors.

### What's in a sector

```
┌──────────────────────────────────────────────────────────┐
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ░ ▓▓▓▓▓▓▓ ░░░░░░░░░░░░░░░░░░░░░░ ▓▓▓▓▓▓▓ ░░░░░░░░░░░░░░░ │
│ ░ ▓     ▓ ░░░░ ◇ ░░░░░░░░░░░░░░░ ▓     ▓ ░░░░░░░░░░░░░░░ │
│ ░ ▓  $  ▓ ░░░░░░░░░░ ▲  ░░░░░░░░ ▓  ⌬  ▓ ░░░░░░░░░░░░░░░ │
│ ░ ▓     ▓ ░░░░░░░░░░░░░░░░░░░░░░ ▓     ▓ ░░░░░░░░░░░░░░░ │
│ ░ ▓▓▓ ▓▓▓ ░░░░░░░ @ ░░░░░░░░░░░░ ▓▓▓ ▓▓▓ ░░░░░░░░░░░░░░░ │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ░░░ Trace ████░░░░░░░░░░░░░░░░░░░░░░░░░░░  RAM 4/8  ░░░░ │
│ ░░░ Heat  ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  HP   3/3  ░░░ │
└──────────────────────────────────────────────────────────┘
```

Glyph palette (cyan/magenta/violet on near-black):

- `@` avatar
- `▓` firewall (impassable, breachable by `breach.exe`)
- `░` Grid floor
- `◇` gray ICE (engages on sight)
- `▲` black ICE (rare, slow, deadly)
- `▼` white ICE (patrols, raises Trace)
- `$` data node (loot)
- `⌬` gateway to next tier
- `⊙` exit / disconnect node
- `⊘` encrypted file (decrypt = lore unlock)

### Trace — the central pacing mechanic

Counter `[0, 100]`, shown as a HUD bar.

**Raises Trace:**
- Tier baseline tick per turn (subnet +1, regional +2, deep-Grid +3)
- Heat overflow: every 5 Heat above threshold = +1 Trace per turn while Heat > 5
- Visible to white ICE: +2 per turn the patrol can see you
- Forcing a gateway / breaching firewall: +5 burst
- Killing ICE: +3 per kill (fighting raises Trace — stealth is incentivized)

**Lowers Trace:**
- Stealth programs (`ghost_trace.exe`: -3, costs RAM + Heat)
- Hiding in unobserved tile for N turns: -1 per turn
- `Intrusion` skill: passive -1 per noisy action

**Breakpoints:**
- `≥ 50` — alert: white ICE actively converges
- `≥ 75` — gray ICE reinforcements spawn
- `= 100` — **black ICE summon**: an extra black ICE spawns at a known node and pursues. Trace stays at 100 until you escape or die.

### Heat — the player-controlled risk dial

Per-deck, `[0, deck.heat_cap]`. T1 deck heat_cap = 10, +2 per tier.

- Each program firing adds `heat_cost`.
- Heat decays `cooling_rate` per turn (default 1).
- While Heat > 5, +1 Trace tick per turn (Heat→Trace coupling).
- Heat > heat_cap → forced **deck reboot**: skip 2 turns, lose all RAM, Trace +10.

### Programs (canonical examples — full catalog in `docs/items.md`)

| Program | Kind | RAM | Heat | Effect |
|---|---|---|---|---|
| `icebreaker_lite.exe` | ATK | 2 | 2 | 1d4 dmg to ICE in line of sight |
| `pulse_hammer.exe` | ATK | 4 | 5 | AoE 1d6 dmg to all ICE adjacent to a target tile |
| `ghost_trace.exe` | STL | 3 | 0 | -3 Trace, invisible to white ICE for 3 turns |
| `cooldown.exe` | STL | 2 | 0 | Heat -4 instantly |
| `breach.exe` | UTL | 3 | 3 | Open one firewall tile or unlock one gateway level |
| `decrypt.exe` | UTL | 2 | 1 | Read one encrypted file (lore drop) |
| `daemon_hijack.exe` | UTL | 5 | 4 | Take control of one ICE for 3 turns |
| `reboot_optics.qh` | QH | 1 | n/a | (real world) blind a camera/turret for 4 turns |
| `friendly_fire.qh` | QH | 3 | n/a | (real world) turret fires on its allies for 2 turns |
| `data_leech.qh` | QH | 2 | n/a | (real world) read NPC implant — get next dialog branch hint |

Quickhacks (`.qh`) only fire in the real world; their `heat` is replaced by a global **Detection** value (+1–3 per QH) that decays out of combat. Soft alarm system, not Trace.

### Sector generation

- Subnet sectors are seeded from `(network_id, device_id)` — same room generates if you re-enter the same turret.
- Regional darknet sectors are pre-generated when the galaxy seed is established and persist for the run.
- Deep-Grid anchor sectors are hand-authored layouts (small library) or seeded from `consciousness_id` so a player's deep-Grid base is the same shape every rebirth.

### Voluntary / forced disconnect

- **Voluntary**: walk to `⊙` → 1-turn channel → safe out, all loot kept.
- **Mid-sector hard jack-out**: hotkey, +10 Trace burst, drops 50% of unsaved loot.
- **Forced (avatar HP = 0)**: per Q4b model — black-ICE death = real HP damage, possible IRL death. Non-black-ICE death = avatar wiped, body wakes with debuff and unsaved loot lost.

---

## 4. UX: PDA, terminal, and the quickhack flow

### Hard constraint — behavior-preserving refactor

The PDA tab extraction is a **pure refactor**. Each existing tab's rendered output and input behavior must be identical before and after extraction. The only user-visible changes from this work are:

1. The screen title/frame says "PDA."
2. A new HACKING tab is added at the end.
3. Tab navigation is unchanged.

Frame restyle scope: cosmetic only and limited to the outer chrome (title bar text, possibly a subtle device-edge motif). The interior of every existing tab stays pixel-identical.

### PDA replaces `character_screen`

Existing 9-tab character screen becomes the PDA. Tabs preserved as-is: `Skills, Attributes, Equipment, Tinkering, Cooking, Journal, Quests, Reputation, Ship`. **New tab added: `Hacking`** (10th).

`character_screen.h/cpp` (4347 lines) is split into:

- `pda_screen.h/cpp` — thin tab dispatcher (~300 lines).
- One module per tab: `pda_skills_tab`, `pda_attributes_tab`, `pda_equipment_tab`, `pda_tinkering_tab`, `pda_cooking_tab`, `pda_journal_tab`, `pda_quests_tab`, `pda_reputation_tab`, `pda_ship_tab`, `pda_hacking_tab`.

Each existing-tab extraction lands as its own commit (verify visual parity between commits).

### Hacking tab — terminal-flavored

When `Cat_Hacking` not unlocked: locked-screen splash explaining how to unlock, plus a list of equipped quickhack programs (visible, usable in the world).

When unlocked:

```
┌──────────────────────────────────────────────────────────────────┐
│  PDA › HACKING                                  Polyglot DCK-2   │
├──────────────────────────────────────────────────────────────────┤
│  RAM 6/8   CPU 2   SLOTS 4   STEALTH +1   COOLING 1/turn         │
│                                                                  │
│  pda> programs ls                                                │
│    [a] icebreaker_lite.exe   ATK   2 RAM   2 Heat                │
│    [b] reboot_optics.qh      QH    1 RAM   —                     │
│    [c] ghost_trace.exe       STL   3 RAM   0 Heat                │
│    [-] (slot empty)                                              │
│                                                                  │
│  pda> netmap                                                     │
│    [Hangar.7]──[Station.Spine]──╳──[??.regional]                 │
│           │                                                      │
│         [Vault.Local]                                            │
│                                                                  │
│  pda> jack -t Hangar.7                                           │
│    >> uploading consciousness... <<                              │
│                                                                  │
│  pda> _                                                          │
└──────────────────────────────────────────────────────────────────┘
```

### Bounded command set (v1)

Every command has a menu fallback (arrow-driven list).

| Command | Menu equivalent | Notes |
|---|---|---|
| `help` | `?` key | List commands |
| `deck info` | tab default view | Show deck stats + heat cap |
| `programs ls` | `P` key | List loaded programs |
| `programs load <slot> <id>` | select slot → menu of inventory programs | Load from inventory |
| `programs unload <slot>` | select slot → "unload" | Free a slot |
| `netmap` | `N` key | Show graph of known networks |
| `jack -t <node>` | select node on netmap → "Jack In" | Enters Grid (if `Cat_Hacking` unlocked) |
| `lore` | `L` key | Decrypted lore archives (deep-Grid loot) |
| `clear` / `history` | n/a | UX niceties |

Tab-completion: prefix-match on commands and known-node IDs only. No globbing, no piping. The terminal is a UI metaphor with strict bounds.

### Graph view — three zoom layers

Parallel to the existing star chart pattern.

```
SUBNET zoom            REGIONAL zoom              DEEP-GRID zoom
┌──────────┐           ┌────────────────┐         ┌──────────────────┐
│ Turret.1 │──┐        │  Station.Spine │─╳─┐     │  Archive.Vault   │
│          │  │        │     │          │   │     │       │          │
│ Door.A   │──┼──[hub] │  Conclave.Net  │   ├──── │  AI.Contact      │
│          │  │        │                │   │     │                  │
│ Console  │──┘        │  Pirate.Skiff  │───┘     │  Consciousness.  │
│          │           │                │         │     Anchor       │
└──────────┘           └────────────────┘         └──────────────────┘
   local                regional darknet            persistent layer
```

Cracked gateways become traversable edges. Locked gateways show as `╳` with a `[breach required]` hint.

### Real-world quickhack flow (B-layer)

Press `H` (only enabled if a deck is equipped).

```
1. H → cursor enters quickhack-target mode (cyan crosshair)
2. Cursor hovers a Hackable → side panel shows:
     - device name + tier
     - state (Clean / Compromised / Alarmed)
     - available QH programs from loaded slots that match target filter
     - greyed-out programs you don't have but could use
3. Enter on target → program picker popup (only matching .qh)
4. Select → spend RAM, +Detection, apply effect, message log line
5. Esc → cancel
```

Visual cue: hackable objects in the world get a subtle cyan tint or a `▾` indicator when a deck is equipped (similar pattern to the interactables widget). Without a deck, no indicator.

### Detection (real-world soft alarm)

Per zone, `[0, 100]`. Each loud QH adds 1–3. Decays `-1` per N turns out of sight.

- `≥ 50`: nearby NPCs investigate
- `≥ 75`: faction reputation hit, hostile responders may spawn
- `= 100`: zone-wide alarm — turrets re-arm, doors lock, station guards respond per existing reputation system

Integrates with existing reputation-driven hostility.

### Body while jacked in

Per Q4a-B: body is **phased out** — invulnerable and removed from threat AI. Render a faint `@` ghost glyph at the jack-in console so the player knows where the body is. (When the vulnerable-body model is revisited, the ghost is replaced with a real `@` and AI eligibility re-enabled — natural attachment point.)

### Grid HUD — deferred to Plan 5

Plan 3 ships a debug-quality HUD: four uncoloured monospaced bars (HP / RAM / Trace / Heat) and the sector tilemap. That's enough to verify mechanics; it is not a real interface. A full Grid HUD pass is **owned by Plan 5**, brainstormed fresh once Plans 3 and 4 are merged and playtested. The brainstorm should not start from the bullets below — those are seed material, not a locked layout.

**Load-bearing constraints any Grid HUD design must respect:**

- The Grid HUD is its own subsystem, not the play HUD reskinned. Distinct translation unit, distinct palette, distinct information model. The play HUD's surface-mode coupling and player-stat assumptions don't transfer.
- **Trace is the primary on-screen quantity.** The player loses to Trace, not HP. It must be the most visible thing on screen, color-shifting at the 50/75/100 breakpoints.
- **Every visible ICE must be enumerable at a glance.** Each one is a per-turn Trace source; the player needs to see who sees them and at what distance, without scanning the tilemap.
- **Program slots must be selectable.** The Task 10 "fire the first loaded `.exe`" model is a stub. The HUD must surface each loaded slot with its RAM cost and heat cost, and bind to a real picker (probably `f` then a letter key, mirroring the play ability bar pattern).
- **Effects with remaining duration must be visible.** `GhostCloak`, `BlackIceShock`, `IceBreakerCharge`, future buffs — all need a strip showing name + ticks left. Without this the player can't reason about the cloak window or the post-death debuff.
- **Plan 4 surfaces must have homes.** Grid currency, implant status, signature programs, AI-contact reputation all need on-screen real estate. The layout cannot be designed before Plan 4 lands or it will need to be redone.
- **Tron-styled visual language**, coordinated with `grid_theme.h`. Single accent for primary data, dim for secondary, red reserved for danger states (Trace ≥ 75, deck overheating, black ICE adjacent). Match the in-grid tile palette so the visual language is unified.
- **The Grid renderer and Grid HUD live in separate files.** `grid_renderer.cpp` stays focused on tile + actor rendering; HUD goes in `grid_hud.cpp` (or similar). Once effect strips, threat panels, and program slots land, the combined file would blow past the 600-line discipline target.

**Seed bullets — starting points for the Plan 5 brainstorm, not commitments:**

- A **top bar** showing current node label, tier, and a connection indicator that flickers when Trace is climbing.
- A **right column** mirroring the shape of the play side panel, but stacked: Trace (largest), Heat (with the >5 coupling threshold marked), RAM, avatar HP, then an effects strip.
- A **threat panel** above the program list — one row per visible ICE with color, distance, and "sees you" indicator.
- A **program slot bar** at the bottom (mirroring the existing ability bar), each cell showing program glyph + RAM cost + heat cost, hotkey-driven.
- A **bottom message strip** styled like a CRT readout — green-on-near-black, monospaced, recent program outputs and ICE actions.

---

## 5. Hackable taxonomy, non-hacker access, persistence

### Full hackable taxonomy

The system supports all of these. v1 cut comes in §6.

**Tier 1 — Combat-relevant**
- `Turret` — QHs: reboot, friendly_fire, overheat. Jack-in: subnet (1-room).
- `Drone` — QHs: blind, retarget, self_destruct. Jack-in: subnet.
- `Mine` / `Trap` — QHs: disarm, retarget, see_through. No jack-in.
- `Camera` — QHs: blind, loop_footage. Jack-in: surveillance subnet.
- `Door` / `Lock` — QHs: bypass_lock, jam_shut. No jack-in.

**Tier 2 — Environmental**
- `Light` — QHs: kill_lights (regional), flicker. No jack-in.
- `Vendor terminal` — QHs: discount, dump_stock. No jack-in.
- `Elevator` — QHs: call, lock_floor. No jack-in.
- `Hazard system` — QHs: vent_gas, fire_suppress. No jack-in.
- `Power conduit` — QHs: blackout. Jack-in: connects to regional darknet for the zone.

**Tier 3 — Sci-fi / lore**
- `NPC implant` — QHs: read_memory, induce_glitch, override_loyalty. Jack-in: cybernetic subnet. Hackable on NPCs flagged `cybernetic = true`.
- `Ship system` — QHs: comms_intercept, life_support_toggle, nav_scramble. Jack-in: ship's local network.
- `Precursor / Archive console` — deep-Grid gateways. No QHs; only jack-in. Cracking gates regional → deep-Grid traversal.
- `Faction reputation server` — QHs: read_record, doctor_record, plant_evidence. Jack-in: corporate regional darknet.
- `Wreckage / log fixture` — QHs: download_logs (passive lore loot).

**Tier 4 — Wild**
- `Quest-fixture puzzles` — generic mechanism for replacing "find the button" with "hack the panel."
- `Translator firmware on language-barrier NPCs` — `translate_overlay` makes dialog readable for N turns.
- `Sgr A\* event-horizon node` — endgame; lets you choose what survives the rebirth.

### Non-hacker deep-Grid access path

Two cooperating systems give every build a route to the lore archive:

1. **Cybernetic Neural Backup implant** — late-game gear from ripperdoc NPCs (or deep-Grid loot). Installing it grants:
   - Passive: at every Precursor console you visit, auto-syncs lore archive to a partial deep-Grid backup.
   - Cost: occupies a cybernetic implant slot (introduces implant slots as a future system stub).
   - Stat trade: -1 Will or similar.
2. **Precursor console "soul mirror" mode** — non-hackers interact via existing interaction (no jack-in), can perform a long-channel `Sync Soul` action (10+ turns, vulnerable, costs energy). Saves a sparse subset to consciousness store.

What carries through Sgr A\* per build:

| Build | Mechanism | Survives Sgr A\* |
|---|---|---|
| Hacker (`ConsciousnessAnchor` capstone) | Native deep-Grid base | Full: base, Grid currency, AI rep, lore archive, signature programs, deep-Grid map |
| Hacker (`Cat_Hacking` only) | Deep-Grid presence | Lore archive + Grid currency. No base, no programs. |
| Non-hacker w/ Neural Backup implant | Sync at Precursor consoles | Lore archive only (read-only knowledge). |
| Non-hacker, no implant | Nothing | Nothing — pure roguelike rebirth. |

### Persistence model: two save scopes

```
saves/
  <profile>/
    galaxy_<seed>.dat        # current galaxy. Discarded on Sgr A* rebirth.
    consciousness.dat        # cross-galaxy. Survives all rebirths.
```

**`galaxy_<seed>.dat`** — existing format. Bumps `SAVE_FILE_VERSION`. Adds:
- Network graph state for current galaxy (regional darknet layouts, gateway crack status).
- `Hackable` state on world objects (Compromised / Alarmed flags).
- Detection counter per zone.
- Active `GridSession` if jacked in at save time (treated as non-resumable; see edge case).

**`consciousness.dat`** — new. Small, stable schema. Contains:
- `consciousness_id` (assigned on first `Cat_Hacking` unlock, or first Neural Backup install).
- `lore_archive[]` — decrypted lore fragments.
- `grid_currency`.
- `ai_contacts[]` — reputation with Grid-side AI factions.
- `deep_grid_base` — owned anchor sectors + stash inventory (hackers w/ ConsciousnessAnchor only).
- `signature_programs[]` — bound programs (hackers w/ ConsciousnessAnchor only).
- `consciousness_version` — separate schema version.

On Sgr A\* rebirth: `galaxy_*.dat` is deleted; `consciousness.dat` is read and applied to the new character. New galaxy seeded; deep-Grid graph re-resolves base anchor sectors from `consciousness.dat` and stitches them back into freshly generated regional darknets.

Per project rule "no backcompat pre-ship": schema bumps reject old saves. No migration shims.

### Save sequencing edge case

If the player saves while jacked in, on load they wake up with a soft disconnect (Trace cleared, no penalty, body restored). Mid-Grid sessions are not resumable.

---

## 6. v1 scope, future work, risks

### v1 ships

- **PDA refactor** — rename + per-tab modules + new Hacking tab. Pure refactor for existing tabs (visual parity required).
- **Hacking tab** — terminal subwindow, bounded command set, menu fallbacks.
- **Cyberdeck item type** — 2 tiers (T1 pawn-shop, T2 corp surplus). Loot-only. Equipment slot.
- **Program item type** — 8 starter programs:
  - ATK: `icebreaker_lite.exe`
  - STL: `ghost_trace.exe`, `cooldown.exe`
  - UTL: `breach.exe`, `decrypt.exe`
  - QH: `reboot_optics.qh`, `friendly_fire.qh`, `data_leech.qh`
- **Tinkering integration** — code-fragment material category + 3 craftable program recipes.
- **`Cat_Hacking` skill category** — full enum; **3 unlocks implemented**: `Cat_Hacking` (gate), `IceBreaking`, `DeepGridNavigator`. Rest stubbed.
- **`Hackable` component** — wired onto these v1 device kinds:
  - T1: `Turret`, `Camera`, `Door`
  - T2: `Power conduit`
  - T3: `Precursor / Archive console` (jack-in only — the deep-Grid gateway)
- **Quickhack flow** — `H` to target, popup, Detection counter.
- **Grid session** — full HP / RAM / Heat / Trace runtime; ICE actors (white + gray + black, one of each); voluntary + forced disconnect.
- **Subnet sectors** — procedural, 1-room layouts, white/gray ICE only.
- **Regional darknet** — one mini-darknet per station with 3-4 sectors, hand-tuned generation.
- **Deep-Grid skeleton** — *one* hand-authored anchor sector: the **Consciousness Anchor** safe room. Reachable only with `DeepGridNavigator` + cracking a Precursor gateway. Houses the lore-archive interface and the consciousness save touchpoint.
- **Two save files** — `galaxy_*.dat` schema bump + new `consciousness.dat`.
- **Sgr A\* rebirth wiring** — `consciousness.dat` survives. Lore archive carries through. Anchor sector re-stitches.
- **Reputation/Detection coupling** — Detection ≥ 75 triggers existing reputation hostility; ≥ 100 zone alarm.

### v1 explicitly excludes (future work)

- All other Tier 1 hackable kinds (Drone, Mine/Trap-as-Hackable, Lock-as-distinct).
- Tier 2 environmental hacks beyond Power conduit.
- All Tier 3 except Precursor console.
- All Tier 4.
- Cybernetic Neural Backup implant + cybernetics slot system (non-hacker access path).
- Vulnerable-body model (Q4a-A — body slumps in real world).
- T3+ cyberdecks; advanced programs (`pulse_hammer`, `daemon_hijack`, etc.).
- Per-device diegetic OSes (distinct mock OSes per device).
- Multiple deep-Grid anchor sectors.
- Grid AI faction reputation system.
- All `Cat_Hacking` skills beyond the v1 three.

### Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Terminal command-set scope creep | Medium | Hard freeze on the bounded command list. Menu fallbacks always exist. If a command needs more than a 50-line parser, reject it. |
| `pda_screen` tab refactor regresses existing screens | Medium | Each tab extracted in its own commit. Run game and visit every tab between extractions. Visual parity required. |
| Two-save-file architecture confuses load/save UX | Low | `consciousness.dat` is invisible to the player; resolved automatically based on profile. No new menus. |
| Trace pacing wrong on first playtest | High | Trace tick rates are constants in `grid_session.cpp`. Easy to retune. Spec calls them initial values, not contract. |
| Black ICE bleed-through HP feels unfair | Medium | `NeuralFortitude` skill exists explicitly to soften this; v1 doesn't ship it but the path is in. Tune `bleed_pct` constant. |
| Grid sectors feel like "just dungeons" | Medium | Tron palette + glyphs is a renderer-level skin, easy to iterate. If still bland, push Heat/Trace HUD prominently. |
| Save schema bumps lock out playtesters | Low | Per project rule: reject old saves. Playtest builds are throwaway. |
| Sgr A\* rebirth restoration of deep-Grid anchor breaks | Medium | Consciousness save schema kept dead-simple. Anchor restoration logic gets unit tests if test infra supports it; otherwise dev-console `:rebirth` for manual verification. |

### Open questions for the implementation plan

These are tactical, not design — they belong in the plan, not here.

- Order of phases: PDA refactor first vs. `Hackable` component first?
- How to dev-console-spawn a turret-with-Hackable for early testing.
- Renderer palette: dedicated Grid color set or reuse existing palette with semantic remap?
- Does the existing star chart's three-zoom widget code generalize to the netmap zoom layers, or is netmap a fresh widget?

---

## 7. Cross-references

- `docs/mechanics.md` — add Hacking section (Trace formula, Heat→Trace coupling, Detection).
- `docs/items.md` — add Cyberdeck and Program item families.
- `docs/roadmap.md` — add Hacking & The Grid feature line.
- Related specs: `2026-04-22-dungeon-puzzle-framework-design.md` (puzzle hooks for Tier 4), `2026-04-13-dice-combat-system-design.md` (combat reuse), `2026-04-21-tinkering-salvage-design.md` (program crafting).
