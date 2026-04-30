# Hacking & The Grid — D-Layer (Plan 4) Design Spec

**Date:** 2026-04-30
**Status:** Draft, awaiting user review
**Branch (target):** `feature/hacking-deep-grid`
**Parent spec:** `2026-04-29-hacking-design.md` (this is the Plan 4 implementation spec for §5 of the parent)

---

## 1. Concept

Plan 4 ships the **D-layer** of the Hacking & The Grid feature: the persistent cyberspace meta-layer that survives Sgr A\* rebirth.

Plans 1–3 already shipped the PDA refactor, quickhacks, and the in-Grid runtime (jack-in lifecycle, sectors, ICE, programs, save schema v54). What is missing is the *meta*: the second save scope that carries identity across galaxy lifetimes, the non-hacker access path, the rebirth wiring, the navigable graph view, the capstone skills, and a player-owned deep-Grid base.

### Pillars carried forward from the parent spec
- **The deep-Grid is the meta-layer.** It is the literal mechanical expression of "infinite knowledge of the universe."
- **Cross-Sgr-A\* persistence is build-gated.** A hacker carries the most; a non-hacker with cybernetics carries less; an unaugmented body carries nothing.
- **Identity through resource allocation.** The line between dabbler and decker is the `Cat_Hacking` skill investment; the line between decker and meta-survivor is the `ConsciousnessAnchor` capstone; the non-hacker's parallel is the Neural Backup implant.

### Non-goals for Plan 4
- Grid HUD redesign (owned by Plan 5).
- Hackable kinds beyond v1 (Drone, Light, Vendor, Elevator, Mine, etc.) — owned by later plans.
- Multi-device LAN-shaped subnets — owned by Plan 6.
- Vulnerable-body model — body stays phased-out.
- T3+ cyberdecks — current 2 tiers stay.
- AI-faction reputation gameplay loop — schema only, no runtime interactions.
- Multiple deep-Grid anchor sectors beyond the existing Consciousness Anchor and the new player base.
- Ripperdoc NPC for implant install — implants are loot/dev-spawn for now.

---

## 2. Architecture

### New subsystems

| Unit | Header | Responsibility |
|---|---|---|
| `ConsciousnessSave` | `consciousness_save.h/cpp` | Second save scope. Reads/writes `consciousness.dat`. Independent of `SaveData`/galaxy save lifetime. Owns its own schema version. |
| `RebirthSequence` | `rebirth_sequence.h/cpp` | Drives the Sgr A\* event-horizon UX: confirmation modal, monospaced text-crawl cinematic, galaxy-file deletion, consciousness application, new-galaxy seed kickoff. |
| `GridNetmapWidget` | `grid_netmap_widget.h/cpp` | Overlay popup on the PDA Hacking tab. Static positioned graph rendering, two zoom layers (regional, deep-Grid), cursor stepping, `enter` to jack, `b` to breach locked gateway. |
| `GridRegionalGenerator` | `grid_regional_generator.h/cpp` | Thin BSP-generator wrapper that emits 4–8 room regional darknet sectors using the existing dungeon BSP and a Grid tile-mapping function. Matches the Plan 3 single-room regional's firewall style and palette. |
| `GridCamera` | embedded in `grid_renderer.cpp` | Follow-avatar camera with deadzone. Replaces the current fixed `(1, 1)` origin. Unblocks sector growth past ~30×20. |
| `SoulMirrorChannel` | `soul_mirror.h/cpp` | Real-world channel runtime: progress per turn, EP cost, damage interrupt, Detection coupling, lore-fragment commit on threshold. Shared backend used by both manual `Sync Soul` and the passive Neural Backup auto-sync. |
| `Implant` (item subclass) | `implant.h` | Item subclass for cybernetic implants. Currently one concrete: Neural Backup. Pattern matches `Cyberdeck` and `Program`. |

### Existing systems extended

- `Player` gains a fixed-size `std::array<std::optional<Item>, 2> implants`. Two slots in v1.
- `Equipment` tab gains a paper-doll **view toggle** (`Tab` key): swaps between Equipment view (existing 14-slot paper doll) and Implant view (2-slot paper doll). Tab content otherwise unchanged.
- `SkillId::CodeCraft` and `SkillId::ConsciousnessAnchor` runtime effects wired (the last two `Cat_Hacking` capstones).
- `Program` table gains 2 T3 entries (`pulse_hammer.exe`, `daemon_hijack.exe`) gated behind a `CodeCraft`-required tinker recipe.
- `SAVE_FILE_VERSION` bumps `54 → 55`. Rejects v54. New persisted state on the galaxy save: implant slot contents, soul-mirror channel progress per console.
- `consciousness.dat` schema v1: independent file, independent versioning.
- World-side jump-into-Sgr-A\* code path in the navigation layer fires `RebirthSequence::begin()` when the player crosses the event horizon.

### File-size discipline

Every new `.cpp` targets ≤ ~400 lines, hard cap 600. Heavy modules split: `consciousness_save.cpp` is just (de)serialization; rebirth sequencing is its own file; netmap widget is its own file with a layout helper free function; soul-mirror is its own file. `grid_renderer.cpp` gains a camera but stays under 200.

### Data flow — Soul Mirror channel (real-world)

```
Player walks onto a Precursor console tile
  → press [interact] → menu now includes "Sync Soul"
  → SoulMirrorChannel::begin(player, console_id)
  → each game turn while channel active:
       - if player on tile: progress += 1, ep -= 2
       - if progress % 10 == 0: commit one new lore fragment to consciousness.dat
       - if player damaged: pause channel, detection += 5, no reset
       - if player moved off tile: pause channel, no reset
  → SoulMirrorChannel::end() — channel data persists on the console (per-console progress)

Neural Backup variant — same backend, different entry:
  Player walks onto a Precursor console tile WHILE Neural Backup equipped
    → SoulMirrorChannel::begin_passive(player, console_id) — no menu, no ep cost
    → progress += 1 per turn while standing on tile
    → same commit threshold, same damage interrupt
```

### Data flow — Sgr A\* rebirth

```
Player approaches Sgr A* in star-chart navigation, attempts to jump
  → RebirthSequence::begin()
    1. Confirmation modal:
         "Enter the Sagittarius A* event horizon?"
         "Your current galaxy will be lost."
         "Surviving the crossing: <list per build>"
         [Confirm] [Cancel]
    2. On Confirm:
         - if first crossing this profile: full text-crawl cinematic (~6s, paced
           monospaced glyphs falling, "Crossing the event horizon... knowledge
           preserved... awakening..."), then [Press any key]
         - if not first: minimal one-line transition message, ~1s pause
    3. ConsciousnessSave::write(current_state)  // commit any unsaved lore picks
    4. delete galaxy_<seed>.astra
    5. seed new galaxy from system_clock
    6. Player::rebirth() — reset to starter state, then ConsciousnessSave::apply()
       overlays surviving fields:
         - lore_archive copied
         - grid_currency copied
         - ai_contacts copied
         - if ConsciousnessAnchor capstone: deep_grid_base + signature_programs
    7. New galaxy boots, player wakes in fresh starter ship, anchor sector
       re-stitches into the freshly generated Grid network.
```

---

## 3. The Sgr A\* rebirth ceremony

### Confirmation modal — content

The modal text is dynamic based on the player's build:

```
┌─────────────────────────────────────────────────────────────┐
│            ENTER THE SAGITTARIUS A* EVENT HORIZON?          │
│                                                             │
│  Your current galaxy will be lost.                          │
│                                                             │
│  Surviving the crossing:                                    │
│   ✓ Lore archive (3 fragments)                              │
│   ✓ Grid currency (240 cR)                                  │
│   ✓ Deep-Grid base                                          │
│   ✓ Signature programs (icebreaker_lite.exe, ghost_trace…)  │
│   ✓ AI contacts (2)                                         │
│                                                             │
│   [Enter]  Confirm           [Esc]  Cancel                  │
└─────────────────────────────────────────────────────────────┘
```

Build-conditional rows:
- `lore_archive` — always shown if non-empty
- `grid_currency` — always shown if non-zero
- `deep_grid_base` — only with `ConsciousnessAnchor` capstone
- `signature_programs` — only with `ConsciousnessAnchor` capstone
- `ai_contacts` — always shown if non-empty
- If *nothing* survives: the list shows `(nothing — you are unaugmented; this is a true rebirth)`.

### Text-crawl cinematic (first crossing only)

Six lines of paced monospaced text drop in over ~6 seconds. Tron-palette glyphs (cyan/magenta), centered. Lines:

```
   The body unmakes itself at the event horizon.
   Spacetime  curves  into  a  single   bright   point.
   ...consciousness uploads...
   ...the galaxy collapses behind you...
   ...knowledge persists through the singularity...
                          [Press any key]
```

After the first crossing, a per-profile flag in `consciousness.dat` (`seen_first_rebirth = true`) makes subsequent crossings skip the full cinematic — they get a single line + 1s pause + new galaxy.

### Delete-galaxy semantics

Galaxy file deleted via `delete_save(galaxy_filename)`. No backup. The user has confirmed in the modal; this is destructive by design. Per `feedback_no_backcompat_pre_ship.md`: no migration shims, no rollback.

---

## 4. Persistence model — `consciousness.dat`

### File location

```
~/.astra/saves/consciousness.dat
```

One file per profile (Astra is single-profile in v1; the file is a singleton). Lives next to `save_<seed>.astra` files but is not tied to any specific galaxy.

### Schema v1

```cpp
namespace astra {

inline constexpr uint32_t CONSCIOUSNESS_SAVE_VERSION = 1;

struct LoreFragmentRef {
    std::string archive_id;     // e.g. "ARCH-Hangar7-12x4"
    uint32_t galaxy_seed_origin; // which galaxy this was discovered in
    uint32_t world_tick_origin;  // when, for narrative ordering
};

struct AiContact {
    uint32_t faction_id;
    int32_t reputation;          // -100..+100
};

struct ConsciousnessSave {
    uint32_t version = CONSCIOUSNESS_SAVE_VERSION;
    uint64_t consciousness_id = 0;       // assigned on first qualification
    uint32_t rebirth_count = 0;          // how many times you've crossed Sgr A*
    bool seen_first_rebirth = false;     // gates the cinematic skip
    std::vector<LoreFragmentRef> lore_archive;
    int32_t grid_currency = 0;
    std::vector<AiContact> ai_contacts;

    // Hacker-only — populated only with ConsciousnessAnchor capstone
    std::optional<GridSector> deep_grid_base;       // hand-authored layout, snapshot of contents
    std::vector<Item> signature_program_rack;       // bound programs
};

void write_consciousness(const ConsciousnessSave& cs);
bool read_consciousness(ConsciousnessSave& cs);
bool delete_consciousness();   // dev verb only, not exposed to player
} // namespace astra
```

### Assignment of `consciousness_id`

Created on the first event that qualifies the player for the meta-layer:
1. Unlocking `Cat_Hacking` skill (any character takes this path), OR
2. Equipping a Neural Backup implant for the first time, OR
3. Performing a manual `Sync Soul` channel for the first time at any Precursor console.

ID is a u64 from `std::random_device`. Stored once; never changes for the profile.

### When `consciousness.dat` is written

- On normal save (alongside galaxy save). Galaxy save and consciousness save are written in the same `SaveSystem::save()` call, but to separate files.
- On `Sync Soul` lore commit (incremental write — atomic append-style by re-writing the whole small file).
- On Neural Backup auto-sync lore commit (same path).
- Pre-rebirth (step 3 of the rebirth sequence above), to ensure latest state survives.

### When `consciousness.dat` is read

- On game start: read once. If absent, leave the in-memory copy as default-constructed.
- On post-rebirth `apply()`: in-memory copy is applied to the new player.

### Cross-build survival matrix (locked from parent spec §5)

| Build | What survives the crossing |
|---|---|
| `ConsciousnessAnchor` capstone | Lore + currency + AI contacts + deep-Grid base + signature programs |
| `Cat_Hacking` only | Lore + currency + AI contacts |
| Non-hacker w/ Neural Backup | Lore archive only |
| Non-hacker, no implant | Nothing (true rebirth) |

The schema fields exist for everyone; gating is at the *write* side. A non-hacker without `ConsciousnessAnchor` simply never has `deep_grid_base` populated — the field stays `nullopt`.

---

## 5. Non-hacker access path

### Implant slot UI

The Equipment PDA tab gains a **paper-doll view toggle**. Default view: existing 14-slot equipment paper doll. Press `Tab` → swap to **Implant view**: 2-slot paper doll with implant slots labeled. Press `Tab` again → back to Equipment view. The non-paper-doll side of the tab (stat panel) updates to reflect whichever view is active.

```
EQUIPMENT VIEW                       IMPLANT VIEW         (Tab toggles)

   ╭───╮   Head: helmet                 ╭───╮  Slot 1: Neural Backup
   ╰─┬─╯   Face: -                      ╰─┬─╯           (-1 Will)
   ╭─┴─╮   Body: vest                   ╭─┴─╮  Slot 2: (empty)
   │ @ │   Hand: pistol / -             │ @ │
   ╰─┬─╯   Util1: cyberdeck             ╰─┬─╯
   ╭─┴─╮   Util2: -                     ╭─┴─╮
   ╰───╯   Feet: boots                  ╰───╯

[Tab] Switch to Implants            [Tab] Switch to Equipment
```

Plan 4 ships **2 slots** with the names "Slot 1" and "Slot 2" (no body-region labels yet — that's a later refinement). Equip flow is the standard `e` action from inventory: items with `Item.kind == Implant` are valid for slot equip.

### Neural Backup implant — concrete stats

```
Name:        Neural Backup
Item kind:   Implant
Source:      late-game loot (deep-Grid drops, regional darknets T2+)
Slot:        Implant Slot 1 or 2
Effects:     -1 Will while equipped
             At Precursor consoles, auto-runs Soul Mirror in the background
             with no EP cost, no input, same lore commit threshold.
Save state:  Equipped state and which slot persisted in galaxy save.
```

Loot table integration: a single new entry in the `T3 implant` rarity bucket; deep-Grid anchor and regional darknet T2+ have a small chance to drop one. Dev verb `:spawn implant neural_backup` for testing.

### Soul Mirror channel — concrete numbers

| Parameter | Value |
|---|---|
| Progress per turn (player on console tile) | +1 |
| EP cost per turn | -2 |
| Lore commit threshold | every 10 progress |
| Movement off tile | pauses, no reset |
| Damage taken during channel | pauses, +5 detection burst, no reset |
| Channel persistence | per-console; resumes if you return |
| Stops when | progress = max_lore_archive_size for the consciousness, or player leaves and never returns |

Per-console channel state lives on the world-side `Hackable` component (galaxy save only — does not migrate).

### What lore is committable

A Precursor console exposes a list of *lore fragments* (1–4 per console, generated when the regional darknet is generated). Each commit (every 10 progress) burns one fragment from the console and writes it to `consciousness.dat.lore_archive`. Once all fragments on a console are committed, the console reports "fully synced" and further `Sync Soul` actions on it do nothing.

### Channel UX

Standing on the tile, after pressing `Sync Soul`, a one-line HUD strip shows above the message log:

```
SYNC IN PROGRESS  ████████░░░░░░░░░░░  4/10 next fragment   EP 18/30
```

Updates per turn. Disappears when channel pauses or completes a fragment.

---

## 6. Capstone wiring & T3 programs

### `CodeCraft` (skill 1206)

Unlocks T3 program tinker recipes. Plan 4 ships **two T3 programs as craftable**:

| Program | Kind | RAM | Heat | Effect | Recipe |
|---|---|---|---|---|---|
| `pulse_hammer.exe` | ATK | 4 | 5 | AoE 1d6 dmg to all ICE adjacent to a target tile | 2 T3 code fragments + 1 T2 program disk |
| `daemon_hijack.exe` | UTL | 5 | 4 | Take control of one ICE for 3 turns | 3 T3 code fragments + 1 program disk |

Recipe gate: requires `CodeCraft` unlocked + a Tinkering Workbench. The recipes appear in the Tinkering tab's program family only when `CodeCraft = unlocked`.

### `ConsciousnessAnchor` (skill 1207, capstone)

When unlocked:
1. The player's deep-Grid base layout is generated and stitched into the regional darknet of their current location (a hand-authored layout — see §7).
2. The base sector is added to `GridNetwork` with `kind = DeepGridAnchor`, owner = current `consciousness_id`.
3. `consciousness.dat.deep_grid_base` slot becomes populated and persists across rebirth.
4. `consciousness.dat.signature_program_rack` becomes populated; programs the player binds to it persist across rebirth.

The capstone is a **one-time event** — once unlocked, the base is yours forever (across all rebirths).

---

## 7. Player deep-Grid base — layout

### Hand-authored, single layout, ~30×20 cells

Lives in `grid_anchor_layout.cpp` next to the existing Consciousness Anchor. One concrete layout for v1 — every player gets the same shape. Variety is a future plan.

### Required fixtures

| Fixture | Glyph | Purpose |
|---|---|---|
| Stash terminal | `⊞` | Read/write inventory items (Plan 4 simplified: single shared stash for the profile, persisted in `consciousness.dat`). Optionally future-extended with paid expansions. |
| Lore vault interface | `⊘` | Visual representation of `consciousness.dat.lore_archive`. Walk onto it → opens a list panel showing all committed lore fragments. |
| Signature program rack | `▤` | Walk onto it → list panel of programs in the rack. Add/remove from rack via inventory items. |
| AI contact spots | `☎` | Static actor tiles representing AI contacts. v1: walk onto one shows a one-line message. AI faction reputation gameplay loop is later. |
| Consciousness Anchor link | `⊙` | Exit/disconnect node back to the regional darknet. |

### ASCII layout sketch

```
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ░
░ ▓                          ▓ ░
░ ▓   ⊞     ▤    ⊘           ▓ ░
░ ▓                           ▓ ░
░ ▓▓▓▓ ▓▓▓▓▓▓▓▓▓▓▓ ▓▓▓▓▓▓▓▓▓▓▓ ░
░                              ░
░                              ░
░       @                      ░
░                              ░
░                ☎             ░
░                              ░
░  ☎                           ░
░                              ░
░         ⊙                    ░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
```

(Rough; actual layout polished during implementation.)

### Generation seed

Layout is *literally hardcoded*. The "deep_grid_base" stored in `consciousness.dat` is a `GridSector` snapshot of the current state — fixtures, items in stash, programs in rack — not a re-generation seed.

---

## 8. Regional darknet generator

### Approach: BSP wrapper

Plan 4 introduces `grid_regional_generator.cpp`. It calls the existing dungeon BSP generator (`src/bsp_generator.cpp`) with a Grid tile-mapping function:

```
Dungeon BSP tile  →  Grid tile equivalent
Wall              →  Firewall (▓)
Floor             →  Grid floor (░)
Door              →  Gateway (⌬) — locked or unlocked depending on graph edge state
StairsDown        →  ExitNode (⊙)
StairsUp          →  ExitNode (⊙)
```

Room count: 4–8 (BSP min/max parameters tuned to match). Connectivity preserved from BSP. The wrapper post-processes the BSP output to:
1. Replace tiles per the mapping above.
2. Decorate with Grid-themed flair: occasional standalone firewall pillars (`▓` blocks) inside rooms, matching the existing Plan 3 single-room divided regional's look.
3. Place 1–4 `EncryptedFile` (`⊘`) tiles per regional, randomly distributed.
4. Place 0–2 `DataNode` (`$`) tiles per regional.
5. Place 0–1 `Gateway` (`⌬`) tile leading to deep-Grid (gated by `breach.exe`-based crack).
6. Place ICE actors per the existing tier rules.

### Style consistency

Must feel like the same world as the existing Plan 3 single-room regional darknet. The same firewall glyphs, palette, and decorative idioms apply. The visual difference between v0 (1 room with divider) and v1 (4–8 rooms) should read as *more space*, not *different aesthetic*.

### Sector size

Regional darknets generated by this system can exceed ~30×20. The `grid_renderer` camera (§9) handles this transparently.

---

## 9. Grid renderer camera

### Replace fixed origin with follow-avatar deadzone camera

`src/grid_renderer.cpp` currently draws sectors at fixed origin `(1, 1)`. Plan 4 replaces this with a camera:

```cpp
struct GridCamera {
    int viewport_x;       // top-left of rendering origin (e.g. 1)
    int viewport_y;       // (e.g. 1)
    int viewport_w;       // visible cells horizontal
    int viewport_h;       // visible cells vertical

    int cam_x;            // top-left of sector visible region
    int cam_y;

    // Deadzone: if avatar is within this margin of viewport edge, scroll.
    int deadzone_margin = 4;

    void follow(int avatar_x, int avatar_y, int sector_w, int sector_h);
};
```

`follow()` clamps `cam_x/cam_y` so the camera does not scroll past sector bounds. The renderer draws tiles in `[cam_x, cam_x + viewport_w)` × `[cam_y, cam_y + viewport_h)`. ICE and avatar drawn at `(viewport_x + tile_x - cam_x, viewport_y + tile_y - cam_y)`.

For sectors smaller than the viewport (e.g. current 8×8 subnets), `follow()` keeps `cam_x = cam_y = 0` and the sector is drawn at the original fixed origin — visually identical to today.

### Side effects on Plan 3 sectors

None — small sectors render unchanged. The camera only kicks in when sector dimensions exceed viewport.

---

## 10. Graph view widget — `GridNetmapWidget`

### Form

Overlay popup on the PDA Hacking tab content area. Bordered modal panel, terminal scrollback visible underneath (drawn first, then the modal on top). Esc dismisses.

```
┌────────────── Hacking tab content area ──────────────────────┐
│  pda> netmap                                                  │
│      ┌─── NETMAP ── REGIONAL ──────────────────────────────┐  │
│      │                                                      │  │
│      │  [Hangar.7] ──── [Station.Spine] ─╳─ [??.regional]  │  │
│      │       │                                              │  │
│      │  [Vault.Local]                                       │  │
│      │                                                      │  │
│      │  ◉ you  ○ available  ╳ locked  ⌬ deep-grid gateway   │  │
│      │  [arrows] cursor   [enter] jack    [b] breach        │  │
│      │  [,] zoom to deep-grid              [esc] close      │  │
│      └──────────────────────────────────────────────────────┘  │
│  pda> _                                                        │
└────────────────────────────────────────────────────────────────┘
```

### Two zoom layers in v1

| Zoom | Shows |
|---|---|
| **Regional** (default) | Subnets and station nodes the player has discovered in the current galaxy. Node positions are static `(x, y)` stored on the `GridNetwork` node when it's created. Edges drawn solid (cracked) or `╳` (locked, requires `breach.exe`). |
| **Deep-Grid** | Persistent layer. Always shows the Consciousness Anchor. Shows the player's deep-Grid base if `ConsciousnessAnchor` capstone unlocked. Shows AI contact nodes the player has discovered. Shows the lore-archive interface (links to `consciousness.dat.lore_archive`). |

### Subnet zoom in v1

`.` (zoom into subnet) is a **no-op** in Plan 4 with a footer hint:

```
[.] subnet zoom — single-device subnets (multi-device LANs in a future update)
```

The data structure (`GridNetwork.nodes[].subnet_children`) is shape-supported from Plan 4 onwards — future Plan 6 populates and renders this.

### Layout strategy

**Static positioned grid.** Each `GridNetworkNode` carries `int layout_x, layout_y` populated at network generation. Render reads these and draws at exact `(layout_x, layout_y)` offsets within the modal panel. Edges drawn from each pair of adjacent nodes' positions. No physics, no drift, deterministic.

For initial generation: a simple canonical layout per zoom (e.g. regional = grid-arranged, deep-Grid = the Anchor at center with player base + AI contacts in a ring). Generation runs once; positions persist in the save.

### Cursor stepping

Arrow keys step the cursor between *labeled nodes only* (not free-floating cells). Each press finds the nearest labeled node in the pressed direction. Selected node is bright; non-selected dim; locked nodes drawn dim with `╳` on incoming edges.

### Interactions

| Key | Action |
|---|---|
| arrows | Move cursor between nodes |
| `Enter` | Jack into selected node (if traversable) |
| `b` | Attempt breach on selected gateway (if `breach.exe` loaded; per existing breach rules) |
| `,` | Zoom to deep-Grid (from regional) |
| `,` again | Zoom back to regional (from deep-Grid) |
| `.` | (no-op) subnet zoom — footer hint shown |
| `Esc` | Close overlay, return to terminal |

### Integration with the PDA Hacking tab

The terminal command `netmap` (and the `[N]` shortcut) opens this overlay instead of emitting ASCII art into the scrollback. The current `hack_term_cmd_netmap()` in `src/pda_hacking_tab.cpp` is replaced with `open_netmap_overlay()`. The text-based ASCII output is removed.

### File ownership

`grid_netmap_widget.h/cpp` lives in the `astra` namespace, owned by the PDA Hacking tab. ~250–350 lines target.

---

## 11. Save schema bumps

### Galaxy save: v54 → v55

New persisted state:
- `Player::implants[2]` — array of 2 optional Items.
- `Hackable.soul_mirror_progress` (per-console int) and `Hackable.lore_fragments` (per-console vector of fragment ids).

Per `feedback_no_backcompat_pre_ship.md`: v54 saves rejected on load.

### `consciousness.dat` v1

New file. Schema as in §4. No existing version to break.

### Atomic write semantics

`consciousness.dat` is small. Write goes via temp-file + rename, same pattern as the existing `write_save()` in `src/save_file.cpp`. Single small struct, one write per save event.

---

## 12. v1 scope checklist

What ships in Plan 4:

- [x] `consciousness.dat` schema v1 read/write/apply.
- [x] Sgr A\* rebirth UX: confirmation modal + first-crossing cinematic + skip-on-subsequent.
- [x] World-side hookup: jumping into Sgr A\* triggers `RebirthSequence::begin()`.
- [x] 2-slot implant system on `Player` + Equipment-tab paper-doll toggle.
- [x] Neural Backup implant (loot + equip + auto-sync at Precursor consoles).
- [x] Soul Mirror channel (manual + Neural Backup passive variant, shared backend).
- [x] `CodeCraft` capstone wired: 2 T3 program recipes (pulse_hammer.exe, daemon_hijack.exe).
- [x] `ConsciousnessAnchor` capstone wired: player deep-Grid base unlocks + persists.
- [x] Player deep-Grid base hand-authored layout (~30×20) with stash, lore vault, sig rack, AI spots.
- [x] Regional darknet generator (BSP wrapper, 4–8 rooms, style-matched to Plan 3 single-room).
- [x] Grid renderer camera (follow-avatar with deadzone, replaces fixed origin).
- [x] `GridNetmapWidget` overlay (regional + deep-Grid zoom, cursor stepping, interactions).
- [x] Save schema bump v54 → v55.
- [x] `docs/mechanics.md` and `docs/items.md` updates.
- [x] `docs/roadmap.md` updates (Plan 4 D-layer feature line).

What does NOT ship (deferred):

- Subnet zoom (Plan 6 — multi-device LANs).
- Multi-room regional generators with custom Grid-specific generators (Plan 6 — content expansion).
- Vulnerable-body model (future).
- T3+ cyberdecks (future).
- Multiple deep-Grid anchor sectors beyond Anchor + base (future).
- Grid AI faction reputation gameplay (schema only — gameplay loop is later).
- Ripperdoc NPC implant install workflow (future).
- Grid HUD redesign (Plan 5).

---

## 13. Open questions for the implementation plan

These are tactical and live in the plan, not the spec:

- Order of the ~12 tasks: persistence schema first, or rebirth wiring first?
- Where exactly in the navigation layer does Sgr A\* entry hook? (Need to find the existing black-hole/system-traversal code first.)
- Exact T3 program ids — `ProgramId::PulseHammer = 200`, `DaemonHijack = 201`?
- Should the netmap overlay support clicking/cursor outside of nodes (free pan), or strictly node-stepping? (Spec says strict.)
- Stash inventory storage in `consciousness.dat`: bounded list size or unlimited?
- Implant slot persistence on inventory items: Item subclass with a `kind` discriminator, or a separate `Implant` struct field on Player?

---

## 14. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Sgr A\* rebirth state-restoration bug eats the consciousness | Medium | Pre-rebirth write of `consciousness.dat` happens *before* `delete_save(galaxy)`. If the pre-rebirth write fails, abort the rebirth — the player stays in their current galaxy with an error message. |
| Deep-Grid base re-stitch fails in new galaxy | Medium | Anchor stitching runs on first PDA Hacking tab open after rebirth; fallback to "anchor not yet stitched" message + retry on next open. Idempotent. |
| Soul Mirror channel feels too slow for non-hackers | Medium | All channel constants live in `soul_mirror.cpp` as named constexprs. Easy to retune. Spec calls these initial values, not contract. |
| BSP-generator reskin looks dungeon-y | Medium | Style-match against existing Plan 3 single-room regional — same firewall glyphs, palette, decoration density. Reviewer agent verifies side-by-side screenshots. |
| Save schema bump locks out test users | Low | Per project rule: reject old saves. Pre-release. |
| Camera scroll feels jittery | Low | Deadzone of 4 cells prevents per-step scrolls; only edge-of-deadzone movement triggers scroll. |
| Implant slot UI conflicts with Tab navigation in PDA | Low | Verify Tab-toggle in the Equipment tab doesn't collide with PDA top-level tab navigation (which uses different keys). Test during Task review. |
| Two-save-file architecture surprises load/save UX | Low | `consciousness.dat` is invisible to the player — automatic. Save menu shows only galaxy saves. |

---

## 15. Cross-references

- Parent spec: `docs/superpowers/specs/2026-04-29-hacking-design.md` (§5, §6).
- Plan 1: `docs/superpowers/plans/2026-04-29-hacking-pda-refactor.md`.
- Plan 2: `docs/superpowers/plans/2026-04-29-hacking-cyberdeck-quickhacks.md`.
- Plan 3: `docs/superpowers/plans/2026-04-29-hacking-grid-mode.md`.
- This plan (Plan 4): `docs/superpowers/plans/2026-04-30-hacking-deep-grid.md` (to be written next).
- Related: `2026-04-25-energy-system-design.md` (Soul Mirror EP cost), `2026-04-21-tinkering-salvage-design.md` (T3 program recipes).

---

## 16. Acceptance criteria

The plan is complete when:

1. A non-hacker character can equip a Neural Backup, walk onto a Precursor console, and watch their lore archive auto-populate without any manual action — and that archive survives Sgr A\*.
2. A hacker with `Cat_Hacking` only can sync lore via jack-in but loses their cyberdeck on rebirth — and lore survives.
3. A hacker with `ConsciousnessAnchor` capstone has a persistent deep-Grid base with stash + signature programs that survive Sgr A\* — verified across at least one rebirth in dev testing.
4. The PDA Hacking tab's `netmap` command opens the overlay widget; arrow keys step between nodes; `,` zooms; `b` breaches; `enter` jacks in.
5. Regional darknets generated post-Plan 4 have 4–8 rooms (verified by stepping through one in dev mode) and visually match the existing Plan 3 single-room style.
6. The grid renderer scrolls smoothly when entering a sector larger than the viewport.
7. `cmake --build build` succeeds with `-DDEV=ON` on the merge candidate.
8. All `cmake` build platforms in CI pass.
