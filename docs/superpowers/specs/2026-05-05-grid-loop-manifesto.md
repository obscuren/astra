# Grid Loop Manifesto

**Date:** 2026-05-05
**Status:** Living roadmap. Captures what the Grid is for, who it serves, and the sub-projects required to make it pull its weight. Spawns per-sub-project design specs as work proceeds.
**Audience:** Design + implementation planners. Read this before opening any Grid-related spec.

---

## 0. TL;DR

The Grid is currently a mechanically rich tech demo. Trace, Heat, ICE, programs, the tag-driven device shell, two-tier sector geography — all built, none load-bearing. Players can engage with it, but skipping it costs nothing real, and engaging with it produces little beyond credit top-ups. This manifesto locks the design direction: the Grid becomes a **niche-but-meaningful build pillar** for hackers, comparable to tinkering or ranged combat — *always combined* with another playstyle, *never* a solo path.

The first detailed spec covers the spine: **NPC Anchor entities + Grid combat mechanics + XP rewards for Grid kills.** Subsequent specs handle loot scarcity, real-world locked doors with Grid-side controls, black-market vendors, intel/maps, and lore fragments.

---

## 1. What the Grid is FOR

### 1.1. Two roles, one geography

**A. Combat-time tool layer.** During real-world combat the hacker dips into the LAN, navigates to an enemy's mobile **anchor entity**, applies persistent debuffs / impairs / marks, and jacks out. World time ticks slow during the dip. The kill happens in the real world via the player's other build. **Hacking = control and prep, never finisher.**

**C. Exploration reward layer.** Same LAN, walked at leisure between or ahead of fights. Hacker-relevant rewards drop scarcely (most LANs are flavor; rare LANs are signature). Loot pool: programs, schematics / blueprints, real-world unlocks (Grid fixture → real-world door crack), intel / maps, lore fragments, XP. Credits remain part of the mix but follow normal scarcity rules — the hacker is not a credit farm.

### 1.2. Player archetype: the hacker is a combo build

Pure hacker is not viable. Hacking complements; it does not solo. Intended play patterns:

- **Tinker + Hack.** Lay turrets and mines, jack in, debuff the approaching NPC's anchor, jack out — turrets do the work.
- **Pistol + Hack.** Speed-walk to the anchor, virus, jack out, finish at range with the debuff active.
- **Melee + Hack.** Mark + slow at the anchor, jack out, close distance into a softened target.

The Grid's job is to make whatever else you brought stronger.

### 1.3. What the Grid does NOT do

- **Does not gate the macro arc.** Sgr A* and consciousness rebirth must be reachable by non-hacker builds through real-world play.
- **Does not solo.** No combat-only-via-hacking path — every hacker kill is a real-world finisher.
- **Does not pile credits.** Credit drops follow normal scarcity rules.
- **Does not require a multi-step shell.** Combat-time verbs need to fire at tile speed, not via `ssh` → `ls` → `cat`.

---

## 2. The eight sub-projects

| # | Sub-project | Status | Notes |
|---|---|---|---|
| **1** | **NPC Anchor system + control verbs (THE SPINE)** | New | Mobile guardian entities tied to real-world NPCs. Damaging / killing the anchor strips a "vulnerability cap" on the real-world NPC. |
| **2** | Real-world locked doors | New | Locked-door system in real-world dungeons does not yet exist. Generic locked-door tile / fixture, breachable from the world or via Grid sub-project 3. |
| **3** | Grid-fixture → real-world door / object cracks | Depends on 2 | A Grid-side `breach` fixture in a LAN room cracks a paired real-world locked door. Lets hackers route the dungeon. |
| **4** | Loot economy retune (scarcity + Program / Schematic / Virus drop tables) | Touches existing loot-table work (pre-merge in memory) | Programs, schematics, virii drop scarcely. Schematics feed tinker crafting (cross-system). |
| **5** | Black-market vendors | Depends on 4 | New vendor type. Sells programs, schematics, virii, illegal items. Gated by area / faction. Currency is normal credits. |
| **6** | Intel / map-reveal mechanic | New | Decrypted Grid files reveal real-world dungeon info — patrol gaps, hidden vendors, traps, item locations. Mechanic TBD. |
| **7** | Lore fragments integration | Coupled to a not-yet-implemented world-side lore mechanic | Decrypted files surface lore on the same surface as the future lore-fragment loop. |
| **8** | **Grid XP from ICE / anchor kills** | Small fix | Currently 0 XP. Hooks into the existing player XP pool. Folds into Spec 1. |
| (—) | Original "per-room verb" question (Plan 8 follow-up) | Folds into spine | Subnet rooms hold *exploration* content; combat happens at *anchor entities*, not rooms. The "what fires when I'm in a turret room" answer is "exploration verbs only — combat is at anchors." |

---

## 3. Dependency order

### Phase 1 — the spine
- **Spec 1**: NPC Anchors + Grid combat mechanics + Grid XP (sub-projects 1, 8, plus the new "combat" topic).
- Drafted in the same brainstorm session that produced this manifesto.

### Phase 2 — economy and content
- **Spec 2**: Loot economy retune (sub-project 4).
- **Spec 3**: Black-market vendors (sub-project 5). Depends on Spec 2.

### Phase 3 — physical world ties
- **Spec 4**: Real-world locked doors (sub-project 2).
- **Spec 5**: Grid → real-world door cracks (sub-project 3). Depends on Spec 4.

### Phase 4 — info and lore
- **Spec 6**: Intel / map-reveal mechanic (sub-project 6).
- **Spec 7**: Lore-fragment integration (sub-project 7). Depends on world-side lore system being designed elsewhere.

Sub-projects within a phase can be re-sequenced. Phases must run in order — each phase consumes vocabulary defined in earlier phases.

---

## 4. Plan 7 retirement

The Plan 7 device-shell layer was added before this loop framing. It serves neither role A (too slow for combat) nor role C (tile interactions read better than `ls`/`cat`). The shell is therefore on the table for **substantial retirement** in Spec 1.

**Likely survives:**
- The IP / `Hackable` substrate (used to identify devices).
- The `HackTagMask` taxonomy (drives anchor / fixture behavior).
- The Trace / Heat / RAM economy.

**Likely retires:**
- `DeviceShell` session model.
- `ssh` and the procedural filesystem (`ls`, `cat`, `grep`, `find`, `dump`, `wipe`).
- Per-faction flavor packs (banner / MOTD / file content). May relocate as inline flavor on Grid tiles.
- `(hack) Shell Access` interactable on world fixtures and NPC implants — replaced or absorbed by the anchor model.
- The Aerojack / Untether mod stub from Plan 7 §15 — re-thought as part of Spec 2 if still needed.

Spec 1 finalizes this list.

---

## 5. The first spec — Spine (Spec 1)

**Sub-projects covered:** 1 (Anchors), 8 (XP), and a new "Combat" topic that unifies ICE encounters, anchor takedowns, program-as-weapon mechanics, and status-persistence-past-jack-out.

**Open design questions to be settled in Spec 1 brainstorm:**
- Anchor entity model: mobility (follows NPC? spawns on engagement? fixed?), placement rules, HP / damage model, 1:1 vs N:1 with NPCs.
- "Fully vulnerable" semantics — what real-world status applies when the anchor falls?
- Anchor regen / persistence between dips. Does a damaged anchor heal? Does killing it stick?
- Distinction between anchors (defenders of NPCs) and ICE (defenders of LAN infrastructure).
- Combat-time verb vocabulary: virus, impair, blind, mark, slow, expose-flank, etc. — which ship in v1.
- Programs as the verb-delivery mechanism vs. tile interactions vs. both.
- XP allocation: Cat_Hacking only? Player XP pool? Both? Per kind of kill?
- How much of Plan 7 retires — full or partial.
- Time-dilation ratio during combat-time jack-in. Does it differ from peacetime jack-in?
- LAN sector size implications: are Plan 8's 30×16 → 120×42 LANs walkable in combat-relevant time, or do we need a "combat dip" sub-view?

These are answered in the Spec 1 brainstorm immediately following this manifesto.

---

## 6. Cross-references

- **Plan 5** — LAN expansion + tag taxonomy: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- **Plan 6** — Tron HUD redesign: `docs/superpowers/specs/2026-05-02-grid-hud-design.md`
- **Plan 7** — Device shells (retiring in Spec 1): `docs/superpowers/specs/2026-05-01-device-shells-design.md`
- **Plan 8** — Grid layout / generator (current): `docs/superpowers/specs/2026-05-04-plan-8-grid-layout-design.md`
- **Hacking mechanics**: `docs/mechanics.md` § "Hacking"
- **Loot pre-merge cleanup** (memory note): rename `docs/formulas.md` → `docs/mechanics.md`, possibly split rulebook.

---

## 7. Status

Living manifesto. Spec 1 brainstorm follows. Subsequent specs land in the dependency order in §3.
