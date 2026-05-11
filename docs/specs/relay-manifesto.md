# Relay Loop Manifesto

**Date:** 2026-05-05
**Status:** Living roadmap. Captures what the Relay Network is for in gameplay terms, who it serves, and the sub-projects required to make it pull its weight. Spawns per-sub-project design specs as work proceeds.
**Audience:** Design + implementation planners. Read this before opening any Relay-related spec. Pair with `../lore/overview.md` for the narrative context.

---

## 0. TL;DR

The Relay layer (formerly "the Grid") is currently a mechanically rich tech demo. Trace, the cyberdeck-equivalent, ICE patrols, two-tier sector geography — all built, none load-bearing. Players can engage with it, but skipping it costs nothing real, and engaging with it produces little beyond credit top-ups. This manifesto locks the design direction: the Relay becomes a **niche-but-meaningful build pillar** for **Drifters**, comparable to tinkering or ranged combat — *always combined* with another playstyle, *never* a solo path.

The first detailed spec covers the spine: **NPC Imprint entities + Relay combat mechanics + XP rewards for Relay kills.** Subsequent specs handle loot scarcity, real-world locked doors with Relay-side controls, black-market vendors, intel/maps, and lore fragments.

This is a *technical* manifesto. The narrative — the Substrate, the pre-civilizations, the going dark, Sgr A* and rebirth — lives in `../lore/overview.md`.

---

## 1. Narrative anchor (brief)

The full story arc is in `../lore/overview.md`. Three points to keep in mind while reading the rest of this doc:

1. **The Relay Network is the surface name** for an ancient, decaying, galaxy-spanning lattice of Sites built by pre-civilizations. Most factions use it without grasping it. It is **going dark.**
2. **The Substrate is the deeper truth.** The Relay Network is built atop it; the **Substrate powers the Network.** The Substrate is alive in some sense, predates everything, and *assimilates* — rearranges minds and bestows aeons of accumulated knowledge in return. It is not malicious. It is what it is.
3. **The player is a Drifter** — a small operator who jacks into the Network through a **cyberdeck**, runs **exploits**, and exploits the inheritance. Drifters touch the Substrate at the shallow end. Sgr A* at the galactic core is where the Substrate is most awake; reaching it is rebirth.

Vocabulary in this doc and in Spec 1 reflects that framing. See `../lore/overview.md` § 2–11 for full setting and § 12 for tone notes.

---

## 2. What the Relay is FOR

### 2.1. Two roles, one geography

**A. Combat-time tool layer.** During real-world combat the Drifter dips into the local Relay Site, navigates to an enemy's mobile **Imprint** (their Substrate-projected projection point), applies persistent debuffs / impairs / signals, and jacks out. World time ticks slow during the dip. The kill happens in the real world via the player's other build. **Drifting = control and prep, never finisher.**

**C. Exploration reward layer.** Same Site, walked at leisure between or ahead of fights. Drifter-relevant rewards drop scarcely (most Sites are flavor; rare Sites are signature). Loot pool: exploits, schematics / blueprints, real-world unlocks (Relay-side fixture → real-world door crack), intel / maps, lore fragments, XP. Credits remain part of the mix but follow normal scarcity rules — the Drifter is not a credit farm.

### 2.2. Player archetype: the Drifter is a combo build

Pure Drifter is not viable. Drifting complements; it does not solo. Intended play patterns:

- **Tinker + Drift.** Lay turrets and mines, couple in, debuff the approaching NPC's Imprint, decouple — turrets do the work.
- **Pistol + Drift.** Speed-walk to the Imprint, virus, decouple, finish at range with the debuff active.
- **Melee + Drift.** Sever + slow at the Imprint, decouple, close distance into a softened target.

The Relay's job is to make whatever else you brought stronger.

### 2.3. What the Relay does NOT do

- **Does not gate the macro arc.** Sgr A* and rebirth must be reachable by non-Drifter builds through real-world play.
- **Does not solo.** No combat-only-via-Drifting path — every Drifter kill is a real-world finisher.
- **Does not pile credits.** Credit drops follow normal scarcity rules.
- **Does not require a multi-step shell.** Combat-time exploits need to fire at tile speed, not via `ssh` → `ls` → `cat`.

---

## 3. The eight sub-projects

| # | Sub-project | Status | Notes |
|---|---|---|---|
| **1** | **NPC Imprint system + control exploits (THE SPINE)** | New | Mobile guardian projections of real-world hostiles. Damaging / severing the Imprint strips the linked NPC's defenses permanently for that instance. |
| **2** | Real-world locked doors | New | Locked-door system in real-world dungeons does not yet exist. Generic locked-door tile / fixture, breachable from the world or via Relay sub-project 3. |
| **3** | Relay-fixture → real-world door / object cracks | Depends on 2 | A Relay-side fixture in a Chamber cracks a paired real-world locked door. Lets Drifters route the dungeon. |
| **4** | Loot economy retune (scarcity + exploit / Schematic / implant drop tables) | Partially shipped (item defs + offset rule); core retune pending | Exploits, schematics, implants drop scarcely. Schematics feed tinker crafting (cross-system). |
| **5** | Black-market vendors | Depends on 4 | New vendor type. Sells exploits, schematics, implants, illegal items. Gated by area / faction. Currency is normal credits. |
| **6** | Intel / map-reveal mechanic | New | Decoded Relay Caches reveal real-world dungeon info — patrol gaps, hidden vendors, traps, item locations. Mechanic TBD. |
| **7** | Lore fragments integration | Coupled to a not-yet-implemented world-side lore mechanic | Decoded Caches surface lore on the same surface as the future lore-fragment loop. |
| **8** | **Relay XP from ICE / Imprint kills** | Small fix | Currently 0 XP. Hooks into the existing player XP pool. Folds into Spec 1. |
| (—) | Original "per-room verb" question (Plan 8 follow-up) | Folds into spine | Chambers hold *exploration* content; combat happens at *Imprint entities*, not Chambers. The "what fires when I'm in a turret Chamber" answer is "exploration verbs only — combat is at Imprints." |

---

## 4. Dependency order

### Phase 1 — the spine
- **Spec 1**: NPC Imprints + Relay combat mechanics + Relay XP (sub-projects 1, 8, plus the new "combat" topic).
- Drafted in the same brainstorm session that produced this manifesto.

### Phase 2 — economy and content
- **Spec 2**: Loot economy retune (sub-project 4).
  - **Already shipped (during Spec 1 implementation):** item defs for the 9 exploits (Echo / Lag / Veil / Jitter / Shroud / Worm / Brick / Rot / Spike); placeholder loot-table entries with placeholder pricing; corpse loot-offset rule; Imprint-mini-sector reward distribution as a placeholder.
  - **Pending for Spec 2:** drop-rate weights across loot sources; schematics → tinker crafting tie; whether implants are looted off corpses as items (and if so, how player-equippable they are); reconciling Imprint sector reward distribution against the wider scarcity dial.
- **Spec 3**: Black-market vendors (sub-project 5). Depends on Spec 2.

### Phase 3 — physical world ties
- **Spec 4**: Real-world locked doors (sub-project 2).
- **Spec 5**: Relay → real-world door cracks (sub-project 3). Depends on Spec 4.

### Phase 4 — info and lore
- **Spec 6**: Intel / map-reveal mechanic (sub-project 6).
- **Spec 7**: Lore-fragment integration (sub-project 7). Depends on world-side lore system being designed elsewhere.

Sub-projects within a phase can be re-sequenced. Phases must run in order — each phase consumes vocabulary defined in earlier phases.

---

## 5. Plan 7 retirement (dormant, not deleted)

The Plan 7 device-shell layer was added before this loop framing. It serves neither role A (too slow for combat) nor role C (tile interactions read better than `ls`/`cat`). It is also rendered narratively obsolete: the cyberpunk hacker register doesn't fit Astra's actual setting (ancient Network, dying tech, no maintenance, nobody knows how it works).

**Plan 7 stays in tree, dormant.** No deletion. May find a new home later (deep-Relay story content, AlienTech dialect, sysop persona for one specific faction, etc.).

**Likely survives:**
- The IP / `Hackable` substrate (used to identify devices).
- The `HackTagMask` taxonomy (drives Imprint / fixture behavior).
- The Trace / Heat / RAM economy.

**Disconnected from the new loop (in Spec 1):**
- `(hack) Shell Access` interactable on world fixtures and NPC implants.
- `ssh` and the procedural filesystem (`ls`, `cat`, `grep`, `find`, `dump`, `wipe`).
- Per-faction flavor packs (banner / MOTD / file content).
- The Aerojack / Untether mod stub from Plan 7 §15.

The shell layer code remains in tree but is no longer wired into the combat loop. Spec 1 finalizes the boundary.

---

## 6. Vocabulary register

This manifesto and Spec 1 use the **Relay-Network / Drifter** vocabulary throughout for player-facing prose. The `Anchor` → `Imprint` code rename is **complete** (`struct Imprint` in `anchor.h`, methods renamed to `imprint_for_npc` / `imprint_at` / `add_imprint_for_npc`). The file is still named `anchor.h` / `anchor.cpp` — a file-level rename is a separate deferred pass. Remaining `Grid*` / `ICE` identifiers also stay until a broader rename pass.

| Player-facing term | Code identifier | Notes |
|---|---|---|
| The Relay Network / "the Relay" | `GridSession`, `GridSector`, etc. | Code rename deferred |
| Site (a LAN sector) | (existing LAN classes) | |
| Chamber (a subnet room) | (existing room types) | |
| Drifter (the player, in this build) | (no class identifier yet) | |
| Cyberdeck | `Cyberdeck`, `cyberdeck.h` | |
| Exploit (a program) | `ProgramDef`, `ProgramId` | Code rename deferred |
| RAM | `ram_*` fields | |
| Heat | `heat_*` fields | |
| Trace (Trace — kept) | `trace_*` fields | No rename |
| ICE | `Ice` class | |
| Imprint | `struct Imprint` in `anchor.h` | Code rename complete — methods: `imprint_for_npc`, `imprint_at`, `add_imprint_for_npc`. File still named `anchor.h` / `anchor.cpp` (file rename deferred). |
| Implant | `Hackable`, `implant_*` | Code rename deferred |
| Tether (skill) | new `Bind*` perk identifiers | New code follows new vocab |
| Jack in / Jack out | `jack_in`, `jack_out` | Code rename deferred |
| Cache (DataNode) | `GridTile::DataNode` | Code rename deferred |
| Sealed Cache (EncryptedFile) | `GridTile::EncryptedFile` | Code rename deferred |
| Inner Gate (DeepGridGateway) | `GridTile::DeepGridGateway` | Code rename deferred |
| Beacon (WarpAnchor) | `GridTile::WarpAnchor` | Code rename deferred |
| (corpse jack-in — no special name) | new `imprint_sector_*` files | Per-corpse mini-sector; framed as another jack-in |

The deferred rename is large but mechanical; tracked as its own follow-up task.

---

## 7. The first spec — Spine (Spec 1)

**Sub-projects covered:** 1 (Imprints), 8 (XP), and a new "Combat" topic that unifies ICE encounters, Imprint takedowns, exploit-as-weapon mechanics, and status-persistence-past-jack-out.

**Resolved during the spine brainstorm** (settled in Spec 1, not re-litigated here): Imprint entity model and lifecycle, "fully vulnerable" semantics, Imprint regen rules (none — damage persists), ICE vs Imprint distinction, combat-time exploit vocabulary (Echo, Lag, Veil, Jitter, Shroud, Worm, Brick, Rot, Spike), XP allocation (standard pool, no separate Cat_Drift track), how much of Plan 7 retires (dormant), time dilation behavior (unchanged from Plan 6), Site walkability (no separate combat-dip view).

---

## 8. Cross-references

- **Lore (full story arc):** `../lore/overview.md`
- **Spec 1 (Imprints + combat + XP):** `relay-spine.md`
- **Mechanics:** `../design/mechanics.md`
- **Items:** `../design/items.md`
- **Roadmap:** `../design/roadmap.md`

---

## 9. Status

Living manifesto. Spec 1 follows. Subsequent specs land in the dependency order in §4.
