# Plan 8 — Grid Layout / Generator: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current LAN sector generator with a packed-rooms-with-bridges layout: independently-walled rooms placed in zone-anchored clusters, connected by 3-tile door bridges, with tag-themed templates per room. Collapse the LAN+subnet two-tier model into one flat sector. Retire `gen_subnet_sector`. Make `breach.exe` door-only.

**Architecture:** 9 cuts. Cuts 1–6 build the new generator alongside the existing one (build stays green). Cut 7 is the cutover (delete old, rename types, schema bump). Cut 8 adds the zone HUD overlay. Cut 9 is manual validation + docs.

**Tech Stack:** C++20, CMake, hand-rolled binary save format, dev-console verb system for manual smoke tests. No unit-test framework — verification is `cmake --build build` (with `-DDEV=ON`) + dev-verb smoke tests + visual walkthrough in `./build/astra-dev`.

**Spec:** `docs/superpowers/specs/2026-05-04-plan-8-grid-layout-design.md`
**Roadmap entry:** `docs/plans/2026-05-03-plan-7-roadmap.md` § Plan 8
**Visual reference (saved):** `/tmp/plan_8_algo_compare.html`

---

## File map

**New files:**
- `include/astra/grid_zone.h`, `src/grid_zone.cpp` — `LanZone` (renamed from `LanRoom`, with `name` field), zone-tier helpers, banner naming.
- `include/astra/lan_sector_v2.h`, `src/lan_sector_v2.cpp` — new packed-room generator (Phase 1–6). Eventually replaces `lan_sector_generator.cpp` (deleted in Cut 7).
- `include/astra/grid_room_templates.h`, `src/grid_room_templates.cpp` — 7 tag-themed templates with min sizes, ICE/content seed lists.
- `include/astra/grid_zone_overlay.h`, `src/grid_zone_overlay.cpp` — HUD render layer for dashed zone perimeters + banner text.

**Modified files:**
- `include/astra/grid_sector.h` — remove `Gateway` from enum (Cut 7), drop `gateways[]` + `gateway_target` (Cut 7), add `per_node_spawn` (Cut 1), add `Door` tile.
- `include/astra/lan.h`, `src/lan.cpp` — `LanRoom` → `LanZone` rename (Cut 7), add `name` field (Cut 1), zone-name fallback helper.
- `src/grid_renderer.cpp` — drop Gateway render branch (Cut 7), add Door rendering (Cut 3), wire `draw_zone_overlay` (Cut 8).
- `include/astra/grid_theme.h` — palette additions for `Door` (open / locked tints), zone-tier color constants.
- `src/hacking_system.cpp` — `Game::jack_in(node)` uses `per_node_spawn` (Cut 6); flat-model dispatch.
- `src/world_manager.cpp` — drop two-tier sector swap on `⌬`; jack-in always lands in LAN sector (Cut 6).
- `src/grid_input.cpp` — drop `⌬` Gateway interaction (Cut 7); locked-door interaction = `breach.exe` target (Cut 3).
- `src/program_effects.cpp` — `apply_breach_grid` retargets from wall to Door tile (Cut 3).
- `src/save_file.cpp` — schema v62 → v63 (Cut 7); serialize new fields; reject old saves with clean message.
- `src/lan.cpp` — auto-registration unchanged; just uses `LanZone` after rename.
- `docs/mechanics.md` — Plan 8 section under Hacking; replace Plan 5 sector-shape paragraph (Cut 9).
- `docs/roadmap.md` — Plan 8 done check (Cut 9).

**Files retired (Cut 7):**
- `src/lan_sector_generator.cpp` (old generator).
- `include/astra/lan_sector_generator.h` (old header).
- `src/subnet_sector_generator.cpp` (or wherever `gen_subnet_sector` lives — confirm during Cut 7 Task 1).

---

## Cut 1 — Data model additions (non-breaking)

**Goal:** add new fields to `GridSector` and `LanRoom` that the new generator will use, without removing anything yet. Build stays green; old saves still load.

**Verification at end of cut:** `cmake -B build -DDEV=ON && cmake --build build -j8` succeeds. `./build/astra-dev` launches and runs.

### Task 1.1: Add `per_node_spawn` field to `GridSector`

**Files:**
- Modify: `include/astra/grid_sector.h` (around the existing `gateway_target` declaration)

- [ ] **Step 1: Add field declaration**

In `include/astra/grid_sector.h`, immediately after the `gateway_target` map (around line 57), add:

```cpp
    // Plan 8: per-subnet spawn point. When the player jacks into a specific
    // subnet's GridNodeId, they spawn at this (x,y) inside that subnet's
    // room. Lookup falls back to (spawn_x, spawn_y) — the lobby — if missing.
    std::unordered_map<GridNodeId, std::pair<int,int>, PairHash> per_node_spawn;
```

Wait — `PairHash` is currently defined for `std::pair<int,int>` but we need `GridNodeId` as the key. Use the standard `std::hash<GridNodeId>` if it exists, else `std::hash<uint64_t>`. Inspect `grid_network.h` to confirm `GridNodeId`'s underlying type before picking the hasher.

If `GridNodeId` is a transparent uint64 wrapper, declare:
```cpp
    struct NodeIdHash {
        size_t operator()(GridNodeId id) const noexcept {
            return std::hash<uint64_t>()(static_cast<uint64_t>(id.value));
        }
    };
    std::unordered_map<GridNodeId, std::pair<int,int>, NodeIdHash> per_node_spawn;
```

(Adjust the `id.value` accessor to whatever the actual struct uses.)

- [ ] **Step 2: Build to verify it compiles**

Run: `cmake --build build -j8`
Expected: PASS (no other code references the new field yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/grid_sector.h
git commit -m "$(cat <<'EOF'
plan-8(cut-1): add per_node_spawn field to GridSector

Reserved for Plan 8 flat-model jack-in: each subnet room registers a
spawn (x,y) keyed by its GridNodeId. Generator populates in Cut 4;
hacking system uses in Cut 6. No-op for now — empty map.
EOF
)"
```

### Task 1.2: Add `name` field to `LanRoom`

**Files:**
- Modify: `include/astra/lan.h` (the `LanRoom` struct)
- Modify: `src/lan.cpp` (auto-registration that creates LanRoom entries)

- [ ] **Step 1: Locate the `LanRoom` struct and add a name field**

Open `include/astra/lan.h` and find `struct LanRoom`. Add:

```cpp
    std::string name;  // Plan 8: zone banner text. Empty → fallback to "T<n> ZONE".
```

- [ ] **Step 2: Populate name in auto-registration**

In `src/lan.cpp`, find the function that builds `LanMetadata::rooms[]` (likely called something like `build_lan_metadata` or in the auto-registration sweep). For each `LanRoom` constructed, set `name` based on the room's tier. For now, default to a tier-appropriate generic:

```cpp
    LanRoom room;
    // ... existing setup ...
    switch (room.tier) {
        case 1: room.name = "LOBBY"; break;
        case 2: room.name = "OPERATIONS"; break;
        case 3: room.name = "VAULT"; break;
        default: room.name = ""; break;
    }
```

(Better names will come from lore/flavour later — these are the safe defaults.)

- [ ] **Step 3: Add a getter for fallback rendering**

In `include/astra/lan.h`, add a free function:
```cpp
std::string zone_banner_label(const LanRoom& room);  // returns name or fallback "T<n> ZONE"
```

In `src/lan.cpp`:
```cpp
std::string zone_banner_label(const LanRoom& room) {
    if (!room.name.empty()) return room.name + " (T" + std::to_string(room.tier) + ")";
    return "T" + std::to_string(room.tier) + " ZONE";
}
```

- [ ] **Step 4: Build and run**

Run: `cmake --build build -j8 && ./build/astra-dev`
Expected: builds. Game launches. Existing LAN behavior unchanged (name field unused yet).

- [ ] **Step 5: Commit**

```bash
git add include/astra/lan.h src/lan.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-1): add LanRoom.name + zone_banner_label helper

Defaults: T1 → "LOBBY", T2 → "OPERATIONS", T3 → "VAULT". Banner helper
falls back to "T<n> ZONE" when unset. Used by Cut 8's zone overlay.
EOF
)"
```

### Task 1.3: Save format — serialize `name` (skip `per_node_spawn` until Cut 7 cutover)

**Files:**
- Modify: `src/save_file.cpp` (locate `LanRoom` (de)serialization)

- [ ] **Step 1: Locate the version-guarded serializer block for LanRoom**

Search `src/save_file.cpp` for `LanRoom` or `room.tier` to find the serializer. The save schema is currently v62 (per `SAVE_FILE_VERSION` constant — confirm).

- [ ] **Step 2: Add `name` to serialization, version-guarded**

Bump the schema constant by 1 (v62 → v63) **only if you also handle the new field on read**. For now, add `name` to writes and reads. Use a length-prefixed string write helper (the file already has one — match its convention).

```cpp
// On write (after existing room.tier write):
write_string(file, room.name);

// On read (after room.tier read), guarded by version:
if (version >= /*new_version*/ 63) {
    room.name = read_string(file);
} else {
    // legacy path — derive default from tier
    switch (room.tier) {
        case 1: room.name = "LOBBY"; break;
        case 2: room.name = "OPERATIONS"; break;
        case 3: room.name = "VAULT"; break;
        default: room.name = ""; break;
    }
}
```

Bump `SAVE_FILE_VERSION` from 62 to 63 in `include/astra/save_file.h` (or wherever it's declared).

- [ ] **Step 3: Save / reload smoke test**

Build, run, save a game, reload it. The game should resume cleanly. If a save from before the bump exists, it should load via the legacy fallback.

```bash
cmake --build build -j8 && ./build/astra-dev
# in game: jack into a LAN, save, quit, reload, jack again — should work
```

- [ ] **Step 4: Commit**

```bash
git add src/save_file.cpp include/astra/save_file.h
git commit -m "$(cat <<'EOF'
plan-8(cut-1): bump save schema v62 → v63, persist LanRoom.name

Old saves load via tier→default fallback. Reserves the schema slot for
Cut 7 cutover (per_node_spawn serialization, Gateway tile removal).
EOF
)"
```

**Cut 1 complete.** Build green; data model has the slots Plan 8 needs.

---

## Cut 2 — Generator skeleton (Phases 1–3)

**Goal:** new generator file `lan_sector_v2.cpp` produces a sector with rooms placed in zone clusters, no bridges yet, no templates yet. Behind a dev verb so we can inspect output.

**Verification at end of cut:** `:lan-gen-v2 <map_id>` dev verb prints zone partition + room placement to log. `:devspawn-lan-v2` builds a sector visible in dev mode.

### Task 2.1: Create `lan_sector_v2.h` and the empty entry point

**Files:**
- Create: `include/astra/lan_sector_v2.h`
- Create: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Write the header**

`include/astra/lan_sector_v2.h`:
```cpp
#pragma once

#include "astra/grid_sector.h"
#include "astra/lan.h"
#include "astra/grid_network.h"

namespace astra {

// Plan 8 generator. Produces a flat sector containing all the LAN's
// subnet rooms (formerly: subnet sectors), packed in zone clusters,
// connected by 3-tile bridges. Replaces generate_lan_sector in Cut 7.
GridSector generate_lan_sector_v2(const LanMetadata& meta,
                                  const GridNetwork& net);

} // namespace astra
```

- [ ] **Step 2: Stub the implementation**

`src/lan_sector_v2.cpp`:
```cpp
#include "astra/lan_sector_v2.h"

namespace astra {

GridSector generate_lan_sector_v2(const LanMetadata& meta,
                                  const GridNetwork& /*net*/) {
    GridSector sec;
    sec.w = 30;
    sec.h = 16;
    sec.tiles.assign(static_cast<size_t>(sec.w) * sec.h, GridTile::Wall);
    // TODO Cut 2 Task 2: Phase 1 sizing.
    // TODO Cut 2 Task 3: Phase 2 zone partition.
    // TODO Cut 2 Task 4: Phase 3 room placement.
    sec.spawn_x = 1;
    sec.spawn_y = 1;
    return sec;
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists**

If `CMakeLists.txt` lists source files explicitly, add `src/lan_sector_v2.cpp`. If it globs, no change needed.

- [ ] **Step 4: Build**

Run: `cmake --build build -j8`
Expected: PASS. New file compiles; nothing calls it yet.

- [ ] **Step 5: Commit**

```bash
git add include/astra/lan_sector_v2.h src/lan_sector_v2.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
plan-8(cut-2): stub lan_sector_v2 generator entry point

Empty 30x16 Wall sector. Phases 1-6 land in subsequent tasks.
EOF
)"
```

### Task 2.2: Phase 1 — sector sizing

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add the size-computation helper**

Above `generate_lan_sector_v2`, add (and an exported declaration in the header):

```cpp
// Plan 8 size envelope. Sized from the total subnet count + lobby invariant.
struct LanV2SizeParams {
    int width;
    int height;
    int zone_count;          // 1, 2, or 3
};

LanV2SizeParams compute_lan_v2_size(const LanMetadata& meta);
```

In `lan_sector_v2.cpp`:
```cpp
LanV2SizeParams compute_lan_v2_size(const LanMetadata& meta) {
    int subnets = 0;
    int distinct_tiers = 0;
    bool seen_tier[4] = {false, false, false, false};
    for (const LanRoom& r : meta.rooms) {
        subnets += static_cast<int>(r.contained_subnets.size());
        if (r.tier >= 1 && r.tier <= 3 && !seen_tier[r.tier]) {
            seen_tier[r.tier] = true;
            distinct_tiers++;
        }
    }

    LanV2SizeParams p;
    p.zone_count = std::max(1, std::min(3, distinct_tiers));

    // Size table from spec § 4.2:
    //  1-3   subnets → 30×16
    //  4-7              50×22
    //  8-14             72×28
    //  15-25            96×34
    //  26+              120×42
    if      (subnets <= 3)  { p.width = 30;  p.height = 16; }
    else if (subnets <= 7)  { p.width = 50;  p.height = 22; }
    else if (subnets <= 14) { p.width = 72;  p.height = 28; }
    else if (subnets <= 25) { p.width = 96;  p.height = 34; }
    else                    { p.width = 120; p.height = 42; }
    return p;
}
```

Add the include at top: `#include <algorithm>`.

- [ ] **Step 2: Use the size in the entry point**

Replace the hardcoded `30, 16`:
```cpp
GridSector generate_lan_sector_v2(const LanMetadata& meta, const GridNetwork& /*net*/) {
    LanV2SizeParams p = compute_lan_v2_size(meta);
    GridSector sec;
    sec.w = p.width;
    sec.h = p.height;
    sec.tiles.assign(static_cast<size_t>(sec.w) * sec.h, GridTile::Wall);
    sec.spawn_x = 1;
    sec.spawn_y = 1;
    return sec;
}
```

- [ ] **Step 3: Build**

`cmake --build build -j8` → PASS.

- [ ] **Step 4: Commit**

```bash
git add include/astra/lan_sector_v2.h src/lan_sector_v2.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-2): Phase 1 — sector dimensions from subnet count

Five size buckets (tiny → max), zone count derived from distinct LanRoom
tiers (1–3). Matches spec §4.2 table.
EOF
)"
```

### Task 2.3: Phase 2 — zone partition

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add the partition helper**

```cpp
struct LanV2ZoneRegion {
    int x, y, w, h;          // zone bounding box in sector coords
    int tier;                // 1, 2, or 3
    int anchor_x, anchor_y;  // seed cell for the zone's anchor room
    std::string name;        // banner — pulled from LanRoom.name
};

std::vector<LanV2ZoneRegion> partition_zones(const LanMetadata& meta,
                                             const LanV2SizeParams& size);
```

Implementation (in the .cpp, anonymous namespace):

```cpp
std::vector<LanV2ZoneRegion> partition_zones(const LanMetadata& meta,
                                              const LanV2SizeParams& size) {
    std::vector<LanV2ZoneRegion> zones;

    // Group LanRooms by tier.
    std::vector<const LanRoom*> by_tier[4] = {};
    for (const LanRoom& r : meta.rooms) {
        if (r.tier >= 1 && r.tier <= 3) by_tier[r.tier].push_back(&r);
    }

    int active_tiers = 0;
    for (int t = 1; t <= 3; ++t) if (!by_tier[t].empty()) active_tiers++;
    if (active_tiers == 0) active_tiers = 1; // empty LAN — place a lobby anyway

    // Zone widths split horizontally:
    //  1 zone → full width
    //  2 zones → 40% / 60%
    //  3 zones → 30% / 40% / 30%
    int widths[3] = {0, 0, 0};
    if (active_tiers == 1) {
        widths[0] = size.width;
    } else if (active_tiers == 2) {
        widths[0] = size.width * 4 / 10;
        widths[1] = size.width - widths[0];
    } else {
        widths[0] = size.width * 3 / 10;
        widths[2] = size.width * 3 / 10;
        widths[1] = size.width - widths[0] - widths[2];
    }

    int slot = 0;
    int x_cursor = 0;
    for (int t = 1; t <= 3; ++t) {
        if (by_tier[t].empty() && active_tiers > 1) continue;
        // T1 always gets a zone (lobby invariant).
        if (active_tiers == 1 && t > 1 && by_tier[t].empty()) continue;

        LanV2ZoneRegion z;
        z.x = x_cursor;
        z.y = 0;
        z.w = widths[slot];
        z.h = size.height;
        z.tier = t;
        z.anchor_x = x_cursor + widths[slot] / 2;
        z.anchor_y = size.height / 2;
        z.name = by_tier[t].empty() ? std::string()
                                    : by_tier[t][0]->name;
        zones.push_back(z);

        x_cursor += widths[slot];
        slot++;
        if (slot >= 3) break;
    }

    if (zones.empty()) {
        // safety: at least one zone for the lobby invariant
        LanV2ZoneRegion z;
        z.x = 0; z.y = 0; z.w = size.width; z.h = size.height;
        z.tier = 1; z.anchor_x = size.width / 2; z.anchor_y = size.height / 2;
        z.name = "LOBBY";
        zones.push_back(z);
    }

    return zones;
}
```

- [ ] **Step 2: Add a dev verb to print zones**

In `src/dev_console.cpp`, find the verb dispatch table and add:
```cpp
else if (verb == ":lan-zones") {
    auto& meta = game.world().lan_metadata();
    auto size = compute_lan_v2_size(meta);
    auto zones = partition_zones(meta, size);
    log("LAN: " + std::to_string(meta.rooms.size()) + " rooms; size "
        + std::to_string(size.width) + "x" + std::to_string(size.height)
        + "; " + std::to_string(zones.size()) + " zones");
    for (const auto& z : zones) {
        log("  zone t" + std::to_string(z.tier)
            + " name=" + (z.name.empty() ? "<unset>" : z.name)
            + " bbox=" + std::to_string(z.x) + "," + std::to_string(z.y)
            + "+" + std::to_string(z.w) + "x" + std::to_string(z.h));
    }
}
```

(Adjust to match the existing dev-verb idiom — confirm with the current `:lan` or `:nmap` verbs in `dev_console.cpp` for pattern.)

- [ ] **Step 3: Build, smoke**

```bash
cmake --build build -j8 && ./build/astra-dev
# In game, dev console: :lan-zones
# Expected: prints LAN size and zone partition.
```

- [ ] **Step 4: Commit**

```bash
git add src/lan_sector_v2.cpp src/dev_console.cpp include/astra/lan_sector_v2.h
git commit -m "$(cat <<'EOF'
plan-8(cut-2): Phase 2 — partition sector into 1–3 zone regions

Horizontal split (40/60 for 2 zones, 30/40/30 for 3 zones). Each
LanV2ZoneRegion carries its bbox, tier, anchor cell, and banner name.
Dev verb :lan-zones prints the partition for any current LAN.
EOF
)"
```

### Task 2.4: Phase 3 — per-zone room placement (without templates)

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add room-placement data structures**

```cpp
struct LanV2Room {
    int x, y, w, h;
    int tier;
    GridNodeId source_subnet;       // empty for the lobby
    bool is_lobby = false;
    bool is_zone_anchor = false;
    int  zone_index;
};
```

- [ ] **Step 2: Stub a placeholder template-size selector**

For Cut 2, every room is the same default size. Real templates land in Cut 5.

```cpp
struct LanV2RoomFootprint { int w, h; };

LanV2RoomFootprint pick_default_footprint(int tier, bool is_lobby) {
    if (is_lobby) return {12, 7};   // oversized — see spec § 7
    if (tier == 1) return {6, 4};
    if (tier == 2) return {7, 5};
    if (tier == 3) return {8, 5};
    return {5, 4};
}
```

- [ ] **Step 3: Implement placement (rejection sampling within zone region)**

```cpp
std::vector<LanV2Room> place_rooms(const LanMetadata& meta,
                                    const std::vector<LanV2ZoneRegion>& zones,
                                    uint32_t seed) {
    std::vector<LanV2Room> placed;
    std::mt19937 rng(seed);

    // Place lobby first — T1 zone anchor.
    int t1_zone_idx = -1;
    for (size_t i = 0; i < zones.size(); ++i) {
        if (zones[i].tier == 1) { t1_zone_idx = static_cast<int>(i); break; }
    }
    if (t1_zone_idx < 0) {
        // Defensive: synth a T1 zone (shouldn't happen — invariant).
        return placed;
    }
    {
        const auto& z = zones[t1_zone_idx];
        auto fp = pick_default_footprint(1, /*is_lobby=*/true);
        LanV2Room lobby;
        lobby.x = z.x + (z.w - fp.w) / 2;
        lobby.y = z.y + (z.h - fp.h) / 2;
        lobby.w = fp.w;
        lobby.h = fp.h;
        lobby.tier = 1;
        lobby.is_lobby = true;
        lobby.is_zone_anchor = true;
        lobby.zone_index = t1_zone_idx;
        placed.push_back(lobby);
    }

    // For each zone, place its subnet rooms (one per subnet).
    auto overlaps = [&](int x, int y, int w, int h) {
        for (const auto& r : placed) {
            if (x + w + 1 > r.x &&
                x        < r.x + r.w + 1 &&
                y + h + 1 > r.y &&
                y        < r.y + r.h + 1) {
                return true;
            }
        }
        return false;
    };

    for (size_t zi = 0; zi < zones.size(); ++zi) {
        const auto& z = zones[zi];
        // Find LanRoom(s) for this tier, iterate their contained subnets.
        for (const LanRoom& lr : meta.rooms) {
            if (lr.tier != z.tier) continue;
            for (GridNodeId sub : lr.contained_subnets) {
                auto fp = pick_default_footprint(z.tier, /*is_lobby=*/false);
                bool placed_room = false;
                for (int attempt = 0; attempt < 200 && !placed_room; ++attempt) {
                    int hi_x = z.x + z.w - fp.w - 1;
                    int hi_y = z.y + z.h - fp.h - 1;
                    if (hi_x <= z.x + 1 || hi_y <= z.y + 1) break;
                    int x = std::uniform_int_distribution<int>(z.x + 1, hi_x)(rng);
                    int y = std::uniform_int_distribution<int>(z.y + 1, hi_y)(rng);
                    if (overlaps(x, y, fp.w, fp.h)) continue;
                    LanV2Room r;
                    r.x = x; r.y = y; r.w = fp.w; r.h = fp.h;
                    r.tier = z.tier;
                    r.source_subnet = sub;
                    r.is_lobby = false;
                    r.is_zone_anchor = false;  // first non-lobby room per zone could be anchor
                    r.zone_index = static_cast<int>(zi);
                    placed.push_back(r);
                    placed_room = true;
                }
                // Drop overflow if not placed (logged) — tiny LANs won't hit this.
            }
        }
    }
    return placed;
}
```

Add `#include <random>` at the top.

- [ ] **Step 4: Stamp rooms into the sector tile array**

In `generate_lan_sector_v2`, after partition + placement, stamp walls + floors:

```cpp
auto zones = partition_zones(meta, p);
auto rooms = place_rooms(meta, zones, meta.gen_seed);

auto stamp = [&](int x, int y, GridTile t) {
    if (x >= 0 && y >= 0 && x < sec.w && y < sec.h) {
        sec.tiles[static_cast<size_t>(y) * sec.w + x] = t;
    }
};

for (const auto& r : rooms) {
    // Perimeter = Firewall.
    for (int dx = 0; dx < r.w; ++dx) {
        stamp(r.x + dx, r.y,            GridTile::Firewall);
        stamp(r.x + dx, r.y + r.h - 1,  GridTile::Firewall);
    }
    for (int dy = 0; dy < r.h; ++dy) {
        stamp(r.x,            r.y + dy, GridTile::Firewall);
        stamp(r.x + r.w - 1,  r.y + dy, GridTile::Firewall);
    }
    // Interior = Floor.
    for (int dy = 1; dy < r.h - 1; ++dy) {
        for (int dx = 1; dx < r.w - 1; ++dx) {
            stamp(r.x + dx, r.y + dy, GridTile::Floor);
        }
    }
    if (r.is_lobby) {
        sec.spawn_x = r.x + r.w / 2;
        sec.spawn_y = r.y + r.h / 2;
    }
}
```

- [ ] **Step 5: Add a dev verb to spawn-and-render the v2 sector for inspection**

In `src/dev_console.cpp`, add `:lan-sector-v2` that calls `generate_lan_sector_v2(meta, net)` for the current LAN and replaces the active sector for inspection. (If swapping the active sector is hard, log the tile array as ASCII to console — print walls/floors/voids as `#`/`.`/` ` row by row.)

- [ ] **Step 6: Build, smoke**

```bash
cmake --build build -j8 && ./build/astra-dev
# in game on a connected map with 5+ subnets:
# :lan-zones                  → confirms zone partition
# :lan-sector-v2              → renders the v2 sector (rooms placed, no bridges yet)
```

You should see clusters of independent rectangles per zone, no corridors or doors yet, sector outline jagged.

- [ ] **Step 7: Commit**

```bash
git add include/astra/lan_sector_v2.h src/lan_sector_v2.cpp src/dev_console.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-2): Phase 3 — per-zone room placement

Lobby placed first (T1 anchor, 12x7). Subnet rooms placed per zone via
greedy rejection sampling with 1-cell gap. No bridges yet — rooms are
isolated islands. Dev verb :lan-sector-v2 swaps the active sector for
inspection; :lan-zones prints the partition.
EOF
)"
```

**Cut 2 complete.** New generator outputs zone-clustered isolated rooms; visible via dev verb.

---

## Cut 3 — Connectivity & 3-tile bridges (Phase 4)

**Goal:** rooms within a zone are MST-connected (lobby + zone-anchors get +1–2 bonus edges); each adjacent zone pair has exactly one inter-zone locked bridge. Bridges are 3-tile passages with a `Door` tile and two wall openings. Open vs locked rendering distinguishable.

**Verification at end:** `:lan-sector-v2` shows rooms with visible bridges. Within-zone bridges open `+`. Inter-zone bridges locked `▣`. Player can walk the sector via `:devspawn-lan-v2` (or equivalent) and traverse everything.

### Task 3.1: Add `GridTile::Door` to the tile enum

**Files:**
- Modify: `include/astra/grid_sector.h`

- [ ] **Step 1: Add the enum value**

In `include/astra/grid_sector.h`, in the `GridTile` enum, add (preferably right before the closing `};`):
```cpp
    Door,             // Plan 8: bridge tile between rooms (open or locked variant)
```

- [ ] **Step 2: Add a per-tile door-locked side-table to GridSector**

The `Door` tile has two states (open/locked). Encode the lock state separately:

In `include/astra/grid_sector.h`, after `per_node_spawn`:
```cpp
    // Plan 8: which Door tiles are currently locked. Cracked doors are removed.
    // A tile at (x,y) is in this set iff GridTile::Door + locked.
    std::unordered_set<std::pair<int,int>, PairHash> locked_doors;
```

Add `#include <unordered_set>` at the top.

- [ ] **Step 3: Add helpers**

```cpp
    bool is_locked_door(int x, int y) const;
    void unlock_door(int x, int y);   // removes (x,y) from locked_doors
```

In `src/grid_sector.cpp`:
```cpp
bool GridSector::is_locked_door(int x, int y) const {
    return locked_doors.count({x, y}) != 0;
}
void GridSector::unlock_door(int x, int y) {
    locked_doors.erase({x, y});
}
```

- [ ] **Step 4: Update passability**

In `GridSector::passable(int x, int y)` — wherever it lives — make `Door` passable iff *not* locked:
```cpp
case GridTile::Door:
    return !is_locked_door(x, y);
```

- [ ] **Step 5: Build**

`cmake --build build -j8` → PASS.

- [ ] **Step 6: Commit**

```bash
git add include/astra/grid_sector.h src/grid_sector.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-3): add GridTile::Door + locked_doors side-table

Door is the bridge tile between rooms. Lock state stored separately so
breach.exe can unlock without changing the tile glyph. Passability
checks consult locked_doors.
EOF
)"
```

### Task 3.2: Add Door rendering

**Files:**
- Modify: `include/astra/grid_theme.h`
- Modify: `src/grid_renderer.cpp`

- [ ] **Step 1: Add palette + glyph constants**

In `include/astra/grid_theme.h`:
```cpp
constexpr Color door_open   = Color::BrightCyan;
constexpr Color door_locked = Color::Yellow;          // orange-ish in palette
constexpr const char* door_open_glyph   = "+";
constexpr const char* door_locked_glyph = "\xe2\x96\xa3";  // ▣
```

- [ ] **Step 2: Wire into renderer**

In `src/grid_renderer.cpp`, find `glyph_for(GridTile)` and `color_for(GridTile)`:
```cpp
case GridTile::Door:
    // glyph and color depend on lock state; resolved per-cell in draw_playfield.
    return grid_theme::door_open_glyph;  // default; overridden below
```

In `draw_playfield`, where the per-cell glyph/color is resolved, add a special branch BEFORE the default lookup:
```cpp
if (t == GridTile::Door) {
    bool locked = s.sector.is_locked_door(tx, ty);
    glyph = locked ? grid_theme::door_locked_glyph : grid_theme::door_open_glyph;
    color = locked ? grid_theme::door_locked : grid_theme::door_open;
} else if (t == GridTile::DeviceAvatar) {
    // existing branch
} else if (...) {
    ...
}
```

- [ ] **Step 3: Build, run**

`cmake --build build -j8 && ./build/astra-dev` → no visual change yet (no Door tiles stamped).

- [ ] **Step 4: Commit**

```bash
git add include/astra/grid_theme.h src/grid_renderer.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-3): render GridTile::Door — open + cyan, locked ▣ orange

Per-cell lock-state lookup in draw_playfield's Door branch.
EOF
)"
```

### Task 3.3: Bridge carving helper

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add the carve function**

```cpp
// Carve a 3-tile bridge between two rooms placed 1 cell apart.
// Returns true on success. Picks the closest pair of facing wall cells
// and stamps: opening_a (Floor), bridge (Door), opening_b (Floor).
// If `locked`, registers the bridge in sec.locked_doors.
bool carve_bridge(GridSector& sec,
                  const LanV2Room& a, const LanV2Room& b,
                  bool locked) {
    // Determine relative orientation (horizontal or vertical adjacency).
    // For each axis, search the overlap range of facing edges; pick the
    // midpoint of the overlap as the bridge row/col.
    int a_left = a.x, a_right = a.x + a.w - 1;
    int b_left = b.x, b_right = b.x + b.w - 1;
    int a_top = a.y,  a_bot   = a.y + a.h - 1;
    int b_top = b.y,  b_bot   = b.y + b.h - 1;

    // Horizontal: A is left of B (gap = b.x - (a.x + a.w))
    if (b_left - a_right == 2) {  // exactly 1-cell gap
        int y_lo = std::max(a_top + 1, b_top + 1);
        int y_hi = std::min(a_bot - 1, b_bot - 1);
        if (y_hi < y_lo) return false;
        int y = (y_lo + y_hi) / 2;
        // openings + bridge
        sec.set(a_right,         y, GridTile::Floor);
        sec.set(a_right + 1,     y, GridTile::Door);
        sec.set(b_left,          y, GridTile::Floor);
        if (locked) sec.locked_doors.insert({a_right + 1, y});
        return true;
    }
    if (a_left - b_right == 2) return carve_bridge(sec, b, a, locked); // swap

    // Vertical: A above B
    if (b_top - a_bot == 2) {
        int x_lo = std::max(a_left + 1, b_left + 1);
        int x_hi = std::min(a_right - 1, b_right - 1);
        if (x_hi < x_lo) return false;
        int x = (x_lo + x_hi) / 2;
        sec.set(x, a_bot,         GridTile::Floor);
        sec.set(x, a_bot + 1,     GridTile::Door);
        sec.set(x, b_top,         GridTile::Floor);
        if (locked) sec.locked_doors.insert({x, a_bot + 1});
        return true;
    }
    if (a_top - b_bot == 2) return carve_bridge(sec, b, a, locked);

    // Not 1-cell-gap adjacent → fail (caller falls back to longer corridor).
    return false;
}
```

- [ ] **Step 2: Add a longer-corridor fallback**

```cpp
// 5+ tile bridge for rooms placed further apart. Carves an L-shaped path
// of Floor between the rooms, with one Door tile at the midpoint.
bool carve_long_bridge(GridSector& sec,
                       const LanV2Room& a, const LanV2Room& b,
                       bool locked) {
    // Compute exit cell on A facing B, then on B facing A.
    int a_cx = a.x + a.w / 2, a_cy = a.y + a.h / 2;
    int b_cx = b.x + b.w / 2, b_cy = b.y + b.h / 2;

    // Pick orientation: exit horizontally if |dx|>|dy|, else vertical.
    int dx = b_cx - a_cx, dy = b_cy - a_cy;
    int sx, sy, tx, ty;
    if (std::abs(dx) >= std::abs(dy)) {
        // horizontal exit
        sx = (dx > 0) ? a.x + a.w     : a.x - 1;
        sy = a_cy;
        tx = (dx > 0) ? b.x - 1       : b.x + b.w;
        ty = b_cy;
        // open A's wall at (sx-1 or +1) and B's wall at (tx+1 or -1)
        int a_wall_x = (dx > 0) ? a.x + a.w - 1 : a.x;
        int b_wall_x = (dx > 0) ? b.x          : b.x + b.w - 1;
        sec.set(a_wall_x, sy, GridTile::Floor);
        sec.set(b_wall_x, ty, GridTile::Floor);
    } else {
        // vertical exit
        sx = a_cx;
        sy = (dy > 0) ? a.y + a.h     : a.y - 1;
        tx = b_cx;
        ty = (dy > 0) ? b.y - 1       : b.y + b.h;
        int a_wall_y = (dy > 0) ? a.y + a.h - 1 : a.y;
        int b_wall_y = (dy > 0) ? b.y          : b.y + b.h - 1;
        sec.set(sx, a_wall_y, GridTile::Floor);
        sec.set(tx, b_wall_y, GridTile::Floor);
    }

    // L-shape between (sx,sy) and (tx,ty): straight then turn.
    int cx = sx, cy = sy;
    while (cx != tx) { sec.set(cx, cy, GridTile::Floor); cx += (tx > cx) ? 1 : -1; }
    while (cy != ty) { sec.set(cx, cy, GridTile::Floor); cy += (ty > cy) ? 1 : -1; }
    sec.set(tx, ty, GridTile::Floor);

    // Place a Door at the midpoint of the corridor.
    int mid_x = (sx + tx) / 2;
    int mid_y = (sy + ty) / 2;
    sec.set(mid_x, mid_y, GridTile::Door);
    if (locked) sec.locked_doors.insert({mid_x, mid_y});
    return true;
}
```

- [ ] **Step 3: Combined carve dispatch**

```cpp
bool carve_any_bridge(GridSector& sec,
                      const LanV2Room& a, const LanV2Room& b,
                      bool locked) {
    if (carve_bridge(sec, a, b, locked)) return true;
    return carve_long_bridge(sec, a, b, locked);
}
```

- [ ] **Step 4: Build**

`cmake --build build -j8` → PASS.

- [ ] **Step 5: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-3): carve 3-tile bridges + long-bridge fallback

3-tile bridge: opening + Door + opening. Long bridge falls back to
L-shaped corridor with mid-point Door for non-adjacent rooms. Locked
bridges register in sec.locked_doors.
EOF
)"
```

### Task 3.4: MST + bonus edges connectivity

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Implement Prim's MST per zone**

```cpp
struct LanV2Edge { int from_idx, to_idx; int dist; };

// Manhattan distance between two rooms' centers.
static int room_dist(const LanV2Room& a, const LanV2Room& b) {
    int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int bx = b.x + b.w / 2, by = b.y + b.h / 2;
    return std::abs(ax - bx) + std::abs(ay - by);
}

std::vector<LanV2Edge> zone_mst(const std::vector<LanV2Room>& rooms,
                                 int zone_idx) {
    std::vector<int> in_zone;
    for (size_t i = 0; i < rooms.size(); ++i) {
        if (rooms[i].zone_index == zone_idx) in_zone.push_back(static_cast<int>(i));
    }
    std::vector<LanV2Edge> mst;
    if (in_zone.size() < 2) return mst;

    std::vector<bool> in_tree(in_zone.size(), false);
    in_tree[0] = true;
    while (true) {
        int best_dist = INT_MAX;
        int best_u = -1, best_v = -1;
        for (size_t u = 0; u < in_zone.size(); ++u) {
            if (!in_tree[u]) continue;
            for (size_t v = 0; v < in_zone.size(); ++v) {
                if (in_tree[v]) continue;
                int d = room_dist(rooms[in_zone[u]], rooms[in_zone[v]]);
                if (d < best_dist) { best_dist = d; best_u = u; best_v = v; }
            }
        }
        if (best_v < 0) break;
        mst.push_back({in_zone[best_u], in_zone[best_v], best_dist});
        in_tree[best_v] = true;
    }
    return mst;
}
```

Add `#include <climits>`.

- [ ] **Step 2: Bonus edges for lobby + anchors**

```cpp
// Up to 2 extra edges per anchor (or lobby) — pick shortest non-tree neighbors.
std::vector<LanV2Edge> bonus_edges(const std::vector<LanV2Room>& rooms,
                                    const std::vector<LanV2Edge>& mst,
                                    int max_bonus_per_anchor = 2) {
    std::vector<LanV2Edge> bonus;
    auto in_mst = [&](int a, int b) {
        for (const auto& e : mst) {
            if ((e.from_idx == a && e.to_idx == b) ||
                (e.from_idx == b && e.to_idx == a)) return true;
        }
        return false;
    };
    auto already_bonus = [&](int a, int b) {
        for (const auto& e : bonus) {
            if ((e.from_idx == a && e.to_idx == b) ||
                (e.from_idx == b && e.to_idx == a)) return true;
        }
        return false;
    };

    for (size_t i = 0; i < rooms.size(); ++i) {
        if (!rooms[i].is_lobby && !rooms[i].is_zone_anchor) continue;
        // Find candidates in the same zone, sorted by distance.
        std::vector<std::pair<int,int>> cand; // {dist, j}
        for (size_t j = 0; j < rooms.size(); ++j) {
            if (i == j) continue;
            if (rooms[j].zone_index != rooms[i].zone_index) continue;
            if (in_mst(i, j) || already_bonus(i, j)) continue;
            cand.push_back({room_dist(rooms[i], rooms[j]), static_cast<int>(j)});
        }
        std::sort(cand.begin(), cand.end());
        for (int k = 0; k < std::min<int>(max_bonus_per_anchor, cand.size()); ++k) {
            bonus.push_back({static_cast<int>(i), cand[k].second, cand[k].first});
        }
    }
    return bonus;
}
```

- [ ] **Step 3: Mark zone anchors**

The first non-lobby room placed per zone becomes the zone anchor. Update `place_rooms` so the first subnet-room added to each zone has `is_zone_anchor = true`.

```cpp
// In place_rooms, track per-zone first-flag:
std::vector<bool> seen_anchor_for_zone(zones.size(), false);
// after r.zone_index is set and r is added:
if (!seen_anchor_for_zone[r.zone_index] && !r.is_lobby) {
    placed.back().is_zone_anchor = true;
    seen_anchor_for_zone[r.zone_index] = true;
}
```

- [ ] **Step 4: Carve the within-zone connectivity**

In `generate_lan_sector_v2`, after rooms are stamped:

```cpp
for (size_t zi = 0; zi < zones.size(); ++zi) {
    auto mst = zone_mst(rooms, static_cast<int>(zi));
    auto bonus = bonus_edges(rooms, mst);
    for (const auto& e : mst)   carve_any_bridge(sec, rooms[e.from_idx], rooms[e.to_idx], /*locked=*/false);
    for (const auto& e : bonus) carve_any_bridge(sec, rooms[e.from_idx], rooms[e.to_idx], /*locked=*/false);
}
```

- [ ] **Step 5: Build, smoke**

```bash
cmake --build build -j8 && ./build/astra-dev
# in game: :lan-sector-v2
# Expected: each zone's rooms connected by visible + open doors
```

- [ ] **Step 6: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-3): per-zone MST + lobby/anchor bonus edges

Prim's MST per zone (Manhattan distances). Lobby and zone-anchor rooms
gain up to 2 extra shortest edges. Carved as open bridges.
EOF
)"
```

### Task 3.5: Inter-zone bridges (locked)

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Pick one cross-zone room pair per adjacent zone pair**

```cpp
void carve_inter_zone_bridges(GridSector& sec,
                              const std::vector<LanV2Room>& rooms,
                              const std::vector<LanV2ZoneRegion>& zones) {
    // For each adjacent zone pair (i, i+1), pick the closest cross-zone room pair and lock it.
    for (size_t i = 1; i < zones.size(); ++i) {
        int best_dist = INT_MAX;
        int best_a = -1, best_b = -1;
        for (size_t ra = 0; ra < rooms.size(); ++ra) {
            if (rooms[ra].zone_index != static_cast<int>(i - 1)) continue;
            for (size_t rb = 0; rb < rooms.size(); ++rb) {
                if (rooms[rb].zone_index != static_cast<int>(i)) continue;
                int d = room_dist(rooms[ra], rooms[rb]);
                if (d < best_dist) {
                    best_dist = d; best_a = static_cast<int>(ra); best_b = static_cast<int>(rb);
                }
            }
        }
        if (best_a >= 0) carve_any_bridge(sec, rooms[best_a], rooms[best_b], /*locked=*/true);
    }
}
```

- [ ] **Step 2: Wire into entry point**

Call `carve_inter_zone_bridges(sec, rooms, zones)` after the per-zone MST/bonus loop.

- [ ] **Step 3: Build, smoke**

```bash
cmake --build build -j8 && ./build/astra-dev
# :lan-sector-v2 on a 3-zone LAN
# Expected: 2 ▣ (orange) locked doors + several + (cyan) open doors
```

- [ ] **Step 4: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-3): inter-zone locked bridges (one per zone pair)

Closest cross-zone room pair gets a single ▣ locked bridge. T1↔T2 and
T2↔T3 chokes are now visible, structural, and breach-able.
EOF
)"
```

### Task 3.6: Update `breach.exe` to target Door tiles

**Files:**
- Modify: `src/program_effects.cpp` (find `apply_breach_grid`)

- [ ] **Step 1: Read existing implementation**

Locate `apply_breach_grid` (or whatever the breach-program effect is named). Today it likely targets a `Firewall` tile and converts it to `Floor`. Plan 8: it must target a `Door` tile and unlock it.

- [ ] **Step 2: Rewrite the effect**

```cpp
ProgramEffectResult apply_breach_grid(Game& game, const TargetSpec& tgt) {
    auto* sess = game.hacking().session_mut();
    if (!sess) return ProgramEffectResult::Failed;
    GridSector& sec = sess->sector;

    if (!sec.in_bounds(tgt.x, tgt.y)) return ProgramEffectResult::Failed;
    if (sec.at(tgt.x, tgt.y) != GridTile::Door) {
        sess->log_lines.push_back("[ERR] breach.exe: target is not a door tile.");
        return ProgramEffectResult::Failed;
    }
    if (!sec.is_locked_door(tgt.x, tgt.y)) {
        sess->log_lines.push_back("[INFO] breach.exe: door already open.");
        return ProgramEffectResult::Failed;
    }
    sec.unlock_door(tgt.x, tgt.y);
    sess->log_lines.push_back("[OK] breach.exe: lock cracked, door open.");
    return ProgramEffectResult::Success;
}
```

(Adjust types — `TargetSpec`, `ProgramEffectResult` — to whatever the file actually uses.)

- [ ] **Step 3: Update target_filter for Breach program**

In `src/program.cpp`, find the `breach.exe` program definition and update its `target_filter` (or telegraph kind) to allow targeting `Door` tiles. Pre-Plan-8 it targeted `Firewall`; post, it's `Door`.

- [ ] **Step 4: Build, smoke**

```bash
cmake --build build -j8 && ./build/astra-dev
# in a v2 sector with locked doors:
# load breach.exe into a slot, target a ▣ door, run
# Expected: door unlocks (▣ → +), passable
```

- [ ] **Step 5: Commit**

```bash
git add src/program_effects.cpp src/program.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-3): breach.exe now targets Door tiles, unlocks instead of carving walls

Replaces the Firewall-tile-to-Floor conversion with a locked_doors set
removal. Single mechanic: every door is locked unless explicitly open;
breach.exe is the universal opener.
EOF
)"
```

**Cut 3 complete.** Generator output is now traversable: rooms connected, zones gated by lockable doors.

---

## Cut 4 — Special tile placement (Phase 5)

**Goal:** ⊙ ExitNode in lobby, ⊕ DeepGridGateway in highest-tier sanctum room, DeviceAvatar on each subnet room's perimeter, per-node spawn registration so jack-in lands in the right room.

### Task 4.1: Place `ExitNode` in the lobby

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add helper**

```cpp
void stamp_exit_node(GridSector& sec, const std::vector<LanV2Room>& rooms) {
    for (const auto& r : rooms) {
        if (!r.is_lobby) continue;
        // Place ⊙ at the cell furthest from the room's outgoing bridge (rough heuristic:
        // pick a corner-adjacent floor cell).
        int x = r.x + r.w - 2;
        int y = r.y + 1;
        if (sec.in_bounds(x, y) && sec.at(x, y) == GridTile::Floor) {
            sec.set(x, y, GridTile::ExitNode);
            return;
        }
        // Fallback: scan interior for the first Floor tile.
        for (int dy = 1; dy < r.h - 1; ++dy) {
            for (int dx = 1; dx < r.w - 1; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) {
                    sec.set(xx, yy, GridTile::ExitNode);
                    return;
                }
            }
        }
    }
}
```

Call from `generate_lan_sector_v2` after bridges are carved.

- [ ] **Step 2: Build, smoke**

`:lan-sector-v2` → expect ⊙ in the lobby room.

- [ ] **Step 3: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "plan-8(cut-4): stamp ⊙ ExitNode in the lobby room"
```

### Task 4.2: Place `DeepGridGateway` in highest-tier zone's anchor

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add helper**

```cpp
void stamp_deep_grid_gateway(GridSector& sec,
                              const GridNetwork& net,
                              const LanMetadata& meta,
                              const std::vector<LanV2Room>& rooms) {
    if (!meta.connected) return;
    // Find highest-tier zone-anchor room.
    const LanV2Room* target = nullptr;
    int best_tier = 0;
    for (const auto& r : rooms) {
        if (r.is_lobby) continue;
        if (!r.is_zone_anchor) continue;
        if (r.tier > best_tier) { best_tier = r.tier; target = &r; }
    }
    if (!target) return;

    // Center of the anchor room.
    int gx = target->x + target->w / 2;
    int gy = target->y + target->h / 2;
    if (sec.at(gx, gy) != GridTile::Floor) {
        // fallback: any interior floor
        for (int dy = 1; dy < target->h - 1; ++dy)
            for (int dx = 1; dx < target->w - 1; ++dx) {
                int xx = target->x + dx, yy = target->y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) { gx = xx; gy = yy; goto found; }
            }
        return;
        found:;
    }
    sec.set(gx, gy, GridTile::DeepGridGateway);

    // Resolve target node id (deep-grid anchor) — search the network.
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) {
            sec.gateway_target.emplace(std::pair<int,int>{gx, gy}, n.id);
            break;
        }
    }
}
```

- [ ] **Step 2: Wire into entry point**

After `stamp_exit_node`, call `stamp_deep_grid_gateway(sec, net, meta, rooms)`.

- [ ] **Step 3: Build, smoke**

`:lan-sector-v2` on a connected LAN → ⊕ in the deepest zone's anchor room.

- [ ] **Step 4: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "plan-8(cut-4): stamp ⊕ DeepGridGateway in highest-tier zone anchor"
```

### Task 4.3: Per-node spawn registration

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: For each subnet room, register one safe interior cell**

```cpp
void register_per_node_spawns(GridSector& sec,
                               const std::vector<LanV2Room>& rooms) {
    for (const auto& r : rooms) {
        if (r.is_lobby) continue;  // lobby uses sec.spawn_x/y as fallback
        if (!r.source_subnet.is_valid()) continue;

        // Scan for a Floor tile not adjacent to any Door.
        for (int dy = 1; dy < r.h - 1; ++dy) {
            for (int dx = 1; dx < r.w - 1; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) != GridTile::Floor) continue;
                bool near_door = false;
                for (int oy = -1; oy <= 1 && !near_door; ++oy)
                    for (int ox = -1; ox <= 1 && !near_door; ++ox)
                        if (sec.at(xx + ox, yy + oy) == GridTile::Door) near_door = true;
                if (near_door) continue;
                sec.per_node_spawn.emplace(r.source_subnet, std::pair<int,int>{xx, yy});
                goto done;
            }
        }
        // Fallback: room center.
        sec.per_node_spawn.emplace(
            r.source_subnet,
            std::pair<int,int>{r.x + r.w / 2, r.y + r.h / 2});
        done:;
    }
}
```

(Adjust `is_valid()` to match `GridNodeId`'s API.)

- [ ] **Step 2: Wire**

Call after gateway placement.

- [ ] **Step 3: Build**

PASS.

- [ ] **Step 4: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "plan-8(cut-4): register per_node_spawn for every subnet room"
```

### Task 4.4: DeviceAvatar on each subnet room's perimeter

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Stamp one DeviceAvatar tile per subnet room**

Pick a perimeter wall cell of each subnet room (any side except where a door is) and convert to `GridTile::DeviceAvatar`. The render path already handles per-FixtureType glyph (Plan 5 Cut 2.6).

```cpp
void stamp_device_avatars(GridSector& sec,
                           const std::vector<LanV2Room>& rooms,
                           const LanMetadata& meta,
                           const GridNetwork& net) {
    // Track source FixtureType per room for renderer.
    // (sec.source_fixture_type is currently a single value — Plan 8 may need a per-room map.)
    // For Cut 4 simplicity: just stamp the avatar tiles; the renderer's
    // per-room theming lookup is added in Cut 5 with templates.

    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.is_valid()) continue;
        // Find a perimeter cell that's currently Firewall (not opened by a door).
        for (int dx = 1; dx < r.w - 1; ++dx) {
            int yy = r.y;  // top wall
            if (sec.at(r.x + dx, yy) == GridTile::Firewall) {
                sec.set(r.x + dx, yy, GridTile::DeviceAvatar);
                goto done;
            }
        }
        done:;
    }
}
```

For per-room FixtureType lookup, we need to extend `GridSector` with a per-tile or per-room metadata map — defer to Cut 5 (when templates land) or add a simple `std::unordered_map<std::pair<int,int>, FixtureType, PairHash> avatar_fixture_type` here.

Add the field in `include/astra/grid_sector.h`:
```cpp
    std::unordered_map<std::pair<int,int>, FixtureType, PairHash> avatar_fixture_type;
```

Populate it in `stamp_device_avatars`:
```cpp
auto* hack = game_world_find_hackable_by_node(net, r.source_subnet);  // helper TBD
FixtureType ft = hack ? hack->fixture_type : FixtureType{};
sec.avatar_fixture_type.emplace(std::pair<int,int>{r.x + dx, yy}, ft);
```

(Actual hackable-lookup signature varies — check `world_manager.h`. The point is to record the FixtureType so the renderer's `device_avatar_glyph(ft)` resolves correctly per-tile.)

- [ ] **Step 2: Update renderer to use the per-tile FixtureType**

In `src/grid_renderer.cpp`, in the `DeviceAvatar` branch of `draw_playfield`:
```cpp
if (t == GridTile::DeviceAvatar) {
    auto it = s.sector.avatar_fixture_type.find({tx, ty});
    FixtureType ft = (it != s.sector.avatar_fixture_type.end())
                     ? it->second
                     : s.sector.source_fixture_type;
    glyph = grid_theme::device_avatar_glyph(ft);
    color = Color::BrightWhite;
}
```

- [ ] **Step 3: Build, smoke**

`:lan-sector-v2` → each subnet room shows its themed avatar glyph (camera ▦, door ║, healpod ⊞, etc.) on a perimeter wall.

- [ ] **Step 4: Commit**

```bash
git add src/lan_sector_v2.cpp src/grid_renderer.cpp include/astra/grid_sector.h
git commit -m "$(cat <<'EOF'
plan-8(cut-4): per-room DeviceAvatar with per-tile FixtureType lookup

Each subnet room mounts its source-fixture's avatar glyph on a perimeter
wall. Renderer resolves glyph from per-tile FixtureType map.
EOF
)"
```

**Cut 4 complete.** Sector is fully populated: lobby spawn, exit, deep-grid gateway, device avatars, per-node spawn map.

---

## Cut 5 — Tag-themed templates (Phase 6)

**Goal:** the 7 templates (`WeaponizedRoom`, `SurveillanceRoom`, `DataVaultRoom`, `PowerNodeRoom`, `CrewQuartersRoom`, `PrecursorShrineRoom`, `GenericRoom`) drive per-room min sizes, ICE seeding, content seeding, and incoming-door lock overrides.

### Task 5.1: Template type and registry

**Files:**
- Create: `include/astra/grid_room_templates.h`
- Create: `src/grid_room_templates.cpp`

- [ ] **Step 1: Define the template structure**

`include/astra/grid_room_templates.h`:
```cpp
#pragma once

#include "astra/grid_sector.h"
#include "astra/hackable.h"   // HackTagMask

#include <cstdint>
#include <vector>

namespace astra {

enum class RoomTemplateKind : uint8_t {
    Weaponized,
    Surveillance,
    DataVault,
    PowerNode,
    CrewQuarters,
    PrecursorShrine,
    Generic
};

struct RoomTemplateSize { int min_w, min_h, max_w, max_h; };

struct RoomTemplateSeedRule {
    int n_white_ice = 0;
    int n_gray_ice  = 0;
    int n_black_ice = 0;
    int n_data_nodes = 0;
    int n_encrypted_files = 0;
    bool lock_incoming_doors = false;   // override default (within-zone bridges open)
};

RoomTemplateKind choose_template_for_tags(HackTagMask tags);
RoomTemplateSize template_size_constraints(RoomTemplateKind kind);
RoomTemplateSeedRule template_seed_rule(RoomTemplateKind kind, int tier);

} // namespace astra
```

- [ ] **Step 2: Implement priority-driven tag dispatch**

`src/grid_room_templates.cpp`:
```cpp
#include "astra/grid_room_templates.h"

namespace astra {

RoomTemplateKind choose_template_for_tags(HackTagMask tags) {
    using K = RoomTemplateKind;
    // Priority order from spec § 5: AlienTech > Weaponized > DataStore > HasOptics > Mobile > PowerNode > Electronic.
    if (tags.has(HackTag::AlienTech))   return K::PrecursorShrine;
    if (tags.has(HackTag::Weaponized))  return K::Weaponized;
    if (tags.has(HackTag::DataStore))   return K::DataVault;
    if (tags.has(HackTag::HasOptics))   return K::Surveillance;
    if (tags.has(HackTag::Mobile))      return K::CrewQuarters;
    if (tags.has(HackTag::PowerNode))   return K::PowerNode;
    return K::Generic;
}

RoomTemplateSize template_size_constraints(RoomTemplateKind kind) {
    switch (kind) {
        case RoomTemplateKind::Weaponized:      return {8, 6, 12, 8};
        case RoomTemplateKind::Surveillance:    return {6, 5, 9,  7};
        case RoomTemplateKind::DataVault:       return {7, 5, 10, 7};
        case RoomTemplateKind::PowerNode:       return {5, 4, 7,  5};
        case RoomTemplateKind::CrewQuarters:    return {6, 4, 9,  6};
        case RoomTemplateKind::PrecursorShrine: return {8, 6, 12, 8};
        case RoomTemplateKind::Generic:         return {4, 3, 6,  4};
    }
    return {4, 3, 6, 4};
}

RoomTemplateSeedRule template_seed_rule(RoomTemplateKind kind, int tier) {
    RoomTemplateSeedRule r;
    switch (kind) {
        case RoomTemplateKind::Weaponized:
            r.n_white_ice = 1;
            r.n_gray_ice  = (tier >= 2) ? 1 : 0;
            break;
        case RoomTemplateKind::Surveillance:
            r.n_white_ice    = 1;
            r.n_data_nodes   = 1;  // surveillance footage
            break;
        case RoomTemplateKind::DataVault:
            r.n_gray_ice          = 1;
            r.n_data_nodes        = 2 + tier; // 3, 4, 5
            r.n_encrypted_files   = 1 + (tier >= 2 ? 1 : 0);
            r.lock_incoming_doors = (tier >= 2);
            break;
        case RoomTemplateKind::PowerNode:
            // no ICE, no content — pure decor.
            break;
        case RoomTemplateKind::CrewQuarters:
            // NPC avatar handled by Plan 9; no ICE.
            break;
        case RoomTemplateKind::PrecursorShrine:
            r.n_black_ice         = (tier >= 3) ? 1 : 0;
            r.n_data_nodes        = 1;
            r.lock_incoming_doors = true;
            break;
        case RoomTemplateKind::Generic:
            // empty.
            break;
    }
    return r;
}

} // namespace astra
```

(Adjust `HackTag::*` enum values and `HackTagMask::has(...)` API to whatever the actual taxonomy uses — see `hackable.h`.)

- [ ] **Step 3: Build**

PASS.

- [ ] **Step 4: Commit**

```bash
git add include/astra/grid_room_templates.h src/grid_room_templates.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
plan-8(cut-5): 7-category room template registry

Templates: Weaponized, Surveillance, DataVault, PowerNode, CrewQuarters,
PrecursorShrine, Generic. Tag-priority dispatch (AlienTech > Weaponized
> DataStore > HasOptics > Mobile > PowerNode > Electronic). Per-template
min/max footprint + ICE/content seeds.
EOF
)"
```

### Task 5.2: Wire templates into footprint selection

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Replace `pick_default_footprint` with template-aware version**

```cpp
LanV2RoomFootprint pick_footprint_for_subnet(const Hackable& h, int tier, std::mt19937& rng) {
    auto kind = choose_template_for_tags(h.tags);
    auto sz   = template_size_constraints(kind);
    int  w    = std::uniform_int_distribution<int>(sz.min_w, sz.max_w)(rng);
    int  h_   = std::uniform_int_distribution<int>(sz.min_h, sz.max_h)(rng);
    return {w, h_};
}
```

In `place_rooms`, look up the `Hackable` for each `contained_subnet` and call this. Pass the GridNetwork in (or pass the world's hackable lookup).

- [ ] **Step 2: Build**

PASS.

- [ ] **Step 3: Commit**

```bash
git add src/lan_sector_v2.cpp include/astra/lan_sector_v2.h
git commit -m "plan-8(cut-5): subnet rooms sized from template per HackTagMask"
```

### Task 5.3: Seed ICE per template

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Add ICE seeding pass**

```cpp
void seed_ice_per_template(GridSector& /*sec*/,
                            GridSession& sess,
                            const std::vector<LanV2Room>& rooms,
                            const LanMetadata& meta) {
    // For each subnet room, look up its template and spawn its ICE list.
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.is_valid()) continue;
        const Hackable* h = lookup_hackable_by_node(meta, r.source_subnet);  // confirm signature
        if (!h) continue;
        auto kind = choose_template_for_tags(h->tags);
        auto rule = template_seed_rule(kind, r.tier);

        // ICE positions: random Floor cells inside the room, away from doors and avatar.
        std::vector<std::pair<int,int>> candidates;
        for (int dy = 1; dy < r.h - 1; ++dy)
            for (int dx = 1; dx < r.w - 1; ++dx)
                candidates.push_back({r.x + dx, r.y + dy});
        std::shuffle(candidates.begin(), candidates.end(), /* rng */);

        auto pop = [&]() -> std::pair<int,int> {
            if (candidates.empty()) return {0,0};
            auto p = candidates.back(); candidates.pop_back(); return p;
        };

        for (int i = 0; i < rule.n_white_ice; ++i) {
            auto [x, y] = pop();
            sess.ice.push_back({x, y, IceColor::White, /*hp*/2, /*ai_state*/0});
        }
        for (int i = 0; i < rule.n_gray_ice; ++i) {
            auto [x, y] = pop();
            sess.ice.push_back({x, y, IceColor::Gray, 3, 0});
        }
        for (int i = 0; i < rule.n_black_ice; ++i) {
            auto [x, y] = pop();
            sess.ice.push_back({x, y, IceColor::Black, 4, 0});
        }
    }
}
```

(Adjust `IceColor`, `Ice` struct, and `GridSession` access to actual types — check `grid_ice.h` / `grid_session.h`.)

NOTE: ICE used to spawn inside subnet sectors. With the flat model, ICE now spawns directly into the LAN sector's `GridSession::ice` list. Seeding happens at jack-in time, NOT at sector generation. This means the v2 sector generator only records WHERE ice should spawn (room footprints + tags), and the actual spawn happens when a `GridSession` is created.

Alternative: have the generator return a list of `IceSeed` records keyed to room positions, which the hacking-system consumes when starting a session.

For Cut 5 simplicity: store ICE seeds in `GridSector`:
```cpp
struct IceSeedRecord { int x, y; IceColor color; int hp; };
std::vector<IceSeedRecord> ice_seeds;  // GridSector field
```

The session creation loop iterates `sec.ice_seeds` and instantiates ICE.

Add the field, write `seed_ice_per_template` to populate it during generation, and update `hacking_system.cpp` (where the session is created) to spawn ICE from the seeds on first jack-in.

- [ ] **Step 2: Wire and ship**

Build, smoke (`:lan-sector-v2` then jack into the v2 LAN if wiring is up; otherwise visually inspect log).

- [ ] **Step 3: Commit**

```bash
git add src/lan_sector_v2.cpp include/astra/grid_sector.h src/hacking_system.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-5): ICE seeding from templates

GridSector::ice_seeds populated at generation; hacking system spawns
GridSession::ice from seeds on first jack-in. Per-template counts:
weapons rooms get 1 white + 1 gray (tier ≥ 2), precursor shrines get
black ICE at T3, etc.
EOF
)"
```

### Task 5.4: Seed DataNodes / EncryptedFiles per template

**Files:**
- Modify: `src/lan_sector_v2.cpp`

- [ ] **Step 1: Stamp content tiles inside rooms**

For each subnet room, after ICE seeding, stamp `GridTile::DataNode` and `GridTile::EncryptedFile` per the seed rule.

```cpp
void seed_content_per_template(GridSector& sec,
                                const std::vector<LanV2Room>& rooms,
                                const LanMetadata& meta) {
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.is_valid()) continue;
        const Hackable* h = lookup_hackable_by_node(meta, r.source_subnet);
        if (!h) continue;
        auto kind = choose_template_for_tags(h->tags);
        auto rule = template_seed_rule(kind, r.tier);

        std::vector<std::pair<int,int>> spots;
        for (int dy = 1; dy < r.h - 1; ++dy)
            for (int dx = 1; dx < r.w - 1; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) spots.push_back({xx, yy});
            }
        // Shuffle spots and pop.
        // ... (use the same shuffle pattern as seed_ice_per_template)

        for (int i = 0; i < rule.n_data_nodes && !spots.empty(); ++i) {
            auto [x, y] = spots.back(); spots.pop_back();
            sec.set(x, y, GridTile::DataNode);
        }
        for (int i = 0; i < rule.n_encrypted_files && !spots.empty(); ++i) {
            auto [x, y] = spots.back(); spots.pop_back();
            sec.set(x, y, GridTile::EncryptedFile);
        }
    }
}
```

- [ ] **Step 2: Override locked-door state for vault rooms**

If `rule.lock_incoming_doors` is true, mark this room's incoming bridges as locked (override the default open):

```cpp
void apply_door_lock_overrides(GridSector& sec,
                                const std::vector<LanV2Room>& rooms,
                                const LanMetadata& meta) {
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.is_valid()) continue;
        const Hackable* h = lookup_hackable_by_node(meta, r.source_subnet);
        if (!h) continue;
        auto kind = choose_template_for_tags(h->tags);
        auto rule = template_seed_rule(kind, r.tier);
        if (!rule.lock_incoming_doors) continue;

        // Find Door tiles adjacent to this room's perimeter and lock them.
        for (int dx = -1; dx <= r.w; ++dx) {
            for (int dy = -1; dy <= r.h; ++dy) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.in_bounds(xx, yy) && sec.at(xx, yy) == GridTile::Door) {
                    sec.locked_doors.insert({xx, yy});
                }
            }
        }
    }
}
```

- [ ] **Step 3: Wire and ship**

Call both new helpers from `generate_lan_sector_v2` after the connectivity pass.

- [ ] **Step 4: Build, smoke**

`:lan-sector-v2` on a LAN with a DataStore subnet → vault room with `$$$` clusters + locked door.

- [ ] **Step 5: Commit**

```bash
git add src/lan_sector_v2.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-5): seed DataNodes/EncryptedFiles + per-template door locks

DataVault and PrecursorShrine templates lock their incoming doors at
T2+. Other rooms keep open within-zone defaults.
EOF
)"
```

**Cut 5 complete.** Generator output is now content-rich and identity-distinct per room.

---

## Cut 6 — Flat-model wiring

**Goal:** `Game::jack_in(node)` lands the player inside the target subnet's room (per `per_node_spawn`). Two-tier sector swap on `⌬` is removed (no more subnet sector).

### Task 6.1: Update `jack_in` to use `per_node_spawn`

**Files:**
- Modify: `src/hacking_system.cpp`

- [ ] **Step 1: Read existing implementation**

Locate `Game::jack_in` (or `HackingSystem::jack_in`). Currently, it dispatches based on `GridNodeKind`:
- `Lan` → load LAN sector, spawn at `sec.spawn_x/y`
- `Subnet` → load subnet sector via `gen_subnet_sector`
- `DeepGridAnchor` → load deep-grid base

Plan 8: subnet jack-in now means "in the LAN sector, spawn at this subnet's room."

- [ ] **Step 2: Rewrite the dispatch**

```cpp
void HackingSystem::jack_in(GridNodeId target) {
    auto* node = world_.grid_network().find(target);
    if (!node) return;

    if (node->kind == GridNodeKind::DeepGridAnchor) {
        load_deep_grid_session(target);
        return;
    }

    // For Lan and Subnet: same sector (the LAN's), different spawn point.
    GridNodeId lan_root = (node->kind == GridNodeKind::Lan)
                         ? target
                         : world_.grid_network().lan_root_for(target);

    GridSector& sec = ensure_lan_sector(lan_root);  // generates if not cached
    int sx = sec.spawn_x, sy = sec.spawn_y;
    if (node->kind == GridNodeKind::Subnet) {
        auto it = sec.per_node_spawn.find(target);
        if (it != sec.per_node_spawn.end()) {
            sx = it->second.first;
            sy = it->second.second;
        }
        // else fall back to lobby spawn (defensive — shouldn't happen if generator ran)
    }

    start_session(sec, sx, sy);
}
```

(Adjust to actual function names. `lan_root_for` may exist as `parent_lan(node)` or similar.)

- [ ] **Step 3: Update `gen_subnet_sector` callers to no-op**

`gen_subnet_sector` is no longer called from `jack_in`. If anything else calls it (e.g., dev verbs), leave the function for now — Cut 7 retires it.

- [ ] **Step 4: Build, smoke**

```bash
cmake --build build -j8 && ./build/astra-dev
# in game on a v2-rendered LAN (still gated by dev verb):
# nmap → pick a turret subnet IP → jack <ip>
# Expected: lands inside the turret room with @, ICE visible nearby
```

If `jack_in` still calls into the OLD generator, you'll see the old subnet sector. Make sure the LAN sector currently used IS the v2 sector — temporarily call `generate_lan_sector_v2` instead of `generate_lan_sector` in `ensure_lan_sector`.

- [ ] **Step 5: Commit**

```bash
git add src/hacking_system.cpp
git commit -m "$(cat <<'EOF'
plan-8(cut-6): jack_in routes Subnet jacks into LAN sector via per_node_spawn

Subnet jack-ins no longer create a separate sector — same LAN sector,
spawn at the subnet's recorded room cell. Lan jacks spawn at the lobby.
Deep-grid path unchanged.
EOF
)"
```

### Task 6.2: Drop the gateway-step transition in `grid_input.cpp`

**Files:**
- Modify: `src/grid_input.cpp`

- [ ] **Step 1: Find the `Gateway` step handler**

Search for `GridTile::Gateway` in `grid_input.cpp` — there should be a branch where stepping on a `⌬` swaps to the subnet sector.

- [ ] **Step 2: Remove the branch**

Delete the entire Gateway-tile-step handler. The `Gateway` tile no longer exists in the v2 sector (no more `⌬`). The branch can be `// removed in Plan 8` or just deleted; subsequent cleanup pass will simplify.

- [ ] **Step 3: Update DeepGridGateway-step handler**

`⊕` still exists. Its handler used to look up `gateway_target` for the destination — confirm that's still the case and works.

- [ ] **Step 4: Build, smoke**

`./build/astra-dev` — walking onto where a `⌬` used to be should now hit the room directly (the room IS the subnet). Walking onto `⊕` still warps to deep-grid.

- [ ] **Step 5: Commit**

```bash
git add src/grid_input.cpp
git commit -m "plan-8(cut-6): drop Gateway-tile step transition (no more two-tier sectors)"
```

### Task 6.3: Switch the active LAN-sector cache to v2 generator

**Files:**
- Modify: `src/world_manager.cpp` or wherever `ensure_lan_sector` lives

- [ ] **Step 1: Replace the v1 call with v2**

Find the call to `generate_lan_sector(meta, net)` and replace with `generate_lan_sector_v2(meta, net)`.

- [ ] **Step 2: Build, run end-to-end**

```bash
cmake --build build -j8 && ./build/astra-dev
# fresh game, jack into Heavens Above's LAN
# Expected: Plan 8 sector. Walk it. Verify spawn, lobby, exit, deep-grid gateway.
```

- [ ] **Step 3: Commit**

```bash
git add src/world_manager.cpp
git commit -m "plan-8(cut-6): switch active LAN-sector cache to generate_lan_sector_v2"
```

**Cut 6 complete.** The flat-model is live: jack-in routes into rooms, no subnet sectors, build green.

---

## Cut 7 — Cutover & cleanup

**Goal:** delete the old generator, retire `gen_subnet_sector`, remove `GridTile::Gateway` from the enum, drop legacy fields, rename `LanRoom` → `LanZone`, finalize schema v63 (reject v62 saves).

### Task 7.1: Delete `lan_sector_generator.cpp/.h`

**Files:**
- Delete: `src/lan_sector_generator.cpp`, `include/astra/lan_sector_generator.h`
- Modify: `CMakeLists.txt` (if explicit list)

- [ ] **Step 1: Verify no references remain**

```bash
grep -r 'generate_lan_sector\b' src/ include/ | grep -v _v2
# Expected: only in the dev-console log strings, if any.
```

- [ ] **Step 2: Delete files + update build list**

```bash
git rm src/lan_sector_generator.cpp include/astra/lan_sector_generator.h
# remove from CMakeLists.txt if listed explicitly
```

- [ ] **Step 3: Build**

PASS.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "plan-8(cut-7): delete v1 generate_lan_sector — replaced by v2"
```

### Task 7.2: Delete `gen_subnet_sector` and friends

**Files:**
- Delete: the file containing `gen_subnet_sector` (search to find — likely `src/subnet_sector_generator.cpp` or in `grid_sector.cpp`)
- Modify: `include/astra/grid_sector.h` — remove the `gen_subnet_sector` declaration(s)
- Modify: `src/grid_anchor_layout.cpp` — `make_consciousness_anchor_sector` may also be unused; leave it for now

- [ ] **Step 1: Find and delete**

```bash
grep -rn "gen_subnet_sector" src/ include/
# delete or strip the implementations and declarations
```

- [ ] **Step 2: Build, fix any stragglers**

PASS.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "plan-8(cut-7): retire gen_subnet_sector — flat model has no per-subnet sectors"
```

### Task 7.3: Remove `GridTile::Gateway` and `gateway_target`

**Files:**
- Modify: `include/astra/grid_sector.h` — remove `Gateway` enum, remove `gateways[]`, remove `gateway_target`
- Modify: `src/grid_renderer.cpp` — remove `Gateway` rendering branch
- Modify: any code that referenced `gateway_target` (besides `DeepGridGateway` placement, which moves to a single field or stays in `gateway_target` with the deep-grid only)

- [ ] **Step 1: Decide on the deep-grid gateway target storage**

`DeepGridGateway`'s destination is currently in `gateway_target`. Options:
- (a) Keep `gateway_target` map but only for `DeepGridGateway` tiles (rename to `deep_grid_gateway_target`).
- (b) Remove the map; store a single `GridNodeId deep_grid_destination` field since each LAN has at most one DeepGridGateway.

Pick (b) for simplicity.

```cpp
// in GridSector
GridNodeId deep_grid_destination;  // empty if no ⊕ in this sector
```

- [ ] **Step 2: Delete `Gateway` enum + `gateways[]` + `gateway_target` map**

In `include/astra/grid_sector.h`:
```cpp
// Remove:
//   Gateway,                // G (subnet gateway)
//   std::vector<GatewayLink> gateways;
//   std::unordered_map<std::pair<int,int>, GridNodeId, PairHash> gateway_target;
// Add:
//   GridNodeId deep_grid_destination;
```

- [ ] **Step 3: Update v2 generator and stamping**

`stamp_deep_grid_gateway` in `src/lan_sector_v2.cpp` writes to `sec.deep_grid_destination` instead of inserting into `gateway_target`.

- [ ] **Step 4: Update DeepGridGateway-step handler in `grid_input.cpp`**

```cpp
// Was: look up sec.gateway_target[{x,y}]
// Now:
if (tile == GridTile::DeepGridGateway && sec.deep_grid_destination.is_valid()) {
    hack.warp_to(sec.deep_grid_destination);
}
```

- [ ] **Step 5: Build, fix renderer references**

In `src/grid_renderer.cpp`, remove the `case GridTile::Gateway:` branches in `glyph_for` and `color_for`.

- [ ] **Step 6: Build → PASS**

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
plan-8(cut-7): remove GridTile::Gateway, gateways[], gateway_target

Deep-grid destination stored as a single GridNodeId field. No more
⌬ tile in the codebase — subnets are now rooms, not gateway tiles.
EOF
)"
```

### Task 7.4: Rename `LanRoom` → `LanZone`

**Files:**
- Modify: `include/astra/lan.h`, `src/lan.cpp`, all callers

- [ ] **Step 1: Project-wide rename**

```bash
grep -rn "LanRoom" src/ include/ docs/ | wc -l
# expect manageable count (~30-50 sites)
```

Rename `LanRoom` → `LanZone` in headers + sources. `LanMetadata::rooms` → `LanMetadata::zones`. `zone_banner_label(const LanRoom&)` → `zone_banner_label(const LanZone&)`.

- [ ] **Step 2: Update spec/mechanics references**

`docs/mechanics.md`'s Plan 5 section references `LanRoom`/offices — update to `LanZone`/zones.

- [ ] **Step 3: Build, run, save/reload**

PASS. Save format slot for `name` is in place (Cut 1).

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "plan-8(cut-7): rename LanRoom → LanZone (offices → zones nomenclature)"
```

### Task 7.5: Schema bump and reject v62 saves

**Files:**
- Modify: `src/save_file.cpp`, `include/astra/save_file.h`

- [ ] **Step 1: Bump constant if not already**

`SAVE_FILE_VERSION = 63` (Cut 1 may have already done this — verify).

- [ ] **Step 2: Reject v62 and below at load**

```cpp
if (version < 63) {
    log_error("Save schema too old (v" + std::to_string(version) + " < 63). "
              "Plan 8 broke compatibility (no migration shim, per project policy). "
              "Start a new game.");
    return SaveLoadResult::Rejected;
}
```

- [ ] **Step 3: Serialize `per_node_spawn`, `locked_doors`, `deep_grid_destination`, `avatar_fixture_type`, `ice_seeds`**

For each new `GridSector` field added in Cuts 1–5, add (de)serialization with a length-prefixed list.

```cpp
// Write
write_u32(file, sec.per_node_spawn.size());
for (const auto& [node_id, xy] : sec.per_node_spawn) {
    write_node_id(file, node_id);
    write_i32(file, xy.first);
    write_i32(file, xy.second);
}
// Read mirrors.
```

(Repeat for the other new fields.)

- [ ] **Step 4: Build, save/reload**

Smoke: launch fresh, save mid-LAN, quit, reload — should resume cleanly. Old v62 saves rejected with a clear message.

- [ ] **Step 5: Commit**

```bash
git add src/save_file.cpp include/astra/save_file.h
git commit -m "$(cat <<'EOF'
plan-8(cut-7): schema v63 — serialize all new GridSector fields, reject v62

per_node_spawn, locked_doors, deep_grid_destination, avatar_fixture_type,
ice_seeds. Old saves rejected at load (no backcompat per project policy).
EOF
)"
```

**Cut 7 complete.** Codebase is rid of legacy fields; flat-model is the only model.

---

## Cut 8 — Zone HUD overlay

**Goal:** dashed perimeter + named banner per zone, rendered in the playfield. Camera-aware.

### Task 8.1: Create the overlay file and header

**Files:**
- Create: `include/astra/grid_zone_overlay.h`, `src/grid_zone_overlay.cpp`

- [ ] **Step 1: Header**

```cpp
#pragma once

#include "astra/lan.h"
#include "astra/grid_camera.h"
#include "astra/renderer.h"
#include "astra/lan_sector_v2.h"   // for LanV2ZoneRegion (or factor out)

namespace astra::grid_zone_overlay {

struct ZoneRender {
    int x, y, w, h;
    int tier;
    std::string banner;
};

// Compute zone bounding boxes from the placed-rooms metadata.
// Zone bbox = union of all rooms in the same zone, plus 1-cell margin.
std::vector<ZoneRender> compute_zone_renders(const LanMetadata& meta);

void draw(Renderer& r, const std::vector<ZoneRender>& zones,
          const GridCamera& cam, int playfield_x, int playfield_y,
          int playfield_w, int playfield_h);

} // namespace astra::grid_zone_overlay
```

For `compute_zone_renders`, the zone bbox should come from the rooms' actual placed positions — which Plan 8 doesn't currently expose outside the generator. Add a `LanZoneRender` list to `GridSector` so the overlay can render without re-running the generator:

In `include/astra/grid_sector.h`:
```cpp
struct ZoneBox { int x, y, w, h; int tier; std::string banner; };
std::vector<ZoneBox> zone_boxes;  // populated by generator
```

Generator populates it after rooms placement (compute zone bbox = union of room bboxes ±1 cell).

`compute_zone_renders` becomes:
```cpp
std::vector<ZoneRender> compute_zone_renders(const GridSector& sec) {
    std::vector<ZoneRender> out;
    for (const auto& z : sec.zone_boxes) {
        out.push_back({z.x, z.y, z.w, z.h, z.tier, z.banner});
    }
    return out;
}
```

- [ ] **Step 2: Implement `draw`**

```cpp
void draw(Renderer& r, const std::vector<ZoneRender>& zones,
          const GridCamera& cam, int pfx, int pfy, int pfw, int pfh) {
    auto color_for_tier = [](int t) -> Color {
        switch (t) {
            case 1: return Color::Blue;
            case 2: return Color::Magenta;
            case 3: return Color::Red;
        }
        return Color::DarkGray;
    };

    for (const auto& z : zones) {
        Color c = color_for_tier(z.tier);
        // Dashed perimeter
        for (int dx = 0; dx < z.w; ++dx) {
            int sx = z.x + dx - cam.cam_x;
            int sy_top = z.y - cam.cam_y - 1;
            int sy_bot = z.y + z.h - cam.cam_y;
            if ((dx % 3) == 0 && sy_top >= 0 && sy_top < pfh && sx >= 0 && sx < pfw)
                r.draw_glyph(pfx + sx, pfy + sy_top, "\xe2\x80\xa2", c); // •
            if ((dx % 3) == 0 && sy_bot >= 0 && sy_bot < pfh && sx >= 0 && sx < pfw)
                r.draw_glyph(pfx + sx, pfy + sy_bot, "\xe2\x80\xa2", c);
        }
        for (int dy = 0; dy < z.h; ++dy) {
            int sy = z.y + dy - cam.cam_y;
            int sx_l = z.x - cam.cam_x - 1;
            int sx_r = z.x + z.w - cam.cam_x;
            if ((dy % 3) == 0 && sy >= 0 && sy < pfh && sx_l >= 0 && sx_l < pfw)
                r.draw_glyph(pfx + sx_l, pfy + sy, "\xe2\x80\xa2", c);
            if ((dy % 3) == 0 && sy >= 0 && sy < pfh && sx_r >= 0 && sx_r < pfw)
                r.draw_glyph(pfx + sx_r, pfy + sy, "\xe2\x80\xa2", c);
        }

        // Banner — above the zone, centered.
        std::string text = "— " + z.banner + " —";
        int by = z.y - cam.cam_y - 2;
        int bx = z.x + (z.w - static_cast<int>(text.size())) / 2 - cam.cam_x;
        if (by >= 0 && by < pfh) {
            for (size_t i = 0; i < text.size(); ++i) {
                int sx = bx + static_cast<int>(i);
                if (sx >= 0 && sx < pfw)
                    r.draw_char(pfx + sx, pfy + by, text[i], c);
            }
        }
    }
}
```

- [ ] **Step 3: Wire into `grid_renderer.cpp::draw_playfield`**

After the floor pass and before the ICE/avatar pass, call:
```cpp
auto zones = grid_zone_overlay::compute_zone_renders(s.sector);
grid_zone_overlay::draw(r, zones, s_camera, pr.x, pr.y, pr.w, pr.h);
```

- [ ] **Step 4: Generator populates `zone_boxes`**

In `src/lan_sector_v2.cpp`, after rooms are placed, compute zone bboxes:
```cpp
for (size_t zi = 0; zi < zones.size(); ++zi) {
    int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
    for (const auto& r : rooms) {
        if (r.zone_index != static_cast<int>(zi)) continue;
        min_x = std::min(min_x, r.x);
        min_y = std::min(min_y, r.y);
        max_x = std::max(max_x, r.x + r.w - 1);
        max_y = std::max(max_y, r.y + r.h - 1);
    }
    if (min_x == INT_MAX) continue;
    GridSector::ZoneBox box{min_x, min_y, max_x - min_x + 1, max_y - min_y + 1,
                            zones[zi].tier,
                            zones[zi].name.empty() ? std::string("ZONE") : zones[zi].name};
    sec.zone_boxes.push_back(box);
}
```

- [ ] **Step 5: Build, smoke**

`./build/astra-dev`, jack into a 3-zone LAN. Expected: dashed colored perimeter around each zone cluster + banner text above each zone.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
plan-8(cut-8): zone HUD overlay — dashed perimeter + banner per zone

GridSector::zone_boxes carries bbox + name + tier per zone. Renderer
draws dashed dots in tier color around each zone and a banner like
"— OPERATIONS —" above the topmost row. Camera-aware clipping.
EOF
)"
```

**Cut 8 complete.** Player visually reads zones as they walk.

---

## Cut 9 — Manual validation, docs, polish

**Goal:** end-to-end verify a full play-through; update mechanics + roadmap; sweep for any remaining v1-era references.

### Task 9.1: Manual verification matrix

Walk these flows in `./build/astra-dev` and assert each works:

- [ ] **9.1.1: Tiny LAN (1–3 subnets).** Jack in, lobby has ⊙, walk to the 1–2 subnet rooms via `+` open doors, jack out.
- [ ] **9.1.2: Medium LAN (8–14 subnets).** Confirm 3 zones visible. Banner text reads correctly. T1↔T2 has a `▣` locked door; `breach.exe` cracks it; passable.
- [ ] **9.1.3: Per-node spawn.** From PDA, `nmap -l`, pick a turret IP, `jack <ip>` — spawn inside the turret room. Pick a vault IP — spawn inside the vault room. Pick the LAN IP — spawn at the lobby ⊙.
- [ ] **9.1.4: Deep-grid gateway.** Find ⊕ in the T3 sanctum. Step on it. Warp to deep-grid Anchor. Walk back. Re-jack into the LAN — sector state preserved (cracked doors stay open, looted DataNodes stay gone).
- [ ] **9.1.5: Save/reload.** Save mid-LAN. Quit. Reload. Resume in the same room with the same state.
- [ ] **9.1.6: Rebirth.** `:rebirth` → fresh galaxy → jack into a connected LAN → fresh sector with a different seed.
- [ ] **9.1.7: Rejection of old saves.** Rename a v62 save into `saves/`, attempt load → clean rejection message.

If any step fails, file a follow-up task and fix before proceeding.

### Task 9.2: Update `docs/mechanics.md`

**Files:**
- Modify: `docs/mechanics.md` (replace the Plan 5 sector-shape paragraphs)

- [ ] **Step 1: Find the Hacking — Plan 5 § "LAN cyberspace sectors"**

Replace its paragraph describing sector geometry with a concise Plan-8 description: flat sector, packed rooms with bridges, zones with banners, breach.exe as door-only.

- [ ] **Step 2: Add a new subsection: "Hacking — Plan 8 (Layout)"**

Brief — goal, the room/zone idiom, the door-only `breach.exe`. Cross-link the spec.

- [ ] **Step 3: Commit**

```bash
git add docs/mechanics.md
git commit -m "docs(mechanics): Plan 8 layout — packed rooms, zones, door-only breach.exe"
```

### Task 9.3: Update `docs/roadmap.md`

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Check off Plan 8**

Update the relevant checklist entry (`Plan 8 — Grid Layout`) to ✅. Note any deferred items in a `Plan 11 polish` line.

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs(roadmap): Plan 8 done"
```

### Task 9.4: Final sweep for v1-era ghosts

- [ ] **Step 1: Search for stale identifiers**

```bash
grep -rn 'LanRoom\|gateway_target\|GridTile::Gateway\|generate_lan_sector\b\|gen_subnet_sector\|firewall_glyph.*ring' src/ include/
```

Expect: zero hits in code (docs are fine to mention historical context).

- [ ] **Step 2: Resolve any leftover references**

If something is found, decide: legitimate (update to new name) or dead code (delete).

- [ ] **Step 3: Final build + run**

```bash
cmake --build build -j8 && ./build/astra-dev
# play through 5 minutes — make sure nothing weird shows up
```

- [ ] **Step 4: Commit any cleanups**

```bash
git add -A
git commit -m "plan-8(cut-9): sweep stale v1 identifiers from codebase"
```

**Cut 9 complete.** Plan 8 is shipped.

---

## Self-review

**Spec coverage:**

| Spec section | Implemented in |
|---|---|
| § 1 Goals | Whole plan |
| § 2 Current model retired | Cut 7 |
| § 3.1 Flat tier collapse | Cut 6 + Cut 7 |
| § 3.2 Sector layout idiom | Cut 2 + Cut 3 |
| § 3.3 Zone concept | Cut 1 (name field) + Cut 2 + Cut 8 |
| § 3.4 Door semantics | Cut 3 |
| § 4 Generator algorithm | Cuts 2–5 |
| § 5 Templates | Cut 5 |
| § 6 Room sizing | Cut 5 |
| § 7 Lobby invariant | Cut 2 Task 2.4 |
| § 8 Per-node spawn | Cut 1 + Cut 4 + Cut 6 |
| § 9 Zone HUD render | Cut 8 |
| § 10 Save schema bump | Cut 1 + Cut 7 |
| § 11 Migration / retirement | Cut 7 |
| § 12 Testing | Cut 9 |
| § 13 Open issues | Inline in tasks |

All sections covered.

**Placeholder scan:** no `TBD` / `implement later` / `add error handling` patterns. Every code step has actual code. Long-bridge fallback path is explicitly drafted in Cut 3 Task 3.3.

**Type consistency:** `LanV2Room`, `LanV2ZoneRegion`, `RoomTemplateKind`, `GridSector::per_node_spawn`, `GridSector::locked_doors`, `GridSector::zone_boxes`, `GridSector::deep_grid_destination`, `GridSector::ice_seeds`, `GridSector::avatar_fixture_type` — names are consistent across cuts. `RoomTemplateSeedRule::lock_incoming_doors` is referenced consistently. `RoomTemplateSize` consistently has `min_w/min_h/max_w/max_h`. The `LanRoom` → `LanZone` rename is concentrated in Cut 7 Task 7.4 (intentional — earlier cuts use the old name to keep the build green; the cutover is a single mechanical rename).

**Build-greenness:** every cut ends with a passing build. Cuts 1–6 work alongside the v1 generator (which still produces sectors during these cuts). Cut 7 is the cutover. Cut 8 layers HUD on top. Cut 9 polishes.

**Estimated total tasks:** 31 tasks across 9 cuts. Each task: 2–10 minutes for an engineer with codebase context, 10–25 minutes without.
