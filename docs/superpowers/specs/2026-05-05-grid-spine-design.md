# Grid Spine — Anchors, Combat, XP (Spec 1)

**Date:** 2026-05-05
**Status:** Spec — pending implementation plan.
**Audience:** Implementation planner (`writing-plans` skill) and contributors picking up the Grid combat layer.
**Parent:** [Grid Loop Manifesto](2026-05-05-grid-loop-manifesto.md) — read this first.

---

## 0. TL;DR

Spec 1 turns the Grid into a load-bearing combat-tool layer for hacker builds and a small-but-real exploration reward layer. It introduces:

1. **NPC Anchor entities** — Grid-side guardian projections of real-world hostile NPCs. Damaging an anchor strips defenses on the linked NPC; severing it leaves the NPC permanently exposed.
2. **A new `.exe` program pool** focused on anchor control + Grid combat. Sci-fi-named (`ping.exe`, `lag.exe`, `static.exe`, `cryptovirus.exe`, …). Programs deal **anchor damage + real-world status effects**; rare master programs deal small **real-world HP damage** directly (capped — outright kills via Grid alone are impossible by design).
3. **Proper Grid combat** — the avatar gets ranged (program fire) + melee (bump-attack into adjacent ICE / anchor / hostile entity).
4. **Grid kills now grant XP** into the player's main pool (currently they grant zero — bug fix).
5. **Hacker progression is gear-driven**, not skill-XP-driven. No separate Cat_Hacking XP track. Hackers level by collecting more programs and better cyberdeck mods.
6. **Dead-NPC implant deep-dive** — when a hostile NPC dies, the hacker can jack into the corpse's dormant implant and explore a small per-corpse procgen brain-space (4×4–8×8) for lore / schematics / data crypts.
7. **Plan 7 device-shell layer goes dormant** — kept in tree, disconnected from the combat loop, no maintenance.

The design philosophy: hacking is **always combo with another build, never solo**. Hackers prep / control; the kill comes from turrets, pistols, melee, etc.

---

## 1. Concept and pillars

### 1.1. The Grid does two things, in one geography

- **Combat-time tool layer.** Hacker dips into the LAN mid-fight, navigates to an enemy's mobile anchor, fires programs to apply persistent debuffs / impairs / marks, jacks out. World ticks slow during the dip (already implemented). Kill happens in real-world via the player's other build. **Hacking = control and prep, never finisher.**
- **Exploration reward layer.** Same LAN, walked at leisure. Scarce hacker-relevant rewards. Anchors are the *combat* targets; rooms / data tiles / dead-implant deep-dives are the *exploration* targets.

### 1.2. Player archetype

The hacker is **always a combo build** — tinker+hack, pistol+hack, melee+hack. Pure hacker is not viable. Hacking complements; it does not solo.

- **Tinker + Hack.** Lay turrets/mines, jack in, debuff approaching NPC's anchor, jack out — turrets do the work.
- **Pistol + Hack.** Speed-walk to anchor, virus, jack out, finish at range with the debuff active.
- **Melee + Hack.** Mark + slow at anchor, jack out, close distance into a softened target.

### 1.3. What this spec does NOT do

- Does not gate the macro arc. Sgr A* / consciousness rebirth remain reachable by non-hacker builds.
- Does not add a separate Cat_Hacking XP track. Skill perks stay tied to existing character-level + perk-tree mechanics.
- Does not touch quickhacks (`.qh`). Out-of-scope. They may later be **re-specced as a hacker-only meatspace mechanic** (a deliberate, scoped feature for the hacker class), or **replaced by a different meatspace-damage category**, or **retired entirely**. Whichever direction is picked happens in its own spec, not here.
- Does not implement loot scarcity or new vendor types (Spec 2 / Spec 3).
- Does not add real-world locked doors (Spec 4).

---

## 2. The Anchor entity

### 2.1. Definition

An **Anchor** is a Grid-side entity tied 1:1 to a single hostile NPC. While the anchor lives, the NPC is "armored" against certain real-world status effects. As the anchor takes damage, the linked NPC's defenses degrade *proportionally*. When the anchor falls (HP = 0), the NPC drops into a **permanent vulnerability state** for that NPC instance.

The anchor is **not** the NPC's HP. Killing the anchor does not kill the NPC — it strips their defense layer so the player's other build can close.

### 2.2. Lifecycle

| Event | Behavior |
|---|---|
| NPC enters LAN's region | Anchor spawns at projected coordinates. |
| NPC moves in real-world | Anchor mirrors position via fixed coord-projection. |
| Anchor takes damage | Anchor HP decreases. Real-world NPC's vulnerability stack updates proportionally. |
| Anchor reaches 0 HP | Severed. Vulnerability stack pinned at full. NPC is now "fully vulnerable" until killed. |
| NPC dies in real-world | Anchor disappears from the LAN. Corpse retains a dormant implant (§ 9). |

NPCs almost never leave the LAN's region — when they do, the anchor despawns and re-spawns when they return (anchor HP carries over via per-NPC state).

**Anchors do not regenerate.** Damage is damage. A half-killed anchor stays half-killed across jack-out / jack-in cycles.

### 2.3. Anchor HP and proportional vulnerability

```
anchor_max_hp = f(npc_threat_tier)   // e.g. tier 1 NPC -> 10 HP, tier 5 -> 100 HP
anchor_hp_pct = anchor_hp / anchor_max_hp

vulnerability_pct = 1.0 - anchor_hp_pct      // 0% at full HP, 100% at severed
```

Vulnerability acts as a *modifier* on the NPC's defenses in real-world combat. Each program that requires a "vulnerable" NPC checks the current vulnerability stack. Concrete effects depend on what real-world status the program applied, but the rule is: damage is damage, applied proportionally, and the cap is "fully vulnerable" at sever.

### 2.4. Anchor visual and label

- **Glyph:** `※` in the linked NPC's faction color.
- **Label:** by default, anchors render with a numeric ID (`※1`, `※2`, `※3`).
- **Once identified** via `look` (§ 3) or tether (§ 4), the anchor's label upgrades to the NPC's name in faction color.
- **HP indicator:** small bar / fraction overlay rendered next to the anchor when within scanner range.

### 2.5. Anchors vs. ICE — the two combat targets

| | ICE | Anchor |
|---|---|---|
| **Tied to** | LAN infrastructure (rooms, data nodes) | A specific real-world NPC |
| **Behavior** | Patrols, aggros on detection | Mirrors NPC position; may spawn guardian ICE if damaged |
| **Killable with** | `.exe` programs + melee | `.exe` programs + melee |
| **Drops** | Programs / RAM / schematics / credits / XP (Spec 2 finalizes) | Nothing direct — the *NPC* drops normally in real-world. |
| **Purpose** | Friction during exploration | Setup target during combat |

ICE serves the exploration-friction role. Anchors serve the combat-tool role. Same Grid, two reasons to fight.

---

## 3. Real-world identification — the `look` widget

### 3.1. Widget extension

The existing `look` (cursor-mode inspect) command is extended to surface **all** NPC-related info in one widget:

- Name, race / class
- Threat tier
- HP bar (existing)
- Active status effects + remaining duration
- **Implant status** (hacker-only — § 3.2)

The widget redesign is part of this spec. Output should be readable in a single glance — one panel, one column.

### 3.2. Implant identification — `Implant Reader` Cat_Hacking unlock

A new Cat_Hacking skill node, **Implant Reader**, gates the implant-status info in the look widget:

- **Without unlock:** look at NPC → standard combat info, no implant data. Implant line says `(implant: unrecognized hardware)`.
- **With unlock:** look at NPC → adds `Implant: HAS / NONE`. If HAS, also shows the linked anchor's Grid coordinates and current anchor HP.
- **In Grid:** anchor renders with NPC's name (instead of numeric ID) once that NPC has been `look`-identified at least once.

Skill node placement: low tier of the Cat_Hacking tree — first / second unlock. Should be one of the things a player picks early when committing to hacking.

### 3.3. NPCs without implants

Some NPC types have no native implant: animals, drones, Feral, raw biological monsters. They have:
- No anchor in the Grid by default.
- `look` (with Implant Reader): `Implant: NONE — tether required`.
- Only addressable from the Grid after a successful **Tether** (§ 4).

---

## 4. The Tether mechanic

### 4.1. Concept

**Tether** is a hacker ability that artificially uploads a target NPC into the Grid, projecting an anchor for them. It opens up:

- NPCs without native implants (animals, drones, Feral, etc.).
- Edge cases where the anchor is hard to find or out of LAN range.

### 4.2. UX

- **Trigger:** real-world key (default: `T`?) when a target is selected via `look` cursor or directly adjacent.
- **Effect:** projectile-like — line-of-sight check from player to target. On success, the target gains a temporary anchor in the LAN at the projected position.
- **Cost:** RAM + Heat. Defaults: `kTetherRamCost = 2`, `kTetherHeatCost = 4` (placeholder, tunable). Constants live next to the melee constants in `grid_combat.h`.
- **Duration:** persists until the anchor is severed or the NPC dies.
- **Charges:** Tether might consume a consumable item (`tether dart`?) or be charge-limited per cyberdeck mod. v1 ships with one of: free with cooldown, item-consuming, or charge-pool. Pick at implementation time.
- **Range:** v1 baseline = melee / line-of-sight short range. Tier-2 unlock (Cat_Hacking perk): long range. Tier-3: AoE 3-tile.

### 4.3. Skill tree node

Cat_Hacking gains a new branch:
- **Tether (level 1)** — basic line-of-sight short-range tether.
- **Tether (level 2)** — long-range tether.
- **Tether (level 3)** — AoE tether (3 tiles).

These are perks unlocked via the existing perk-tree mechanic (level-gated, point-spent). Not XP-grindable separately.

---

## 5. New program pool (`.exe`)

All anchor-affecting programs are `.exe` files fired from inside the Grid. (`.qh` quickhacks are out of scope and may be retired in a future spec — Spec 1 does not extend them.)

### 5.1. Naming and aesthetic

Programs read like sci-fi malware: short evocative names, lower-case `.exe` filename. The status they apply has a **clearly meatspace-suggesting name** so the player understands the real-world effect from the program name alone.

### 5.2. v1 program list

Tier and balance numbers are placeholders; tunable in playtest.

| Program | Kind | RAM | Heat | Anchor dmg | Real-world status applied | RW HP dmg | Duration | Notes |
|---|---|---|---|---|---|---|---|---|
| `ping.exe`        | Utl | 1 | 1 | 5  | Marked (visible thru walls) | 0 | 30 t | radar pulse |
| `lag.exe`         | Utl | 2 | 2 | 10 | Slowed (-50% speed)         | 0 | 10 t | network lag metaphor |
| `static.exe`      | Utl | 2 | 3 | 10 | Blinded (cannot see player) | 0 | 10 t | TV-static senses |
| `jitter.exe`      | Utl | 4 | 5 | 25 | Impaired (-50% acc / dodge) | 0 | 15 t | hand-shake feedback |
| `eclipse.exe`     | Utl | 5 | 6 | 40 | Exposed (2× incoming dmg)   | 0 | 10 t | defensive blackout |
| `cryptovirus.exe` | Atk | 3 | 4 | 15 | DoT-source                  | 1/turn | 20 t | sustained virus |
| `bricked.exe`     | Atk | 8 | 10 | full | Severed (perma-vulnerable) | 0 | persistent | full takedown |
| `necrosis.exe`    | Atk | 5 | 6 | 5  | DoT-source                  | 2/turn | 15 t | rare drop, RW DoT |
| `severance.exe`   | Atk | 7 | 8 | 10 | none                        | 5–10 instant | — | rare drop, chunk RW dmg |

`bricked.exe` is the full-takedown — drops the anchor's HP to 0, applies persistent vulnerability stack. Big spend, big payoff.

`necrosis.exe` and `severance.exe` are master-tier rare drops. They demonstrate the principle: most programs do **anchor damage + status only** (no real-world HP). Master tier adds **small real-world HP damage**, capped — outright Grid-only kills are impossible.

The existing programs (`Breach`, `Decrypt`, `IcebreakerLite`, `PulseHammer`, `GhostTrace`, etc.) all stay; the v1 anchor pool above sits alongside them.

### 5.3. Adjacency vs. range

Anchor-affecting programs default to **range** (Telegraph picks the anchor tile from LoS). Some flagged programs require adjacency (e.g., `bricked.exe` — full takedown shouldn't be no-risk). Adjacency is a per-program flag:

```cpp
struct ProgramDef {
    // ... existing ...
    bool requires_adjacency = false;
};
```

Master-tier real-world-damage programs (`necrosis`, `severance`) require adjacency to balance their power.

---

## 6. Grid combat

### 6.1. Ranged combat — programs

Existing system. Number-key fires program from cyberdeck slot, Telegraph picks the target tile. Hits ICE or anchors. Already implemented; spec 1 just expands the program pool (§ 5).

### 6.2. Melee combat — bump-attack

The avatar gets a default close-range attack via **bump-into-target**. Walking into a tile occupied by an ICE or anchor triggers a melee hit on that entity.

| Constant | Default | Notes |
|---|---|---|
| `kGridMeleeDamage`   | 3 | bump-attack damage |
| `kGridMeleeRamCost`  | 0 | configurable, may add cost later |
| `kGridMeleeHeatCost` | 0 | configurable, may add cost later |
| `kGridMeleeRange`    | 1 | always 1 — adjacency only |

These live as named constants in `include/astra/grid_combat.h` (new file). Future tuning happens in one place.

Melee damage applies to the entity at the target tile. The avatar does not move into the tile (bump-attack consumes the turn but the avatar holds position).

### 6.3. Targets

Both ICE and anchors are valid targets for ranged programs and melee. Hostile non-anchor NPCs in the Grid (rare — drone-like Grid mobs, optional v1) are also bumpable.

### 6.4. ICE behavior reaction

When an anchor takes damage, with some probability, **guardian ICE** spawn from the anchor's tile (configurable; v1 default: 50% chance per major hit, capped at 2 active guardians per anchor). Guardians patrol the anchor's local area until destroyed.

---

## 7. Combat-time pacing (already implemented — no spec changes)

The existing Plan 6 pacing already handles:
- Slow real-world ticks during Grid jack-in.
- Heat / Trace tick rates.
- Jack-out triggers (Trace cap, Black ICE damage, voluntary).

Spec 1 inherits this behavior unchanged. Section retained for reference; no new code.

---

## 8. XP — fix the bug, no new track

### 8.1. The current bug

Killing ICE in the Grid grants **0 XP**. This makes the Grid feel unrewarding even when the player engages directly with it.

### 8.2. The fix

Grid kills grant standard player-pool XP, scaled by the killed entity's tier:

| Kill type | XP grant |
|---|---|
| ICE (white)        | `kXpIceWhite` (≈ tier-1 mob) |
| ICE (gray)         | `kXpIceGray`  (≈ tier-2) |
| ICE (black)        | `kXpIceBlack` (≈ tier-3) |
| Anchor sever       | `kXpAnchorSever × npc_threat_tier` |
| Anchor partial dmg | none — only sever grants the bounty |

All XP goes into the **same pool** as real-world combat XP. There is no separate Cat_Hacking XP track. Hackers progress overall character level the same way other classes do; their *hacking power* comes from the programs and cyberdeck gear they collect (loot-driven).

### 8.3. Cat_Hacking perk progression

Existing perk-tree mechanic. Cat_Hacking gains new perks in this spec:
- **Implant Reader** (§ 3.2)
- **Tether** L1, L2, L3 (§ 4.3)

These unlock at character-level milestones with skill points spent — same as every other skill tree.

---

## 9. Dead-NPC implant deep-dive

### 9.1. Concept

When a hostile NPC with an implant dies, the corpse retains a **dormant implant** the hacker can jack into. Each corpse opens up a small per-corpse procgen brain-space (Option **A** from brainstorm). Pure hacker-only content; non-hacker builds walk past corpses normally.

### 9.2. UX

- **Trigger:** hacker stands adjacent to corpse → `(hack) Jack Into Implant` interactable. Cat_Hacking gated.
- **Geography:** small procgen sector, **4×4–8×8** tiles. Single room or 2-room layout. Faction-flavored visuals (Corp / Cartel / Civilian / Stellari palette).
- **Time:** instant in real-world (corpse is dead; nothing's moving). Pure exploration.
- **Trace / Heat:** low cost — corpse implants are unguarded, no ICE patrols.
- **Returnable?:** **No.** Each corpse is jacked-into once. Looted = empty. Subsequent jack-ins return immediately with "implant residue exhausted."

### 9.3. Content

Each brain-space contains 1–3 reward tiles, drawn from a faction-weighted table:

| Reward | Frequency | Notes |
|---|---|---|
| Lore note (memory fragment) | common | hooks into Spec 7's lore-fragment system once that lands |
| Schematic (blueprint)       | uncommon | hooks into Spec 2's schematic system |
| Encrypted file (intel)      | uncommon | hooks into Spec 6's intel/map system |
| Credits cache               | rare | small, never bank-breaking |
| Program (`.exe` drop)       | very rare | rare master-tier programs may drop here |

Drop tables finalize in Spec 2 (loot economy retune). Spec 1 ships placeholder rewards.

### 9.4. Generator

A new sector generator **`gen_implant_sector`** produces the brain-space. Reuses Plan 8's room-template machinery at small scale:
- 1 oversized room (70%) or 2 small rooms with one bridge (30%).
- Walls in faction palette.
- 1–3 reward tiles placed randomly, ≥1 cell from each wall.
- No anchor, no ICE, no DataNode/EncryptedFile in the standard sense — only reward tiles.

Per-corpse seed = `(npc_id, npc_death_tick)`. Re-jack is impossible by design (one-shot), so the seed only ever generates once.

### 9.5. Architecture note

A dead NPC's corpse-implant lives on the **corpse fixture in the world**, not on the LAN sector. Jacking into a corpse implant pushes the player into a *transient sector* that's disposed when the player jacks out and the rewards have been collected (or when the player jacks out without collecting — the rewards are then lost; corpse marked exhausted).

---

## 10. Loot rendering — corpse offset

When a corpse drops loot in real-world space, **offset the loot tile** so it does not overlap the corpse glyph. Existing convention applies:
- Corpse glyph stays at the death tile.
- Loot drops to the nearest empty adjacent walkable tile (orthogonal preferred, diagonal fallback).
- If no adjacent tile is free, loot stacks on the corpse but the corpse glyph wins on render (loot pickup-able via stand-on).

This is a small renderer / drop-system fix coupled to Spec 1 because the dead-implant deep-dive *also* drops items at corpse tiles. Both paths use the same offset rule.

---

## 11. Plan 7 dormancy — keep, don't extend

Plan 7's device-shell layer (DeviceShell, ssh, FS, flavor packs, `(hack) Shell Access`) **stays in tree, dormant**. Spec 1 disconnects it from the new combat loop:

**Disconnected (no longer wired into the new path):**
- `(hack) Shell Access` interactable on world fixtures and NPC implants — **removed** from the dialog manager (replaced by anchor adjacency in the Grid + the new dead-implant `(hack) Jack Into Implant` interactable on corpses).
- `ssh` command from the PDA hacking tab — **removed** (or stays as a no-op stub returning "deprecated"; pick at implementation).

**Kept dormant in tree** (no maintenance, no removal):
- `DeviceShell` class, all its plumbing.
- `hack_command` registry + all `cmd_*.cpp`.
- `HackFlavor` packs and `DeviceFsView`.

These may find a new purpose later (deep-Grid story content, AlienTech dialect, sysop persona, etc.). Spec 1 does not delete them; it just stops using them.

`HackTagMask`, `Hackable` substrate, IP system, Trace / Heat / RAM economy all stay live and active.

---

## 12. Architecture sketch

| Unit | Header | Responsibility | Status |
|---|---|---|---|
| `Anchor`           | `anchor.h` / `anchor.cpp`           | Anchor entity: HP, position, NPC link, status. | New |
| `AnchorTracker`    | `anchor_tracker.h` / `.cpp`         | Spawn / despawn lifecycle, RW↔Grid coord projection, dispatch program effects. | New |
| `VulnerabilityStack` | `vulnerability.h`                 | Real-world-side state on NPCs: status list, source, duration. | New |
| `grid_combat.h`    | constants + bump-attack logic       | Melee constants, bump-into resolution. | New |
| `gen_implant_sector` | `implant_sector_generator.h/.cpp` | Per-corpse brain-space generator. | New |
| `Hackable`         | (existing)                          | Gains: `has_implant_native: bool`, `tethered: bool`, `corpse_exhausted: bool`. | Extended |
| `Npc`              | (existing)                          | Gains link to its anchor (anchor_id or null). | Extended |
| `GridSession`      | (existing)                          | New `anchors` list; renderer + input pipelines consume. | Extended |
| `grid_input::on_step`, `move_with_step` | (existing)        | Bump-attack integration (walk into anchor / ICE → melee hit). | Extended |
| `grid_renderer`    | (existing)                          | Anchor glyph + label render layer. | Extended |
| `look_widget`      | (existing or new)                   | Unified NPC info display, hacker-gated implant line. | Extended |
| `combat.cpp`       | (existing)                          | Apply / decay vulnerability stack; XP grant on Grid kill. | Extended |
| `program.cpp`      | (existing)                          | New program registry entries (`ping.exe`, `lag.exe`, …). | Extended |
| `program_effects`  | (existing)                          | New effect functions per anchor program. | Extended |
| `cat_hacking_perks`| (existing perk tree)                | `Implant Reader`, `Tether L1/L2/L3`. | Extended |
| `dialog_manager`   | (existing)                          | **Remove** `append_shell_access_option*` wiring; **add** corpse `(hack) Jack Into Implant` interactable. | Extended |
| `device_shell.*`, `hack_command.*`, `hack_flavor.*`, `device_fs.*`, `cmd_*.cpp` | (existing) | **Dormant** — no changes, no deletion. | Untouched |

### 12.1. File-size discipline

- `anchor.cpp` ≤ ~250 lines.
- `anchor_tracker.cpp` ≤ ~300 lines.
- `implant_sector_generator.cpp` ≤ ~250 lines.
- New program effect functions: one per file in `src/program_effects_anchor/`, ≤ ~80 lines each.

---

## 13. Save schema — bump v63 → v64

**New persistent fields:**
- Per-anchor: `npc_id`, `current_hp`, `max_hp`, `severed: bool`.
- Per-NPC: `vulnerability_stack` (list of `{status, source_program, remaining_turns}`).
- Per-corpse: `corpse_implant_exhausted: bool`, `corpse_implant_seed: uint32`.

**Rejected on load:** v63 saves are rejected per project rule (no backward compat pre-ship). Clean error message: `save schema too old; start a new game`.

---

## 14. Testing strategy

### 14.1. Unit tests (gtest)

- Anchor coord projection: real-world (x,y) → LAN (gx, gy) is deterministic.
- Anchor HP arithmetic: damage clamps at 0; severed flag latches; vulnerability_pct in [0.0, 1.0].
- Vulnerability stack: status entries decay correctly, expire on duration tick.
- XP grants: ICE kill / anchor sever both produce expected XP into the player's main pool.
- Implant sector generator: same `(npc_id, death_tick)` seed produces identical sector; reward tiles count in [1, 3].
- Bump-attack: walking into anchor / ICE deals `kGridMeleeDamage`; walking into floor moves normally.
- Tether: target-NPC without native implant gains a tracked anchor on success; respects RAM/Heat.

### 14.2. Integration

- Player kills an NPC after fully severing the anchor → vulnerability stack persisted into combat → real-world finisher kills NPC → XP for both anchor sever and NPC kill granted.
- Player jacks into a corpse → enters a 4×4–8×8 sector → picks up rewards → jacks out → corpse marked exhausted → second jack-in returns "exhausted" message.
- `look` at an NPC with Implant Reader unlocked → widget shows implant info.
- `look` at an NPC without unlock → widget shows "unrecognized hardware".
- Tether L1 → projects anchor for an animal; anchor takes damage; effects apply on the linked animal.

### 14.3. Manual

- Dev mode walk: jack into a populated LAN, walk to an anchor, fire `ping.exe`, observe the linked NPC marked in real-world view; jack out; verify the mark persists.
- Combat-dip: real-world combat with hostile approaching, mid-fight jack-in, fire `lag.exe`, jack out; observe the slowed NPC behavior.
- Master program: drop `severance.exe` via dev console; fire at an anchor; observe small real-world HP damage on linked NPC.

---

## 15. Out of scope (defers to other specs)

- Loot economy retune, Program / Schematic / Virus drop tables (Spec 2).
- Black-market vendors (Spec 3).
- Real-world locked doors and Grid-fixture cracks (Specs 4, 5).
- Intel / map-reveal mechanic (Spec 6).
- Lore-fragment integration (Spec 7).
- Quickhack (`.qh`) retirement / re-spec as a hacker-only meatspace category / replacement with another meatspace-damage system. Tracked as a future spec.
- Boss / multi-anchor encounters (v1 ships 1 anchor per NPC).
- AlienTech / Precursor anchor dialect.
- Cyberdeck mod system polish (Aerojack / Untether stubs from Plan 7 §15) — re-thought in Spec 2 if still needed.

---

## 16. Open questions for the implementation plan

These are tactical, not design — belong in the plan file:

- Anchor max HP curve — exact `f(npc_threat_tier)` shape.
- Tether resource shape — free with cooldown vs. item-consuming vs. charge-pool. Pick during item authoring.
- Implant Reader perk placement in the Cat_Hacking tree — first node, second, branch?
- `ssh` deprecation cleanup — remove the command outright vs. leave as no-op stub.
- Anchor mirror-projection function — direct linear, scaled, modulo? Linear scaled is the default; tune if LANs feel cramped.
- Guardian ICE spawn parameters (which ICE color, spawn delay, despawn rule when anchor dies).
- Look-widget visual layout — single panel or split panes.
- Status-effect glyph rendering on real-world NPCs — superscript glyph, color-tint, both?
- Loot offset rule precise behavior (orthogonal preferred / diagonal fallback / stack-on-corpse-as-last-resort).

---

## 17. Cross-references

- **Manifesto:** `docs/superpowers/specs/2026-05-05-grid-loop-manifesto.md`
- **Plan 5** — LAN expansion + tag taxonomy: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- **Plan 6** — Tron HUD redesign: `docs/superpowers/specs/2026-05-02-grid-hud-design.md`
- **Plan 7** — Device shells (dormant after this spec): `docs/superpowers/specs/2026-05-01-device-shells-design.md`
- **Plan 8** — Grid layout / generator: `docs/superpowers/specs/2026-05-04-plan-8-grid-layout-design.md`
- **Hacking mechanics:** `docs/mechanics.md` § "Hacking"

---

## 18. Decision log (locked during brainstorm)

| # | Decision | Rationale |
|---|---|---|
| 1  | Grid serves A (combat-time tool) + C (exploration) roles, not D (politics). Hacking is niche, never solo. | User decision — pure-hacker is not viable; hackers level standard XP, gear-driven hacking power. |
| 2  | Anchors are mobile entities, not rooms. Mirror NPC position. | User decision. |
| 3  | Damage is damage; anchors don't regen; severed = persistent vulnerability. | User decision. |
| 4  | NPC↔anchor identification via `look` + `Implant Reader` unlock + Tether for non-implanted NPCs. | User decision. |
| 5  | Tether is a Cat_Hacking perk chain (L1/L2/L3). | User-aligned. |
| 6  | New `.exe` programs only; quickhacks (`.qh`) out-of-scope. | User clarification. |
| 7  | Master-tier programs deal small real-world HP damage. Outright Grid kill impossible by design. | User decision. |
| 8  | Bump-attack melee in Grid. RAM/Heat default to 0 but configurable. | User decision. |
| 9  | Grid kills grant standard XP into main pool. No separate Cat_Hacking XP track. | User decision — symmetric with other classes. |
| 10 | Dead-NPC corpse opens a per-corpse 4×4–8×8 procgen brain-space (Option A). One-shot loot. | User decision. |
| 11 | Loot drops offset from corpse glyph. | User callout. |
| 12 | Plan 7 stays dormant in tree; combat loop disconnects from it. | User decision. |
| 13 | Save schema bump v63 → v64; reject older saves. | Project policy (no backcompat pre-ship). |

---

## 19. Status

Spec — pending implementation plan. Once approved, hand off to the `writing-plans` skill for the detailed task breakdown.
