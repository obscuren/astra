# Plan 5 — handoff for Plans 5.5 / 6 / 7

**Date:** 2026-05-02
**Branch:** `feature/grid-expansion` — 44 commits beyond `5106703` on `main`.
**Status:** Plan 5 functionally complete; documented carry-overs below.

This document is the source of truth for what Plan 5 left undone. It supersedes the in-spec "Cut N carry-over deltas" sections, which were written incrementally during implementation.

---

## 1. State

Plan 5 shipped across four cuts plus two sub-cuts plus Plan 5.5. Highlights:

* **Cut 1.** Tag-driven capability model (`HackTagMask` replaces `DeviceKind`); `Hackable.tags + Hackable.ip`; LAN auto-registration on every map enter; map-gen content pass via centralised `make_fixture` auto-attach (~30 fixtures across stations/asteroids/ruins now hackable); save schema v59 → v60; `consciousness.dat` v1 → v2; unified `:spawn`.
* **Cut 2.** New `GridTile`s (`Connector`, `DeepGridGateway`, `WarpAnchor`); box-drawing wall renderer (global); `LanSectorGenerator` (procedural cyberspace, scales with N); mid-jack-in sector traversal; tile-mutation persistence; multi-Gateway `apply_breach_grid` fix.
* **Cut 2.5/2.6.** Organic floor-plan layout (variable rooms, A* connector routing, inner sanctum ring); per-subnet wall-mounted device avatars (~16 themed glyphs in BrightWhite).
* **Cut 3.** 60×40 hand-authored deep-Grid base (Anchor v1 + Atlas + Frontier); owned-anchor dispatch in `jack_in`; `WarpAnchor` Atlas population on first `⊕` crack; `Game::start_new_galaxy(fresh_seed)` wired into `RebirthSequence::apply`; past-galaxy memorial flagging.
* **Cut 4.** `netmap` → `nmap` rename + IP-driven `nmap [-l|-m]`/`ping <ip>`/`jack <ip>`; `lore` reads consciousness archive; `ai_contacts` schema populated on first `⊕` crack; nmap-side `b`-key breach; self-anchor entry bypass; Atlas view in widget.
* **Plan 5.5.** Multi-map LAN persistence: `WorldManager::lan_metadatas_` keyed by `LocationKey`; `Game::on_map_loaded()` calls `switch_active_lan` instead of `lan_full_reset`; save schema v61 → v62 with per-key serialisation; `nmap` widget + `nmap -l` filter to active LAN's lan_root. Cracked / looted / decrypted / killed_ice survive cross-map round-trips.

Plus five playtest fixes during implementation (clear-order bug, netmap column-width overflow, fixture-interaction hijacking by cyber menu, hostile-QH-on-own-ship suppression, dual-DeepGridAnchor ownership stamping).

Spec: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`. Plan: `docs/superpowers/plans/2026-05-01-grid-expansion.md`.

---

## 2. Pre-Plan-6 must-do — Multi-map LAN persistence — DONE

**Status:** SHIPPED on `feature/grid-expansion` (Plan 5.5, commits `5794778..a1d6a00`). Plan 6 is unblocked.

Implementation summary: `WorldManager::lan_metadatas_` is now an `unordered_map<LocationKey, LanMetadata>` keyed by the same identity used by `location_cache_`. `Game::on_map_loaded()` calls `WorldManager::switch_active_lan(current_location_key())` which lazy-creates the bucket on first visit and is a no-op on revisit. `lan_full_reset()` is repurposed for the dev `:spawn fixture` path (touches active LAN only). Save schema bumped v61 → v62 with per-key serialisation. `nmap`/`nmap -l` widget filters by active LAN root so sibling maps' LANs stay hidden. Cracked firewalls, looted DataNodes, decrypted EncryptedFiles, killed ICE all survive cross-map round-trips.

The original design notes are kept below for archival reference.

---

**Plan 6 (UI/HUD redesign) must wait on this.** Plan 6 is designed against the final LAN gameplay shape, and right now LAN state is wiped on every map transition — that's not the final shape.

### What's broken today

`WorldManager` holds **one** `LanMetadata lan_metadata_`. `Game::on_map_loaded()` (called on `enter_ship`, `exit_ship_to_station`, `enter_detail_map`, `descend_stairs`, `transition_detail_edge`, `travel_to_destination` warp/dock/land branches, `enter_lost_detail`, `enter_maintenance_tunnels`, etc. — ~20 call sites) fires `WorldManager::lan_full_reset()` every time, which:

1. Drops every Subnet GridNode reachable from `lan_metadata_.lan_root` (via `on_hackable_removed`).
2. Drops the `lan_root` node itself.
3. Default-constructs `lan_metadata_ = LanMetadata{}` — wipes `lan_sector_state`, `subnet_states`, all room data.
4. Re-runs `register_hackables_in_lan` for the now-active map.

Behavioural consequence:

```
Crack a firewall in Heavens Above LAN → mutation recorded.
Walk to ship terminal, board ship → on_map_loaded → reset → HA's mutation lost.
Exit ship back to HA → on_map_loaded → reset → HA LAN re-registered FRESH.
Jack back into HA LAN → cracked firewall is back to firewall.
DataNode you looted is back. Decrypted file is back encrypted.
ICE you killed has respawned.
```

Save/reload preserves whatever LAN is in the slot at save time, but mid-session round-trips between maps are destructive.

### Required structural change

Replace single-LAN scope with a per-map keyed map.

**Data model:**
```cpp
// include/astra/world_manager.h
class WorldManager {
    // BEFORE
    LanMetadata lan_metadata_;
    // AFTER
    std::unordered_map<LocationKey, LanMetadata, LocationKeyHash> lan_metadatas_;
    LocationKey current_lan_key_ = {};
    LanMetadata empty_lan_;   // returned when no LAN exists for the current map (read-only)

    LanMetadata&       lan_metadata();           // returns lan_metadatas_[current_lan_key_], inserting if absent
    const LanMetadata& lan_metadata() const;     // returns by key or empty_lan_
};
```

`LocationKey` already exists in the codebase (used by `WorldManager::location_cache()` and the save layer). Reuse it. Hash function may need adding.

**Lifecycle:**
- `Game::on_map_loaded()` no longer calls `lan_full_reset`. Instead it calls a new `WorldManager::switch_active_lan(LocationKey new_key)` which:
  1. Sets `current_lan_key_ = new_key`.
  2. If `lan_metadatas_[new_key]` doesn't exist, default-constructs and runs `register_hackables_in_lan(*this, grid_network_, lan_metadatas_[new_key])`.
  3. If it exists, just refreshes the GridNetwork's runtime view of which Subnet nodes belong to the active LAN. (See "GridNetwork churn" below.)
- `lan_full_reset()` is repurposed for the dev `:spawn fixture` path — wipes JUST the active map's `LanMetadata` and re-registers, leaving other maps' LANs intact.

**GridNetwork churn:**

Currently `register_hackables_in_lan` mutates `WorldManager::grid_network_` directly. With multiple LANs, you have two options:

- **(A) Per-LAN GridNetworks.** Move `GridNetwork` from `WorldManager` to `LanMetadata`. Each LAN owns its node graph. `nmap` widget reads from the active LAN's network. Cleanest separation; biggest refactor (every `world_.grid_network()` call site needs to ask the active LAN).
- **(B) Single GridNetwork, partition by LanRoot.** Keep one GridNetwork with all LANs' nodes. The active LAN is identified by `lan_metadata_.lan_root`; nmap filters to nodes reachable from that root. The shared `DeepGridAnchor` is referenced from every connected LAN's `lan_root` via a tier-2 edge. Smaller refactor; some queries get more complex.

**Recommendation: B.** It matches the existing data shape, doesn't touch most call sites, and the shared DeepGridAnchor sits naturally as a single node referenced by edges from every connected LAN. The cost is filtering: `nmap -l` must restrict to subnets edged from the current LAN's root; auto-registration sweep must avoid stomping on inactive LANs' nodes.

The implementation hint in `register_hackables_in_lan` already partly assumes this — it walks `net.nodes()` looking for an existing DeepGridAnchor before creating one (Cut 1's `find_or_create_deep_grid_anchor`). Extending that to "look for an existing LanRoot for this LocationKey" is straightforward.

**Save schema:**

`SaveData::lan_metadata` (current) → `SaveData::lan_metadatas` (`std::unordered_map<LocationKey, LanMetadata>`). `SAVE_FILE_VERSION` bumps v60 → v61 (or v62 if Cut 2.6 already bumped to v61 — check). v60 saves get rejected per the no-backcompat rule.

`LocationKey` already serialises somewhere — reuse the existing serializer.

**Test plan:**

1. New game on Heavens Above. Run `:lan-info`. Note the cracked-state baseline.
2. Jack into HA, crack a firewall in security, loot a DataNode in ops, decrypt a file in lobby, kill an ICE.
3. Jack out. Run `:lan-info` → confirm `nodes_cracked > 0`, mutations present in `lan_sector_state`.
4. Board ship. `:lan-info` → ship's LAN, with empty mutations.
5. Disembark back to HA. `:lan-info` → HA LAN restored, all four mutations still present.
6. Re-jack into HA. The cracked firewall is still floor; the looted DataNode is gone; the decrypted file is gone; the killed ICE is absent.
7. Save + quit + reload. `:lan-info` matches step 5.
8. Repeat with three different maps in rotation (HA, ship, asteroid). Each preserves its own state independently.

**Estimated complexity:** medium. Touches WorldManager (header + impl), Game::on_map_loaded, ~20 map-enter call sites (no change needed if they all funnel through on_map_loaded), nmap widget rendering filter, save_file (de)serialisation, and `register_hackables_in_lan` (subnet collision avoidance with sibling LANs in the same network).

**Estimated commits:** 6-10 incremental. Suggested structure:
1. Add `lan_metadatas_` + `current_lan_key_` infrastructure.
2. Wire `switch_active_lan` from `on_map_loaded`. Migrate `lan_full_reset` semantics.
3. Update `register_hackables_in_lan` to scope subnet creation to the active LAN.
4. Update nmap widget + `nmap -l` to filter by active LAN root.
5. Save schema bump + serialisation.
6. Per-LAN tile-mutation persistence (verify it Just Works once metadata is per-key).
7. Smoke test the test plan above.

### Related — Ship-merges-into-docked-LAN

Once multi-map LAN persistence lands, the next layer is the ship merge: when the player is docked at a station/asteroid/etc., the ship's hackables join the host LAN as one unified graph. `nmap -l` from the ship's CommandTerminal lists everything in the host station + ship; same nmap from a station console lists ship terminals.

**Implementation hint:** `register_hackables_in_lan(world, key)` could detect "is the active map the player's ship AND is the ship docked?" — if yes, walk fixtures from BOTH the ship interior map and the host station's map, register them all under one LanRoot keyed by the host station's LocationKey. The ship's own LanMetadata stays empty/inert while docked; populated when in flight.

Detection: `WorldManager` already tracks docked vs in-flight via `world_.navigation()`. Use that.

This isn't strictly required pre-Plan-6 — multi-map LAN persistence is enough for Plan 6 to design against. Ship-merge is Plan 7 territory but worth keeping in mind during the multi-map refactor (don't paint into a corner).

---

## 3. Plan 7 carry-overs

These are content/feature items that need design and implementation in Plan 7. They are **not** blocking Plan 6 (UI) or Plan 5.5 (multi-map LAN persistence).

### 3.1 Your.Anchor full mechanics

Cut 3 ships Your.Anchor v1: a 60×40 hand-authored deep-Grid sector with an Anchor region containing a single `$` lore-archive `DataNode`. It's the player's spawn point, persisted across rebirths via `consciousness.dat`.

Plan 7 expands this to:
- **Personal stash.** A dedicated container in the Anchor that survives rebirth — holds a curated subset of items (limited slots, lore-justified).
- **Customisation.** Place fixtures inside the Anchor (decorative or functional).
- **AI-contact locus.** An ARIA-style AI lives in the Anchor; player can converse with it.
- **Expanded ownership.** Per-region ACLs: maybe friends can visit but not modify; maybe rivals can intrude in late game.
- **Rebirth-survivor identity surface.** The Anchor is the diegetic embodiment of "what survives Sgr A\*".

Spec §17 lists this; Plan 7 needs its own design doc.

### 3.2 AI contacts UI

Cut 4 populates `consciousness_save.ai_contacts` with placeholder records on every first ⊕ crack (`AiContactRecord{ id = "aria.<region-slug>", display_name = "ARIA — <region>", origin_galaxy_id }`). No UI surfaces them yet.

Plan 7 designs:
- Where AI contacts appear in the UI (PDA tab? deep-Grid Anchor avatar? netmap node?).
- What interactions they support (dialogue, quest hooks, info trades).
- Personality differentiation — ARIA on Heavens Above shouldn't read identical to ARIA at a derelict station.
- Cross-galaxy carry-over. Past-galaxy AI contacts: do they survive? If so, how?

### 3.3 Hostile-QH owner suppression — proper model

**Current placeholder:** while `world_.map().map_type() == MapType::Starship`, all QHs are hidden from the fixture-menu. This is a heuristic that breaks once multi-map LAN persistence merges the ship into the docked station's LAN (the merged LAN sector is no longer `MapType::Starship`).

**Plan 7 model:**
- Add `bool ProgramDef::hostile = true` (default). Mark `data_leech.qh`, `reboot_optics.qh`, `friendly_fire.qh` as hostile (true). Future programs that are owner-friendly (decrypt-own-data, ping-without-trace, etc.) set `hostile = false`.
- Add `bool Hackable::owned_by_player = false` (default). Set to true on:
  - Every electrical fixture inside the player's ship interior at map-gen.
  - The player's deep-Grid base hackables (when those exist — Plan 7 also adds them).
  - NPC implants the player has decrypted/befriended (future).
- The fixture-menu QH filter excludes hostile QHs against owned fixtures.

This replaces the map-type heuristic in `Game::open_hackable_menu` (around `src/game_input.cpp:725` per Cut 1's commit).

### 3.4 PrecursorConsole `AlienTech` variant on map-gen

`tags_for_fixture(FixtureType::Console)` returns base terminal tags (`Electronic | DataStore | JackInPort`). Map-gen ruin / crashed-ship generators stamp Console fixtures via the centralised `make_fixture` auto-attach, but don't add the `AlienTech` overlay. So Soul Mirror / lore-archive paths silently skip in-world Precursor consoles.

The dev `:spawn-hackable console` path (now retired) had a special branch that stamped `AlienTech` and seeded lore fragments. The new `:spawn fixture Console` doesn't.

**Fix locations:**
- `src/generators/ruin_generator.cpp` (or `ruin_stamps.cpp`) — wherever a Precursor Console is placed in a ruin, after `make_fixture` add `fd.cyber->tags |= AlienTech` and seed `fd.cyber->lore_fragments`.
- `src/generators/crashed_ship_generator.cpp` — same. The implementer flagged this as already partially-handled in a spot (`crashed_ship_generator.cpp:575`); double-check whether it's complete.
- Add a dev shorthand: `:spawn fixture PrecursorConsole` could be a special-cased FixtureType *or* an inline shim in `dev_console.cpp` that stamps the AlienTech overlay after `make_fixture(Console)`.

Required for the `lore` viewer to actually populate from in-world play (Plan 4's Soul Mirror sync flow).

### 3.5 Sgr A\* in-world warp trigger

Currently `:rebirth` is the only way to trigger the rebirth sequence. Per the original Plan 5 design pillar "Sgr A\* survival", the player should be able to navigate their ship into Sgr A\* (system id 0) and trigger the rebirth sequence diegetically.

Implementation: hook into the warp-target validator in the navigation layer. When the player attempts to warp to system id 0 (Sgr A\*), instead of rejecting, fire `RebirthSequence::begin()` (which already exists from Plan 4).

Touches `src/navigation_*.cpp` or wherever the warp-target validation lives.

### 3.6 WarpAnchor traversal

Cut 4 wires `step on ◉ → log LAN identity + warpable status`. Plan 7 turns this into a real warp:

- Step on a warpable WarpAnchor → physically warp the `@` avatar to the destination station.
- Re-jack-in to that LAN automatically (or land at the host station's ShipTerminal so the player can choose).
- Past-galaxy WarpAnchors stay un-warpable (current behaviour).

Depends on multi-map LAN persistence being in place (so the destination LAN's state is preserved across the warp).

### 3.7 Frontier zone content

The deep-Grid's Frontier region (cols 46-58 of the 60×40 base) is empty Floor behind a tier-2 firewall. Plan 7 fills it with darknet content: hidden terminals, contraband programs, AI black-market contacts, lore that's only available off-grid.

Spec §7 sketches the design intent.

---

## 4. Smaller polish items

These don't need their own design pass; pick them up opportunistically.

### 4.1 `World::galaxy_id_` not in galaxy save

`Game::start_new_galaxy(fresh_seed)` increments `world_.set_galaxy_id(world_.galaxy_id() + 1)`. The new id is stored in `LanMetadata.origin_galaxy_id` and `WarpAnchorRecord.galaxy_id` correctly. But the galaxy_id itself isn't serialised in the galaxy save — save/reload mid-session resets it to 0.

Doesn't break anything visibly today (galaxy_id only matters across rebirths and the WarpAnchor tracking already records it on each anchor), but if Plan 7 surfaces galaxy_id anywhere player-visible, it needs to round-trip.

Trivial fix: add `world.galaxy_id()` u16 to `SaveData` and (de)serialize.

### 4.2 NPC death tombstones

`WorldManager::on_hackable_removed` relabels the dead NPC's Subnet GridNode to `"[removed]"` and zeros its tier, but leaves the node in `GridNetwork::nodes_`. Over time the netmap accumulates tombstones.

Better: actually remove the node + edges from the GridNetwork. Adjust id-allocation to allow gaps.

Or: leave tombstones for memorial purposes (a cyberpunk graveyard of dead drones), and add a `tombstoned` flag for the netmap to filter on.

### 4.3 Disk-read every traverse

`HackingSystem::resolve_sector_for_` for `DeepGridAnchor` calls `read_consciousness(cs)` on every traverse. Cheap for now, but:
- Multi-traverse mid-session re-reads the same file repeatedly.
- Worth caching `ConsciousnessSave` in `WorldManager` or `Game` once.

Same applies to `register_deep_grid_warp_anchor`, `apply_skill_side_effects(ConsciousnessAnchor)`, and the new self-anchor bypass + Atlas view in the nmap widget.

### 4.4 Edge ambiguity for `b`-key netmap breach

The widget picks the first locked edge whose `to == cursor.id`. If a node ever has multiple locked inbound edges, only the first cracks per press. Sufficient today (Subnets have one inbound edge from LanRoot) but may need refinement when Plan 7 introduces richer topologies.

### 4.5 Atlas view past-galaxy grouping

Current Atlas view groups by `galaxy_id` in storage order. If the player has done many rebirths, galaxies appear interleaved. Better: live galaxy first, past lives sorted by recency.

### 4.6 nmap widget Tab gate

The Tab cycle (LAN ↔ Atlas) is unconditional. Should be gated to "only when the player is jacked into the deep-Grid". The `in_deep_grid_` flag exists on the widget but isn't passed in by the host. Pass it.

### 4.7 Lore archive body text

Cut 4's `cat <archive-id>` shows archive metadata only — `LoreArchiveRecord` doesn't carry body lines today (Plan 4's schema). Plan 7 should attach hand-authored body text to each lore fragment seed; until then, archives have no readable contents.

### 4.8 nmap `-l` host column

**(DONE — superseded by `lan_hostname()` in `src/lan.cpp`.)** The HOST column now shows `<tag>-<host>.<region-slug>.lan` (e.g. `console-3.tha.lan`). Original concern: HOST duplicated IP. Resolved Cut 4.5.

### 4.9 `decrypt.exe` — lore archive integration

The `decrypt.exe` Grid program runs successfully (turns the `EncryptedFile` tile into floor + records the mutation) but only writes the archive id into a transient `session.loot.lore_unlocked` vector. **The archive never reaches `consciousness_save.lore_archive`**, so the `lore` PDA command never surfaces decrypted archives.

Wire-up needed:
- On jack-out, push every `loot.lore_unlocked` entry into `consciousness_save.lore_archive` as a `LoreFragmentRef` (or whatever the actual struct shape is). Persist via `write_consciousness`.
- Optionally seed body text per archive id at decrypt time. Currently no body text exists for player-decrypted archives — the `cat <archive-id>` command shows metadata only. Plan 7 owns the body-text content layer; for the wire-up, generating a placeholder paragraph from a per-id seed is enough to make the chain visible.

Touches: `src/program_effects.cpp::apply_decrypt_grid` (record more than just the id?), `src/hacking_system.cpp::jack_out` (commit loot.lore_unlocked → cs.lore_archive), `src/consciousness_save.h` (verify LoreFragmentRef is the right shape).

### 4.10 `reboot_optics.qh` — no in-game targets

The QH is fully wired (sets `Compromised` state for 4 turns + log line) but its filter is `{HasOptics}`, and **no fixture in `tags_for_fixture` carries `HasOptics`**. The legacy DeviceKind-era Camera/Turret entities are not yet present as separate `FixtureType`s; their content lives in a future plan.

Until then, `reboot_optics.qh` is dead content — it loads from cyberdecks and consumes RAM if loaded, but its filter never matches anything in the world.

**Resolution paths (Plan 6 or Plan 7):**
- Add `FixtureType::Camera` and `FixtureType::Turret`. Tag them `Electronic | HasOptics` (Camera) / `Electronic | HasOptics | Weaponized` (Turret). Place them in station / asteroid generators. `reboot_optics.qh` and `friendly_fire.qh` (against turrets specifically) become reachable.
- OR: assign `HasOptics` to existing fixtures that have lensed surfaces (StarChart's observatory dome could plausibly count).
- Until either lands, leaving the program in the registry is fine — `nmap -l`'s tag column already shows what each device matches, so the player sees "no compatible programs loaded" and moves on.

---

## 5. Known dev-workflow caveats

* `:spawn-hackable` retired in Cut 1. Use `:spawn fixture <FixtureType>` (e.g. `Console`, `Door`, `Conduit`).
* `:spawn fixture Camera` and `:spawn fixture Turret` are NOT supported — those FixtureTypes don't exist in the current code (they were `DeviceKind` labels in Plan 4). Plan 6+ may add real `FixtureType::Camera` / `FixtureType::Turret`.
* `:spawn fixture Console` produces a plain Console — no `AlienTech`, no lore fragments. The Precursor variant needs a dedicated dev shortcut (item 3.4 above).
* `:lan-info` dumps the active LAN's state for debugging. After multi-map LAN persistence lands, it shows whichever map you're currently on.
* `:rebirth` triggers the rebirth sequence directly; useful for testing galaxy reseed without finding Sgr A\* in-world.
* `:rebirth-reset` wipes `consciousness.dat` (clean slate).

---

## 6. Files / paths a fresh agent will want

* Spec: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
* Plan: `docs/superpowers/plans/2026-05-01-grid-expansion.md`
* Per-device diegetic shells (Plan 7 design): `docs/superpowers/specs/2026-05-01-per-device-diegetic-shells-design.md` *(committed mid-Plan-5; revisit in Plan 7 context)*
* Hacking mechanics doc: `docs/mechanics.md` — search "Hacking — Plan 5"
* Items doc: `docs/items.md` — implant tags note

Source-of-truth surfaces likely to change in Plan 5.5 / Plan 6 / Plan 7:

* `include/astra/world_manager.h`, `src/world_manager.cpp` — multi-map LAN refactor lives here.
* `src/game_world.cpp` — ~20 map-enter call sites that route through `Game::on_map_loaded()`.
* `include/astra/lan.h`, `src/lan.cpp` — `register_hackables_in_lan`, `find_or_create_deep_grid_anchor`, IP allocation.
* `src/hacking_system.cpp` — `jack_in`, `traverse_to`, `resolve_sector_for_` (DeepGridAnchor branch).
* `src/grid_nmap_widget.cpp`, `include/astra/grid_nmap_widget.h` — nmap widget filtering for active-LAN rendering.
* `include/astra/save_file.h`, `src/save_file.cpp` — schema bump + serialisation.
* `src/game.cpp` — `Game::new_game()` (both overloads), `Game::start_new_galaxy()`, `Game::post_load()`.
* `src/program_effects.cpp` — `apply_breach_grid` (DeepGridGateway branch + WarpAnchor population).
* `src/skill_grant.cpp` — `apply_skill_side_effects(ConsciousnessAnchor)`.

---

## 7. What I'd do next

~~Plan 5.5~~ shipped. Next on deck:

1. **Plan 6** — UI / Grid HUD redesign. Designed against the now-stable LAN gameplay shape.
2. **Plan 7** — content + Your.Anchor v2 + AI contacts UI + hostile-QH proper model + PrecursorConsole AlienTech + Sgr A\* warp + WarpAnchor traversal + Frontier zone. The big content layer.

Polish items in §4 are opportunistic — pick them up when adjacent code is being touched anyway.
