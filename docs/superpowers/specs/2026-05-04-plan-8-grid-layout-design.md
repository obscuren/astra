# Plan 8 — Grid Layout / Generator redesign

**Date:** 2026-05-04
**Status:** Spec — pending implementation plan.
**Audience:** Implementation planner (`writing-plans` skill) and contributors picking up the LAN-sector generator work.
**Supersedes:** the Plan 5 sector-layout decisions (firewall ring + organic offices + A* connectors).

---

## 0. TL;DR

The current LAN sector generator produces a functional but illegible "rectangles + A\* corridors inside a firewall ring" layout. Plan 8 reshapes it into a packed cluster of **independently-walled rooms connected by short bridges**, organized into **named zones** with visible tier progression. The two-tier model (LAN sector + per-subnet sector) collapses into **one flat sector** that holds everything: ICE, datanodes, encrypted files, device avatars, all in their own room. `gen_subnet_sector` is retired. `breach.exe` becomes door-only — every door is locked unless explicitly open.

---

## 1. Goals

1. **Spatial legibility.** A glance at the sector tells the player what's going on — which rooms exist, which are dangerous, where the boundaries are.
2. **Visible progression.** Crossing tier-2 → tier-3 should feel structural and earned, not just colored differently.
3. **One mental model.** Player jacks in once, walks one map, jacks out. No "you stepped on a gateway, now you're in a different sector" moment.
4. **Per-room identity.** A turret room reads differently from a data room from a healpod from a precursor shrine.
5. **Lobby as a place.** Every LAN has a hub the player can recognize as the LAN's main node — a "central hallway" diegetically equivalent to the LAN root in `GridNetwork`.
6. **Preserve existing data flow.** `LanMetadata` already groups subnets into offices; the new generator reuses that data, renamed and re-rendered.

---

## 2. The current model — what gets replaced

### 2.1. Two-tier sector geography

- **`generate_lan_sector`** (`src/lan_sector_generator.cpp`) — produces the LAN-level sector you arrive in when jacking into a connected map. Contains rooms (offices), subnet gateway tiles `⌬`, deep-grid gateway `⊕`, exit `⊙`, A\* corridors. **No ICE. No DataNodes. No EncryptedFiles.**
- **`gen_subnet_sector`** (`include/astra/grid_sector.h` + corresponding `.cpp`) — produces the per-device sector you enter when stepping on `⌬`. 28×14 hand-shaped layout. **This is where ICE patrols, DataNodes, EncryptedFiles, and the wall-mounted DeviceAvatar live.**

Stepping on `⌬` swaps the active sector to the subnet sector. Stepping on `⊙` jacks out of either tier.

### 2.2. What's wrong with it

| Complaint | Root cause |
|---|---|
| Spatial illegibility | Variable-size rectangles drift in a sea of empty Floor; A\* connectors meander; rooms blur. |
| No sense of progression | Tier-1 rooms have a 1-tile doorway, tier-2/3 are sealed. No visible boundary between zones. |
| Two-tier is confusing | Player has to learn "this tile drops me into a different sector"; ICE invisible until you commit. |
| Per-room identity weak | A turret office and a data office look the same from the outside. |

### 2.3. What's retained

- `GridNetwork` — the logical graph of LAN nodes. Untouched.
- `LanMetadata` — gets a struct rename (`LanRoom` → `LanZone`) and one new field (zone name banner). Otherwise stable.
- `HackTagMask` taxonomy from Plan 5 — drives template selection.
- `Cell`, tile rendering pipeline, camera, HUD chrome from Plan 6. Untouched.

---

## 3. The new model

### 3.1. Flat tier collapse

**Decision:** the LAN sector contains everything. `gen_subnet_sector` is removed; subnet gateways `⌬` no longer exist as a tile type; subnet sectors no longer exist as a distinct concept.

**Implications:**
- Every `GridNodeKind::Subnet` node maps directly to a **room** in the LAN sector.
- ICE, DataNodes, EncryptedFiles, DeviceAvatars all live inside subnet rooms.
- `WorldManager::on_jack_in(node)` resolves to a (sector, spawn_x, spawn_y) tuple where the sector is the LAN sector and (spawn_x, spawn_y) is the room-specific spawn.
- The `GridTile::Gateway` tile is **removed from the enum**. `gateway_target` map is removed from `GridSector`. Replaced by per-node spawn lookup (§ 8).
- `sec.spawn_x/y` is now the *lobby* spawn — used when jacking into the LAN root or when a per-node lookup fails.

### 3.2. Sector layout idiom

Each LAN sector is a **packed cluster of independently-walled rooms** connected by short bridges.

**Tile vocabulary (post-Plan-8):**

| Tile | Glyph | Purpose |
|---|---|---|
| `Floor`         | `░` | Walkable interior of a room or corridor |
| `Wall`          | (none) | Void / out-of-room background; impassable; not rendered |
| `Firewall`      | `▓` | Room perimeter walls — magenta, impassable, only crossed via `Door` |
| `Door`          | `+` (open) / `▣` (locked) | The bridge tile + the room's wall openings |
| `DataNode`      | `$` | Looted by `nmap`/program use |
| `EncryptedFile` | `⊘` | Decrypted via `decrypt.exe` |
| `ExitNode`      | `⊙` | Jack-out tile, lives in the lobby |
| `DeepGridGateway` | `⊕` | One-way to deep-Grid (placed in sanctum zone) |
| `WarpAnchor`    | `◉` | Atlas tile (deep-Grid only — irrelevant here) |
| `DeviceAvatar`  | (varied per FixtureType) | Each subnet room has its mounted avatar at a designated wall-position |

**Removed:** `GridTile::Gateway` (the `⌬` subnet-pointer tile) — subnets are now rooms, not gateway tiles.

**Geometry rules:**
- Each room is a complete enclosure: floor interior surrounded by wall perimeter.
- Adjacent rooms placed with a **1-cell gap** between their walls (no shared walls anywhere).
- Out-of-room space is `Wall` (background void) — *not* floor.
- Sector outline is **jagged** — the sector's bounding rectangle is whatever the rooms collectively occupy, not a clean rectangle.

**Room shapes:**
- Default: rectangles, 4×3 minimum to 12×8 maximum.
- Optional **shape mutation pass** after placement: notch one corner, extend one wall by 1–2 cells, knock out a 1-cell bump. Cosmetic; preserves valid bounding box.
- L-shaped rooms allowed for the lobby and zone-anchor rooms (a single room with an inner notch).

### 3.3. Zone concept

Subnets group into **zones** (was: offices). Each zone:

- Has a **tier** (1, 2, or 3) — drives floor color, banner color, door-lock cost.
- Has a **name** carried over from `LanMetadata` (e.g., "Operations", "Vault", "Lobby"). Default fallback: `T1 ZONE` / `T2 ZONE` / `T3 ZONE`.
- Contains **N subnet rooms** placed in a contiguous spatial cluster.
- Is rendered with:
  - A faint **dashed perimeter** in the zone-tier color (rendered between floor layer and content layer).
  - A **banner text** above the topmost row of the zone's bounding box: `— OPERATIONS (T2) —`.
  - Optional **floor tint** per zone — slightly different floor color shade (T1 cool blue, T2 magenta, T3 warm red).

**Zone count per LAN:**
- Tiny LAN (1–3 subnets): 1 zone (everything T1, including the lobby).
- Small LAN (4–7 subnets): 1–2 zones.
- Medium LAN (8–14 subnets): 3 zones (one per tier).
- Large+ LANs: 3 zones, tiered T1/T2/T3.

Zone count = `min(3, distinct_tier_count(LanMetadata.rooms))`. Lobby is always its own (T1) zone or part of the T1 zone.

### 3.4. Door semantics

**Doors are the only way to traverse rooms.** `breach.exe` is door-only — it cracks the lock on a `Door` tile. There are no more breachable wall tiles.

**Door states:**
- **Open** (`+`, cyan) — passable freely.
- **Locked** (`▣`, orange) — passable only after `breach.exe` runs successfully against the door tile. Cracking flips state to open. Costs Trace + heat per the Plan 5 economy (carry over costs unchanged).

**Default state rule:** **all doors are locked unless explicitly marked open.**

**Default-open exemptions:**
- Bridges *within* a single zone — open by default. Walking around an Operations zone shouldn't require breaching every door.
- Lobby's outer doors — lobby is the entry point; its doors to T1 sub-rooms are open. (Note: locked-by-default still applies to lobby ↔ T2-zone choke.)

**Default-locked cases:**
- The single bridge between two zones (T1↔T2 choke, T2↔T3 choke).
- Doors into rooms that contain particularly valuable content — TBD by template (§ 5). E.g., `DataStore` rooms with multiple `$` clusters can opt into a locked door.

**Bridge tile model:** a bridge is a **3-tile passage at the door row**:

```
        col x   x+1   x+2
row y:   .      +     .         ← floor opening, BRIDGE (door tile), floor opening
```

- (x, y): the wall opening on Room A's right wall — was `Wall`, now `Floor`.
- (x+1, y): the **bridge tile** — `Door` (open or locked).
- (x+2, y): the wall opening on Room B's left wall — was `Wall`, now `Floor`.

Player walks A.interior → A.opening → bridge → B.opening → B.interior — five tiles of passage including the two interior endpoints.

**Long bridges** (when rooms can't be placed 1 cell apart): stretch to 5 or 7 tiles by inserting corridor `Floor` between the openings and the door tile. Generator avoids long bridges when possible.

---

## 4. Generator algorithm

Replaces `generate_lan_sector` and removes `gen_subnet_sector`.

### 4.1. Inputs

```cpp
struct LanGenInput {
    LanMetadata     meta;        // rooms[] (now zones), nodes_total, gen_seed, etc.
    GridNetwork     net;         // graph for resolving DeepGridGateway target
    GalaxyContext   ctx;         // for naming, lore-flavored zone banners
};
```

### 4.2. Phase 1 — Sector dimensions

Compute sector size from total subnet count:

| Subnets | Sector dims (rough) | Notes |
|---|---|---|
| 1–3   | 30×16   | Tiny LAN, 1 zone |
| 4–7   | 50×22   | Small, 1–2 zones |
| 8–14  | 72×28   | Medium, 3 zones |
| 15–25 | 96×34   | Large, 3 zones |
| 26–35 | 120×42  | Max, 3 zones — capped to keep traversal sane |

```cpp
LanSizeParams compute_lan_size(int subnet_count, const LanMetadata& meta) {
    LanSizeParams p;
    p.zone_count = std::min<int>(3, distinct_tier_count(meta.rooms));
    p.has_lobby  = true; // invariant (§ 7)
    // size scales sublinearly with subnet count;
    // see Plan 8 size table.
    p.width  = clamp(...);
    p.height = clamp(...);
    return p;
}
```

### 4.3. Phase 2 — Zone partition

Partition the sector into 1–3 spatial **zone regions**:

- **1 zone:** entire sector is the single zone.
- **2 zones:** sector split horizontally (T1 left ~40%, T2 right ~60%).
- **3 zones:** sector split horizontally into thirds (T1 ~30% left, T2 ~40% middle, T3 ~30% right). Adjusted by zone density (a zone with more subnets gets proportionally more area).

Each zone region gets an **anchor point** — a chosen seed cell within the region where the zone's anchor room (the lobby, for T1; the deepest/most important room, for T2 and T3) gets placed first.

Zone regions are *not* drawn as walls — they're a generator-time concept. The dashed-perimeter render (§ 9) is computed at draw-time from the rooms' tier metadata.

### 4.4. Phase 3 — Per-zone room placement

For each zone:

1. Place the **zone-anchor room** at the zone's anchor point. For T1, this is the **lobby** — oversized (10×6 minimum), positioned so the player has visual access to multiple T1 sub-rooms from the spawn point.
2. Place **subnet rooms** one at a time:
   - Pick footprint from the room's tag-themed template (§ 5), respecting per-template min/max size (§ 6).
   - Place via greedy rejection sampling within the zone's region, requiring ≥1-cell gap from any other room.
   - Record the room's subnet GridNodeId, footprint, and chosen template.
3. After all rooms placed, optionally apply the **shape mutation pass** (§ 3.2 — notch one corner, extend wall, etc.).
4. Record the zone's bounding-box (used by HUD render § 9).

Failure mode: if a zone can't fit all its subnet rooms after N attempts, downsize the largest non-anchor rooms by 1 cell and retry. If still failing, drop overflow subnet(s) (very rare; logs a warning).

### 4.5. Phase 4 — Connectivity & door carving

For each zone independently:

1. Build a connectivity graph among the zone's rooms. Use **Prim's algorithm** with edge weights = Manhattan distance between room centers, to produce a minimum spanning tree.
2. Allow **bonus edges** for the **lobby and zone-anchor rooms only** — these can have up to 3 incident edges. Pick the 2 next-shortest edges from the anchor and add them as bonus edges (Q-room-degree decision: option C).
3. For each tree edge, **carve a bridge** (§ 3.4):
   - Pick the closest pair of facing wall cells (one per room) along the cardinal axis.
   - Stamp the bridge tile + the two wall openings.
   - **Within-zone bridges:** mark door as **open** by default (with template-driven exceptions).
4. Connect zones with **exactly one inter-zone bridge** per adjacent pair. Pick the closest pair of rooms across the zone boundary (one in each zone). Carve the bridge; **mark the door locked** (▣).
   - T1↔T2 choke: locked, standard breach cost.
   - T2↔T3 choke: locked, higher breach cost (Trace +X, heat +Y — exact values match what `breach.exe` already charges for tier-3 targets in Plan 5; no new economy).

### 4.6. Phase 5 — Special tile placement

1. **ExitNode `⊙`** — placed in the lobby room, at the cell furthest from the lobby's outgoing bridge (so quick re-jack from the spawn is easy).
2. **DeepGridGateway `⊕`** — placed in the **highest-tier zone's anchor room** (typically the T3 sanctum's "server core" or equivalent). Connected LANs only (`meta.connected == true`).
3. **DeviceAvatar tiles** — each subnet room places one DeviceAvatar tile on its perimeter wall, themed by the source `FixtureType` (Plan 5 Cut 2.6 logic carries over — see `src/grid_renderer.cpp` `device_avatar_glyph`).
4. **Per-node spawn registration** — for each subnet room, register one interior `Floor` cell (centered, away from doors) as the room's `per_node_spawn` (§ 8).

### 4.7. Phase 6 — Template fill (per-tag decoration)

For each subnet room, apply its tag-themed template (§ 5) to populate interior content:
- ICE patrols
- DataNodes / EncryptedFiles
- Decoration tiles (template-specific glyphs / floor textures)
- Optional locked-door overrides on the room's incoming bridges

Templates run *after* connectivity so they have access to door positions and can avoid placing content directly on a doorway.

### 4.8. Determinism

All RNG seeded from `meta.gen_seed`. Same seed → identical sector. Required for save/load consistency.

---

## 5. Tag-themed templates

Astra has many `FixtureType` values, but cyberspace only cares about the `HackTagMask` capability bits (Plan 5). Templates are keyed off the **dominant tag** of a room's source fixture. Seven categories:

| Category | Required tag(s) | Template feel | Min footprint | Default ICE | Default content |
|---|---|---|---|---|---|
| **WeaponizedRoom**     | `Weaponized`           | Combat arena. Scorched-floor texture, weapon-display tiles on walls. | 8×6 | 1× white ICE + 1× gray ICE | weapon-display decor |
| **SurveillanceRoom**   | `HasOptics`            | Lens/camera vibe. Round-ish footprint (notched corners), camera-glyph fixtures. | 6×5 | 1× white ICE | 1× DataNode (footage) |
| **DataVaultRoom**      | `DataStore`            | Grid-pattern floor, stacked `$`/`⊘` clusters. Often locks its incoming door. | 7×5 | 1× gray ICE | 2–4× DataNode + 1–2× EncryptedFile |
| **PowerNodeRoom**      | `PowerNode`            | Pulsing-floor decor, central conduit `≈` glyph. | 5×4 | none | conduit decor |
| **CrewQuartersRoom**   | `Mobile` (NPC)         | Sparse floor, dialog-NPC avatar, less ICE. (Plan 9 will surface conversation here.) | 6×4 | none | NPC avatar tile |
| **PrecursorShrineRoom**| `AlienTech`            | Irregular footprint. Alien glyphs, locked-by-default doors, requires `decode.exe` for any DataNode. | 8×6 | 1× black ICE (T3 only) | precursor `※` decor + 1× DataNode |
| **GenericRoom**        | `Electronic` only      | Small, sparsely-decorated fallback for devices with no special tag. | 4×3 | none | minimal |

**Tag override priority:** if a fixture has multiple tags, pick the most "spectacle" tag in this order — `AlienTech` > `Weaponized` > `DataStore` > `HasOptics` > `Mobile` > `PowerNode` > `Electronic`. Single decision per room, no template blending.

**Authoring footprint:** each template is a small data structure declaring (min size, max size, ICE seed list, content seed list, optional decoration patterns). One template per category × ~2 layout variants = 14 templates total. All in `src/grid_room_templates.cpp` (new file).

---

## 6. Room sizing rules

| Aspect | Rule |
|---|---|
| Hard min | 4×3 (no room can be smaller than this) |
| Hard max | 12×8 |
| Combat tags (`Weaponized`, `AlienTech`) | min 8×6 (Q7 decision: enough patrol space) |
| Data-heavy (`DataStore`) | min 7×5 |
| Lobby | min 10×6, max 14×8 (oversized; T1 anchor) |
| Default | 5×4 to 8×6 randomized |

If a zone region can't fit a room at its template's min size, the room **fails to place** and is dropped (logged). Sector size growth (§ 4.2) prevents this in practice.

---

## 7. Lobby invariant

Every LAN sector contains exactly one **lobby**:

- Always present (even on a 1-subnet LAN — minimum 2 rooms: lobby + that subnet).
- Always **T1**, regardless of subnet tier mix.
- Acts as the **T1 zone anchor**.
- Hosts the **`⊙` ExitNode** at its furthest-from-bridge cell.
- Default jack-in spawn for the LAN root or any failed `per_node_spawn` lookup.
- Oversized: min 10×6, larger than any other room.
- Visually marked: floor banner text or central marker (TBD; suggest centered `LAN.LOCAL` glyph row painted on the floor — implementation detail, can land in template work).
- Open doors to all direct-connected T1 sub-rooms.

The lobby corresponds to the LAN root `GridNode` (`GridNodeKind::Lan`). It is **not** stored as a separate `LanRoom` / `LanZone` entry — it's the implicit T1 anchor stamped first during generation.

---

## 8. Per-node jack-in spawn

`Game::jack_in(target_node)` must place the player **inside the target subnet's room**, not at the LAN root.

**Data model:**

```cpp
struct GridSector {
    // ... existing fields ...
    int spawn_x = 0;  // lobby spawn (LAN root / fallback)
    int spawn_y = 0;
    // NEW (replaces gateway_target):
    std::unordered_map<GridNodeId, std::pair<int,int>> per_node_spawn;
};
```

**Generator responsibility (Phase 5):** for each subnet room, register one safe interior `Floor` cell (typically room center, ≥2 cells from any door) as that node's spawn point.

**Lookup fallback:**
```cpp
auto spawn_for(GridNodeId node) const -> std::pair<int,int> {
    auto it = per_node_spawn.find(node);
    if (it != per_node_spawn.end()) return it->second;
    return {spawn_x, spawn_y};  // fall back to lobby
}
```

**Re-jack semantics** (re-entering a previously-visited LAN, jacking into the same node a second time): looks up `per_node_spawn` again — same room, same tile, every time. Tile mutations (cracked doors, looted DataNodes) persist via existing `SectorState` carryover.

---

## 9. Zone HUD rendering

A new render layer in `grid_renderer.cpp`'s playfield draw, between the floor pass and the content pass.

**Per zone:**

1. **Dashed perimeter rectangle** drawn around the zone's bounding-box (in sector coordinates, transformed by camera). Color = zone-tier color (T1 `#5577cc`, T2 `#cc66cc`, T3 `#ee5555`). Style: dashed (3-on, 2-off).
2. **Banner text** rendered above the zone's top edge: `— OPERATIONS (T2) —` or `— LOBBY —` for the T1 zone. Color matches the dashed-perimeter color. Font: same as the rest of the Grid HUD.

**Camera-aware visibility:**
- Render only zones whose bounding-box intersects the visible playfield + a small margin.
- A tiny LAN with 1 zone shows no banner (banners assume 2+ zones to make sense). Configurable threshold.

**Implementation locus:** new helper `draw_zone_overlay(Renderer&, const LanMetadata&, const GridSector&, const GridCamera&)` called from `draw_playfield` after the floor pass and before the ICE/avatar pass.

---

## 10. Save format / schema bump

**Schema bump: v62 → v63.**

**Breaking changes:**
- `GridSector::tiles` no longer contains `GridTile::Gateway` values (legacy field).
- `GridSector::gateway_target` field is **removed**.
- New field: `GridSector::per_node_spawn` (serialized as a list of `{node_id, x, y}`).
- `LanMetadata::rooms` → `LanMetadata::zones` (rename + a new `name` string field).
- `GridTile` enum: `Gateway` removed. Existing serialized values must be migrated (any old `Gateway` tiles in saved sectors → reject the save).

**Migration policy:** **no backward compatibility.** Per the user's stated norm (no backcompat pre-ship), v62 saves are rejected at load with a clean message ("save schema too old; start a new game"). No migration shim.

**`consciousness.dat` impact:** none. The deep-Grid base stays hand-authored (`make_deep_grid_base()`) and is unaffected. (Plan 9 redesigns it later.)

---

## 11. Migration & retirement

**Files retired:**
- `src/lan_sector_generator.cpp` (replaced wholesale).
- `src/subnet_sector_generator.cpp` (or wherever `gen_subnet_sector` lives — confirm during plan-writing).
- All references to `GridTile::Gateway` and `gateway_target` purged.

**Files modified:**
- `include/astra/grid_sector.h` — remove `Gateway` enum, `gateway_target` field, `gateways` field. Add `per_node_spawn`.
- `include/astra/lan.h` — rename `LanRoom` → `LanZone`, add `name` field.
- `src/lan.cpp` — naming update, `zone_for_subnet()` helper.
- `src/grid_renderer.cpp` — drop Gateway rendering branch, add zone overlay layer.
- `src/hacking_system.cpp` (or wherever jack-in is wired) — use `per_node_spawn` lookup.
- `src/world_manager.cpp` — drop two-tier sector swap logic; jack-in always lands in the LAN sector.
- `src/save_load.cpp` — schema v63, drop legacy fields.

**Files added:**
- `src/lan_sector_generator.cpp` (new) — Phase 1–6 generator.
- `src/grid_room_templates.cpp` (new) — 7 templates, 14 variants.
- `include/astra/grid_room_templates.h`.
- `include/astra/grid_zone_overlay.h` + corresponding `.cpp` (or inline into `grid_renderer.cpp` if small).

---

## 12. Testing strategy

**Unit tests (gtest):**
- `compute_lan_size()` produces sane dimensions across tiny → max subnet counts.
- Room placement respects min footprints and 1-cell gap rule. Property test: generate 100 sectors per size band, assert no two rooms touch.
- Connectivity graph is connected within each zone; exactly N-1 inter-zone bridges for N zones.
- Determinism: same seed → identical tile array.
- Per-node spawn lookup never falls outside a Floor tile.
- Lobby invariant holds for every generated sector.

**Visual regression:**
- Render 5 representative LAN configurations to PPM (or terminal capture) and diff against committed golden images. Re-bless on intentional layout changes.

**Integration:**
- Jack into each subnet of a sample LAN; verify spawn is inside that subnet's room.
- `breach.exe` against a locked door flips it to open; door is now passable.
- `:rebirth` generates a fresh LAN with a different seed; sectors look different but all invariants hold.

**Manual:**
- Walk the sample LANs in dev mode (`-DDEV=ON`); confirm zone banners read correctly, spatial progression feels right.
- Verify the cluttered Oraine-style packing is preserved across sizes.

---

## 13. Open issues / TBD (non-blocking for plan)

These are spec-clarification questions that the implementation plan or the implementer can resolve without re-brainstorming:

1. **Lobby floor banner glyph** — should the lobby have a center floor decoration like `LAN.LOCAL` or a proper `Lobby` ASCII banner? Implementation detail; pick during template work.
2. **Floor color tint per zone** — exact RGB for T1/T2/T3 floor tints. Match the dashed-perimeter colors; pick at template time.
3. **Long-bridge fallback path-finding** — if 1-cell-gap placement fails for two rooms that must be connected, exactly how does the longer corridor get carved? Default suggestion: orthogonal L-shape, prefer shorter path. Implementer's call.
4. **Zone region boundaries with non-rectangular zones** — Phase 2's "horizontal third split" works for typical LANs but not for unusual aspect ratios. Defer; current heuristic is fine.
5. **Save schema field-name finalization** — `per_node_spawn` is a working name; pick at implementation time.

---

## 14. Out of scope (defers to other plans)

- **AI-contact dialog inside Grid rooms.** Plan 9 (Anchor v2 + AI contacts) — `Mobile`-tagged NPC rooms get dialog rendering then.
- **Deep-Grid sector redesign.** Plan 9 — Anchor v2 reshapes `make_deep_grid_base()`.
- **Frontier zone content.** Plan 11 polish.
- **Long-channel CLI commands inside Grid rooms.** Plan 7 (Device Shells) handles this orthogonally.
- **Real-body damage while jacked.** Plan 11.
- **Galaxy-survival warp tile interactions.** Plan 10.

---

## 15. Cross-references

- Roadmap: `docs/plans/2026-05-03-plan-7-roadmap.md`
- Plan 5 spec (current model being replaced): `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- Plan 5 plan: `docs/superpowers/plans/2026-05-01-grid-expansion.md`
- Plan 6 HUD spec (zone overlay layer integrates here): `docs/superpowers/specs/2026-05-02-grid-hud-design.md`
- Plan 7 spec (Device Shells, runs orthogonal to Plan 8): `docs/superpowers/specs/2026-05-01-device-shells-design.md`
- Hacking mechanics: `docs/mechanics.md` § "Hacking — Plan 5"
- Brainstorm visual reference (algorithm A vs B): `/tmp/plan_8_algo_compare.html`

---

## 16. Decision log (locked during brainstorm)

| Q | Decision | Rationale |
|---|---|---|
| Critique addressed | Spatial illegibility (A) + no progression (D) + Oraine-style packing (E) | User callouts |
| Per-room identity | B + C — tag themes + zone progression | Both legibility and progression |
| Subnet model | B — one subnet = one room | Cleanest mapping; matches the "rooms as places" reference |
| Tier collapse | A — flat | One mental model; ICE visible through doors; drops a layer of confusion |
| Zone boundaries | B — soft barrier, locked door at choke | Visible structural progression; reuses breach.exe |
| Algorithm | B — zone-anchored region grow | Required for spatial cluster legibility; A's scatter buys nothing the player perceives |
| Identity richness | B — tag-themed templates (~7 categories) | Sweet spot vs A's flat content-only or C's vault library |
| Door semantics | All locked unless explicitly open; breach.exe is door-only | User decision; simplifies tile vocabulary (no breachable walls) |
| Lobby invariant | Always present, T1 anchor, oversized, hosts ⊙ | User decision |
| Per-node spawn | New `per_node_spawn` map; lobby fallback | Required for flat-model jack-in to land in correct room |
| Zone HUD render | Dashed perimeter + banner per zone | User decision; unique to Grid HUD |
| Room degree | C — internal tree per zone, lobby + anchors can have 2–3 bridges | Matches Oraine vibe; not all rooms feel like dead-ends |
| Combat geometry | C — per-template min sizes for combat tags; reasonable mins for the rest | Non-combat rooms still readable; combat rooms not cramped |
| `LanRoom`/office | A — keep grouping, rename to `LanZone`, add `name` | Named zone banners ("Operations") read better than generic |
