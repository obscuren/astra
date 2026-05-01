# Grid Expansion & Change (Plan 5) — Design Spec

**Date:** 2026-05-01
**Status:** Draft, awaiting user review
**Branch (target):** `feature/grid-expansion`
**Parent specs:** `2026-04-29-hacking-design.md` (parent), `2026-04-30-hacking-deep-grid-design.md` (Plan 4)
**Handoff:** `docs/plans/2026-05-01-grid-loop-handoff.md`

---

## 1. Concept

Plan 5 redesigns the LAN layer of the hacking system around a single non-negotiable design pillar: **anything electrical can be hacked**. Today the LAN graph is a hand-tuned five-kind universe (`Turret`/`Camera`/`Door`/`PowerConduit`/`PrecursorConsole`) with a per-fixture `available_qh` whitelist that disagrees with `ProgramDef::target_filter` and is read by no gameplay code. Plan 5 rebuilds that into a tag-driven graph where every electrical fixture is automatically reachable, every program declares its required capabilities once, and a real LAN sector — designed cyberspace, not a stand-in — connects them all.

### Pillars

- **Tag-driven capability.** Fixtures carry a `HackTagMask`. Programs require capability sets. There is no kind-keyed filter anywhere in gameplay code.
- **The LAN is a place, not a list.** When the player jacks in via a `JackInPort`, they enter a generated cyberspace map specific to that LAN, walk it as `@`, and physically reach Gateway tiles that lead to per-device sub-sectors.
- **Connected vs isolated LANs.** Stations and asteroids are connected — their LAN holds a Deep-Grid Gateway out to the universal cyberspace. Dungeons are isolated — LAN-only, no deep-Grid escape.
- **The deep-Grid is the universal layer.** Vast, hand-authored, consciousness-scoped, persists across rebirths. Atlas region holds warp anchors back to every connected LAN ever cracked.
- **The netmap (now `nmap`) is logical; the LAN sector is spatial.** Two views, two purposes. Walking is physical; nmap is the planning + remote-action surface.
- **Clean, unambiguous visuals.** Existing Tron Cyan/Magenta palette only. No broken-tile variants. Each tile type has one canonical glyph + color.

### Non-goals

- HUD redesign (Plan 6).
- **Your.Anchor full mechanics.** Plan 5 ships the v1 — recognisable spawn-hub geometry inside the deep-Grid sector, with the player's lore archive surfaced as a `DataNode` tile. Stash, customization, AI-contact locus, expanded ownership rules, and rebirth-survivor identity surface are **Plan 7**, after Plan 6 (UI). Plan 5 reserves space in the Anchor room layout for those additions but does not implement them.
- Darknet content / regional darknet revival (Plan 7 will design from scratch).
- Sgr A\* in-world warp trigger (deferred — `:rebirth` remains the in-game path).
- Full AI-contact gameplay loop (schema-write only in this plan).
- Public commerce / forum / marketplace deep-Grid content (later).
- Programs beyond the existing 9 (no new programs in this plan).
- Multi-galaxy concurrent state (only the active galaxy is live; previous-galaxy Atlas entries are memorial-only).

---

## 2. Architecture

### New subsystems

| Unit | Header | Responsibility |
|---|---|---|
| `HackTagMask` + tag table | `hackable.h` (extended) | Bitflag mask of capability tags; per-`FixtureType` tag table; replaces `DeviceKind`. |
| `LanMetadata` + `LanRoom` | `lan.h/cpp` | Per-world-map LAN data: subnet base, room list, flavour, progress counters. Serialized into the galaxy save under the world map. |
| `LanSectorGenerator` | `lan_sector_generator.h/cpp` | Procedural cyberspace generator. A+B+E layered (firewall rings + circuit-board offices + connector wiring). Map size scales with node count; per-flavour structural parameters. |
| `DeepGridSector` (data) | embedded in `consciousness_save.cpp` | Hand-authored ~60×40 sector with Anchor / Atlas / Frontier regions, firewalled. Lives in `consciousness.dat`. |
| `GridNmapWidget` (renamed) | `grid_nmap_widget.h/cpp` | Was `GridNetmapWidget`. World-geometry-mirrored layout; LAN view + Atlas view; netmap-side `b`-key breach. |
| `nmap` / `ping` / `jack` real implementations | `pda_hacking_tab.cpp` | CLI-shaped commands with IP-driven addressing. |

### Existing systems reshaped

- `Hackable` — `device_kind` and `available_qh` fields **removed**; `tags: HackTagMask` and `ip: uint32_t` added. `lore_fragments` and `soul_mirror_progress` retained (Plan 4 carryover).
- `ProgramDef::target_filter` — `vector<DeviceKind>` → `vector<TagSet>`. AND within a `TagSet`, OR across the vector. A device satisfies the filter if any one `TagSet` is fully covered by the device's tags.
- `GridNetwork` — node-per-Hackable model. `RegionalDarknet` node kind retired; new `LanRoot` kind added. `Subnet` and `DeepGridAnchor` (Plan 4 carryover) preserved. `entry_redirect` retired (every subnet has a real sector). One shared `DeepGridAnchor` node per consciousness; every connected LAN's `LanRoot` has a tier-2 edge directly to it.
- `HackingSystem::jack_in` — sector dispatch by node kind. For `LanRoot`, generate/load LAN sector from `LanMetadata`. For `Subnet`, generate from `(network_id, device_id)` seed. For `DeepGridAnchor`, load from `consciousness.dat.deep_grid_base` when self-owned, else `make_consciousness_anchor_sector()`. Sector traversal mid-jack-in (no full jack-out between LAN ↔ subnet ↔ deep-Grid hops).
- `program_effects::apply_breach_grid` — uses the gateway tile's `target_node_id`, not "first locked edge of current node".
- `RebirthSequence::apply` — calls `Game::start_new_galaxy(fresh_seed)` instead of `MainMenu` shortcut. Galaxy reseed lives there.
- `GridRenderer` — wall renderer extended to box-drawing-connected glyphs; new `Connector` and `DeepGridGateway` tiles.
- Map-gen content pass — every generator that emits an electrical fixture starts attaching a tagged `Hackable`. Touches station, asteroid, ruin, settlement, dungeon, ship-interior, crashed-ship pipelines.

### File-size discipline

`lan.cpp` and `lan_sector_generator.cpp` target ≤ ~400 lines each. The deep-Grid base author lives in its own helper to keep `consciousness_save.cpp` lean. `grid_renderer.cpp` gains the box-drawing wall path but stays under ~250 lines (small lookup table). `pda_hacking_tab.cpp` is already ~900 lines — the new `nmap` / `ping` / `jack` bodies stay under 100 lines combined; if it crosses 1000 we extract `hack_term_commands.cpp`.

---

## 3. Tag-based capability model

### `HackTagMask`

```cpp
enum class HackTag : uint32_t {
    None        = 0,
    Electronic  = 1u << 0,   // anything with power; required for LAN membership
    Locked      = 1u << 1,   // has a lock affordance (door, gate, locker)
    PowerNode   = 1u << 2,   // power conduit / lamp / lighting / cooling
    DataStore   = 1u << 3,   // stores data (terminal, console, vending records)
    HasOptics   = 1u << 4,   // has a sensor (camera, turret optics, holo-display)
    Weaponized  = 1u << 5,   // can deal damage (turret, mine)
    Mobile      = 1u << 6,   // mobile platform (NPC implant, drone)
    AlienTech   = 1u << 7,   // Precursor / non-human origin
    JackInPort  = 1u << 8,   // can serve as the player's entry portal into the LAN
};

using HackTagMask = uint32_t;
using TagSet      = HackTagMask;   // alias for clarity at the program-filter site
```

### Static tag table per `FixtureType`

```cpp
HackTagMask tags_for_fixture(FixtureType f);   // returns 0 for non-hackable
```
Tags are static — never mutated at runtime. An open door still carries `Locked`; the *effect* of `bypass_lock` short-circuits to no-op when the runtime door state is already open. The filter is shape-of-thing; the effect is current-state.

### Program filter — `vector<TagSet>`

```cpp
struct ProgramDef {
    // ... existing fields ...
    std::vector<TagSet> target_filter;   // QH only; for non-QH, empty
};
```

Match semantics: device satisfies the filter iff *any* `TagSet` in `target_filter` is fully covered by the device's tags. Within a `TagSet`, all bits must be set on the device (AND). Across the `vector`, any single match is sufficient (OR).

Examples:
```cpp
reboot_optics.target_filter   = { HasOptics };
bypass_lock.target_filter     = { Locked | Electronic };
blackout.target_filter        = { PowerNode };
data_leech.target_filter      = { DataStore };           // also: { Electronic } if widening desired
friendly_fire.target_filter   = { Weaponized | Mobile };
```

### Migration table — the 5 retired `DeviceKind`s

| Was | Becomes (tags) |
|---|---|
| `Turret` | `Electronic | HasOptics | Weaponized` |
| `Camera` | `Electronic | HasOptics` |
| `Door` | `Electronic | Locked` |
| `PowerConduit` | `Electronic | PowerNode` |
| `PrecursorConsole` | `Electronic | DataStore | AlienTech | JackInPort` |

`DeviceKind` enum and `device_kind_name` are retired. The dev-console `:spawn-hackable <kind>` verb is removed and replaced by the unified `:spawn fixture <FixtureType>` (see §12). The old kind labels are retired permanently — content authors pick `FixtureType` names directly.

### Why this works

- **Two-source-of-truth bug dies.** `Hackable::available_qh` is removed entirely (and from the save serializer). `ProgramDef::target_filter` is the only filter.
- **New programs land in one line.** A new `.qh` declares its `target_filter` and works on every fixture in the world that already carries the matching tag.
- **New fixtures land in one tag stamp.** Add the `FixtureType`, add the tag mask in the table, done. The CLAUDE.md rule reminds future contributors to pick the tags before merging.

---

## 4. LAN graph & auto-registration

### LAN data model

```cpp
struct LanRoom {
    std::string name;                       // "security", "lobby", "exec"; never rendered as in-sector text
    Rect        extents;                    // cells in the LAN sector
    int         tier;                       // 1=open doorway, 2=breach-required, 3=inner sanctum
    std::vector<GridNodeId> contained_subnets;
};

enum class LanFlavour : uint8_t { Station, Asteroid, Dungeon, Precursor };

struct LanMetadata {
    GridNodeId      lan_root;               // the LanRoot node in GridNetwork
    bool            has_deep_grid_edge;     // true iff connected (edge LanRoot → shared DeepGridAnchor)
    std::string     region_label;           // "Heavens Above"
    std::string     display_name;           // "Concourse LAN"
    LanFlavour      flavour;
    int             security_tier;
    bool            connected;
    uint32_t        gen_seed;               // for sector generator
    uint32_t        subnet_base;            // packed 10.X.Y.0 for IP allocation
    std::vector<LanRoom> rooms;
    uint64_t        last_visited_tick;
    int             nodes_total;
    int             nodes_cracked;
    int             ice_killed;
    int             lore_extracted;
    uint16_t        origin_galaxy_id;
};
```

### Auto-registration sweep

On map enter (or save load), `register_hackables_in_lan(world_map, GridNetwork&, LanMetadata&)`:
1. Collect all `Hackable` references on the map (fixtures + NPC implants where `tags & Electronic`).
2. Sort by world `(y, x)` for stable ordering.
3. Allocate `LanMetadata` (one per map). Determine `flavour` from map kind (`Station`/`Asteroid`/`Dungeon`/`Precursor`). Determine `connected` from flavour.
4. Compute `subnet_base = 0x0A000000 | ((map_seed >> 8) & 0x00FFFF00)`. (`10.X.Y.0/24`.)
5. For each Hackable: assign `host = i + 1`, set `Hackable.ip = subnet_base | host`.
6. Add `LanRoot` node to `GridNetwork`. Add `Subnet` node per Hackable; edge `LanRoot → Subnet` with tier from per-fixture security tier.
7. If `connected`, look up the shared `DeepGridAnchor` node from the consciousness save (or create it on first need) and add a tier-2 edge `LanRoot → DeepGridAnchor`. Set `has_deep_grid_edge = true`.
8. Generate `LanRoom`s by k-means clustering Hackables by world coords (k = `ceil(N/3)`); name each from per-flavour pool; assign `tier` based on whether the room contains a `JackInPort` or sits at the cluster centroid (innermost room becomes inner-sanctum tier 3 when N ≥ 9).
9. Write `LanMetadata` into the world map's save section.

### LAN persistence

- `LanMetadata` and the GridNetwork's per-LAN nodes serialize with the galaxy save.
- The LAN sector itself is generated lazily on first jack-in (from `LanMetadata.gen_seed`) and **never regenerates** in production gameplay. The base geometry is recoverable from the seed; mutable runtime state is persisted as a tile-mutation overlay (see below).
- Each per-Subnet 8×8 sector the player has visited is persisted in the same shape, keyed by Subnet `GridNodeId`.
- The deep-Grid sector lives in `consciousness.dat` (Plan 4 carryover; Plan 5 expands its geometry); same tile-mutation overlay applies.

### Tile-mutation runtime state

```cpp
struct SectorMutation {
    uint8_t  x, y;
    GridTile new_tile;     // what the tile now is (Floor for cracked / looted / decrypted)
};

struct SectorRuntimeState {
    std::vector<SectorMutation>          mutations;     // applied as overlay after seed-regen
    std::vector<std::pair<uint8_t,uint8_t>> killed_ice; // ICE positions that don't respawn
};

struct LanMetadata {
    // ... fields above ...
    SectorRuntimeState                              lan_sector_state;
    std::unordered_map<uint32_t, SectorRuntimeState> subnet_states;   // keyed by Subnet node id .value
};
```

Tracked mutations on every sector type:
- Cracked firewall: `▓` → `Floor`.
- Looted DataNode: `$` → `Floor`.
- Decrypted EncryptedFile: `⊘` → `Floor`.
- Cracked Deep-Grid Gateway: `⊕` → `Floor` (after first breach; tile becomes free passage).
- Killed ICE: position recorded; no respawn within the LAN session-to-session.

Anything that *can* be undone by future gameplay (e.g. a Hackable transitioning Compromised → Clean) is **not** a mutation — it's runtime state on the Hackable, persisted on the Hackable, not on the sector.

### Regeneration policy

Production gameplay never regenerates a sector once it has been generated. Persistence is the default and only behaviour.

The single exception is the dev path:
- `:spawn fixture <Type>` (and any future dev verb that adds a Hackable to a live world map): triggers `lan_full_reset(map_id)` which wipes `LanMetadata.lan_sector_state`, clears `subnet_states`, re-allocates IPs, re-clusters rooms, re-derives sector size, bumps `gen_seed`. Documented as a destructive testing-only operation in the `:spawn` help text.
- NPC death and any other production remove path: **metadata-only**. The Hackable deregisters from `GridNetwork`; the orphan subnet's `⌬` Gateway tile in the LAN sector remains visually but becomes inert — `jack`-ing onto it returns `host unreachable`. All cracked / looted / decrypted state on every sector is preserved.

---

## 5. LAN sector generator

### Layout — A+B+E layered

- **A. Circuit-board base.** Bus-trace walls between rectangular office pads. Connector tiles run between rooms.
- **B. Firewall rings.** 0–3 concentric `▓` rings carving security zones. Outer ring is the LAN perimeter; inner rings gate sanctum offices.
- **E. Office grid.** Rectangular rooms (`LanRoom`s) holding `⌬` Gateway tiles per device subnet. Open-doorway tier-1 rooms have a `░` floor gap in the firewall; tier-2/3 rooms are fully bounded and require breach.

### Size scaling

```cpp
struct LanSizeParams {
    int office_count;   // = max(1, ceil(N / 3))
    int ring_count;
    int width;
    int height;
};

LanSizeParams compute_lan_size(int n_nodes) {
    int oc = std::max(1, (n_nodes + 2) / 3);
    int rc = (oc == 1) ? 0
           : (oc <= 4) ? 1
           : (oc <= 9) ? 2
           : 3;
    int w = std::clamp(20 + 6 * static_cast<int>(std::ceil(std::sqrt((double)oc))), 24, 80);
    int h = std::clamp(12 + 4 * static_cast<int>(std::ceil(std::sqrt((double)oc))), 14, 40);
    return { oc, rc, w, h };
}
```

| N | Offices | Rings | Size |
|---|---|---|---|
| 3 | 1 | 0 | 24×14 |
| 8 | 3 | 1 | 32×20 |
| 18 | 6 | 2 | 44×26 |
| 30 | 10 | 3 | 64×36 |

### Per-flavour structural parameters (palette is uniform; only structure varies)

| Flavour | Office density | Ring count adj | ICE bias |
|---|---|---|---|
| Station | dense (full N/3) | +0 | mostly white, some grey |
| Asteroid | medium (N/4) | -1 | white + occasional black |
| Dungeon | sparse (N/5) | -1 (min 0) | white only |
| Precursor | dense (full N/3) | +1 nested | grey + black bias |

### Deep-Grid Gateway placement

- Connected LANs only.
- One `⊕` tile total per LAN sector.
- Placed inside the highest-tier office (inner sanctum if `ring_count >= 2`, otherwise the highest-tier room).
- Carries `target_node_id` pointing at the shared `DeepGridAnchor` node.

### Jack-out (`⊙`) placement

- Always present in every LAN sector.
- Placed on the LAN floor near the avatar's spawn point, outside any office.

### Generation algorithm

```
seed-driven, deterministic:
  1. compute_lan_size(N)
  2. pick spawn location near top-left floor area
  3. place outer firewall ring at perimeter
  4. for each inner ring (1..ring_count - 1):
       carve a concentric firewall rectangle inset by 4 cells per ring level
  5. for each LanRoom:
       carve an axis-aligned rectangle of floor inside its zone
       wall-bound the rectangle with firewall (▓)
       if room.tier == 1: punch a 1-cell ░ doorway in the firewall
       stamp ⌬ Gateway tiles at the room interior, one per subnet,
         each carrying target_node_id of its Subnet
  6. for each pair of rooms within the same security zone:
       route a 1-cell-wide Connector path (═║...) between them on the floor;
       Connectors are decorative impassable; they don't block sight
  7. if connected: stamp ⊕ DeepGridGateway in the highest-tier room
  8. stamp ⊙ ExitNode near spawn
```

### Persistence

- Generator output is hashed and the generator version recorded.
- Mutable runtime state (cracked firewall tiles, killed ICE positions, decrypted file flags) is tracked separately in `LanMetadata.runtime_state`.
- On load: regenerate base sector from seed, then apply `runtime_state` overlay.

---

## 6. Tile types & rendering

### `GridTile` enum after Plan 5

| Tile | Glyph | Color | Walkable | Breachable | Notes |
|---|---|---|---|---|---|
| `Floor` | `░` | Blue | yes | — | unchanged |
| `Firewall` | `▓` | Magenta | no | yes | unchanged; bounding walls of offices and rings |
| `Wall` | (box-drawing, neighbour-resolved) | DarkGray | no | no | **was** invisible space; now connect-rendered |
| `Connector` *(new)* | (box-drawing, neighbour-resolved) | DarkGray | no | no | decorative wiring between rooms |
| `Gateway` (subnet) | `⌬` | BrightMagenta | yes | — | unchanged; carries `target_node_id` |
| `DeepGridGateway` *(new)* | `⊕` | BrightCyan | yes | — | connected LANs only |
| `ExitNode` (jack-out) | `⊙` | BrightWhite | yes | — | unchanged |
| `DataNode` | `$` | Yellow | yes | — | unchanged |
| `EncryptedFile` | `⊘` | Green | yes | — | unchanged |

Note: `Wall` and `Connector` share glyph and color but live in different `GridTile` slots so the generator can stamp one or the other for clarity. The renderer uses the same neighbour-aware glyph picker for both.

### Box-drawing wall rendering

```cpp
const char* wall_glyph_for_neighbours(bool n, bool s, bool e, bool w);
// Returns one of: ║ ═ ╔ ╗ ╚ ╝ ╠ ╣ ╦ ╩ ╬ depending on which sides have walls.
```

A 16-entry lookup table covers all neighbour combinations. The renderer treats `Wall` and `Connector` tiles as connectable; `Firewall` is **not** connectable to walls (firewalls render as solid `▓` blocks regardless of neighbours).

Scope: **global** — all hacking sectors use the new wall renderer, including the existing 8×8 subnet sectors and the deep-Grid base. The existing sectors get free polish.

### Per-flavour palette

**None.** The palette is uniform across all LAN flavours. Per-flavour differentiation comes from structural parameters (office density, ring count) and ICE composition. No "rotted" / "alien" / "corporate" palette swap.

---

## 7. Deep-Grid sector

### Authoring

The deep-Grid is **one hand-authored ~60×40 sector** stored in `consciousness.dat` (independent of any galaxy save). It replaces the current `make_player_deep_grid_base` (30×20 single-room) with a much larger designed space. Same authoring approach (hand-placed tiles), much more designed surface.

### Regions (separated by firewall partitions)

- **Anchor** (~12×10) — small spawn hub; **this is Your.Anchor v1**. Player avatar spawns here on first deep-Grid entry. Plan 5 establishes the recognisable spatial home: the room exists with hand-authored geometry, the spawn tile is at its centre, and Plan 4's existing `owned_by_consciousness_id` bypass on the nmap predicate guarantees the player can always reach it. The room also contains a single `DataNode` (`$`) tile that surfaces the player's lore archive (mirroring the `lore` terminal command — readable from inside the anchor as a small "this is your space" affordance). **The full Your.Anchor design — personal stash, customization, AI-contact locus, expanded ownership semantics, rebirth-survivor identity surface — is expanded in Plan 7**, after Plan 6's UI work. Plan 5 must not pre-empt those decisions; the room's hand-authored layout reserves space for the Plan 7 additions but doesn't implement them.
- **Atlas** (~24×20) — the "every discovered node" region. Empty floor at gen time; each cracked connected LAN spawns a `WarpAnchor` tile here, organised by galaxy → system. Anchor → Atlas firewall is open by default (no breach required).
- **Frontier** (~24×10) — placeholder zone for Plan 7 darknet content. Visible firewalled zones with `breach.exe` requirements that are intentionally tier-3 and reward a "no content here yet" lore message when cracked. Telegraphs that there's more coming.
- **Optional flavour rooms** — 1–2 small thematic spaces (an old terminal echoing Precursor inscriptions, an empty data vault). Cheap mood payoff.

### Firewalls

- **Anchor ↔ Atlas:** open (no firewall, just a wide doorway).
- **Atlas ↔ Frontier:** firewall, tier-2 breach required to reach Frontier zone 1; tier-3 for inner Frontier zones.
- **Frontier sub-zones:** firewall partitions with `breach.exe` gates, contents Plan 7-bound.

### `WarpAnchor` (new tile)

```cpp
struct WarpAnchorData {
    uint16_t    galaxy_id;              // current galaxy's id, or a previous one
    uint32_t    region_seed;            // identifies the destination LAN
    std::string lan_display_name;
    int         nodes_total;
    int         nodes_cracked;
    bool        warpable;               // false for past-galaxy (memorial) entries
};
```

Glyph: `◉` BrightWhite (new tile type, chosen to read distinctly from `⊙` jack-out and `⊕` deep-Grid gateway). Past-galaxy entries are rendered dimmed via `UITag::TextDim`.

### Population

When the player cracks a connected LAN's Deep-Grid Gateway for the first time (`apply_breach_grid` on the `⊕` tile), `consciousness.dat.deep_grid_base` gets a new `WarpAnchor` tile inserted into the next free Atlas cell, populated with `LanMetadata` snapshots. Persistence is automatic — `consciousness.dat` is the durable store.

### Cross-galaxy memorial behaviour

On rebirth: the new galaxy's `region_seed`s do not match any previous galaxy's, so existing `WarpAnchor` tiles whose `galaxy_id` no longer matches the live galaxy are flagged `warpable = false`. They remain visible (you see your past lives' work) but stepping on them gives a "cannot warp — galaxy lost" message. New cracks add new tiles ahead of them.

### Capacity

Atlas region capacity ≈ 200 anchor tiles (24×20 with corridors). At ~3-5 connected LANs per galaxy, this carries 40+ rebirths before the Atlas fills. Beyond that we'll add scrolling or a "purge memorial entries" Plan 7 feature.

---

## 8. Sector traversal & multi-Gateway encoding

### Traversal model

The current model treats every jack-in as `world → sector → world`. Plan 5 introduces **mid-jack-in sector traversal**: stepping on a Gateway swaps the active sector without exiting the hacking session. The avatar's RAM/Heat/Trace state, Soul Mirror channel, and ICE status carry across.

### Three traversal kinds

1. **LAN ↔ device subnet.** Player walks LAN, steps on `⌬`, the renderer swaps to that subnet's 8×8 sector. Reverse: subnet has its own gateway-back tile that swaps back to the LAN sector at the originating `⌬` position.
2. **LAN → deep-Grid.** Player walks LAN, steps on `⊕`, the renderer swaps to the deep-Grid sector (loaded from `consciousness.dat`). Avatar spawns at the Anchor region's entry tile. Reverse: an Atlas `WarpAnchor` tile traversal — see (3).
3. **Deep-Grid Atlas → LAN.** Player in Atlas steps on a `WarpAnchor` tile, the renderer swaps to that LAN's sector at the `⊕` position. Reverse path is symmetric.

### Sector-source dispatch in `HackingSystem::jack_in`

```cpp
void HackingSystem::jack_in(const GridNode& target) {
    switch (target.kind) {
        case GridNodeKind::LanRoot: {
            // Generate or load LAN sector; spawn avatar near ⊙
            const auto& meta = world_->lan_metadata_for(target.id);
            session_.sector = generate_or_load_lan_sector(meta);
            session_.avatar_xy = meta.spawn_xy;
            break;
        }
        case GridNodeKind::Subnet: {
            session_.sector = generate_subnet_sector(target.source_seed, target.security_tier);
            session_.avatar_xy = subnet_spawn_xy();
            break;
        }
        case GridNodeKind::DeepGridAnchor: {
            // Owned-by-this-consciousness check bypasses lock predicate (Plan 4 carryover)
            const auto& cs = consciousness_save_;
            if (target.owned_by_consciousness_id == cs.consciousness_id) {
                session_.sector = load_deep_grid_sector(cs.deep_grid_base);
            } else {
                session_.sector = make_consciousness_anchor_sector();
            }
            session_.avatar_xy = anchor_spawn_xy();
            break;
        }
    }
}
```

The mid-jack-in `traverse_to(target_node_id)` reuses the same dispatch but skips the entry preamble.

### Multi-Gateway-per-sector encoding

Each Gateway tile carries the target node id it leads to:
```cpp
struct GatewayTileData {
    GridNodeId target_node_id;
    int        edge_tier;        // mirrors GridEdge.gateway_tier
    bool       cracked;          // mirrors GridEdge.cracked
};
```

`apply_breach_grid` fix:
```cpp
// before: cracks "first locked edge" of current node
// after:  cracks the edge identified by the gateway tile under cursor
GridEdge* find_edge_for_gateway(GridNetwork& net, GridNodeId from, GridNodeId gateway_target);
```

This eliminates the latent ambiguity bug the handoff §2 calls out. Already-correct in single-gateway sectors; required for multi-gateway LAN sectors.

---

## 9. Hacking terminal — `nmap` / `ping` / `jack` + IP addressing

### IP scheme

- Per-LAN /24: `10.X.Y.0/24` where `X.Y` derives from `(map_seed >> 8) & 0xFFFF`.
- Hosts `1..253` for normal subnets.
- Host `254` reserved for the LAN's Deep-Grid Gateway.
- `0` and `255` reserved (network / broadcast).
- IPs persist on `Hackable.ip`; deterministic and stable across save/load.
- Display via `format_ip(uint32_t)`; parse via `std::optional<uint32_t> parse_ip(std::string_view)`.

### `nmap` — list / map the LAN

```
nmap [-l] [-m] [-h]

  -l, --list     enumerate nodes on the current LAN with IPs and status
  -m, --map      open the visual logical view (graph widget)
  -h, --help     usage

  with no flag: prints usage.
```

Output of `nmap -l`:
```
LAN: Heavens Above   (10.42.7.0/24)   8 nodes, 2 cracked

  IP            HOST                       STATUS    TAGS
  10.42.7.1     [lobby.console-2]          open      Electronic DataStore JackInPort
  10.42.7.2     [lobby.door-5]             locked.1  Electronic Locked
  10.42.7.3     [security.camera-7]        cracked   Electronic HasOptics
  10.42.7.4     [security.door-9]          locked.2  Electronic Locked
  10.42.7.5     [ops.conduit-1]            locked.2  Electronic PowerNode
  10.42.7.6     [ops.console-12]           locked.2  Electronic DataStore JackInPort
  10.42.7.254   [⊕ deep-grid]              locked.3  DeepGridGateway
```
- Nodes sorted by IP (which is by world-coord ordering — stable, recognisable).
- `STATUS` is one of `open`, `locked.N`, `cracked`.
- `TAGS` is the human-readable list of `HackTag`s on the fixture.

### `nmap -m`

Opens `GridNmapWidget` (the visual logical view from §10). Same widget that the `'N'` shortcut binds to.

### `ping IP` — recon a single node, free action

```
> ping 10.42.7.5
PING 10.42.7.5 (ops.conduit-1):
  64 bytes from 10.42.7.5: time=2ms
  tier:    2 (locked)
  tags:    Electronic, PowerNode
  state:   Clean
```
- Free action: zero RAM, zero Heat.
- Resolves IP → fixture in the current LAN (and also Atlas warp tiles when in deep-Grid).
- Output: latency (cosmetic, deterministic from `host` octet so it's stable per node), tier, tags, state.
- Errors: `ping: 10.42.7.99: host unreachable` for unknown IP.

### `jack IP` — jack into a node, IP-keyed

Replaces the label form (`jack -t <node-label>`). Same deferred-jack flow; the host (`PdaScreen`) still polls `jack_in_request_node_id_` after `handle_input`.

```
> jack 10.42.7.6
>> uploading consciousness... <<
```

Errors:
- `jack: 10.42.7.99: host unreachable` (unknown IP).
- `jack: requires Cat_Hacking skill.` (existing skill check).
- `jack: locked — try breach.exe` (destination edge locked; unchanged path otherwise).

### `lore` — read decrypted archives

Implements consumption of `consciousness.dat.lore_archive`. Lists archive ids with timestamps and a `cat` shortcut to read each. Text body comes from the per-fragment seed at the source `PrecursorConsole`.

### Stub-marker cleanup

- `help`: drop `(stub in Plan 2/3)` from `ping` / `netmap` / `jack` lines; rename `netmap` → `nmap`; add explicit `lore` line.
- `man`: rewrite `nmap` (with `-l/-m/-h` flags), `ping` (real shape), `jack` (IP-keyed) entries with no STATUS-stub blocks. Add `nmap` entry; remove `netmap`.
- Tab-complete list update: `whoami`, `ping `, `nmap`, `nmap -l`, `nmap -m`, `jack `, `lore`.

---

## 10. `nmap` widget (renamed from `netmap`)

### Modes

- **LAN view** — current map's LAN graph. Default when widget opens. World-geometry-mirrored layout: each subnet node sits at its corresponding world-coordinate, scaled into the netmap viewport. Room name (`LanRoom.name`) appears in the node label as a prefix `[room.subnet-id]`. Edges show graph connectivity; locked = `╳`. The deep-Grid edge `⊕` sits as a distinct node.
- **Atlas view** — only available when the player is inside the deep-Grid sector. Renders `WarpAnchor`s grouped by galaxy → system. Past-galaxy entries dimmed.

### Controls

```
arrows / hjkl   move cursor between nodes (graph adjacency)
Enter           jump to node's sector (or warp to LAN, in Atlas view)
b               run breach.exe on a locked edge from outside the sector
                (charges program cost; flips cracked = true; no entry)
Tab             cycle LAN ↔ Atlas (when in deep-Grid; no-op otherwise)
Esc             close
```

### Self-anchor entry

```cpp
bool can_enter(const GridNode& target, const ConsciousnessSave& cs) {
    if (target.owned_by_consciousness_id == cs.consciousness_id) return true;
    return !node_is_locked(net_, target.id);
}
```
Self-owned nodes bypass the lock predicate (you own it; locks don't apply).

### Reshaped components

- `enum class NetmapZoom { Regional, DeepGrid }` → `enum class NmapMode { Lan, Atlas }`.
- `kind_tag` and `tag_for` — drop `RegionalDarknet`, add `LanRoot`. `DeepGridAnchor` already in the enum from Plan 4; gets refreshed labels for the LAN view.
- `visible_nodes` — filters by current world map for LAN mode; filters by consciousness anchor for Atlas mode.
- New `apply_breach_from_nmap(GridEdge&)` helper — charges program cost via `HackingSystem` without opening a sector.

### Sketches

LAN view:
```
╔══ Heavens Above LAN ═══════════════════════════════╗
║                                                    ║
║   [lobby.console-2]                                 ║
║         │                  [security.camera-7]      ║
║   [lobby.door-5] ────────── [security.door-9]       ║
║                                                    ║
║                  [ops.conduit-1]──╳──[ops.console-12]║
║                         │                           ║
║                  [⊕ deep-grid]                      ║
║                                                    ║
╚════════════════════════════════════════════════════╝
[arrows] move  [Enter] jack in  [b] breach  [Tab] cycle  [Esc] close
```

Atlas view:
```
╔══ Deep-Grid Atlas ═════════════════════════════════╗
║                                                    ║
║   GALAXY: Sol Sector                               ║
║                                                    ║
║     [Heavens Above LAN]      [Mining Asteroid LAN] ║
║      └ 8/12 cracked          └ 3/3 cracked         ║
║                                                    ║
║   GALAXY: Cygnus  (past life)                      ║
║                                                    ║
║     [Outpost Theta LAN]  (ghosted)                 ║
║                                                    ║
╚════════════════════════════════════════════════════╝
[Enter] warp to LAN  [Tab] LAN view  [Esc] close
```

---

## 11. Stitching gap fixes

From handoff §2:

| Gap | Fix in this plan |
|---|---|
| Deep-Grid sector unreachable | LAN sector includes `⊕` `DeepGridGateway` tile on connected LANs; nmap widget unlock predicate handles owned anchors; `grid_input` traversal handles `⊕`. |
| `jack_in` ignores saved base | Sector-source dispatch in `HackingSystem::jack_in`: `DeepGridAnchor` with `owned_by_consciousness_id == cs.consciousness_id` loads `consciousness.dat.deep_grid_base`. Falls back to `make_consciousness_anchor_sector()` otherwise. |
| Galaxy reseed deferred | `RebirthSequence::apply` calls `Game::start_new_galaxy(fresh_seed)` instead of returning to `MainMenu`. New-game pipeline runs with the fresh seed; `consciousness.dat` is re-applied. |
| Lore viewer stub | `hack_term_cmd_lore` reads `consciousness.dat.lore_archive`; lists archive ids; `cat <archive-id>` reads body text. |
| AI contacts unwritten | `ConsciousnessSave::ai_contacts` schema is populated when a Plan 5 path warrants it (currently: each cracked connected LAN may attach a placeholder AI contact for that station). Full UI deferred to Plan 7. |
| Sgr A\* in-world trigger | **Deferred to Plan 7+.** `:rebirth` remains the in-game path. |
| Multi-Gateway-per-sector ambiguity | `apply_breach_grid` uses gateway-tile `target_node_id`. |
| Fixture-menu vs nmap asymmetry | Every fixture with `JackInPort` tag offers "Jack In" in the fixture interaction menu. The hard-coded PrecursorConsole check at `game_input.cpp:724` becomes `if (h.tags & JackInPort)`. |

---

## 12. Dev console + dynamic LAN regeneration

### Unified `:spawn`

The current `:spawn` (NPCs only) and `:spawn-hackable` (5 hardcoded device kinds) are folded into a single `:spawn` verb. Every spawnable entity routes through one command:

```
spawn <name>                          # auto-detect: NPC role or FixtureType
spawn npc <role>                      # explicit NPC
spawn fixture <FixtureType>           # explicit fixture (auto-Hackable if Electronic)
spawn ice <white|gray|black>          # mid-jack-in only (existing :spawn-ice)
spawn trap <kind>                     # delegates to existing :spawn-trap path
```

Auto-detect order: NPC role lookup first (snake_case match against `create_npc_by_role` keys), then FixtureType lookup (case-insensitive match against the enum). Names that exist in both spaces require the explicit form. Today no collisions exist (NPC roles are snake_case, FixtureTypes are PascalCase).

When `spawn fixture <FixtureType>` runs:
1. Place the fixture on the first passable adjacent tile (existing `:spawn-hackable` placement logic, generalised).
2. If `tags_for_fixture(type) != 0`, attach `Hackable{ tags = tags_for_fixture(type), security_tier = 1 }`.
3. Trigger LAN regeneration (below).

`:spawn-hackable` is **removed** with no alias. The 5 old kind labels (`turret`/`camera`/`door`/`conduit`/`console`) translate to fixture-type spawns: `spawn fixture Console`, `spawn fixture Door`, etc. Per the no-backcompat policy, the old verb is gone.

### Topology-change paths

Hackables can change at runtime in two paths:

- **Production (NPC death; future fixture-deletion):** **metadata-only update.** The Hackable deregisters from `GridNetwork`. The orphan subnet's `⌬` Gateway tile in the LAN sector remains visually but becomes inert — `jack`-ing onto it returns `host unreachable`. No sector regen. **All cracked / looted / decrypted state is preserved** across every sector.
  ```cpp
  void World::on_hackable_removed(GridNodeId subnet_id);
  // Removes the subnet node + its edge from GridNetwork.
  // Does NOT touch LanMetadata.gen_seed, LanMetadata.lan_sector_state, or subnet_states.
  // The orphan ⌬ tile stays in the LAN sector; jack_in returns host-unreachable.
  ```

- **Dev (testing only — `:spawn fixture <Electronic-tagged>`):** **full LAN reset.** Calls `lan_full_reset(map_id)`:
  1. Re-runs the auto-registration sweep (§4) on `map_id`.
  2. Re-allocates `Hackable.ip` in world-coord-stable order.
  3. Re-clusters rooms (k-means re-derive of `LanRoom` list).
  4. Recomputes LAN sector dimensions via `compute_lan_size(N)`.
  5. Bumps `LanMetadata.gen_seed` so the next jack-in regenerates the sector geometry.
  6. **Wipes `LanMetadata.lan_sector_state` and clears `subnet_states`.** All persisted breach / loot / decrypt state for this LAN is lost.
  7. Persists the new `LanMetadata`.

  Documented in `:spawn` help text as a destructive testing-only operation. The dev path is the only place persistence is reset; production gameplay never wipes runtime state.

Production gameplay never regenerates a LAN sector once it has been generated. The "stays hacked" / "stays looted" guarantee is the default.

---

## 13. Migration & save schema

### Save schema bump

`SAVE_FILE_VERSION` bumps `v59 → v60`. **Rejects v59 saves** (no migration shim, per the no-backcompat policy). v60 reserves all fields used through Cuts 1-4; **no further bumps within Plan 5**. Cuts 2-4 fill empty fields without re-bumping.

Removed from save:
- `Hackable.device_kind` (uint8)
- `Hackable.available_qh` (vector<uint16>)

Added to save:
- `Hackable.tags` (uint32 mask)
- `Hackable.ip` (uint32 packed)
- `LanMetadata` per world map, keyed by `(galaxy_id, map_seed, map_kind)`. Full struct from §4 — including `lan_sector_state` and `subnet_states` (both empty until Cut 2 starts populating them).
- `GridNetwork` updated node kinds (new `LanRoot`; `DeepGridAnchor` retained from Plan 4; `RegionalDarknet` retired).

`consciousness.dat` schema bumps `v1 → v2`. **Rejects v1 consciousness.dat saves.** Per the no-backcompat policy, any Plan 4 player loses their consciousness on first Plan 5 launch — every cracked LAN history, every lore archive, every anchor capstone. One-time wipe at the Plan 4→5 boundary; documented in the launch notes for that release.

`consciousness.dat` v2 additions:
- New `deep_grid_base` blob (replaces 30×20 layout with the new 60×40 hand-authored geometry; serialized as the fully-authored sector).
- `deep_grid_sector_state: SectorRuntimeState` for the deep-Grid sector (empty until Cut 3 starts populating).
- `WarpAnchor` list (Atlas region content, one entry per cracked connected LAN ever; empty until Cut 3).
- `ai_contacts` schema (placeholder list, written by Cut 4 paths; full UI deferred to Plan 7).

Save-size budget — measured at Cut 4: ~50KB galaxy save bloat per LAN ($\le$ 250 nodes), $\le$ 5KB consciousness.dat baseline + ~200 bytes per Atlas WarpAnchor ($\le$ 200 anchors before Plan 7 eviction).

### `DeviceKind` retirement migration

`DeviceKind` enum and `device_kind_name` are removed entirely. The dev console's `:spawn-hackable` verb is also removed (folded into the unified `:spawn` — see §12). The old kind labels (`turret`/`camera`/`door`/`conduit`/`console`) are retired permanently — content authors pick `FixtureType` names directly via `:spawn fixture <Type>`.

### Code-path migration order

Phase 1 (cut 1) does both the tag refactor *and* the map-gen content pass in a single coherent commit set. The migration table from §3 is the only data conversion; runtime changes are mechanical replacements of `device_kind ==` with `(tags & ...)` masks.

---

## 14. Fixture audit table

### Hackable fixtures (~18 types)

| FixtureType | Tags | JackInPort? | Notes |
|---|---|---|---|
| `Console` | `Electronic | DataStore` | yes | generic terminal |
| `CommandTerminal` (ARIA) | `Electronic | DataStore` | yes | display name override "ARIA" |
| `ShipTerminal` | `Electronic | DataStore` | yes | board-your-ship terminal |
| `DataTerminal` | `Electronic | DataStore` | yes | settlement knowledge terminal |
| `StarChart` (+L/R) | `Electronic | DataStore` | yes | observatory lore terminal |
| `Door` (electronic only) | `Electronic | Locked` | — | wood doors stay non-hackable |
| `Gate` | `Electronic | Locked` | — | perimeter |
| `Conduit` | `Electronic | PowerNode` | — | engineering pipes |
| `Lamp` | `Electronic | PowerNode` | — | electric lamp |
| `HoloLight` | `Electronic | PowerNode` | — | advanced lighting |
| `Torch` (electronic flame) | `Electronic | PowerNode` | — | only for electric variants |
| `HealPod` | `Electronic | DataStore` | — | medbay |
| `FoodTerminal` | `Electronic | DataStore` | — | vending |
| `WeaponDisplay` | `Electronic | DataStore` | — | commerce |
| `RepairBench` | `Electronic` | — | gear tinker |
| `SupplyLocker` | `Electronic | Locked | DataStore` | — | locked storage |
| `Locker` | `Electronic | Locked | DataStore` | — | advanced storage |
| `RestPod` | `Electronic` | — | crew quarters |
| Precursor `Console` (existing PrecursorConsole) | `Electronic | DataStore | AlienTech` | yes | retains lore + soul-mirror state |

### Hackable on NPCs

- NPC cybernetic implant (`npc.cyber`) when present: `Electronic | Mobile`. JackInPort no.

### Non-hackable (explicit)

Decorative pillars (`ResonancePillar*`, `Plinth`, `Brazier`), brackets (`PrecursorBracketL/R`, `StarChartL/R` are exceptions — they ARE part of hackable star-chart), structural (`Pillar`, `CrystalColumn`), passages (`Door` non-electronic, `DungeonHatch`, `Stairs*`), terrain (`NaturalObstacle`, `ShoreDebris`, `BridgeRail/Floor`), settlement furniture without circuitry (`Table`, `Bench`, `Chair`, `Stool`, `Crate`, `Bunk`, `Rack`, `Shelf`, `BookCabinet`, `CampStove`, `Campfire`, `Kitchen`, `Planter`, `Window`, `Inscription`, `Altar`, `Viewport`, `ShuttleClamp`, `Debris`), terminal box-drawing decorations (`TerminalCornerTop/Side/CornerBot/Junction/Center`), flora, minerals, scrap, `PrecursorButton`, `QuestFixture` (varies — host-driven, not auto-tagged).

### Map-gen content pass — generators touched

- Settlement generator (lights, locks, terminals, vending, lockers).
- Station / overworld station interior (consoles, doors, conduits, lights).
- Asteroid surface + interior (lights, doors, conduits, terminals).
- Ruin generator (electrified Precursor consoles already wired).
- Dungeon generator (sparse — most dungeons stay non-electrical; some mid/late dungeons get a single console + a few cameras for LAN seeding).
- Ship interior (CommandTerminal, ShipTerminal, conduits, lights).
- Crashed ship (Precursor consoles, conduits).
- NPC factories with cybernetic implants (faction-specific).

Each generator's fixture-placement pass attaches `Hackable{ tags = tags_for_fixture(type), security_tier = ... }`. Tier comes from the map's tier (dungeons cap at 2; stations / Precursor go up to 3).

---

## 15. Edge cases & error handling

- **LAN with zero hackables.** No `LanMetadata` allocated. Fixture menu offers no "Jack In". `nmap` reports `nmap: no LAN on this map`. Quickhacks via `.qh` continue to work on individual fixtures (no LAN required for the QH path).
- **Isolated LAN (dungeon).** Generator emits no `⊕` tile. Atlas warp anchor never appears. `nmap -l` shows no `10.X.Y.254` entry.
- **Cracked firewall on revisit.** Persisted as a `SectorMutation` in `LanMetadata.lan_sector_state` (or the relevant `subnet_states[id]`). On revisit the seed-regen produces the original geometry, then `mutations` are applied as overlay — cracked tile shows as `Floor`. No `▒` damaged variant.
- **Looted DataNode / decrypted EncryptedFile on revisit.** Same shape as cracked firewall: `SectorMutation` overlay turns the consumed tile into `Floor`. Player cannot re-loot or re-decrypt.
- **Killed ICE on revisit.** Position recorded in `SectorRuntimeState.killed_ice`; the LAN sector regen omits ICE at those positions. ICE composition for that sector is otherwise deterministic from `gen_seed`.
- **Orphan subnet `⌬` (NPC death).** The Hackable deregisters from `GridNetwork`, but the `⌬` tile in the LAN sector stays — production never regenerates the sector. Walking onto the tile and pressing `Enter` returns "host unreachable"; `nmap -l` no longer lists the IP. State on every other sector is preserved.
- **`:spawn fixture` resets persistence.** The dev path is the only place persistence is destroyed — `lan_full_reset(map_id)` wipes `lan_sector_state` and `subnet_states` entirely. Documented in the help text. Production never hits this.
- **Static tag + runtime no-op.** `bypass_lock` matches an open door's tags (`Locked` is static); the program effect short-circuits to "door already open" without consuming RAM. Displayed in the program log so the player knows.
- **Program filter against multi-tag fixture.** `vector<TagSet>` matches if *any* `TagSet` is fully covered by the device's tags. No AND across the list.
- **Self-anchor entry across rebirths.** `consciousness_id` survives Sgr A\*; the player's deep-Grid base nodes remain self-owned and bypass-able.
- **Past-galaxy Atlas warp attempt.** `WarpAnchor.warpable == false` triggers `jack: target lost — galaxy purged on rebirth`. The tile remains visible (memorial).
- **NPC implants and corpse cleanup.** When an NPC dies, their implant `Hackable` deregisters from the LAN graph via `World::on_hackable_removed`. The IP is freed (recyclable within a LAN's `1..253` space). The orphan subnet's `⌬` tile in the LAN sector stays inert — production does not regenerate sector geometry.
- **Subnet-base collision across maps.** `subnet_base` is derived from `(map_seed >> 8) & 0xFFFF`, so two maps in the same galaxy can theoretically collide. Collision resolution: probe-and-shift the second LAN's `Y` octet by 1 until unique. Stable as long as map_seed is stable.
- **Net-side breach success but server-side failure.** `apply_breach_from_nmap` charges program cost atomically with the `cracked = true` write. Atomic via in-game tick boundary.

---

## 16. Testing

### Unit

- `tags_for_fixture` returns the expected mask for every `FixtureType`.
- `program.target_filter` matches against synthetic `Hackable`s for all 9 programs × ~18 fixture types (~162 cases). Asserts that pre-Plan-5 behaviour is preserved for the 5 existing kinds (turret/camera/door/conduit/console).
- `compute_lan_size` produces expected dimensions for `N = 1, 3, 8, 18, 30`.
- `format_ip` / `parse_ip` round-trip.
- `find_edge_for_gateway` returns the correct edge for each gateway tile in a multi-gateway sector.
- `wall_glyph_for_neighbours` covers all 16 neighbour combinations.

### Integration

- Every `JackInPort` fixture offers "Jack In" in the fixture menu.
- LAN sector generator produces valid sectors (no overlapping firewalls, every Gateway has a target node id, every office has a doorway or breach point) at `N = 1, 3, 8, 18, 30`.
- Save/load round-trip preserves: `LanMetadata`, cracked firewall state, ICE positions, `Hackable.ip`s, deep-Grid Atlas entries.
- Sector traversal LAN ↔ subnet ↔ deep-Grid preserves RAM/Heat/Trace/Soul-Mirror/ICE state.
- `nmap -l` output sorted by IP (== world-coord stable).
- `jack <ip>` resolves correctly; `ping <ip>` is free.
- **Dynamic regen (dev path).** `:spawn fixture Camera` immediately reflects in `nmap -l`; the new IP is allocated; the LAN sector regenerates on next jack-in with the new node count; **`lan_sector_state` and `subnet_states` are wiped** (verified by jack-in showing all firewalls/data-nodes/encrypted-files reset).
- **Production remove path.** NPC death deregisters its implant from the LAN graph; orphan `⌬` tile in the LAN sector remains; jacking onto it returns `host unreachable`; **all other cracked / looted / decrypted state in that LAN persists**.
- **Persistence round-trip.** Crack a firewall, loot a DataNode, decrypt an EncryptedFile, kill an ICE in both the LAN sector and one Subnet sector. Save & quit. Reload. Re-enter both sectors. All four mutations and the ICE absence are present.
- **Past-galaxy persistence.** Cross Sgr A\* with a populated Atlas. Reload after rebirth. Past-life WarpAnchor tiles still visible in the deep-Grid Atlas, dimmed; un-warpable.

### Gameplay

- **Heavens Above run (mid station LAN).** ~8 nodes, 1 ring, connected. Walk in via ShipTerminal → reach `⌬` in security via doorway → enter ops via breach → reach `⊕` → cross to deep-Grid → return via Atlas warp tile → jack out.
- **Isolated dungeon LAN run.** Walk in via dungeon console → reach single vault office → hack 3 subnets → jack out (no `⊕`, no deep-Grid).
- **Rebirth loop.** Cross Sgr A\* → rebirth → spawn in new galaxy → enter deep-Grid (Atlas shows previous-life entries dimmed) → crack new connected LAN → new Atlas entry appears.
- **Quickhack from world.** Walk up to camera in world map, run `reboot_optics.qh` from PDA. Works without entering LAN. Confirms QH path is independent of LAN sector.

### Regression

- Plan 1-4 mechanics (deck, programs, breakpoints, Soul Mirror, lore-fragment commit, neural backup) remain functional with no behaviour change.
- The 9 existing programs still match the same set of fixtures they used to (verified by the migration table).

---

## 17. Implementation plan — Approach B (4 cuts)

Each cut is internally complete and shippable; merge between cuts.

### Cut 1 — Tag refactor + map-gen content pass

- `HackTag` enum + `HackTagMask` typedef + `tags_for_fixture` table.
- `Hackable`: drop `device_kind` + `available_qh`; add `tags` + `ip`.
- `ProgramDef::target_filter`: `vector<DeviceKind>` → `vector<TagSet>`. Migrate the 9 program defs.
- Replace every `device_kind ==` site with `(tags & ...)` mask check.
- Remove `DeviceKind` enum and `device_kind_name`.
- `LanMetadata` struct + auto-registration sweep on map enter.
- Map-gen content pass: every generator that places an electrical fixture attaches a tagged `Hackable`.
- Save schema bump to v60 (galaxy save) + v2 (consciousness.dat). Reject v59 + v1. v60 reserves all Plan 5 fields up front; cuts 2-4 fill empty fields without re-bumping.
- Unified dev console `:spawn` (§12); remove `:spawn-hackable`. Two hooks:
  - `World::on_hackable_removed(GridNodeId)` — production NPC-death path. Metadata-only, preserves persistence.
  - `World::lan_full_reset(map_id)` — dev `:spawn fixture` path. Wipes `lan_sector_state` + `subnet_states`.

**Validation:** existing dev-spawned hackables still work via `.qh`; new map-generated hackables show up in `nmap -l` immediately; `:spawn fixture Camera` adds a hackable, performs full reset, and the new IP appears in `nmap -l`; save/load round-trip clean.

### Cut 2 — LAN sector generator + traversal + multi-Gateway encoding

- `Connector` and `DeepGridGateway` `GridTile`s.
- Box-drawing wall renderer (global; lookup table).
- `LanSectorGenerator` (A+B+E layered; size scales with N).
- `apply_breach_grid` fix: gateway-tile target_node_id.
- Mid-jack-in sector traversal (LAN ↔ subnet, LAN → deep-Grid).
- `SectorRuntimeState` + `SectorMutation` types; tile-mutation overlay applied after seed-regen.
- `LanMetadata.lan_sector_state` and `LanMetadata.subnet_states` populated on tile-state changes (cracked firewall, looted DataNode, decrypted EncryptedFile, killed ICE).
- Persistence round-trip verified: crack/loot/decrypt/kill, save, reload, re-enter sector — all four mutations and the ICE absence are present.
- `JackInPort` tag opens the "Jack In" fixture-menu entry on every tagged fixture.

**Validation:** jack into Heavens Above, walk a generated LAN sector, breach a firewall, enter a subnet, return.

### Cut 3 — Deep-Grid expansion + saved base + galaxy reseed

- New ~60×40 hand-authored deep-Grid sector with Anchor / Atlas / Frontier regions.
- `WarpAnchor` tile + Atlas population on `⊕` first crack.
- `HackingSystem::jack_in` sector dispatch honours `consciousness.dat.deep_grid_base` for self-owned anchors.
- `RebirthSequence::apply` → `Game::start_new_galaxy(fresh_seed)`.
- Past-galaxy Atlas entries flagged `warpable = false`.

**Validation:** crack Heavens Above's `⊕`, see new Atlas entry, jack out and back in via Atlas, rebirth and see past-life entry dimmed.

### Cut 4 — Stitching gaps + nmap/ping/jack/lore

- Rename `netmap` → `nmap`; `GridNetmapWidget` → `GridNmapWidget`.
- IP-driven `jack <ip>`, `ping <ip>`, `nmap [-l] [-m]`.
- `lore` reads `consciousness.dat.lore_archive`.
- `AI contacts` schema population (placeholder for Plan 7 UI).
- `help` / `man` text cleanup — drop all `(stub)` markers.
- Self-anchor entry bypass in nmap widget.
- Netmap-side `b`-key breach (`apply_breach_from_nmap`).

**Validation:** full hacking-tab smoke test with no `(stub)` strings remaining; complete a Heavens Above run end-to-end without dev commands.

---

## 18. Open items deferred from this plan

- **Your.Anchor — full mechanics** (Plan 5 ships v1: spawn-hub + lore-archive `DataNode`; **Plan 7 expands** with stash, customization, AI-contact locus, expanded ownership rules, rebirth-survivor identity surface — designed alongside Plan 6's UI work).
- **Sgr A\* in-world warp trigger** — Plan 7+. `:rebirth` is the in-game path until then.
- **Darknet content** — Plan 7. Frontier zone of the deep-Grid is the stub.
- **AI contact gameplay loop** — Plan 7+. Schema-write only here.
- **Public commerce / forum / marketplace deep-Grid content** — later.
- **Multi-galaxy concurrent state** — never; previous-galaxy Atlas entries are memorial-only.
- **Programs beyond the existing 9** — separate plan.
- **Hot-pursuit between sectors, LAN sub-room labels rendered in-sector, persistent LAN damage UI** — flavour pointers from the handoff that didn't make this cut. Possibly Plan 6 or sprinkled in as polish.

---

## Appendix A — Directory of touched files

Source-of-truth files Plan 5 will reshape:

- `include/astra/hackable.h` — `HackTag` enum, `HackTagMask`, `Hackable` struct
- `src/hackable.cpp` — `tags_for_fixture`, `make_hackable` rewrite
- `include/astra/program.h` — `ProgramDef::target_filter` type change
- `src/program.cpp` — 9 program registry entries with new target_filter shape
- `src/program_effects.cpp` — `apply_breach_grid` fix
- `include/astra/grid_network.h` — add `LanRoot` node kind; retire `RegionalDarknet` + `entry_redirect`; `Subnet` and `DeepGridAnchor` retained
- `src/grid_network.cpp` — auto-registration helpers
- `include/astra/lan.h` (new) — `LanMetadata`, `LanRoom`, `LanFlavour`
- `src/lan.cpp` (new) — auto-registration sweep, room clustering, IP allocation
- `include/astra/lan_sector_generator.h` (new)
- `src/lan_sector_generator.cpp` (new) — A+B+E layered generator
- `include/astra/grid_theme.h` — add `connector` and `deep_grid_gateway` palette entries
- `include/astra/grid_renderer.h` / `src/grid_renderer.cpp` — box-drawing wall rendering, `Connector` and `DeepGridGateway` glyph paths
- `include/astra/grid_nmap_widget.h` (renamed) / `src/grid_nmap_widget.cpp` (renamed) — modes, `b`-key breach, world-geometry layout
- `src/hacking_system.cpp` — `jack_in` sector dispatch, mid-jack-in traversal
- `src/grid_input.cpp` — `⊕` and `WarpAnchor` tile interactions
- `src/grid_sector.cpp` — subnet sector unchanged; regional retired
- `src/grid_anchor_layout.cpp` — replaced by hand-authored deep-Grid base
- `src/consciousness_save.cpp` — `deep_grid_base` blob format, `WarpAnchor` list, `ai_contacts` schema
- `src/rebirth_sequence.cpp` — `Game::start_new_galaxy` call
- `src/pda_hacking_tab.cpp` — `nmap`, `ping`, `jack`, `lore`, `help`, `man`
- `src/game_input.cpp` — fixture-menu `JackInPort` tag check (line 724)
- `src/dev_console.cpp` — unified `:spawn` (npc/fixture/ice/trap subkinds); remove `:spawn-hackable`
- `include/astra/world.h` / `src/world.cpp` — `on_hackable_topology_changed()` hook
- `src/save_file.cpp` — schema v60, `Hackable` (de)serialization, `LanMetadata` (de)serialization
- Map-gen pipeline files: settlement, station, asteroid, ruin, dungeon, ship-interior, crashed-ship — each gets a one-line tag-Hackable attachment.
- `CLAUDE.md` — already updated with the "new electronic fixture" rule.

---

## Appendix B — Glossary

- **LAN**: a per-world-map cyberspace network containing every electrical Hackable on that map.
- **LAN sector**: the procedurally generated cyberspace map you walk when jacked into a LAN.
- **Subnet**: a per-Hackable sub-sector reached via a `⌬` Gateway tile from the LAN sector.
- **Deep-Grid**: the consciousness-scoped universal cyberspace, hand-authored, persists across rebirths.
- **Atlas**: a region of the deep-Grid containing `WarpAnchor` tiles to every cracked connected LAN.
- **Frontier**: firewalled placeholder zones in the deep-Grid reserved for Plan 7 darknet content.
- **JackInPort**: capability tag on fixtures that may serve as the player's entry portal into the LAN.
- **Connector**: decorative impassable tile representing visible bus-trace wiring between LAN offices.
- **Anchor**: a deep-Grid sector tile/region tied to the player's consciousness; bypasses lock predicates.
- **WarpAnchor**: a deep-Grid Atlas tile that warps the player back to a specific LAN.
- **Connected LAN**: LAN with a Deep-Grid Gateway (`⊕`); typically station / asteroid.
- **Isolated LAN**: LAN with no Deep-Grid Gateway; typically dungeon.
- **`HackTagMask`**: bitfield of capability tags on a Hackable.
- **`TagSet`**: alias for `HackTagMask` used at the program-filter site to read more naturally as "AND-within".

