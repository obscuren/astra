# Plan 7+ Roadmap — Sub-project Map

**Date:** 2026-05-03
**Status:** Planning. Plan 6 just shipped; Plans 7–11 are the forward arc.
**Audience:** Future sessions picking up the Hacking & Grid work.

---

## 1. What's already shipped

| Plan | Topic | Status |
|---|---|---|
| 3 | A-layer / The Grid (initial jack-in lifecycle) | ✅ Done |
| 4 | D-layer / Deep-Grid persistence (`consciousness.dat`, Soul Mirror, T3 programs) | ✅ Done |
| 5 | Grid expansion + LAN redesign (tag-driven hacking, procedural LAN sectors, deep-Grid base, WarpAnchors, nmap/ping/jack/lore CLI) | ✅ Done |
| 5.5 | Multi-map LAN persistence (per-`LocationKey` LAN bucket; save schema v62) | ✅ Done |
| 6 | Grid HUD redesign (Tron 70%×70% overlay; Telegraph-driven targeting; isolated log) | ✅ Done |

---

## 2. Plans 7–11 — sub-project map

The remaining hacking work is too big for a single plan. It splits into five named sub-projects. Plans 7 and 8 are independent and can ship in parallel; Plan 9 depends on Plan 8.

```
Plan 5 ─┬─→ Plan 6 ─┬─→ Plan 7 (Device Shells) ──────────────────┐
        │           │                                              │
        │           ├─→ Plan 8 (Grid Layout) ──→ Plan 9 (Anchor) ─┤
        │           │                                              ├─→ Plan 11 (Polish)
        │           ├─→ Plan 10 (Galaxy Survival) ────────────────┘
        │           │
        └───────────┘ (already ✅)
```

### Plan 7 — Device Shells

**Spec:** `docs/superpowers/specs/2026-05-01-device-shells-design.md` (~700 lines, complete).

**Requires** brainstorming session to figure out the exact scope and playstyle. Maybe merge interely in to one single hacking style or keep as two.

**Concept.** Every electronic device runs a fake mini-OS the player can drop a CLI shell into. Tags (Plan 5's `HackTagMask`) determine the command set; `FixtureType` provides the OS banner; faction provides flavor. Two doorways: real-world (walk up + wired) and in-Grid (`ssh root@<ip>` + pivots). Two privilege tiers: guest (free) and root (escalated via `hashcat` long-channel). Privileged commands are long-channels with inline progress bars; reads are instant.

**Why it matters.** Doubles the playstyle space — the **Sysadmin** pivots through the LAN's logical graph without ever seeing an ICE; the **Netrunner** walks the spatial sector and engages ICE the Plan 5 way. Same gear, same skills, different rhythm.

**Subsumes** (drop-or-fold from the old Plan 7 list):
- nmap-from-Grid binding (`m` key) — **dropped**; `nmap` is a shell command.
- Help overlay / `?` key — **dropped**; in-shell `help` + `<cmd> --help` self-document.
- Lore-archive viewer redesign — **dropped**; `cat /var/log/lore-XX.txt` reads are filesystem.
- Hostile-QH owner-suppression model — **mostly subsumed**; shells use guest/root auth.
- PrecursorConsole `AlienTech` variant on map-gen — **folded in**; AlienTech tag lights `decode`/`mirror`/`query` shell commands.
- F2/F3/F4 right-pane content — **reduced**; shell becomes the deep-info surface.

**Prereqs.** ✅ Plan 5 tags, ✅ Plan 6 HUD. Ready to plan.

**Estimated scope.** Comparable to Plan 5 — 6–8 cuts, ~40 tasks. New subsystems: `HackCommand` registry, `DeviceShell`, `HackChannel`, `HackFlavor` packs (5 factions), `DeviceFsView`. Three new skill nodes (`PivotMaster`, `ColdHands`, `RootKit`). Save-schema bump.

**Independent of Plans 8, 9, 10.**

---

### Plan 8 — Grid Layout / Generator changes

**Spec:** *TBD — needs brainstorming session.*

**Concept (placeholder).** Reshape how LAN/subnet sectors are generated so cyberspace reads more clearly to the player. The current Plan 5 generator (firewall ring + organic offices + A* connectors) is functional but the user has indicated the visual/spatial idiom needs revisiting before more content lands on top of it.

**Why before Plan 9.** The Anchor's interior geometry should fit the new layout idiom — designing the Anchor against the *current* generator and then re-laying-it-out after Plan 8 is wasted work.

**Prereqs.** ✅ Plan 5, ✅ Plan 6.

**Independent of Plan 7** — shells touch fixture commands, not sector geometry. The two can ship in parallel without conflicts.

**Estimated scope.** Unknown until brainstormed. Likely smaller than Plan 7 (single-system change vs new subsystem stack), but depends on how ambitious the redesign is.

---

### Plan 9 — Your.Anchor v2 + AI contacts (as Grid dialogs)

**Spec:** *TBD — needs brainstorming session, after Plan 8 ships.*

**Concept.** Expand the deep-Grid Anchor from its v1 spawn-hub-with-lore-DataNode form into a fully inhabited base:

- **Personal stash.** A dedicated container in the Anchor that survives rebirth.
- **Customisation.** Place fixtures inside the Anchor (decorative or functional).
- **AI-contact locus.** AI characters live in the Anchor (and possibly Atlas) as **walkable Grid avatars** with `DialogManager`-driven conversation — *not* shell endpoints. Same interaction idiom as world NPCs, just rendered Tron-themed inside the Grid HUD overlay.
- **Expanded ownership.** Per-region ACLs.
- **Rebirth-survivor identity surface.** What survives Sgr A* — rendered diegetically in the Anchor.

**Folds in:** AI contacts UI (schema is already populated in Plan 5; just needs the dialog surface).

**Architecture choice (settled 2026-05-03):** AI contacts are **people**, not systems. They use `DialogManager` (Plan 6 already routes its dialogs through the system; we just need a Grid-themed render path) — not the device-shell CLI. Forcing AI dialogue through `ssh` flattens personality and conflates two diegetic registers.

**Prereqs.** Plan 8 (layout). DialogManager already exists; needs Grid-themed rendering inside the Tron window.

---

### Plan 10 — Galaxy Survival Loop

**Concept.** Make the inter-galaxy loop diegetic and interactive instead of dev-command-only.

- **Sgr A\* in-world warp trigger.** Navigate ship to system id 0 → fires `RebirthSequence::begin()` instead of rejecting. (Currently `:rebirth` is the only path.)
- **WarpAnchor traversal.** Step on `◉` → physically warp the `@` avatar to the destination station (currently just logs LAN identity).
- **`galaxy_id` save round-trip.** Currently `World::galaxy_id_` resets to 0 on save/reload — fine while it's not player-visible, but Plan 10 surfaces it.
- **Past-galaxy memorial surface.** Atlas entries from purged galaxies stay visible as memorials. Cleanup / eviction policy when the Atlas fills (>200 anchors).

**Prereqs.** Multi-map LAN persistence (✅ Plan 5.5). Otherwise independent.

**Estimated scope.** Medium — touches navigation, rebirth, and Atlas rendering, but no new subsystems.

**Independent of Plans 7, 8, 9.** Could slot in as a parallel side-track.

---

### Plan 11 — Content + Polish

Opportunistic; pick items up as adjacent code is touched.

- **Frontier zone content** (the deep-Grid's east region — currently firewalled empty Floor). Darknet content: hidden terminals, contraband programs, AI black-market contacts, off-grid-only lore.
- **Real-body damage / meatspace vulnerability while jacked.** Design decision, not UI.
- **NPC death tombstones cleanup.** `WorldManager::on_hackable_removed` leaves dead-NPC Subnet nodes in `GridNetwork` as `[removed]` tombstones. Either purge them or add a `tombstoned` flag for netmap filtering.
- **Residual hostile-QH owner model.** If Plan 7 doesn't fully subsume the `MapType::Starship` heuristic for non-shell QHs, add the proper `ProgramDef::hostile` + `Hackable::owned_by_player` data model.
- **F2/F3/F4 right-pane content.** Whatever survives Plan 7's reduction — possibly a tiny "channel-in-progress" indicator.

---

## 3. Recommended order

The dependency graph allows several valid orderings. Two reasonable picks:

### Option A — Big systems first

```
Plan 7 (Device Shells)  →  Plan 8 (Layout)  →  Plan 9 (Anchor)  →  Plan 10  →  Plan 11
```

Front-loads the heaviest interaction-model change. Plan 9 (Anchor) lands last in the sequence so it benefits from both shells *and* the new layout.

### Option B — World first, systems later (recommended)

```
Plan 8 (Layout)  →  Plan 7 (Device Shells)  →  Plan 9 (Anchor)  →  Plan 10  →  Plan 11
```

Get the Grid *looking* right first (8), then deepen the systems layer (7), then populate the people layer (9). The user's hint — "if we change the Grid World, it's going to make more sense to the players" — leans this way.

### Parallel option

Plans 7 and 8 can run on parallel branches (no shared blast radius). Plan 10 can also run in parallel with any of them. Plan 9 is the only hard dependency (waits on 8).

---

## 4. Pointers for the next session

- **Plan 7 spec is done.** Skip straight to brainstorming the implementation plan or use `superpowers:writing-plans` against the spec directly.
- **Plan 8 needs a brainstorm pass first.** No spec exists. User has indicated the LAN/sector layout needs reshaping but hasn't named specific changes yet.
- **Plan 9 needs a brainstorm pass after Plan 8 lands.** AI-contacts-as-Grid-dialogs is the architectural call (settled 2026-05-03); the rest is open.
- **Plan 10 can be brainstormed any time** — independent of the others.
- **Plan 11 doesn't need a brainstorm** — pick items off opportunistically as adjacent code is touched.

### Cross-references

- Plan 5 spec: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- Plan 5 plan: `docs/superpowers/plans/2026-05-01-grid-expansion.md`
- Plan 5 handoff (origin of the Plan 7 carry-over list): `docs/plans/2026-05-02-plan-5-handoff.md`
- Plan 6 spec: `docs/superpowers/specs/2026-05-02-grid-hud-design.md`
- Plan 6 plan: `docs/superpowers/plans/2026-05-02-grid-hud.md`
- Plan 7 spec (Device Shells): `docs/superpowers/specs/2026-05-01-device-shells-design.md`
- Project roadmap: `docs/roadmap.md`
- Hacking mechanics: `docs/mechanics.md` (search "Hacking — Plan 5")
