# Relay Spine — Marks, Combat, XP (Spec 1)

**Date:** 2026-05-05
**Status:** Spec — pending implementation plan.
**Audience:** Implementation planner (`writing-plans` skill) and contributors picking up the Relay combat layer.
**Parent:** [Relay Loop Manifesto](2026-05-05-grid-loop-manifesto.md) — read this first.
**Lore:** [`docs/lore.md`](../../lore.md) — for the in-fiction context (Substrate, Sgr A*, going dark, Drifter archetype).

---

## 0. TL;DR

Spec 1 turns the Relay layer into a load-bearing combat-tool layer for **Drifter** builds and a small-but-real exploration reward layer. It introduces:

1. **NPC Mark entities** — Substrate-projected guardian entities of real-world hostile NPCs. Damaging a Mark strips defenses on the linked NPC; severing it leaves the NPC permanently exposed.
2. **A new Sigil pool** focused on Mark control + Relay combat. Atmospheric, ancient-feeling names (`Echo`, `Lull`, `Veil`, `Falter`, `Shroud`, `Wither`, `Snuff`, `Fester`, `Lance`). Sigils deal **Mark damage + real-world status effects**; rare master Sigils deal small **real-world HP damage** directly (capped — outright kills via Relay alone are impossible by design).
3. **Proper Relay combat** — the avatar gets ranged (Sigil fire) + melee (bump-attack into adjacent Warden / Mark / hostile entity).
4. **Relay kills now grant XP** into the player's main pool (currently they grant zero — bug fix).
5. **Drifter progression is gear-driven**, not skill-XP-driven. No separate Cat_Drift XP track. Drifters level by collecting more Sigils and better Resonator gear.
6. **Dead-NPC Crystal deep-dive** — when a hostile NPC dies, the Drifter can couple into the corpse's dormant Crystal and walk a small per-corpse procgen **Imprint** (4×4–8×8) for lore / schematics / data crypts.
7. **Plan 7 device-shell layer goes dormant** — kept in tree, disconnected from the combat loop, no maintenance.

The design philosophy: Drifting is **always combo with another build, never solo**. Drifters prep / control; the kill comes from turrets, pistols, melee, etc.

Vocabulary note: this spec uses the player-facing **Relay / Drifter / Sigil / Mark / Crystal / Warden** register. Code identifiers (`GridSession`, `Anchor`, `ICE`, etc.) keep their existing names — a separate code-rename pass is a deferred follow-up.

---

## 1. Concept and pillars

### 1.1. The Relay does two things, in one geography

- **Combat-time tool layer.** Drifter dips into the local Site mid-fight, navigates to an enemy's mobile Mark, fires Sigils to apply persistent debuffs / impairs / signals, decouples. World ticks slow during the dip (already implemented). Kill happens in real-world via the player's other build. **Drifting = control and prep, never finisher.**
- **Exploration reward layer.** Same Site, walked at leisure. Scarce Drifter-relevant rewards. Marks are the *combat* targets; Chambers / Caches / Imprints are the *exploration* targets.

### 1.2. Player archetype

The Drifter is **always a combo build** — tinker+drift, pistol+drift, melee+drift. Pure Drifter is not viable. Drifting complements; it does not solo.

- **Tinker + Drift.** Lay turrets/mines, couple in, debuff approaching NPC's Mark, decouple — turrets do the work.
- **Pistol + Drift.** Speed-walk to Mark, virus, decouple, finish at range with the debuff active.
- **Melee + Drift.** Mark + slow at the Mark, decouple, close distance into a softened target.

### 1.3. What this spec does NOT do

- Does not gate the macro arc. Sgr A* / rebirth remain reachable by non-Drifter builds.
- Does not add a separate Cat_Drift XP track. Skill perks stay tied to existing character-level + perk-tree mechanics.
- Does not touch quickhacks (`.qh`). Out-of-scope. They may later be **re-specced as a Drifter-only meatspace mechanic** (a deliberate, scoped feature for the Drifter build), or **replaced by a different meatspace-damage category**, or **retired entirely**. Whichever direction is picked happens in its own spec, not here.
- Does not implement loot scarcity or new vendor types (Spec 2 / Spec 3).
- Does not add real-world locked doors (Spec 4).
- Does not perform the code-side vocabulary rename (deferred follow-up). Player-facing strings, log lines, and new code use the new register; existing `Grid*` / `Anchor` / `ICE` identifiers stay until the rename pass.

---

## 2. The Mark entity

### 2.1. Definition

A **Mark** is a Substrate-projected entity tied 1:1 to a single hostile NPC. While the Mark holds, the NPC is "armored" against certain real-world status effects. As the Mark takes damage, the linked NPC's defenses degrade *proportionally*. When the Mark falls (HP = 0), the NPC drops into a **permanent vulnerability state** for that NPC instance.

The Mark is **not** the NPC's HP. Severing the Mark does not kill the NPC — it strips their defense layer so the player's other build can close.

In-fiction: every NPC carrying a **Crystal** projects through it onto the Substrate, where the projection appears (to a Drifter using a Resonator) as a tile-bound entity in the local Site. That's the Mark.

### 2.2. Lifecycle

| Event | Behavior |
|---|---|
| NPC enters Site's region | Mark projects at mirrored coordinates. |
| NPC moves in real-world | Mark mirrors position via fixed coord-projection. |
| Mark takes damage | Mark HP decreases. Real-world NPC's vulnerability stack updates proportionally. |
| Mark reaches 0 HP | Severed. Vulnerability stack pinned at full. NPC is "fully vulnerable" until killed. |
| NPC dies in real-world | Mark dissolves from the Site. Corpse retains a dormant **Cooling Crystal** (§ 9). |

NPCs almost never leave the Site's region in v1.

**Marks do not regenerate.** Damage is damage. A half-severed Mark stays half-severed across couple / decouple cycles.

### 2.3. Mark HP and proportional vulnerability

```
mark_max_hp = f(npc_threat_tier)   // e.g. tier 1 NPC -> 10 HP, tier 5 -> 100 HP
mark_hp_pct = mark_hp / mark_max_hp

vulnerability_pct = 1.0 - mark_hp_pct      // 0% at full HP, 100% at severed
```

Vulnerability acts as a *modifier* on the NPC's defenses in real-world combat. Each Sigil that requires a "vulnerable" NPC checks the current vulnerability stack. The rule is: damage is damage, applied proportionally, and the cap is "fully vulnerable" at sever.

### 2.4. Mark visual and label

- **Glyph:** `※` in the linked NPC's faction color.
- **Label:** by default, Marks render with a numeric ID (`※1`, `※2`, `※3`).
- **Once identified** via `look` (§ 3) or **Bind** (§ 4), the Mark's label upgrades to the NPC's name in faction color.
- **HP indicator:** small bar / fraction overlay rendered next to the Mark when within scanner range.

### 2.5. Marks vs. Wardens — the two combat targets

| | Warden | Mark |
|---|---|---|
| **Tied to** | Site infrastructure (Chambers, Caches) | A specific real-world NPC |
| **Behavior** | Patrols, aggros on detection | Mirrors NPC position; may spawn guardian Wardens if damaged |
| **Killable with** | Sigils + melee | Sigils + melee |
| **Drops** | Sigils / Channel upgrades / schematics / credits / XP (Spec 2 finalizes) | Nothing direct — the *NPC* drops normally in real-world. |
| **Purpose** | Friction during exploration | Setup target during combat |

Wardens serve the exploration-friction role. Marks serve the combat-tool role. Same Site, two reasons to fight.

---

## 3. Real-world identification — the `look` widget

### 3.1. Widget extension

The existing `look` (cursor-mode inspect) command is extended to surface **all** NPC-related info in one widget:

- Name, race / class
- Threat tier
- HP bar (existing)
- Active status effects + remaining duration
- **Crystal status** (Drifter-only — § 3.2)

The widget redesign is part of this spec. Output should be readable in a single glance — one panel, one column.

### 3.2. Crystal identification — `Crystal-Decoder` Cat_Drift unlock

A new Cat_Drift skill node, **Crystal-Decoder**, gates the Crystal-status info in the look widget:

- **Without unlock:** look at NPC → standard combat info, no Crystal data. Crystal line says `(crystal: unrecognized)`.
- **With unlock:** look at NPC → adds `Crystal: HAS / NONE`. If HAS, also shows the linked Mark's Site coordinates and current Mark HP.
- **In Site:** Mark renders with NPC's name (instead of numeric ID) once that NPC has been `look`-identified at least once.

Skill node placement: low tier of the Cat_Drift tree — first / second unlock. Should be one of the things a player picks early when committing to Drifting.

### 3.3. NPCs without Crystals

Some NPC types have no native Crystal: animals, drones, Feral, raw biological monsters. They have:
- No Mark in the Site by default.
- `look` (with Crystal-Decoder): `Crystal: NONE — bind required`.
- Only addressable from the Site after a successful **Bind** (§ 4).

---

## 4. The Bind mechanic

### 4.1. Concept

**Bind** is a Drifter ability that *forces* a non-Crystal-bearing target into the Substrate, projecting an artificial Mark for them. In-fiction: the Drifter's Resonator improvises an interface for a target without the proper hardware. The Substrate doesn't object — it tolerates — but the Mark is unstable and fades when the target dies.

It opens up:

- NPCs without native Crystals (animals, drones, Feral, etc.).
- Edge cases where the Mark is hard to find or out of Site range.

### 4.2. UX

- **Trigger:** real-world key (default: `B`?) when a target is selected via `look` cursor or directly adjacent.
- **Effect:** projectile-like — line-of-sight check from player to target. On success, the target gains a temporary Mark in the Site at the projected position.
- **Cost:** Channel + Drift. Defaults: `kBindChannelCost = 2`, `kBindDriftCost = 4` (placeholder, tunable). Constants live next to the melee constants in `grid_combat.h` (file rename deferred).
- **Duration:** persists until the Mark is severed or the NPC dies.
- **Charges:** Bind might consume a consumable item (`bind shard`?) or be charge-limited per Resonator mod. v1 ships with one of: free with cooldown, item-consuming, or charge-pool. Pick at implementation time.
- **Range:** v1 baseline = melee / line-of-sight short range. Tier-2 unlock (Cat_Drift perk): long range. Tier-3: AoE 3-tile.

### 4.3. Skill tree node

Cat_Drift gains a new branch:
- **Bind (level 1)** — basic line-of-sight short-range Bind.
- **Bind (level 2)** — long-range Bind.
- **Bind (level 3)** — AoE Bind (3 tiles).

These are perks unlocked via the existing perk-tree mechanic (level-gated, point-spent). Not XP-grindable separately.

---

## 5. New Sigil pool

All Mark-affecting actions are **Sigils** the Drifter loads into Channels on their Resonator. (`.qh` quickhacks are out of scope and may be retired or re-specced in a future spec — Spec 1 does not extend them.)

### 5.1. Naming and aesthetic

Sigils read as ancient, atmospheric, one-syllable-or-short — fragments of the pre-civ language fragmented into discrete invocations the modern Drifter can speak. The status they apply has a clearly meatspace-suggesting name so the player understands the real-world effect from the Sigil name alone.

### 5.2. v1 Sigil list

Tier and balance numbers are placeholders; tunable in playtest.

| Sigil | Kind | Channel | Drift | Mark dmg | Real-world status applied | RW HP dmg | Duration | Notes |
|---|---|---|---|---|---|---|---|---|
| **Echo**   | Utl | 1 | 1 | 5    | Marked (visible thru walls)   | 0           | 30 t        | resonance pulse |
| **Lull**   | Utl | 2 | 2 | 10   | Slowed (-50% speed)           | 0           | 10 t        | softens motion |
| **Veil**   | Utl | 2 | 3 | 10   | Blinded (cannot see player)   | 0           | 10 t        | obscures vision |
| **Falter** | Utl | 4 | 5 | 25   | Impaired (-50% acc / dodge)   | 0           | 15 t        | breaks coordination |
| **Shroud** | Utl | 5 | 6 | 40   | Exposed (2× incoming dmg)     | 0           | 10 t        | strips defenses |
| **Wither** | Atk | 3 | 4 | 15   | DoT-source                    | 1/turn      | 20 t        | sustained corruption |
| **Snuff**  | Atk | 8 | 10 | full | Severed (perma-vulnerable)    | 0           | persistent  | full Mark takedown |
| **Fester** | Atk | 5 | 6 | 5    | DoT-source                    | 2/turn      | 15 t        | rare drop, RW DoT |
| **Lance**  | Atk | 7 | 8 | 10   | none                          | 5–10 instant | —          | rare drop, chunk RW dmg |

**Snuff** is the full takedown — drops the Mark's HP to 0, applies persistent vulnerability stack. Big spend, big payoff.

**Fester** and **Lance** are master-tier rare drops. They demonstrate the principle: most Sigils do **Mark damage + status only** (no real-world HP). Master tier adds **small real-world HP damage**, capped — outright Relay-only kills are impossible.

The existing programs (`Breach`, `Decrypt`, `IcebreakerLite`, `PulseHammer`, `GhostTrace`, etc.) all stay; the v1 Sigil pool above sits alongside them. Their player-facing names are flagged for a parallel rename pass (Manifesto §6) — proposed mappings: Breach→Pry, Decrypt→Decode, IcebreakerLite→Pierce, GhostTrace→Whisper, Cooldown→Quench, PulseHammer→Pulse, DaemonHijack→Suborn. Not Spec 1's job to retool.

### 5.3. Adjacency vs. range

Mark-affecting Sigils default to **range** (Telegraph picks the Mark tile from LoS). Some flagged Sigils require adjacency (e.g., `Snuff` — full takedown shouldn't be no-risk). Adjacency is a per-Sigil flag:

```cpp
struct ProgramDef {
    // ... existing ...
    bool requires_adjacency = false;
};
```

Master-tier real-world-damage Sigils (`Fester`, `Lance`) require adjacency to balance their power.

---

## 6. Relay combat

### 6.1. Ranged combat — Sigils

Existing system. Number-key fires Sigil from Resonator slot, Telegraph picks the target tile. Hits Wardens or Marks. Already implemented; spec 1 just expands the Sigil pool (§ 5).

### 6.2. Melee combat — bump-attack

The avatar gets a default close-range attack via **bump-into-target**. Walking into a tile occupied by a Warden or Mark triggers a melee hit on that entity.

| Constant | Default | Notes |
|---|---|---|
| `kGridMeleeDamage`     | 3 | bump-attack damage |
| `kGridMeleeChannelCost` | 0 | configurable, may add cost later |
| `kGridMeleeDriftCost`  | 0 | configurable, may add cost later |
| `kGridMeleeRange`      | 1 | always 1 — adjacency only |

These live as named constants in `include/astra/grid_combat.h` (new file; eventual rename to `relay_combat.h` is part of the deferred code-rename pass). Future tuning happens in one place.

Melee damage applies to the entity at the target tile. The avatar does not move into the tile (bump-attack consumes the turn but the avatar holds position).

### 6.3. Targets

Both Wardens and Marks are valid targets for ranged Sigils and melee. Hostile non-Mark NPCs in the Site (rare — drone-like Site mobs, optional v1) are also bumpable.

### 6.4. Warden behavior reaction

When a Mark takes damage, with some probability, **guardian Wardens** spawn from the Mark's tile (configurable; v1 default: 50% chance per major hit, capped at 2 active guardians per Mark). Guardians patrol the Mark's local area until destroyed.

---

## 7. Combat-time pacing (already implemented — no spec changes)

The existing Plan 6 pacing already handles:
- Slow real-world ticks during couple-in.
- Drift / Trace tick rates.
- Decouple triggers (Trace cap, Black Warden damage, voluntary).

Spec 1 inherits this behavior unchanged. Section retained for reference; no new code.

---

## 8. XP — fix the bug, no new track

### 8.1. The current bug

Killing Wardens in the Relay grants **0 XP**. This makes Drifting feel unrewarding even when the player engages directly with it.

### 8.2. The fix

Relay kills grant standard player-pool XP, scaled by the killed entity's tier:

| Kill type | XP grant |
|---|---|
| Warden (white)        | `kXpWardenWhite` (≈ tier-1 mob) |
| Warden (gray)         | `kXpWardenGray`  (≈ tier-2) |
| Warden (black)        | `kXpWardenBlack` (≈ tier-3) |
| Mark sever            | `kXpMarkSever × npc_threat_tier` |
| Mark partial dmg      | none — only sever grants the bounty |

All XP goes into the **same pool** as real-world combat XP. There is no separate Cat_Drift XP track. Drifters progress overall character level the same way other classes do; their *Drift power* comes from the Sigils and Resonator gear they collect (loot-driven).

### 8.3. Cat_Drift perk progression

Existing perk-tree mechanic. Cat_Drift gains new perks in this spec:
- **Crystal-Decoder** (§ 3.2)
- **Bind** L1, L2, L3 (§ 4.3)

These unlock at character-level milestones with skill points spent — same as every other skill tree.

---

## 9. Dead-NPC Crystal deep-dive

### 9.1. Concept

When a hostile NPC with a Crystal dies, the corpse retains a **Cooling Crystal** the Drifter can couple into. Each corpse opens up a small per-corpse procgen **Imprint** — a spatial echo of the dead person's Crystal-stratum (Option A from brainstorm). Pure Drifter-only content; non-Drifter builds walk past corpses normally.

In-fiction: a Cooling Crystal still holds the dead person's residual imprint on the Substrate. Walking the Imprint is walking through the shape that imprint left behind. Most Imprints are mostly empty — the residue has already begun to dissolve.

### 9.2. UX

- **Trigger:** Drifter stands adjacent to corpse → `(read) Walk the Imprint` interactable. Cat_Drift gated.
- **Geography:** small procgen sector, **4×4–8×8** tiles. Single Chamber or 2-Chamber layout. Faction-flavored visuals.
- **Time:** instant in real-world (corpse is dead; nothing's moving). Pure exploration.
- **Drift / Trace:** low cost — Imprints are unguarded, no Wardens.
- **Returnable?:** **No.** Each corpse is walked once. Looted = empty. Subsequent attempts return immediately with "imprint dissolved."

### 9.3. Content

Loot is **deliberately scarce.** Most corpses yield **nothing** beyond the diegetic experience of poking around a dissolving Imprint. The Imprint itself is the payoff — the rare loot is a bonus.

**Reward count distribution** (per Imprint):

| Rewards in Imprint | Probability |
|---|---|
| 0 rewards (empty / mood only) | ~60% |
| 1 reward                      | ~30% |
| 2 rewards                     | ~10% |
| 3+ rewards                    | never (hard cap at 2) |

**Reward type table** (rolled per reward slot, faction-weighted):

| Reward | Frequency within slot | Notes |
|---|---|---|
| Lore note (memory fragment) | uncommon | hooks into Spec 7's lore-fragment system once that lands |
| Schematic (blueprint)       | rare | hooks into Spec 2's schematic system |
| Sealed Cache (intel)        | rare | hooks into Spec 6's intel/map system |
| Credits cache               | rare | small, never bank-breaking |
| Sigil drop                  | very rare | only master-tier Sigils drop here, signature loot |

Drop tables finalize in Spec 2 (loot economy retune). Spec 1 ships these as placeholders.

**Global scarcity principle.** Astra's loot economy as a whole is intentionally lean — players should not be drowned in rewards. The Imprint numbers above follow that principle and will be **rebalanced in concert with the rest of the loot tables in Spec 2**. If Spec 2's overall scarcity dial is set higher / lower, the Imprint numbers shift accordingly.

### 9.4. Generator

A new sector generator **`gen_imprint_sector`** produces the Imprint. Reuses Plan 8's room-template machinery at small scale:
- 1 oversized Chamber (70%) or 2 small Chambers with one bridge (30%).
- Walls in faction palette.
- 0–2 reward tiles placed randomly per the distribution above, ≥1 cell from each wall.
- No Mark, no Wardens, no Cache/Sealed-Cache in the standard sense — only reward tiles.

Per-corpse seed = `(npc_id, npc_death_tick)`. Re-couple is impossible by design (one-shot), so the seed only ever generates once.

### 9.5. Architecture note

A dead NPC's Cooling Crystal lives on the **corpse fixture in the world**, not on the Site. Coupling into a Cooling Crystal pushes the player into a *transient sector* that's disposed when the player decouples and the rewards have been collected (or when the player decouples without collecting — the rewards are then lost; corpse marked exhausted).

---

## 10. Loot rendering — corpse offset

When a corpse drops loot in real-world space, **offset the loot tile** so it does not overlap the corpse glyph. Existing convention applies:
- Corpse glyph stays at the death tile.
- Loot drops to the nearest empty adjacent walkable tile (orthogonal preferred, diagonal fallback).
- If no adjacent tile is free, loot stacks on the corpse but the corpse glyph wins on render (loot pickup-able via stand-on).

This is a small renderer / drop-system fix coupled to Spec 1 because the Imprint deep-dive *also* drops items at corpse tiles. Both paths use the same offset rule.

---

## 11. Plan 7 dormancy — keep, don't extend

Plan 7's device-shell layer (DeviceShell, ssh, FS, flavor packs, `(hack) Shell Access`) **stays in tree, dormant**. Spec 1 disconnects it from the new combat loop:

**Disconnected (no longer wired into the new path):**
- `(hack) Shell Access` interactable on world fixtures and NPC implants — **removed** from the dialog manager (replaced by Mark adjacency in the Site + the new Cooling-Crystal `(read) Walk the Imprint` interactable on corpses).
- `ssh` command from the PDA hacking tab — **removed** (or stays as a no-op stub returning "deprecated"; pick at implementation).

**Kept dormant in tree** (no maintenance, no removal):
- `DeviceShell` class, all its plumbing.
- `hack_command` registry + all `cmd_*.cpp`.
- `HackFlavor` packs and `DeviceFsView`.

These may find a new purpose later (deep-Relay story content, AlienTech dialect, sysop persona for one specific faction, etc.). Spec 1 does not delete them; it just stops using them. The cyberpunk register also doesn't fit Astra's actual narrative (see `docs/lore.md` for setting); any future re-use would re-flavor the surface.

`HackTagMask`, `Hackable` substrate, IP system, Trace / Drift / Channel economy all stay live and active.

---

## 12. Architecture sketch

Code identifiers reflect the existing convention (`Anchor`, `GridSession`, `ICE`, etc.) — the deferred code-rename pass aligns them to player-facing terms later.

| Unit | Header | Responsibility | Status |
|---|---|---|---|
| `Anchor` (= Mark in player vocab) | `anchor.h` / `anchor.cpp`           | Mark entity: HP, position, NPC link, status. | New |
| `AnchorTracker`    | `anchor_tracker.h` / `.cpp`         | Spawn / despawn lifecycle, RW↔Site coord projection, dispatch Sigil effects. | New |
| `VulnerabilityStack` | `vulnerability.h`                 | Real-world-side state on NPCs: status list, source, duration. | New |
| `grid_combat.h`    | constants + bump-attack logic       | Melee constants, bump-into resolution. | New |
| `gen_imprint_sector` | `imprint_sector_generator.h/.cpp` | Per-corpse Imprint generator. | New |
| `Hackable`         | (existing)                          | Gains: `has_crystal_native: bool`, `bound: bool`, `corpse_exhausted: bool`. | Extended |
| `Npc`              | (existing)                          | Gains link to its Mark (anchor_id or null). | Extended |
| `GridSession`      | (existing)                          | New `anchors` (Marks) list; renderer + input pipelines consume. | Extended |
| `grid_input::on_step`, `move_with_step` | (existing)        | Bump-attack integration (walk into Mark / Warden → melee hit). | Extended |
| `grid_renderer`    | (existing)                          | Mark glyph + label render layer. | Extended |
| `look_widget`      | (existing or new)                   | Unified NPC info display, Drifter-gated Crystal line. | Extended |
| `combat.cpp`       | (existing)                          | Apply / decay vulnerability stack; XP grant on Relay kill. | Extended |
| `program.cpp`      | (existing)                          | New Sigil registry entries (Echo, Lull, Veil, …). | Extended |
| `program_effects`  | (existing)                          | New effect functions per Mark Sigil. | Extended |
| `cat_hacking_perks` (= Cat_Drift in player vocab) | (existing perk tree) | `Crystal-Decoder`, `Bind L1/L2/L3`. | Extended |
| `dialog_manager`   | (existing)                          | **Remove** `append_shell_access_option*` wiring; **add** corpse `(read) Walk the Imprint` interactable. | Extended |
| `device_shell.*`, `hack_command.*`, `hack_flavor.*`, `device_fs.*`, `cmd_*.cpp` | (existing) | **Dormant** — no changes, no deletion. | Untouched |

### 12.1. File-size discipline

- `anchor.cpp` ≤ ~250 lines.
- `anchor_tracker.cpp` ≤ ~300 lines.
- `imprint_sector_generator.cpp` ≤ ~250 lines.
- New Sigil effect functions: one per file in `src/program_effects_anchor/`, ≤ ~80 lines each.

---

## 13. Save schema — bump v63 → v64

**New persistent fields:**
- Per-Mark: `npc_id`, `current_hp`, `max_hp`, `severed: bool`.
- Per-NPC: `vulnerability_stack` (list of `{status, source_sigil, remaining_turns}`).
- Per-corpse: `corpse_imprint_exhausted: bool`, `corpse_imprint_seed: uint32`.

**Rejected on load:** v63 saves are rejected per project rule (no backward compat pre-ship). Clean error message: `save schema too old; start a new game`.

---

## 14. Testing strategy

### 14.1. Unit tests (gtest)

- Mark coord projection: real-world (x,y) → Site (sx, sy) is deterministic.
- Mark HP arithmetic: damage clamps at 0; severed flag latches; vulnerability_pct in [0.0, 1.0].
- Vulnerability stack: status entries decay correctly, expire on duration tick.
- XP grants: Warden kill / Mark sever both produce expected XP into the player's main pool.
- Imprint generator: same `(npc_id, death_tick)` seed produces identical sector; reward tiles count in [0, 2].
- Bump-attack: walking into Mark / Warden deals `kGridMeleeDamage`; walking into floor moves normally.
- Bind: target-NPC without native Crystal gains a tracked Mark on success; respects Channel/Drift.

### 14.2. Integration

- Player kills an NPC after fully severing the Mark → vulnerability stack persisted into combat → real-world finisher kills NPC → XP for both Mark sever and NPC kill granted.
- Player couples into a Cooling Crystal → walks a 4×4–8×8 Imprint → picks up rewards → decouples → corpse marked exhausted → second couple-in returns "dissolved" message.
- `look` at an NPC with Crystal-Decoder unlocked → widget shows Crystal info.
- `look` at an NPC without unlock → widget shows "unrecognized."
- Bind L1 → projects Mark for an animal; Mark takes damage; effects apply on the linked animal.

### 14.3. Manual

- Dev mode walk: couple into a populated Site, walk to a Mark, fire Echo, observe the linked NPC marked in real-world view; decouple; verify the mark persists.
- Combat-dip: real-world combat with hostile approaching, mid-fight couple-in, fire Lull, decouple; observe the slowed NPC behavior.
- Master Sigil: drop Lance via dev console; fire at a Mark; observe small real-world HP damage on linked NPC.

---

## 15. Out of scope (defers to other specs)

- Loot economy retune, Sigil / Schematic / Crystal drop tables (Spec 2).
- Black-market vendors (Spec 3).
- Real-world locked doors and Relay-fixture cracks (Specs 4, 5).
- Intel / map-reveal mechanic (Spec 6).
- Lore-fragment integration (Spec 7).
- Code-side vocabulary rename (`GridSession` → `RelaySession`, etc.) — deferred follow-up pass.
- Quickhack (`.qh`) retirement / re-spec as a Drifter-only meatspace category / replacement with another meatspace-damage system. Tracked as a future spec.
- Boss / multi-Mark encounters (v1 ships 1 Mark per NPC).
- AlienTech / Precursor Sigil dialect.
- Resonator mod system polish (Aerojack / Untether stubs from Plan 7 §15) — re-thought in Spec 2 if still needed.

---

## 16. Open questions for the implementation plan

These are tactical, not design — belong in the plan file:

- Mark max HP curve — exact `f(npc_threat_tier)` shape.
- Bind resource shape — free with cooldown vs. item-consuming vs. charge-pool. Pick during item authoring.
- Crystal-Decoder perk placement in the Cat_Drift tree — first node, second, branch?
- `ssh` deprecation cleanup — remove the command outright vs. leave as no-op stub.
- Mark mirror-projection function — direct linear, scaled, modulo? Linear scaled is the default; tune if Sites feel cramped.
- Guardian Warden spawn parameters (which Warden color, spawn delay, despawn rule when Mark dies).
- Look-widget visual layout — single panel or split panes.
- Status-effect glyph rendering on real-world NPCs — superscript glyph, color-tint, both?
- Loot offset rule precise behavior (orthogonal preferred / diagonal fallback / stack-on-corpse-as-last-resort).

---

## 17. Cross-references

- **Lore (story arc, Substrate, Sgr A*):** `docs/lore.md`
- **Manifesto:** `docs/superpowers/specs/2026-05-05-grid-loop-manifesto.md`
- **Plan 5** — Site (LAN) expansion + tag taxonomy: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- **Plan 6** — Tron HUD redesign: `docs/superpowers/specs/2026-05-02-grid-hud-design.md`
- **Plan 7** — Device shells (dormant after this spec): `docs/superpowers/specs/2026-05-01-device-shells-design.md`
- **Plan 8** — Site layout / generator: `docs/superpowers/specs/2026-05-04-plan-8-grid-layout-design.md`
- **Mechanics:** `docs/mechanics.md`

---

## 18. Decision log (locked during brainstorm)

| # | Decision | Rationale |
|---|---|---|
| 1  | The Relay serves A (combat-time tool) + C (exploration) roles, not D (politics). Drifting is niche, never solo. | User decision — pure-Drifter is not viable; Drifters level standard XP, gear-driven Drift power. |
| 2  | Marks are mobile entities, not Chambers. Mirror NPC position. | User decision. |
| 3  | Damage is damage; Marks don't regen; severed = persistent vulnerability. | User decision. |
| 4  | NPC↔Mark identification via `look` + `Crystal-Decoder` unlock + Bind for non-Crystal-bearing NPCs. | User decision. |
| 5  | Bind is a Cat_Drift perk chain (L1/L2/L3). | User-aligned. |
| 6  | New Sigils only; quickhacks (`.qh`) out-of-scope, possibly re-specced as Drifter-only meatspace later. | User clarification. |
| 7  | Master-tier Sigils deal small real-world HP damage. Outright Relay kill impossible by design. | User decision. |
| 8  | Bump-attack melee in Relay. Channel/Drift default to 0 but configurable. | User decision. |
| 9  | Relay kills grant standard XP into main pool. No separate Cat_Drift XP track. | User decision — symmetric with other classes. |
| 10 | Dead-NPC corpse opens a per-corpse 4×4–8×8 procgen Imprint. One-shot loot. | User decision. |
| 11 | Loot drops offset from corpse glyph. | User callout. |
| 12 | Plan 7 stays dormant in tree; combat loop disconnects from it. | User decision. |
| 13 | Save schema bump v63 → v64; reject older saves. | Project policy (no backcompat pre-ship). |
| 14 | Imprint loot is deliberately scarce — ~60% empty, ~30% 1 reward, ~10% 2 rewards. Aligns with Astra's global loot scarcity principle. | User decision — most corpses yield mood, not loot. Rebalance lives in Spec 2. |
| 15 | Player-facing vocabulary re-skinned to Relay / Drifter / Sigil / Mark / Crystal / Warden / Channel / Drift / Couple register. Code-side rename deferred to a separate follow-up pass. | User decision — narrative re-frame to ancient-tech archaeological tone, away from cyberpunk register. |
| 16 | The Substrate is the in-fiction layer beneath the Relay Network; assimilates and bestows in equal measure; named only by ancients early in the game; powers the Relay; player is "touched" by it at Sgr A* for rebirth. Full lore in `docs/lore.md`. | User narrative direction. |

---

## 19. Status

Spec — pending implementation plan. Once approved, hand off to the `writing-plans` skill for the detailed task breakdown.
