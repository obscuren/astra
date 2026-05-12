# Astra — Documentation Index

Living catalog of every design, narrative, and technical doc in the project. **All docs live in subfolders;** this README is the only file at `docs/` root.

> Player-facing reference lives on the [Astra Wiki](https://github.com/obscuren/astra/wiki). This `docs/` tree is dev/design material; the wiki is for active players. Catalog pages on the wiki are regenerated from `src/` via [`tools/wiki/generate.py`](../tools/wiki/README.md).

---

## Folder layout

| Folder | Purpose |
|---|---|
| [`design/`](design/) | Game design, mechanics, item catalog, crafting, roadmap |
| [`lore/`](lore/) | World-building, story arc, narrative voice, procedural history |
| [`technical/`](technical/) | Code-side architecture, generators, system reference |
| [`specs/`](specs/) | Active design specs (in-flight work; deleted after ship) |
| [`ideas/`](ideas/) | Loose concepts not yet promoted to specs |

---

## design/ — game design & mechanics

How the game works, in numbers and rules.

- [`mechanics.md`](design/mechanics.md) — formulas, derived stats, combat / skill / wayfinding rules, status effects, hacking flow. **Source of truth for game rules.**
- [`items.md`](design/items.md) — full item catalog (weapons, armor, cells, mods, consumables, schematics). Player- and dev-facing reference.
- [`tinkering.md`](design/tinkering.md) — crafting system. Materials catalog, refinement, synthesis, schematic-based crafting.
- [`quest-system.md`](design/quest-system.md) — quest system architecture and content design notes.
- [`roadmap.md`](design/roadmap.md) — what's shipped, what's planned. Top-level feature-tracking. Updated as work lands.
- [`description-style.md`](design/description-style.md) — item description styling convention: Passive/Active/Trigger tags, inline color rules, helper functions, wrap renderer notes.

## lore/ — world & narrative

What the world *is*, in story terms. Not gameplay rules — those go in `design/`.

- [`overview.md`](lore/overview.md) — the full story arc: Heavens Above, factions, the Relay Network, the Substrate, pre-civilizations, Sgr A*, the Drifter profession, open mysteries, tone notes for writers.
- [`world-generation.md`](lore/world-generation.md) — design vision for procedurally generated galactic history (billions of years of layered civilizations driving the world's content).

## technical/ — code architecture & systems

Reference docs for systems future-Claude (or future-you) will need to understand or extend.

- [`poi-generators.md`](technical/poi-generators.md) — living catalog of every POI generator in the detail-map pipeline (variants, selection, stamping).
- [`animation-system.md`](technical/animation-system.md) — visual animation layer (fixture loops + one-shot effects). Architecture, API, animation library.
- [`scenario-graph.md`](technical/scenario-graph.md) — long-term architectural vision for in-code narrative orchestration (events, effects, scenarios).

## specs/ — active design specs

In-flight design work. **Specs are transient — they get deleted once the feature ships and the persistent docs (lore / design / technical) have absorbed anything load-bearing.** New specs land here from brainstorming sessions.

- [`relay-manifesto.md`](specs/relay-manifesto.md) — eight-sub-project roadmap for the Relay Network combat & exploration loop.
- [`relay-spine.md`](specs/relay-spine.md) — Spec 1: NPC Imprints, Relay combat, Drifter XP. The first sub-project under the manifesto.

## ideas/ — pre-spec concepts

Sketched-but-not-committed feature ideas. Promote to a spec when the idea is ready to design properly; delete or archive if the idea is dropped.

- [`interactive-shelves.md`](ideas/interactive-shelves.md) — 3-tile lootable shelf fixtures for settlement interiors.

---

## Conventions

- **Filenames** are kebab-case. No dates in filenames; git history carries that.
- **Cross-references** use relative paths from the doc's own folder (e.g. from `design/items.md`: `[mechanics.md](mechanics.md)` for siblings, `[../lore/overview.md](../lore/overview.md)` for the lore folder).
- **Source-tree references** use `../../src/foo.cpp` from any subfolder doc.
- **Living docs** (mechanics, items, tinkering, lore, etc.) are *updated*, not duplicated. When a feature ships, its persistent details go into the appropriate living doc; the spec is then deleted.
- **Specs cycle quickly.** A folder with stale specs is worse than no folder. After ship → absorb into living docs → delete the spec.
- **Ideas are not promises.** Anything in `ideas/` may be dropped without ceremony.
