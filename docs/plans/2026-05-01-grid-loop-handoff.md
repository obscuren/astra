# Grid loop — session handoff (2026-05-01)

Plan 4 (D-layer persistence) is **merged to main**. This document
captures the design conversation that came out of playtesting and the
proposed plan ordering going forward.

---

## 1. State

* Plan 4 is merged. Tip of `main`: `5106703 fix(dev): :learn-schem
  matches underscored exe filenames`. The 22 commits added on top of
  `978a6b6` cover Tasks 1–14 plus the post-T14 playtest fixes.
* The `feature/hacking-deep-grid` branch was fast-forwarded into main
  and can be deleted at any time. Tag `pre-cleanup-2026-05-01` points
  at the original (32-commit) tip if you want to recover anything.
* Save schema: `v59`.
* Build is clean with `-DDEV=ON`. Dev Commander seeds the full hacking
  loadout on every new game (skills, deck, implant, programs, all
  equipped).
* Plan 4.5 (small stitching pass) was **skipped** by the user. The
  items it would have done are absorbed into Plan 5 (LAN) below.

---

## 2. The "stitching" gaps Plan 4 left behind

These are bugs, not features — Plan 4 introduced concepts that don't
fully connect end-to-end yet. All are scoped into Plan 5 (LAN redesign)
since the LAN restructure forces the architectural changes they need.

| Gap | Where | Impact |
|---|---|---|
| Deep-Grid sector unreachable | `grid_netmap_widget.cpp` lock predicate + `grid_input.cpp:59` traversal | You can never enter the deep-Grid today |
| `HackingSystem::jack_in` ignores saved base | `hacking_system.cpp` always calls `make_consciousness_anchor_sector()` | The hand-authored 30x20 player base is generated, persisted, but never rendered |
| Galaxy reseed deferred | `RebirthSequence::apply` returns to MainMenu | Rebirth doesn't actually loop into a new galaxy; needs `Game::start_new_galaxy` |
| Lore viewer stub | `pda_hacking_tab.cpp::hack_term_cmd_lore` says "no decrypted archives (Plan 3+)" | `consciousness.dat.lore_archive` is being populated, never read back |
| AI contacts unwritten | `ConsciousnessSave::ai_contacts` schema field | No code path adds entries |
| Sgr A\* in-world trigger | nothing wired | `:rebirth` is the only way; no warp UI hooks Sgr A\* (system id 0) |
| Multi-Gateway-per-sector ambiguity | `program_effects.cpp::apply_breach_grid` cracks "first locked edge" of current node, not the edge for *this* gateway tile | Latent bug — only safe today because regional sectors place at most one gateway tile |
| Fixture-menu vs netmap asymmetry | `game_input.cpp:724` — only PrecursorConsole offers in-world "Jack In" | All other Hackables can only be jacked via the netmap; the fixture menu hides the option |

---

## 3. Plan ordering going forward

Plans renumbered after the user decided LAN should land before HUD:

> 1. **Plan 5 — Grid expansion and change** (LAN redesign; was Plan 6).
>    The next plan. Replaces the prior "Plan 6 — LAN subnets / content
>    expansion" roadmap line and absorbs the stitching gaps from §2.
> 2. **Plan 6 — UI / Grid HUD redesign** (was Plan 5). Designed against
>    the final LAN gameplay state.
> 3. **Plan 7 — Darknet content**. Placeholder.

**Why Plan 5 (LAN) before Plan 6 (HUD):** the HUD redesign surfaces
gameplay state (RAM/Trace/Heat, ICE legend, breach indicator,
breakpoint timeline). Plan 5 fundamentally changes what state the
player is tracking (LAN hop indicator, topology breadcrumb, deep-Grid
breach progress, lore archive sync). Designing HUD on top of the final
gameplay shape is cheaper than redesigning HUD twice.

### Plan 5 — Grid expansion and change (LAN redesign — the user's design, refined)

The user proposed:

> Any hackable node in a current map is the LAN.
>
> Maybe they represent a small walled structure on a map or they
> represent a gateway node. They are dynamically added to the LAN grid
> network (so we can map each room to gateway and so it makes sense
> in the real world).
>
> Each LAN has a deep-Grid network node which warps us to the deep
> Grid that ties everything together.

#### Design pillar — "anything electrical can be hacked"

This is the **non-negotiable foundation** of Plan 5. Every other piece
hangs off it:

* If a fixture in the world is electrical, it has a `HackTagMask` that
  includes the `Electronic` tag.
* If a fixture has the `Electronic` tag, it auto-registers in the
  current map's LAN graph the moment the map generates.
* If a fixture is in the LAN, the netmap can reach it and an
  appropriately-tagged quickhack/program can act on it.

There are **no special "hackable types"**. The current `DeviceKind`
enum (Turret/Camera/Door/PowerConduit/PrecursorConsole) becomes either
display-only or retired entirely; it must not gate any gameplay
filter. The 5-kind universe is the wrong shape — anything from a
vending machine to a holo-display to a ship subsystem to an NPC's
neural implant should be reachable through the same machinery the
moment it gets the right tag.

**Concrete fixture audit Plan 5 has to do.** Every fixture in the
world needs a tag pass. A non-exhaustive starter list — Plan 5 will
need to walk every `FixtureType` and `make_*_fixture` to make sure
this is complete:

| Fixture | Tags |
|---|---|
| Turret | `Electronic, HasOptics, Weaponized` |
| Camera | `Electronic, HasOptics` |
| Door (electronic lock) | `Electronic, Locked` |
| PowerConduit | `Electronic, PowerNode` |
| PrecursorConsole | `Electronic, DataStore, AlienTech` |
| Light fixture | `Electronic, PowerNode` *(blackout-able)* |
| Vending machine | `Electronic, DataStore` *(data_leech-able)* |
| Holo-display | `Electronic, HasOptics` *(reboot_optics-able)* |
| Ship subsystem console | `Electronic, DataStore` |
| NPC cybernetic implant | `Electronic, Mobile` *(carrier-on-NPC variant)* |
| Lift / elevator panel | `Electronic, Locked` |
| Sealed loot crate (electronic) | `Electronic, Locked, DataStore` |

Auditing the existing world reveals that most of these fixtures
already exist in code, they just don't expose any hackable surface
today. Plan 5's job is to add the tag, not the fixture.

**Implication for Plan 5's scope.** This is bigger than "refactor the
filter mechanism" — it's a content audit + a refactor + a design
pillar that future fixtures must conform to. The migration table that
maps the existing 5 `DeviceKind`s into tag sets is the *minimum*; the
real work is identifying every electrical thing in the world and
giving it the right tags.

Refined model captured in conversation. **The LAN and the netmap are
two different things — don't conflate them**:

```
world map (where @ walks)
   │
   │  jack in (only via fixtures tagged JackInPort —
   │           consoles, ship terminals, command panels)
   ▼
LAN sector ── a NEW generated grid map, its own designed cyberspace
   │          geometry; NOT a mirror of the world map
   │
   ├── Gateway tile  → Turret.5,3   subnet  (per-device subnet)
   ├── Gateway tile  → Camera.7,1   subnet
   ├── Gateway tile  → Console.12,4 subnet
   └── Deep-Grid Gateway → universal deep-Grid (only on connected LANs)
```

* **A LAN = the set of Hackables on the current world map.** Every
  station, asteroid, ruin, *and dungeon with electronics* gets its
  own LAN graph, populated as Hackables generate.
* **The LAN sector is its own designed cyberspace.** When you jack in
  you enter a generated grid map specific to that network's topology
  — neither a real-world mirror nor a list view. Inside the LAN you
  walk around as `@` and physically reach Gateway tiles; each one is
  the access point to a specific device's sub-sector. The LAN
  geometry is generated procedurally from a per-map seed.
* **LAN persistence.** Each map's LAN sector is persisted: jack in,
  walk around, jack out, jack back in — same geometry, same state.
  Save/load preserves it.
* **Jack-in entry points are tag-gated.** Only fixtures with the
  `JackInPort` tag (ship terminals, command consoles, Precursor
  consoles, similar) act as portals into the LAN. Turrets, cameras,
  conduits, doors are *in* the LAN as device subnets but cannot be
  the player's entry from the world side. Quickhacks (`.qh`) still
  work on any tagged fixture from the world without needing a portal.
* **Two LAN flavours: connected and isolated.** A connected LAN has a
  Deep-Grid Gateway that warps you (after breach) into the universal
  deep-Grid. An isolated LAN has no such gateway — *probably the
  default for dungeon LANs* (still a design question; see below).
* **The netmap (PDA widget) is the abstract graph view, separate from
  the LAN sector geometry.** It's a logical organizational diagram of
  "what's in this LAN" — auto-laid-out for readability — not a
  spatial preview of the LAN sector. Walking the LAN sector is the
  *spatial* navigation; the netmap is the *logical* navigation.
* **Deep-Grid is universal** — *"the internet of the universe"*. Not
  just the player's base; ties together connected LANs across all
  galaxies. One anchor per consciousness; persists across rebirths.
* **Regional darknet stays** as a future content concept — *"maybe for
  expansion. No idea what this is yet but it's something we will
  build"*. Demoted from "the only multi-room sector" to "a special
  kind of LAN we'll figure out later".

#### Open design questions (settle before drafting Plan 5 spec)

1. **Does the regional darknet (28x14 multi-room sector) survive in
   any form once LANs land?** Three options:
   * Retire it. Each Hackable jacks into its own subnet sector reached
     from the LAN sector via Gateway tiles.
   * Keep as a LAN-graph "archive" sector reached via one specific
     Gateway-kind node.
   * Demote to quest-flavoured Precursor LANs only.

   *User signaled keeping the darknet for future expansion, but didn't
   pick which of these three forms it takes.*

2. **Which LAN flavours have a Deep-Grid Gateway?** User signaled that
   dungeon LANs are "perhaps LAN-only — no access to the deep grid".
   Need to decide:
   * Always station/asteroid LANs are connected; always dungeon LANs
     are isolated? (clean rule)
   * Tier-driven? (low-tier networks are isolated, high-tier
     connected)
   * Per-LAN content authoring choice?
   * Strictly one Deep-Grid Gateway per connected LAN, or can some
     have multiple?

3. **What devices count as `JackInPort`?** Confirmed: consoles, ship
   terminals, command panels. Need to enumerate the existing fixtures
   and decide:
   * Every Precursor console → yes
   * Ship cockpit terminal → yes (probably)
   * Maintenance panels → ?
   * Sealed loot crates with electronic locks → no (cracked from the
     outside via `.qh`, not entered)

4. **What ties LANs together inside the deep-Grid?** Are there
   LAN-bridges (AI contacts you've befriended become navigation
   shortcuts), or is the deep-Grid just your private base?

   *User answered: deep-Grid is "the internet of the universe" — so
   yes, the deep-Grid contains structure tying connected LANs
   together. Concrete shape still to design.*

5. **Inside-sector traversal semantics** — when you're in a device
   subnet and step on a Gateway tile, does it (a) take you back to
   the LAN sector you came from (most common case), (b) hop sideways
   to a different device subnet (skip the LAN sector), or (c) escape
   to the deep-Grid? Probably (a) by default, with rare (b)/(c) for
   special tile flavours.

6. **Multi-Gateway-per-sector encoding** — each Gateway tile must
   carry a target node id (the LAN sector has many, each pointing to
   a different device subnet). Confirms the latent bug in
   `program_effects.cpp::apply_breach_grid` needs fixing — breach is
   currently spatial-position-blind.

7. **LAN sector generation** — procedural per-map from a seed (each
   station / asteroid / ruin / electrical dungeon rolls its own
   geometry). Need to decide on the generator's shape: BSP rooms with
   gateway tiles distributed? Hand-shaped variants like the current
   regional darknet? Map-flavour-specific (station = gridded,
   dungeon = chaotic)?

#### Flavour pointers — make the LAN sector feel like a place

Optional polish ideas captured during the design chat. Save for the
Plan 5 spec brainstorm or sprinkle into the implementation as you go:

* **Visual identity per LAN flavour.** Same generator, different
  palettes + room-shape weights:
  * Station LAN — corporate (gridded layout, sterile glyphs, cool
    palette).
  * Dungeon LAN — rotted (sparse rooms, broken edges, dim palette).
  * Precursor / alien LAN — uncanny (curved lines, non-Latin
    glyphs, off-spectrum colours).
  Small effort, big mood payoff. Three palette presets switched on
  the LAN's owner / origin tag.
* **ICE personality per LAN flavour.** Reuse the existing white/gray/
  black ICE machinery, change patrol/aggro params:
  * Station ICE — security drones: cardinal patrol, escalate at
    breakpoints.
  * Dungeon ICE — rogue daemons: territorial, ambush behaviour.
  * Precursor ICE — immune-system cells: swarm, blind-target.
* **LAN sub-rooms as logical zones.** Corporate LAN sub-rooms could
  carry thematic labels (`accounting`, `security`, `R&D`, `HR`),
  each gating a device cluster. Gives the netmap meaningful node
  groupings (`[accounting.subnet]`) and lets the player plan a route
  by intent ("what am I after?") instead of by closest gateway tile.
* **Hot-pursuit between sectors.** If Trace exceeds a threshold when
  the player walks through a Gateway, ICE follow them across into
  the device subnet (or back into the LAN). Makes the LAN feel like
  a single contiguous place even though it's structured as multiple
  sectors.
* **Persistent LAN damage across visits.** Firewall tiles you
  breached, encrypted files you decrypted, gateways you cracked all
  stay in their cracked / opened state on subsequent visits. Repeat
  raids on the same LAN feel different, gives the player a reason to
  come back to known LANs as their build improves.
* **Public terminal / kiosk LANs.** Small open LANs with low-stakes
  content, intended as safe practice grounds — vending machines, info
  kiosks, ad-displays. New-player onboarding plus an excuse for
  ambient hacking that doesn't risk a full station-wide alarm.

#### Architectural changes the redesign forces

* **GridNetwork** restructure: nodes per-Hackable (auto-registered from
  any `Electronic`-tagged fixture), edges reflect logical connectivity
  inside the LAN, plus one node + sector for the LAN itself.
* **LAN sector generator** — new generator that takes a per-map seed
  and produces a designed cyberspace grid with Gateway tiles
  distributed across it. One sector per LAN, persisted in the save.
* **Sector traversal** mid-jack-in — closing/reopening sectors without
  full jack-out. Drops the "Already jacked in" guard for internal hops.
  Used both for LAN ↔ device-subnet transitions and LAN → deep-Grid.
* **Netmap-side breach UX** — `b` key on a locked edge runs `breach.exe`
  cost from outside the sector. Required for LANs because not every
  device needs to be entered to reach others.
* **Lore archive viewer + AI contacts** — both are LAN-flavoured (lore
  lives on archive subnets, AI contacts are special LAN nodes). Both
  are rendered in the netmap.
* **Self-anchor entry** — netmap Enter on a node where
  `owned_by_consciousness_id == cs.consciousness_id` bypasses the lock
  predicate.
* **Sector-source dispatch** in `HackingSystem::jack_in` — for
  `DeepGridAnchor` nodes, prefer `consciousness.dat.deep_grid_base`
  when the player owns the anchor; fall back to
  `make_consciousness_anchor_sector()` otherwise.
* **Galaxy reseed in `RebirthSequence::apply`** — replace the
  return-to-MainMenu shortcut with a real `Game::start_new_galaxy`
  that re-runs the new-game pipeline with a fresh seed and re-applies
  `consciousness.dat`.

#### Tag-based capability model — "anything electronic is in the LAN"

User raised this directly: the current per-fixture `Hackable::available_qh`
whitelist is the wrong shape. We have two parallel sources of truth
(`Hackable::available_qh` and `ProgramDef::target_filter`) and they
already disagree (e.g. `data_leech` accepts Camera per its filter, but
Camera's `available_qh` doesn't list it). Worse, every new program
requires touching every fixture's `available_qh` to opt in.

`Hackable::available_qh` is also dead state today — populated in
`make_hackable`, persisted to saves, but never consumed by gameplay
code. The actual gate is `ProgramDef::target_filter`.

The Plan 5 fix:

* **Fixtures carry capability tags, not kinds.** Replace
  `DeviceKind` matching with a `HackTagMask` on each `Hackable`:
  `Electronic`, `HasOptics`, `Locked`, `PowerNode`, `DataStore`,
  `Weaponized`, `Mobile`, `AlienTech`, etc.
* **Programs declare required capabilities.** `ProgramDef::target_filter`
  changes from `vector<DeviceKind>` to `vector<HackTagMask>` — the
  device must satisfy at least one mask in the list (which gives both
  AND semantics within a mask and OR semantics across the list).

  ```cpp
  reboot_optics  → {HasOptics}                  // any optics
  bypass_lock    → {Locked | Electronic}        // locked AND electronic
  blackout       → {PowerNode}
  data_leech     → {DataStore}, {Electronic}    // store OR generic node
  friendly_fire  → {Weaponized | Mobile}        // weaponised AND mobile
  ```

* **Anything `Electronic` joins the LAN automatically** — no per-device
  registration code, no fixture whitelist. Map gen sets tags; LAN
  registration sweeps the map for `Electronic` fixtures.
* **`available_qh` is deleted** along with its save serializer; the
  per-program filter becomes the canonical answer.
* **`DeviceKind` either retires** (replaced by display name + dominant
  tag) or **demotes to a display-only enum** that drives glyphs and
  human-readable labels but no longer gates anything.

##### Open decisions before this lands

1. **Match semantics confirmation** — proposed shape is
   `vector<HackTagMask>` on a program; device satisfies if it covers
   at least one mask. Tags within a mask are ANDed; masks across the
   vector are ORed. Confirm or pick a different scheme.
2. **Tag persistence** — persist the mask on `Hackable` (so cracked-open
   doors mutate their `Locked` tag at runtime), or always re-derive
   from fixture type? My instinct is persist.
3. **`DeviceKind` survival** — retire entirely, or keep as a
   display-only label?
4. **LAN visibility filter** — every `Electronic` fixture becomes a
   netmap node, or do some need explicit `LanVisible` to opt in
   (so e.g. a hacked-open vending machine doesn't clutter the map)?
5. **Migration** — one mapping table converts the existing 5
   `DeviceKind`s into tag sets. Single commit, no save migration since
   `available_qh` was unread anyway.

##### Side effects this enables for free

* New fixtures (vending machines, holo-displays, lifts, NPC implants,
  ship systems) become hackable just by tagging — no per-program edits.
* New programs ship without touching fixture defs — the program's
  required-tag list is its full contract.
* Door + PowerConduit go from "two empty `available_qh` lists" to
  proper participation: `bypass_lock` and `blackout` ship with their
  tag requirements and immediately work on every fixture that carries
  the matching tag.

This refactor is *the* unifying piece that makes "any electronic thing
is in the LAN" actually work. It belongs in Plan 5 alongside the LAN
graph restructure.

### Plan 6 — UI / Grid HUD redesign (after Plan 5)

Existing roadmap line stands; design will be done with the final LAN
gameplay state in view.

### Plan 7 — Darknet content

Placeholder. The user mentioned wanting darknet to mean *something*,
"no idea what this is yet but it's something we will build". To be
specced when Plan 5 is done.

---

## 4. Files / paths a fresh agent will want

* Plan 4 spec: `docs/superpowers/specs/2026-04-30-hacking-deep-grid-design.md`
* Plan 4 task list: `docs/superpowers/plans/2026-04-30-hacking-deep-grid.md`
* Plan 3 spec (still relevant for game flow): `docs/superpowers/specs/2026-04-29-hacking-design.md`
* Mechanics doc — Hacking sections: `docs/mechanics.md` (search "Hacking")
* Items doc — Cyberdecks/Programs/Implants: `docs/items.md`
* Roadmap entry for Plan 4 + deferred follow-ups: `docs/roadmap.md` (search "Plan 4 (D-layer")

Source of truth for the surfaces Plan 5 will touch:

* `include/astra/grid_network.h` — node + edge types (will be reshaped)
* `src/grid_network.cpp` — registration helpers + layout policy (will be replaced)
* `include/astra/hackable.h` — `DeviceKind` + `available_qh` (both reworked into tags)
* `src/hackable.cpp` — `make_hackable` (tag assignment lands here)
* `src/program.cpp` — `ProgramDef::target_filter` (becomes `vector<HackTagMask>`)
* `src/program_effects.cpp::apply_breach_grid` — multi-gateway-per-sector latent bug
* `src/hacking_system.cpp::jack_in` — sector dispatch + traversal mid-jack-in
* `src/grid_input.cpp` — in-sector tile interactions, including the
  deferred Gateway traversal
* `src/grid_netmap_widget.cpp` — netmap rendering (geometry-mirrored layout)
* `src/grid_sector.cpp` — `gen_subnet_sector` (8x8) and
  `gen_regional_sector` (28x14, three hand-shaped variants)
* `src/grid_anchor_layout.cpp` — `make_consciousness_anchor_sector`
  (14x10) and `make_player_deep_grid_base` (30x20)
* `src/rebirth_sequence.cpp` — modal + cinematic + apply (galaxy reseed)
* `src/soul_mirror.cpp` — channel runtime + HUD strip
* `src/game_input.cpp:724` — fixture-menu Hackable interaction (currently
  hardcodes "Jack In" to PrecursorConsole only)

---

## 5. Memory context worth carrying forward

* User wants the netmap layout to mirror world geometry (a Hackable in
  the NW corner of the map shows in the NW of the netmap). This is
  the answer to "netmap doesn't feel right" feedback from Plan 4.
* `Tab` toggles the Equipment / Implant paper-doll view inside the
  Equipment tab. The hint sits at the top-left of the paper-doll pane.
  (We tried using `i` for the toggle and reverting Tab to cross-pane;
  the user reverted that — Tab toggling the paper-doll *is* the
  current and intended behaviour.)
* User prefers terse responses with no trailing summaries.
* No backwards-compat shims pre-ship — bumping save versions is fine,
  rejecting old saves is fine.
* Always build with `-DDEV=ON`.
* Don't auto-push; merge feature branches frequently but not without
  user verification.
* Squash fix-upon-fix chains before merging (this branch was squashed
  32→22 commits before the FF merge into main).
* Session-handoff prompt files (this one) stay untracked.
