# Hacking Plan 4 — D-Layer (Deep-Grid Persistence) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the deep-Grid meta-layer of Astra's hacking system — a second persistent save scope (`consciousness.dat`) that carries lore, AI contacts, deep-Grid base, and signature programs across Sgr A\* rebirth, plus a non-hacker access path (Neural Backup implant + Soul Mirror channel), the two `Cat_Hacking` capstone wirings, a navigable graph overlay on the PDA, and the supporting renderer/generator infrastructure (camera + regional BSP).

**Architecture:** Persistence layered as two independent files (`save_<seed>.astra` per-galaxy, `consciousness.dat` per-profile). Capstone-gated on the write side: a `Cat_Hacking`-only character writes lore + currency; a `ConsciousnessAnchor` capstone writes everything; a non-hacker with the Neural Backup implant gets passive lore-only sync at Precursor consoles via the same `SoulMirrorChannel` backend that powers the manual `Sync Soul` action. Rebirth is a wiring task in the navigation layer — when the player crosses Sgr A\*, a confirmation modal lists what survives, an optional cinematic plays, the galaxy file is deleted, the consciousness state is applied to a fresh galaxy. The PDA's `netmap` command opens an overlay popup (regional + deep-Grid zoom) drawn into the Hacking tab content area; subnet zoom is reserved (Plan 6). The grid renderer gains a follow-avatar camera so sectors past ~30×20 work.

**Tech Stack:** C++20, CMake, hand-rolled binary save format, the project's dev-console verb system for manual verification (no unit-test framework — verification is `cmake --build build -DDEV=ON` + dev-verb-driven smoke tests).

**Spec:** `docs/superpowers/specs/2026-04-30-hacking-deep-grid-design.md`
**Parent spec:** `docs/superpowers/specs/2026-04-29-hacking-design.md`

---

## File map

**New files:**
- `include/astra/consciousness_save.h`, `src/consciousness_save.cpp` — second save-scope I/O.
- `include/astra/rebirth_sequence.h`, `src/rebirth_sequence.cpp` — Sgr A\* rebirth UX runtime.
- `include/astra/soul_mirror.h`, `src/soul_mirror.cpp` — shared channel backend (manual + passive Neural Backup).
- `include/astra/grid_netmap_widget.h`, `src/grid_netmap_widget.cpp` — PDA Hacking tab graph overlay.
- `include/astra/grid_regional_generator.h`, `src/grid_regional_generator.cpp` — BSP wrapper for 4–8 room regional darknets.
- `include/astra/grid_camera.h`, `src/grid_camera.cpp` — follow-avatar deadzone camera.

**Modified files:**
- `include/astra/save_file.h` — bump `SAVE_FILE_VERSION 54 → 55`; add `Player::implants` array persistence hooks.
- `src/save_file.cpp` — read/write the new schema; reject v54.
- `include/astra/player.h` — add `std::array<std::optional<Item>, 2> implants`.
- `include/astra/item.h` — add `ItemType::Implant`.
- `src/item_defs.cpp` — Neural Backup definition.
- `src/program.cpp` — add `PulseHammer` and `DaemonHijack` T3 entries; `include/astra/program.h` — add `ProgramId::PulseHammer = 200`, `DaemonHijack = 201`.
- `src/tinkering.cpp` — T3 program recipes gated by `CodeCraft`.
- `src/skill_defs.cpp` — wire runtime effects of `CodeCraft` and `ConsciousnessAnchor`.
- `src/grid_anchor_layout.cpp` — add `make_player_deep_grid_base()`.
- `src/grid_renderer.cpp` — replace fixed origin with `GridCamera`.
- `src/pda_hacking_tab.cpp` — `hack_term_cmd_netmap()` opens overlay instead of emitting ASCII.
- `src/pda_screen.cpp` (or wherever Equipment tab lives) — `Tab` toggle between Equipment and Implant paper-doll views.
- `src/dev_console.cpp` — new dev verbs `:rebirth`, `:spawn-implant`, `:unlock-anchor`, `:sync-soul`.
- `src/hackable.cpp` — add `lore_fragments` and `soul_mirror_progress` to `Hackable` for Precursor consoles.
- `src/hacking_system.cpp` — wire rebirth trigger from world-side jump-to-Sgr-A\* handler.
- `docs/mechanics.md`, `docs/items.md`, `docs/roadmap.md` — content updates.

---

## Conventions

Every task follows this rhythm:

1. Write or modify code per the Files list.
2. Run `cmake -B build -DDEV=ON && cmake --build build`. Expected: success.
3. (Where applicable) Manual smoke test using a dev verb listed in the task. Expected behavior is stated.
4. `git add <files> && git commit` with the message shown.

Astra has no unit-test framework; verification is the build + dev-verb smoke test. Where a task is purely structural (e.g. enum addition with no runtime effect yet), the verification is "the build still passes."

Never use `git rebase -i` or `git add -i` — both are forbidden. Use `git add <paths>` and write commits directly.

---

## Task 1 — `consciousness_save.h/cpp` schema and I/O

**Files:**
- Create: `include/astra/consciousness_save.h`
- Create: `src/consciousness_save.cpp`

**Goal:** Define the schema struct and its read/write/delete free functions. Independent of `SaveData`. No integration with `Game` or `Player` yet — pure I/O.

- [ ] **Step 1.1 — Create the header**

```cpp
// include/astra/consciousness_save.h
#pragma once

#include "astra/grid_sector.h"
#include "astra/item.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

inline constexpr uint32_t CONSCIOUSNESS_SAVE_VERSION = 1;

struct LoreFragmentRef {
    std::string archive_id;        // e.g. "ARCH-Hangar7-12x4"
    uint32_t galaxy_seed_origin = 0;
    int32_t  world_tick_origin  = 0;
};

struct AiContact {
    uint32_t faction_id = 0;
    int32_t  reputation = 0;       // -100..+100
};

struct ConsciousnessSave {
    uint32_t version = CONSCIOUSNESS_SAVE_VERSION;
    uint64_t consciousness_id = 0;
    uint32_t rebirth_count = 0;
    bool     seen_first_rebirth = false;

    std::vector<LoreFragmentRef> lore_archive;
    int32_t                      grid_currency = 0;
    std::vector<AiContact>       ai_contacts;

    // Hacker-only — populated only with ConsciousnessAnchor capstone unlocked.
    std::optional<GridSector> deep_grid_base;
    std::vector<Item>         signature_program_rack;
};

// Returns the path the file lives at. Same directory as save_directory().
std::filesystem::path consciousness_save_path();

bool write_consciousness(const ConsciousnessSave& cs);

// Reads into out. Returns false if file missing or schema mismatch (in which
// case `out` is left default-constructed). Never throws.
bool read_consciousness(ConsciousnessSave& out);

// Dev verb only. Used by `:rebirth --reset`.
bool delete_consciousness();

} // namespace astra
```

- [ ] **Step 1.2 — Implement I/O**

Implementation pattern: mirror `write_save`/`read_save` in `src/save_file.cpp`. Use the same `Reader`/`Writer` helpers if possible (they're file-scoped today; the simplest move is to duplicate their tiny shape inside `consciousness_save.cpp` rather than extracting). Atomic-write by writing to `consciousness.dat.tmp` then renaming to `consciousness.dat`. Reject any version other than `CONSCIOUSNESS_SAVE_VERSION`.

```cpp
// src/consciousness_save.cpp
#include "astra/consciousness_save.h"
#include "astra/save_file.h"   // for save_directory()

#include <cstdio>
#include <fstream>
#include <vector>

namespace astra {

std::filesystem::path consciousness_save_path() {
    return save_directory() / "consciousness.dat";
}

namespace {
class Writer {
public:
    explicit Writer(std::ofstream& out) : out_(out) {}
    void write_u8(uint8_t v)  { out_.put(static_cast<char>(v)); }
    void write_u32(uint32_t v){ out_.write(reinterpret_cast<const char*>(&v), 4); }
    void write_u64(uint64_t v){ out_.write(reinterpret_cast<const char*>(&v), 8); }
    void write_i32(int32_t v) { out_.write(reinterpret_cast<const char*>(&v), 4); }
    void write_str(const std::string& s) {
        write_u32(static_cast<uint32_t>(s.size()));
        out_.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
private:
    std::ofstream& out_;
};

class Reader {
public:
    explicit Reader(std::ifstream& in) : in_(in) {}
    uint8_t read_u8()  { char c; in_.get(c); return static_cast<uint8_t>(c); }
    uint32_t read_u32(){ uint32_t v; in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    uint64_t read_u64(){ uint64_t v; in_.read(reinterpret_cast<char*>(&v), 8); return v; }
    int32_t read_i32() { int32_t v;  in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    std::string read_str() {
        uint32_t n = read_u32();
        std::string s(n, '\0');
        in_.read(s.data(), static_cast<std::streamsize>(n));
        return s;
    }
private:
    std::ifstream& in_;
};
} // namespace

bool write_consciousness(const ConsciousnessSave& cs) {
    auto dir = save_directory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto tmp = dir / "consciousness.dat.tmp";
    auto final_path = dir / "consciousness.dat";

    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out) return false;
        Writer w(out);
        w.write_u32(cs.version);
        w.write_u64(cs.consciousness_id);
        w.write_u32(cs.rebirth_count);
        w.write_u8(cs.seen_first_rebirth ? 1 : 0);

        w.write_u32(static_cast<uint32_t>(cs.lore_archive.size()));
        for (const auto& f : cs.lore_archive) {
            w.write_str(f.archive_id);
            w.write_u32(f.galaxy_seed_origin);
            w.write_i32(f.world_tick_origin);
        }

        w.write_i32(cs.grid_currency);

        w.write_u32(static_cast<uint32_t>(cs.ai_contacts.size()));
        for (const auto& c : cs.ai_contacts) {
            w.write_u32(c.faction_id);
            w.write_i32(c.reputation);
        }

        // deep_grid_base — write a present flag + GridSector body.
        // The GridSector serializer will be added in Task 9 — for now,
        // write-only the present flag and assert that a v1 file with no
        // base is the common case.
        w.write_u8(cs.deep_grid_base.has_value() ? 1 : 0);
        // (GridSector body deferred to Task 9 alongside player base.)

        // signature_program_rack — vector<Item>. Reuse the existing
        // Item serializer from save_file.cpp by including it via a
        // forwarded helper; for v1 with empty rack this writes 0.
        w.write_u32(static_cast<uint32_t>(cs.signature_program_rack.size()));
        // Body deferred to Task 9.
    }

    std::filesystem::rename(tmp, final_path, ec);
    return !ec;
}

bool read_consciousness(ConsciousnessSave& out) {
    auto path = consciousness_save_path();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    Reader r(in);
    uint32_t ver = r.read_u32();
    if (ver != CONSCIOUSNESS_SAVE_VERSION) return false;
    out.version = ver;
    out.consciousness_id    = r.read_u64();
    out.rebirth_count       = r.read_u32();
    out.seen_first_rebirth  = r.read_u8() != 0;

    uint32_t lore_n = r.read_u32();
    out.lore_archive.clear();
    out.lore_archive.reserve(lore_n);
    for (uint32_t i = 0; i < lore_n; ++i) {
        LoreFragmentRef f;
        f.archive_id          = r.read_str();
        f.galaxy_seed_origin  = r.read_u32();
        f.world_tick_origin   = r.read_i32();
        out.lore_archive.push_back(std::move(f));
    }

    out.grid_currency = r.read_i32();

    uint32_t ai_n = r.read_u32();
    out.ai_contacts.clear();
    out.ai_contacts.reserve(ai_n);
    for (uint32_t i = 0; i < ai_n; ++i) {
        AiContact c;
        c.faction_id = r.read_u32();
        c.reputation = r.read_i32();
        out.ai_contacts.push_back(c);
    }

    uint8_t base_present = r.read_u8();
    if (base_present) {
        // Task 9 wires GridSector reading.
        out.deep_grid_base = std::nullopt;
    } else {
        out.deep_grid_base = std::nullopt;
    }

    uint32_t rack_n = r.read_u32();
    (void)rack_n;
    out.signature_program_rack.clear();
    // Task 9 wires Item rack body.

    return static_cast<bool>(in);
}

bool delete_consciousness() {
    std::error_code ec;
    return std::filesystem::remove(consciousness_save_path(), ec) && !ec;
}

} // namespace astra
```

- [ ] **Step 1.3 — Wire into CMakeLists**

Add `src/consciousness_save.cpp` to the source list in `CMakeLists.txt`. The existing pattern adds `.cpp` files alphabetically inside the `astra_lib` (or equivalent) target.

- [ ] **Step 1.4 — Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: clean build, 0 warnings from the new file.

- [ ] **Step 1.5 — Commit**

```bash
git add include/astra/consciousness_save.h src/consciousness_save.cpp CMakeLists.txt
git commit -m "feat(grid): consciousness_save schema + I/O scaffolding (Task 1)"
```

---

## Task 2 — Save schema bump v54→v55 + `Player::implants`

**Files:**
- Modify: `include/astra/save_file.h` (bump version)
- Modify: `include/astra/player.h` (add implants array)
- Modify: `src/save_file.cpp` (serialize/deserialize implants)
- Modify: `include/astra/item.h` (add `ItemType::Implant`)

**Goal:** Galaxy save knows about implant slots; old v54 saves are rejected.

- [ ] **Step 2.1 — Bump version**

Edit `include/astra/save_file.h`:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 55;   // v55: Plan 4 — implants + soul-mirror state
```

Update the trailing comment per the existing convention.

- [ ] **Step 2.2 — Add ItemType::Implant**

Edit `include/astra/item.h` — append `Implant` to the `ItemType` enum after `Program`:

```cpp
enum class ItemType : uint8_t {
    // ... existing entries ...
    Cyberdeck,
    Program,
    Implant,        // cybernetic implant — held in an Implant slot
};
```

Update `item_type_name()` in `src/item.cpp` (or wherever it's defined) to add the case.

- [ ] **Step 2.3 — Add implants array to Player**

Edit `include/astra/player.h`. Find the equipment-related fields and add adjacent:

```cpp
#include <array>
#include <optional>

// ... inside Player struct ...
std::array<std::optional<Item>, 2> implants{};   // Plan 4: 2 slots, expand later.
```

(Use the existing `Item` include path; do not introduce new headers.)

- [ ] **Step 2.4 — Persist implants in galaxy save**

Edit `src/save_file.cpp`. Find the section where `Player` is serialized (search for `// Player` or `data.player`). After the existing equipment serialization, write the implants:

```cpp
// implants — v55
for (const auto& slot : data.player.implants) {
    if (slot) {
        w.write_u8(1);
        write_item(w, *slot);   // reuse existing item serializer
    } else {
        w.write_u8(0);
    }
}
```

Mirror in the reader:

```cpp
// implants — v55
for (auto& slot : data.player.implants) {
    if (r.read_u8() != 0) {
        Item it;
        read_item(r, it);
        slot = it;
    } else {
        slot = std::nullopt;
    }
}
```

(`write_item`/`read_item` are placeholders — use the actual function names in `save_file.cpp`. If items are inlined rather than via helpers, inline the serialization in the loop.)

- [ ] **Step 2.5 — Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`

- [ ] **Step 2.6 — Smoke test (manual)**

Launch `./build/astra`. Start a new game (an old save file from v54 should be rejected on load — verify the rejection log line in stderr if you have a v54 save handy).

- [ ] **Step 2.7 — Commit**

```bash
git add include/astra/save_file.h include/astra/player.h include/astra/item.h \
        src/save_file.cpp src/item.cpp
git commit -m "feat(save): v54 -> v55 — Player::implants + ItemType::Implant (Task 2)"
```

---

## Task 3 — Neural Backup item definition

**Files:**
- Create: `include/astra/implant.h` (lightweight struct or just a doc comment — no payload needed if Item can carry implant info via name+effects)
- Modify: `src/item_defs.cpp` — Neural Backup def
- Modify: `src/loot_tables.cpp` (if such file exists) — drop chance entry

**Goal:** Neural Backup is a real Item with the `Implant` type, equippable into an Implant slot, with a `-1 Will` modifier when equipped.

- [ ] **Step 3.1 — Decide on Implant payload**

The other type-specific payloads (`Cyberdeck`, `Program`) live as `std::optional<Cyberdeck>` etc. on `Item`. Implants in v1 have no per-implant unique runtime data beyond the stat modifier, so **do not add a separate `ImplantData` payload**. The `Item` already carries `name`, `description`, `effects` — that's enough.

Add a tag instead. In `include/astra/item.h`, look for any `equip_slot` or `Item.slot` field. The existing pattern uses `std::optional<EquipSlot> slot` for ordinary equipment. For implants, set `Item.type = ItemType::Implant` and use a new field on `Item`:

```cpp
// inside Item struct
bool is_implant = false;   // v55: lightweight discriminator for implant slot
```

Reasoning: implant slots are not in the `EquipSlot` enum (per spec — they're a separate paper-doll). Using `is_implant` cleanly separates the two equipment domains.

Actually, simpler: implants live entirely off `Item.type == ItemType::Implant`. No extra bool needed. Use the type as the discriminator in equip code.

(Drop the bool. Use `Item.type == ItemType::Implant` everywhere implant logic checks fire.)

- [ ] **Step 3.2 — Define Neural Backup**

Edit `src/item_defs.cpp`. Find an existing item definition (e.g. a cyberdeck) and add a parallel Neural Backup entry:

```cpp
// Neural Backup — implant. -1 Will. Auto-syncs lore at Precursor consoles.
Item make_neural_backup() {
    Item it;
    it.type = ItemType::Implant;
    it.id = ItemId::NeuralBackup;     // add to item_ids.h, see below
    it.name = "Neural Backup";
    it.description =
        "A spinal-mounted memory crystal that mirrors your decrypted "
        "lore archive into the deep-Grid each time you stand at a "
        "Precursor console. Costs you a sliver of will.";
    // Stat modifier: -1 Will. Use the existing equipment-stat pipeline.
    it.modifiers.push_back({ StatId::Will, -1 });
    return it;
}
```

Add `NeuralBackup` to `include/astra/item_ids.h` after the last existing id.

- [ ] **Step 3.3 — Wire equip flow for implants**

Find the inventory `equip(...)` function (likely `Player::equip()` or similar). Add a branch:

```cpp
if (item.type == ItemType::Implant) {
    // Find an empty implant slot, equip there.
    for (size_t i = 0; i < implants.size(); ++i) {
        if (!implants[i]) {
            implants[i] = item;
            apply_implant_modifiers(item);  // -1 Will etc.
            return true;
        }
    }
    return false;  // both slots full
}
```

Add the parallel `unequip` branch.

- [ ] **Step 3.4 — Dev verb to spawn the implant**

Add to `src/dev_console.cpp`:

```cpp
if (cmd == "spawn-implant") {
    // Args: name. v1 only "neural-backup".
    if (args.size() != 1) {
        game.log("usage: :spawn-implant <name>"); return;
    }
    if (args[0] == "neural-backup") {
        game.player().inventory.add(make_neural_backup());
        game.log("Spawned Neural Backup in inventory.");
    } else {
        game.log("unknown implant name");
    }
    return;
}
```

- [ ] **Step 3.5 — Build + smoke test**

Run: `cmake --build build`
Run: `./build/astra`. In dev console (`` ` ``), type `:spawn-implant neural-backup`. Open inventory. Verify Neural Backup is present and equippable.

- [ ] **Step 3.6 — Commit**

```bash
git add include/astra/item.h include/astra/item_ids.h include/astra/player.h \
        src/item_defs.cpp src/player.cpp src/dev_console.cpp
git commit -m "feat(grid): Neural Backup implant + equip flow (Task 3)"
```

---

## Task 4 — Equipment tab paper-doll toggle

**Files:**
- Modify: `src/pda_equipment_tab.cpp` (or wherever the Equipment tab renders) — add view toggle.

**Goal:** `Tab` key on the Equipment tab swaps between Equipment paper-doll view and Implant paper-doll view (showing the 2 implant slots).

- [ ] **Step 4.1 — Locate the tab file**

Run: `grep -l "Equipment" src/pda_*.cpp src/character_*.cpp 2>/dev/null` to find the file.

- [ ] **Step 4.2 — Add view enum + state**

In the tab's class, add:

```cpp
enum class EquipmentTabView { Equipment, Implants };
EquipmentTabView equipment_tab_view_ = EquipmentTabView::Equipment;
```

- [ ] **Step 4.3 — Handle Tab key**

In the tab's input handler, add a branch:

```cpp
if (key == KEY_TAB) {
    equipment_tab_view_ = (equipment_tab_view_ == EquipmentTabView::Equipment)
                          ? EquipmentTabView::Implants
                          : EquipmentTabView::Equipment;
    return true;
}
```

(Verify the actual key name — `KEY_TAB` may be `Key::Tab` or similar in this codebase. Match the existing keybind idiom.)

- [ ] **Step 4.4 — Render Implant view**

Add a `render_implant_view()` method that draws a 2-slot paper-doll layout:

```cpp
void render_implant_view(Renderer& r, int x0, int y0) {
    r.draw_string(x0, y0,     "Implants:");
    r.draw_string(x0, y0 + 2, "Slot 1: ");
    if (player.implants[0]) {
        r.draw_string(x0 + 8, y0 + 2, player.implants[0]->name);
    } else {
        r.draw_string(x0 + 8, y0 + 2, "(empty)", Color::DarkGray);
    }
    r.draw_string(x0, y0 + 4, "Slot 2: ");
    if (player.implants[1]) {
        r.draw_string(x0 + 8, y0 + 4, player.implants[1]->name);
    } else {
        r.draw_string(x0 + 8, y0 + 4, "(empty)", Color::DarkGray);
    }
    r.draw_string(x0, y0 + 7, "[Tab] Switch to Equipment", Color::DarkGray);
}
```

In the tab's main render method:

```cpp
if (equipment_tab_view_ == EquipmentTabView::Equipment) {
    render_equipment_view(r, x0, y0);
    r.draw_string(x0, y0 + h - 1, "[Tab] Switch to Implants", Color::DarkGray);
} else {
    render_implant_view(r, x0, y0);
}
```

- [ ] **Step 4.5 — Smoke test**

Run: `./build/astra`. Open PDA → Equipment tab. Press `Tab`. Verify the view swaps to show 2 implant slots. Press `Tab` again — back to equipment.

- [ ] **Step 4.6 — Commit**

```bash
git add src/pda_equipment_tab.cpp src/pda_equipment_tab.h
git commit -m "feat(pda): Tab toggle on Equipment tab — Implant paper-doll view (Task 4)"
```

---

## Task 5 — Lore fragments + soul-mirror progress on `Hackable`

**Files:**
- Modify: `include/astra/hackable.h` — extend `Hackable` for Precursor consoles.
- Modify: `src/hackable.cpp` — populate fragments at world generation.
- Modify: `src/save_file.cpp` — serialize the new fields.

**Goal:** Each Precursor console carries 1–4 lore fragments and a per-console `soul_mirror_progress` int. The `Sync Soul` channel and the Neural Backup auto-sync both consume from this list and increment progress.

- [ ] **Step 5.1 — Extend the Hackable struct**

In `include/astra/hackable.h`:

```cpp
struct LoreFragmentSeed {
    std::string archive_id;        // synthesized at gen time
    bool committed = false;        // true once written to consciousness.dat
};

struct Hackable {
    // ... existing fields ...

    // v55 — Plan 4
    std::vector<LoreFragmentSeed> lore_fragments;   // populated for Precursor consoles only
    int soul_mirror_progress = 0;                    // resets on commit
};
```

- [ ] **Step 5.2 — Populate fragments at console-spawn time**

Find the world-generation code that creates Precursor console interactables (search: `Precursor` in `src/`). Add fragment generation:

```cpp
// when creating a Precursor console
hk.lore_fragments.clear();
int n_fragments = 1 + (rng() % 4);   // 1..4
for (int i = 0; i < n_fragments; ++i) {
    LoreFragmentSeed f;
    f.archive_id = "ARCH-" + std::to_string(console.x) + "x"
                          + std::to_string(console.y) + "-" + std::to_string(i);
    hk.lore_fragments.push_back(std::move(f));
}
```

- [ ] **Step 5.3 — Persist new fields**

In `src/save_file.cpp`, find the Hackable serializer and append:

```cpp
// v55 — Plan 4 fields
w.write_u32(static_cast<uint32_t>(hk.lore_fragments.size()));
for (const auto& f : hk.lore_fragments) {
    w.write_str(f.archive_id);
    w.write_u8(f.committed ? 1 : 0);
}
w.write_i32(hk.soul_mirror_progress);
```

Mirror in the reader.

- [ ] **Step 5.4 — Build + smoke test**

`cmake --build build`. Generate a fresh galaxy. Use a dev verb to dump a Precursor console's `lore_fragments`:

Add to `src/dev_console.cpp`:

```cpp
if (cmd == "dump-precursor") {
    // Find nearest Precursor console; dump its fragments.
    auto* hk = find_nearest_precursor_console(game.player());
    if (!hk) { game.log("no precursor console nearby"); return; }
    game.log("fragments: " + std::to_string(hk->lore_fragments.size())
             + " / progress: " + std::to_string(hk->soul_mirror_progress));
    for (const auto& f : hk->lore_fragments) {
        game.log("  - " + f.archive_id + (f.committed ? " [done]" : ""));
    }
    return;
}
```

Verify a console produces 1–4 fragments after generation.

- [ ] **Step 5.5 — Commit**

```bash
git add include/astra/hackable.h src/hackable.cpp src/save_file.cpp src/dev_console.cpp
git commit -m "feat(grid): per-console lore fragments + soul_mirror_progress (Task 5)"
```

---

## Task 6 — `SoulMirrorChannel` shared backend

**Files:**
- Create: `include/astra/soul_mirror.h`
- Create: `src/soul_mirror.cpp`
- Modify: `CMakeLists.txt`

**Goal:** A small runtime that ticks per turn while the player stands on a Precursor console tile, accumulates progress, commits a fragment to `consciousness.dat` every 10 progress, and pauses (without resetting) on movement / damage. Used by both manual `Sync Soul` and Neural Backup auto-sync.

- [ ] **Step 6.1 — Define the interface**

```cpp
// include/astra/soul_mirror.h
#pragma once

#include "astra/consciousness_save.h"

#include <cstdint>
#include <optional>

namespace astra {

class Game;
struct Hackable;

struct SoulMirrorChannelState {
    bool active = false;
    bool passive = false;        // true = Neural Backup auto-sync (no EP cost)
    Hackable* console = nullptr; // raw ptr — channel lifetime is bounded by the turn
};

namespace soul_mirror {

// Constants — easy retune.
inline constexpr int kProgressPerTurn   = 1;
inline constexpr int kEpCostPerTurn     = 2;     // active channel only
inline constexpr int kCommitThreshold   = 10;    // progress per fragment commit
inline constexpr int kDamageDetectionBurst = 5;

void begin_active(Game& game, Hackable& console);
void begin_passive(Game& game, Hackable& console);

// Called once per game turn. Handles progress, commit, EP debit, pauses.
void tick(Game& game);

// Called when the player takes damage during the current turn.
void on_player_damaged(Game& game);

// Returns true if a channel is actively progressing this turn.
bool is_active(const Game& game);

// Render a one-line strip above the message log, if active.
void render_hud_strip(Game& game, class Renderer& r);

} // namespace soul_mirror
} // namespace astra
```

- [ ] **Step 6.2 — Implement**

```cpp
// src/soul_mirror.cpp
#include "astra/soul_mirror.h"
#include "astra/consciousness_save.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/player.h"
#include "astra/renderer.h"

#include <cstdio>

namespace astra::soul_mirror {

namespace {

bool player_on_console_tile(Game& game, Hackable& console) {
    return game.player().x == console.x && game.player().y == console.y;
}

void commit_fragment(Game& game, Hackable& console) {
    // Find the first uncommitted fragment.
    for (auto& f : console.lore_fragments) {
        if (!f.committed) {
            f.committed = true;
            ConsciousnessSave cs;
            read_consciousness(cs);   // may return false; we still write fresh.
            LoreFragmentRef ref;
            ref.archive_id          = f.archive_id;
            ref.galaxy_seed_origin  = game.world().seed();
            ref.world_tick_origin   = game.world().tick();
            cs.lore_archive.push_back(std::move(ref));
            // Assign consciousness_id on first qualification.
            if (cs.consciousness_id == 0) {
                std::random_device rd;
                cs.consciousness_id =
                    (static_cast<uint64_t>(rd()) << 32) | rd();
            }
            write_consciousness(cs);
            game.log("Lore fragment committed: " + f.archive_id);
            return;
        }
    }
    game.log("Console is fully synced.");
}

} // namespace

void begin_active(Game& game, Hackable& console) {
    auto& s = game.soul_mirror_state();
    s.active  = true;
    s.passive = false;
    s.console = &console;
    game.log("Sync Soul: channel begun.");
}

void begin_passive(Game& game, Hackable& console) {
    auto& s = game.soul_mirror_state();
    s.active  = true;
    s.passive = true;
    s.console = &console;
    // No log message — passive is invisible.
}

void tick(Game& game) {
    auto& s = game.soul_mirror_state();
    if (!s.active || !s.console) return;
    if (!player_on_console_tile(game, *s.console)) {
        s.active = false;   // pause; do not reset progress
        return;
    }
    s.console->soul_mirror_progress += kProgressPerTurn;
    if (!s.passive) {
        game.player().energy = std::max(0, game.player().energy - kEpCostPerTurn);
    }
    if (s.console->soul_mirror_progress >= kCommitThreshold) {
        s.console->soul_mirror_progress = 0;
        commit_fragment(game, *s.console);
    }
}

void on_player_damaged(Game& game) {
    auto& s = game.soul_mirror_state();
    if (!s.active || !s.console) return;
    s.active = false;   // pause; no reset
    game.detection_add(kDamageDetectionBurst);
}

bool is_active(const Game& game) {
    return game.soul_mirror_state().active;
}

void render_hud_strip(Game& game, Renderer& r) {
    const auto& s = game.soul_mirror_state();
    if (!s.active || !s.console) return;
    char buf[80];
    int prog = s.console->soul_mirror_progress;
    std::snprintf(buf, sizeof(buf),
                  "SYNC %d/%d  EP %d/%d",
                  prog, kCommitThreshold,
                  game.player().energy, game.player().energy_max);
    r.draw_string(1, /*hud_y*/ 22, buf);
}

} // namespace astra::soul_mirror
```

- [ ] **Step 6.3 — Add state to Game**

In `include/astra/game.h`:

```cpp
SoulMirrorChannelState& soul_mirror_state();
const SoulMirrorChannelState& soul_mirror_state() const;
```

Implement as a member field.

- [ ] **Step 6.4 — Hook tick + damage**

In the main update loop (probably `Game::tick()`), after movement resolution, add:

```cpp
soul_mirror::tick(*this);
```

In whatever function applies damage to the player, call:

```cpp
soul_mirror::on_player_damaged(game);
```

In the renderer, after the play HUD draws, call:

```cpp
soul_mirror::render_hud_strip(game, r);
```

- [ ] **Step 6.5 — Build + commit**

```bash
cmake --build build
git add include/astra/soul_mirror.h src/soul_mirror.cpp \
        include/astra/game.h src/game.cpp CMakeLists.txt
git commit -m "feat(grid): SoulMirrorChannel runtime + HUD strip (Task 6)"
```

---

## Task 7 — Wire Soul Mirror to the Precursor console interact menu

**Files:**
- Modify: `src/hackable.cpp` (or wherever interact menus are built for hackables)
- Modify: `src/dev_console.cpp` — add `:sync-soul`

**Goal:** Pressing interact on a Precursor console adds a `Sync Soul` entry that calls `begin_active`. With Neural Backup equipped, walking onto the tile triggers `begin_passive` automatically.

- [ ] **Step 7.1 — Add the menu entry**

Find the interact-menu builder for Precursor consoles. Add:

```cpp
if (hackable.kind == DeviceKind::PrecursorConsole) {
    menu.push_back({"Sync Soul", [&](Game& g) {
        soul_mirror::begin_active(g, hackable);
    }});
}
```

- [ ] **Step 7.2 — Auto-trigger for Neural Backup**

In the player's "stepped onto tile" hook (probably `Player::on_step()` or in the movement handler), add:

```cpp
auto* console = hackable_at(game.player().x, game.player().y);
bool has_neural_backup = false;
for (const auto& s : game.player().implants) {
    if (s && s->id == ItemId::NeuralBackup) { has_neural_backup = true; break; }
}
if (console && console->kind == DeviceKind::PrecursorConsole && has_neural_backup) {
    soul_mirror::begin_passive(game, *console);
}
```

- [ ] **Step 7.3 — Dev verb**

```cpp
if (cmd == "sync-soul") {
    auto* console = find_nearest_precursor_console(game.player());
    if (!console) { game.log("no console nearby"); return; }
    soul_mirror::begin_active(game, *console);
    game.log("Forced Sync Soul start.");
    return;
}
```

- [ ] **Step 7.4 — Smoke test**

Run game with `:warp-to` near a Precursor console, equip Neural Backup, walk onto the tile — verify console fragments commit one per 10 turns. Then unequip, manually `Sync Soul`, verify EP drains by 2/turn.

- [ ] **Step 7.5 — Commit**

```bash
git add src/hackable.cpp src/player.cpp src/dev_console.cpp
git commit -m "feat(grid): Sync Soul menu + Neural Backup passive sync (Task 7)"
```

---

## Task 8 — `CodeCraft` capstone + 2 T3 program recipes

**Files:**
- Modify: `include/astra/program.h` — add `PulseHammer = 200`, `DaemonHijack = 201`
- Modify: `src/program.cpp` — add registry entries
- Modify: `src/tinkering.cpp` — recipe gating
- Modify: `src/skill_defs.cpp` — wire `CodeCraft` runtime effect (recipe visibility)
- Modify: `src/program_effects.cpp` (or wherever `apply_program` lives) — implement effects

**Goal:** With `CodeCraft` unlocked, the Tinkering tab shows 2 new T3 program recipes. Both programs work in the Grid.

- [ ] **Step 8.1 — Extend ProgramId**

`include/astra/program.h`:

```cpp
enum class ProgramId : uint16_t {
    IcebreakerLite = 1,
    GhostTrace     = 2,
    Cooldown       = 3,
    Breach         = 4,
    Decrypt        = 5,
    PulseHammer    = 200,    // T3, ATK, AoE
    DaemonHijack   = 201,    // T3, UTL, take control of an ICE
    RebootOptics   = 100,
    FriendlyFire   = 101,
    DataLeech      = 102,
};
```

- [ ] **Step 8.2 — Registry entries**

`src/program.cpp`, in the `program_registry()` initializer:

```cpp
{ ProgramId::PulseHammer,  K::Atk, 3, 4, 5, "Pulse Hammer",  "pulse_hammer.exe",
  "AoE 1d6 dmg to all ICE adjacent to target tile.", 0, {} },
{ ProgramId::DaemonHijack, K::Utl, 3, 5, 4, "Daemon Hijack", "daemon_hijack.exe",
  "Take control of one ICE for 3 turns.", 0, {} },
```

- [ ] **Step 8.3 — Effects**

In `src/program_effects.cpp` (or wherever `apply_program` switches on id), add:

```cpp
case ProgramId::PulseHammer: {
    // 1d6 dmg to ICE adjacent to target tile.
    auto target = pick_target_tile(game);
    if (!target) return false;
    for (auto& ice : session.ice) {
        int dx = std::abs(ice.x - target->x);
        int dy = std::abs(ice.y - target->y);
        if (dx <= 1 && dy <= 1 && (dx + dy) > 0) {
            ice.hp -= 1 + (rng() % 6);
        }
    }
    return true;
}

case ProgramId::DaemonHijack: {
    // Take control of one ICE for 3 turns.
    auto* victim = pick_target_ice(game);
    if (!victim) return false;
    victim->charmed_turns_left = 3;
    return true;
}
```

(`charmed_turns_left` exists on `GridIce` — if not, add it: `int charmed_turns_left = 0`. ICE skip enemy AI when charmed > 0; tick down per turn.)

- [ ] **Step 8.4 — Tinker recipes**

`src/tinkering.cpp`. Find the program-recipe section. Add:

```cpp
add_recipe({
    .name         = "pulse_hammer.exe",
    .output       = make_program(ProgramId::PulseHammer),
    .inputs       = {{ MaterialId::CodeFragmentT3, 2 },
                     { ItemId::ProgramDisk, 1 }},
    .skill_gate   = SkillId::CodeCraft,
});
add_recipe({
    .name         = "daemon_hijack.exe",
    .output       = make_program(ProgramId::DaemonHijack),
    .inputs       = {{ MaterialId::CodeFragmentT3, 3 },
                     { ItemId::ProgramDisk, 1 }},
    .skill_gate   = SkillId::CodeCraft,
});
```

(Field names are illustrative — match the existing recipe struct in this file.)

- [ ] **Step 8.5 — Skill description update**

`src/skill_defs.cpp`, the `CodeCraft` description: replace any "(Plan 4.)" placeholder with the actual user-facing text:

```cpp
"Unlocks T3 program tinker recipes: pulse_hammer.exe (AoE ICE damage) "
"and daemon_hijack.exe (take control of an ICE for 3 turns)."
```

- [ ] **Step 8.6 — Smoke test**

Dev: `:level-up`, allocate `CodeCraft`. Open Tinkering — verify both recipes appear. Craft one. Load into deck. Jack into a sector with ICE; fire — verify effect.

- [ ] **Step 8.7 — Commit**

```bash
git add include/astra/program.h src/program.cpp src/program_effects.cpp \
        src/tinkering.cpp src/skill_defs.cpp include/astra/grid_ice.h
git commit -m "feat(grid): CodeCraft + T3 programs pulse_hammer/daemon_hijack (Task 8)"
```

---

## Task 9 — `ConsciousnessAnchor` capstone + player deep-Grid base

**Files:**
- Modify: `src/grid_anchor_layout.cpp` — add `make_player_deep_grid_base()`
- Modify: `include/astra/grid_sector.h` — declare it
- Modify: `src/skill_defs.cpp` — wire capstone effect
- Modify: `src/consciousness_save.cpp` — finish the GridSector + Item-rack body deferred from Task 1
- Modify: `include/astra/grid_network.h` — track `owned_by_consciousness_id` on deep-Grid anchor nodes

**Goal:** Unlocking `ConsciousnessAnchor` builds the player's personal deep-Grid base sector, registers it in `GridNetwork`, and persists it in `consciousness.dat`.

- [ ] **Step 9.1 — Hand-author the layout**

`src/grid_anchor_layout.cpp`:

```cpp
GridSector make_player_deep_grid_base() {
    GridSector s;
    s.w = 30;
    s.h = 20;
    s.tiles.assign(s.w * s.h, GridTile::Floor);

    // Outer firewall border.
    auto set = [&](int x, int y, GridTile t) {
        if (x >= 0 && x < s.w && y >= 0 && y < s.h)
            s.tiles[y * s.w + x] = t;
    };
    for (int x = 0; x < s.w; ++x) {
        set(x, 0,        GridTile::Firewall);
        set(x, s.h - 1,  GridTile::Firewall);
    }
    for (int y = 0; y < s.h; ++y) {
        set(0,       y, GridTile::Firewall);
        set(s.w - 1, y, GridTile::Firewall);
    }

    // Northern utility room divider.
    for (int x = 1; x < s.w - 1; ++x) set(x, 5, GridTile::Firewall);
    set(8,  5, GridTile::Floor);   // doorway

    // Fixtures in the north room.
    set(4,  3, GridTile::DataNode);       // ⊞ stash terminal placeholder
    set(15, 3, GridTile::DataNode);       // ▤ signature program rack placeholder
    set(25, 3, GridTile::EncryptedFile);  // ⊘ lore vault interface

    // AI contact spots in the south.
    set(5,  12, GridTile::DataNode);
    set(20, 14, GridTile::DataNode);

    // Exit node (back to regional darknet).
    set(15, 18, GridTile::ExitNode);

    s.entry_x = 15;
    s.entry_y = 17;
    return s;
}
```

(The `GridTile::DataNode` is reused as a stand-in for stash/rack/AI fixtures because the spec says distinct glyphs for those are out of v1 scope. Comments tag intent.)

- [ ] **Step 9.2 — Declare it**

`include/astra/grid_sector.h`:

```cpp
GridSector make_consciousness_anchor_sector();
GridSector make_player_deep_grid_base();   // Plan 4
```

- [ ] **Step 9.3 — Wire the capstone**

In `src/skill_defs.cpp` (or the skill-effect dispatcher), find where skill unlocks fire side effects. Add a hook for `ConsciousnessAnchor`:

```cpp
case SkillId::ConsciousnessAnchor: {
    // 1) Generate or retrieve player base.
    auto base = make_player_deep_grid_base();

    // 2) Register an anchor node in GridNetwork.
    GridNetworkNode n;
    n.kind  = GridNodeKind::DeepGridAnchor;
    n.label = "Your.Anchor";
    n.owned_by_consciousness_id = current_consciousness_id();
    n.layout_x = 5;   // deep-Grid zoom default position
    n.layout_y = 5;
    game.world().grid_network.add_node(std::move(n));

    // 3) Persist into consciousness.dat.
    ConsciousnessSave cs;
    read_consciousness(cs);
    cs.deep_grid_base = std::move(base);
    if (cs.consciousness_id == 0) {
        std::random_device rd;
        cs.consciousness_id = (static_cast<uint64_t>(rd()) << 32) | rd();
    }
    write_consciousness(cs);
    break;
}
```

- [ ] **Step 9.4 — Add `owned_by_consciousness_id` field**

`include/astra/grid_network.h`:

```cpp
struct GridNetworkNode {
    // ... existing fields ...
    uint64_t owned_by_consciousness_id = 0;   // 0 = unowned. Plan 4.
    int layout_x = 0;                          // for graph view rendering. Plan 4.
    int layout_y = 0;
};
```

Persist these two fields in `src/save_file.cpp`'s GridNetwork serializer.

- [ ] **Step 9.5 — Finish consciousness.dat body**

In `src/consciousness_save.cpp`'s `write_consciousness`, finish the `deep_grid_base` and `signature_program_rack` body. Use the existing `GridSector` and `Item` serializers (you may need to expose `serialize_grid_sector()` and `serialize_item()` from `src/save_file.cpp` via small helper headers, or duplicate the inline serialization here — pick the smaller change).

Mirror the read side in `read_consciousness`.

- [ ] **Step 9.6 — Dev verb `:unlock-anchor`**

```cpp
if (cmd == "unlock-anchor") {
    game.player().unlock_skill(SkillId::ConsciousnessAnchor);
    game.log("ConsciousnessAnchor unlocked. Base seeded.");
    return;
}
```

- [ ] **Step 9.7 — Smoke test**

Run game; `:unlock-anchor`. Save and quit. Reload — verify `consciousness.dat` exists in `~/.astra/saves/`. Open PDA `netmap` — eventually (after Task 12) the deep-Grid zoom shows your anchor.

- [ ] **Step 9.8 — Commit**

```bash
git add src/grid_anchor_layout.cpp include/astra/grid_sector.h \
        include/astra/grid_network.h src/skill_defs.cpp \
        src/consciousness_save.cpp src/save_file.cpp src/dev_console.cpp
git commit -m "feat(grid): ConsciousnessAnchor capstone + player deep-Grid base (Task 9)"
```

---

## Task 10 — Grid renderer camera

**Files:**
- Create: `include/astra/grid_camera.h`
- Create: `src/grid_camera.cpp`
- Modify: `src/grid_renderer.cpp` — replace fixed origin with camera

**Goal:** `grid_renderer` follows the avatar with a 4-cell deadzone instead of drawing at origin (1, 1). Sectors smaller than viewport render unchanged.

- [ ] **Step 10.1 — Camera struct**

```cpp
// include/astra/grid_camera.h
#pragma once

namespace astra {

struct GridCamera {
    int viewport_w = 60;
    int viewport_h = 22;
    int cam_x = 0;
    int cam_y = 0;
    int deadzone_margin = 4;

    void follow(int avatar_x, int avatar_y, int sector_w, int sector_h);
};

} // namespace astra
```

- [ ] **Step 10.2 — Implement `follow()`**

```cpp
// src/grid_camera.cpp
#include "astra/grid_camera.h"

#include <algorithm>

namespace astra {

void GridCamera::follow(int avatar_x, int avatar_y, int sector_w, int sector_h) {
    // If sector smaller than viewport, never scroll.
    if (sector_w <= viewport_w && sector_h <= viewport_h) {
        cam_x = 0;
        cam_y = 0;
        return;
    }

    // Horizontal deadzone.
    int rel_x = avatar_x - cam_x;
    if (rel_x < deadzone_margin) {
        cam_x = std::max(0, avatar_x - deadzone_margin);
    } else if (rel_x > viewport_w - deadzone_margin) {
        cam_x = std::min(sector_w - viewport_w,
                         avatar_x - (viewport_w - deadzone_margin));
    }
    cam_x = std::clamp(cam_x, 0, std::max(0, sector_w - viewport_w));

    // Vertical deadzone.
    int rel_y = avatar_y - cam_y;
    if (rel_y < deadzone_margin) {
        cam_y = std::max(0, avatar_y - deadzone_margin);
    } else if (rel_y > viewport_h - deadzone_margin) {
        cam_y = std::min(sector_h - viewport_h,
                         avatar_y - (viewport_h - deadzone_margin));
    }
    cam_y = std::clamp(cam_y, 0, std::max(0, sector_h - viewport_h));
}

} // namespace astra
```

- [ ] **Step 10.3 — Wire into grid_renderer.cpp**

Replace the fixed `origin_x = 1; origin_y = 1;` with:

```cpp
static GridCamera s_camera;
s_camera.follow(s.avatar_x, s.avatar_y, s.sector.w, s.sector.h);

const int origin_x = 1;
const int origin_y = 1;

// Tiles
for (int y = 0; y < s_camera.viewport_h; ++y) {
    for (int x = 0; x < s_camera.viewport_w; ++x) {
        int tx = x + s_camera.cam_x;
        int ty = y + s_camera.cam_y;
        if (tx < 0 || ty < 0 || tx >= s.sector.w || ty >= s.sector.h) continue;
        GridTile t = s.sector.at(tx, ty);
        r.draw_glyph(origin_x + x, origin_y + y, glyph_for(t), color_for(t));
    }
}

// ICE
for (const auto& ice : s.ice) {
    int sx = ice.x - s_camera.cam_x;
    int sy = ice.y - s_camera.cam_y;
    if (sx < 0 || sy < 0 || sx >= s_camera.viewport_w || sy >= s_camera.viewport_h) continue;
    // ... draw at origin_x + sx, origin_y + sy
}

// Avatar
r.draw_glyph(origin_x + (s.avatar_x - s_camera.cam_x),
             origin_y + (s.avatar_y - s_camera.cam_y),
             grid_theme::avatar_glyph, grid_theme::avatar);
```

The static camera is fine for v1 — only one Grid session is active at a time.

- [ ] **Step 10.4 — Update HUD position**

The current code uses `hud_x = origin_x + s.sector.w + 2`. Change to:

```cpp
const int hud_x = origin_x + s_camera.viewport_w + 2;
```

- [ ] **Step 10.5 — Smoke test**

`:spawn-hackable console` then jack in to the small subnet — verify rendering looks identical. Then for Task 11 verification (or use `:warp` to a hand-built large sector) verify camera scrolls when avatar approaches the edge.

- [ ] **Step 10.6 — Commit**

```bash
git add include/astra/grid_camera.h src/grid_camera.cpp src/grid_renderer.cpp \
        CMakeLists.txt
git commit -m "feat(grid): GridCamera follow-avatar with deadzone (Task 10)"
```

---

## Task 11 — Regional darknet BSP generator

**Files:**
- Create: `include/astra/grid_regional_generator.h`
- Create: `src/grid_regional_generator.cpp`
- Modify: `src/grid_session.cpp` (or wherever regional darknet sectors are generated) — call the new generator

**Goal:** A regional darknet sector has 4–8 rooms (BSP-shaped) with the same firewall glyphs and palette as the existing single-room regional. Style consistent — does not feel like a dungeon.

- [ ] **Step 11.1 — Define interface**

```cpp
// include/astra/grid_regional_generator.h
#pragma once

#include "astra/grid_sector.h"
#include <cstdint>

namespace astra::grid_regional_generator {

GridSector generate(uint32_t seed, int min_rooms = 4, int max_rooms = 8);

} // namespace astra::grid_regional_generator
```

- [ ] **Step 11.2 — Implement using existing BSP**

Look at `include/astra/generators/bsp_generator.h` to find the BSP API. Wrap it:

```cpp
// src/grid_regional_generator.cpp
#include "astra/grid_regional_generator.h"
#include "astra/generators/bsp_generator.h"

#include <random>

namespace astra::grid_regional_generator {

GridSector generate(uint32_t seed, int min_rooms, int max_rooms) {
    std::mt19937 rng(seed);

    BspGenerator bsp;
    bsp.set_seed(seed);
    bsp.set_min_rooms(min_rooms);
    bsp.set_max_rooms(max_rooms);
    auto bsp_map = bsp.generate(/*w=*/40, /*h=*/24);

    GridSector s;
    s.w = bsp_map.w;
    s.h = bsp_map.h;
    s.tiles.assign(s.w * s.h, GridTile::Floor);

    // Map BSP wall/floor → Grid firewall/floor.
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.w; ++x) {
            auto t = bsp_map.at(x, y);
            if (t == BspTile::Wall)        s.tiles[y * s.w + x] = GridTile::Firewall;
            else if (t == BspTile::Floor)  s.tiles[y * s.w + x] = GridTile::Floor;
            else if (t == BspTile::Door)   s.tiles[y * s.w + x] = GridTile::Gateway;
        }
    }

    // Decorate: 1–4 EncryptedFile, 0–2 DataNode, 0–1 Gateway.
    auto place_random_floor = [&](GridTile t) {
        for (int tries = 0; tries < 50; ++tries) {
            int x = rng() % s.w;
            int y = rng() % s.h;
            if (s.tiles[y * s.w + x] == GridTile::Floor) {
                s.tiles[y * s.w + x] = t;
                return;
            }
        }
    };
    int n_enc = 1 + (rng() % 4);
    for (int i = 0; i < n_enc; ++i) place_random_floor(GridTile::EncryptedFile);
    int n_data = rng() % 3;
    for (int i = 0; i < n_data; ++i) place_random_floor(GridTile::DataNode);
    if (rng() % 2 == 0) place_random_floor(GridTile::Gateway);   // deep-Grid

    // Ensure at least one ExitNode for jack-out.
    place_random_floor(GridTile::ExitNode);

    // entry_x/y = first floor tile.
    for (int y = 0; y < s.h && (s.entry_x == 0 && s.entry_y == 0); ++y) {
        for (int x = 0; x < s.w; ++x) {
            if (s.tiles[y * s.w + x] == GridTile::Floor) {
                s.entry_x = x; s.entry_y = y; break;
            }
        }
    }

    return s;
}

} // namespace astra::grid_regional_generator
```

(The exact BSP API may differ — `BspGenerator` / `BspTile` / `bsp_map.at()` are placeholders. Match the actual class names in `include/astra/generators/bsp_generator.h`.)

- [ ] **Step 11.3 — Use it for regional sectors**

Find where the single-room regional currently generates (likely in `src/grid_session.cpp` or `src/grid_anchor_layout.cpp`). Replace with:

```cpp
case GridNodeKind::RegionalDarknet:
    sector = grid_regional_generator::generate(network_seed_for(node));
    break;
```

- [ ] **Step 11.4 — Style verification**

Run the game; `:jack` into a regional darknet via the netmap. Visually verify:
- Rooms are firewall-bordered (▓ glyph, same palette as existing regional).
- 4–8 rooms.
- Floor tile (░) inside.
- 1–4 encrypted files, 0–2 data nodes, 0–1 deep-Grid gateway, 1 exit node.

- [ ] **Step 11.5 — Commit**

```bash
git add include/astra/grid_regional_generator.h src/grid_regional_generator.cpp \
        src/grid_session.cpp CMakeLists.txt
git commit -m "feat(grid): regional darknet BSP generator, 4-8 rooms (Task 11)"
```

---

## Task 12 — `GridNetmapWidget` overlay

**Files:**
- Create: `include/astra/grid_netmap_widget.h`
- Create: `src/grid_netmap_widget.cpp`
- Modify: `src/pda_hacking_tab.cpp` — `hack_term_cmd_netmap` opens overlay; new keypress handler

**Goal:** Pressing `N` (or typing `netmap`) opens an overlay popup over the Hacking tab content area. Two zoom layers (regional + deep-Grid). Cursor steps between labeled nodes. `Enter` jacks in. `b` breaches. `Esc` closes.

- [ ] **Step 12.1 — Widget interface**

```cpp
// include/astra/grid_netmap_widget.h
#pragma once

#include <cstdint>

namespace astra {

class Game;
class Renderer;

enum class NetmapZoom : uint8_t { Regional, DeepGrid };

struct GridNetmapWidget {
    bool        open = false;
    NetmapZoom  zoom = NetmapZoom::Regional;
    int         cursor_node_idx = 0;

    void open_widget();
    void close_widget();

    // Returns true if the key was consumed.
    bool handle_key(Game& game, int key);

    void render(Game& game, Renderer& r,
                int x, int y, int w, int h);  // position within Hacking tab pane
};

} // namespace astra
```

- [ ] **Step 12.2 — Render**

Implementation outline (~250 lines):

```cpp
// src/grid_netmap_widget.cpp
#include "astra/grid_netmap_widget.h"
#include "astra/game.h"
#include "astra/grid_network.h"
#include "astra/renderer.h"

#include <algorithm>
#include <vector>

namespace astra {

namespace {

struct NodeView {
    int idx;
    int x, y;
    std::string label;
    bool selectable;
    bool locked;
};

std::vector<NodeView> visible_nodes(const Game& game, NetmapZoom zoom) {
    std::vector<NodeView> out;
    const auto& net = game.world().grid_network;
    for (size_t i = 0; i < net.nodes.size(); ++i) {
        const auto& n = net.nodes[i];
        bool match = (zoom == NetmapZoom::Regional)
                     ? (n.kind == GridNodeKind::RegionalDarknet ||
                        n.kind == GridNodeKind::Subnet)
                     : (n.kind == GridNodeKind::DeepGridAnchor ||
                        n.kind == GridNodeKind::DeepGridBase ||
                        n.kind == GridNodeKind::AiContact ||
                        n.kind == GridNodeKind::LoreVault);
        if (!match) continue;
        out.push_back({(int)i, n.layout_x, n.layout_y, n.label,
                       /*selectable*/ true, n.locked});
    }
    return out;
}

} // namespace

void GridNetmapWidget::open_widget() { open = true; cursor_node_idx = 0; }
void GridNetmapWidget::close_widget() { open = false; }

void GridNetmapWidget::render(Game& game, Renderer& r,
                              int x, int y, int w, int h) {
    if (!open) return;
    // Bordered modal panel, centered on the parent pane.
    int pad = 2;
    int px = x + pad;
    int py = y + pad;
    int pw = w - 2 * pad;
    int ph = h - 2 * pad;
    r.draw_box(px, py, pw, ph);
    const char* title = (zoom == NetmapZoom::Regional)
                        ? " NETMAP — REGIONAL "
                        : " NETMAP — DEEP-GRID ";
    r.draw_string(px + 2, py, title);

    auto nodes = visible_nodes(game, zoom);

    // Edges first (so node labels overprint).
    const auto& net = game.world().grid_network;
    for (const auto& e : net.edges) {
        // ... draw line/cross between nodes' layout positions.
    }

    // Nodes.
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        Color c = (n.locked) ? Color::DarkGray
                : ((int)i == cursor_node_idx) ? Color::BrightCyan
                : Color::Cyan;
        r.draw_string(px + 2 + n.x, py + 2 + n.y, "[" + n.label + "]", c);
    }

    // Footer hints.
    int fy = py + ph - 4;
    r.draw_string(px + 2, fy + 0,
        (zoom == NetmapZoom::Regional)
            ? "[arrows] cursor   [enter] jack   [b] breach"
            : "[arrows] cursor   [enter] enter sector",
        Color::DarkGray);
    r.draw_string(px + 2, fy + 1,
        (zoom == NetmapZoom::Regional)
            ? "[,] zoom to deep-Grid   [.] subnet zoom (single-device — coming later)"
            : "[,] zoom to regional",
        Color::DarkGray);
    r.draw_string(px + 2, fy + 2, "[esc] close", Color::DarkGray);
}

bool GridNetmapWidget::handle_key(Game& game, int key) {
    if (!open) return false;
    auto nodes = visible_nodes(game, zoom);
    if (nodes.empty()) {
        if (key == KEY_ESCAPE) { close_widget(); return true; }
        return true;
    }
    auto step_cursor = [&](int dx, int dy) {
        const auto& cur = nodes[cursor_node_idx];
        int best = cursor_node_idx;
        int best_dist = 1<<30;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if ((int)i == cursor_node_idx) continue;
            const auto& n = nodes[i];
            int ddx = n.x - cur.x;
            int ddy = n.y - cur.y;
            // Direction filter: step must be in (dx, dy) hemisphere.
            if (dx > 0 && ddx <= 0) continue;
            if (dx < 0 && ddx >= 0) continue;
            if (dy > 0 && ddy <= 0) continue;
            if (dy < 0 && ddy >= 0) continue;
            int dist = ddx*ddx + ddy*ddy;
            if (dist < best_dist) { best_dist = dist; best = i; }
        }
        cursor_node_idx = best;
    };
    switch (key) {
        case KEY_LEFT:  step_cursor(-1, 0); return true;
        case KEY_RIGHT: step_cursor(+1, 0); return true;
        case KEY_UP:    step_cursor(0, -1); return true;
        case KEY_DOWN:  step_cursor(0, +1); return true;
        case ',':
            zoom = (zoom == NetmapZoom::Regional) ? NetmapZoom::DeepGrid
                                                  : NetmapZoom::Regional;
            cursor_node_idx = 0; return true;
        case '.':
            // Subnet zoom no-op (footer hint shown).
            return true;
        case KEY_ENTER: {
            // Jack into selected node.
            auto& n = game.world().grid_network.nodes[nodes[cursor_node_idx].idx];
            if (n.locked) { game.log("Locked. Try [b] to breach."); return true; }
            game.hacking().jack_in(game, n.id);
            close_widget();
            return true;
        }
        case 'b': {
            // Attempt breach.
            // ... call existing breach logic.
            return true;
        }
        case KEY_ESCAPE: close_widget(); return true;
    }
    return false;
}

} // namespace astra
```

(The renderer's `draw_box` / `KEY_*` constants are placeholders — match the actual project conventions.)

- [ ] **Step 12.3 — Wire to PDA Hacking tab**

In `src/pda_hacking_tab.cpp`:

```cpp
GridNetmapWidget netmap_widget_;

void hack_term_cmd_netmap(const std::vector<std::string>& args) {
    netmap_widget_.open_widget();
    // Do NOT emit ASCII to scrollback anymore.
}

bool handle_input(int key) {
    if (netmap_widget_.handle_key(game(), key)) return true;
    // ... existing terminal input
}

void render(Renderer& r, int x, int y, int w, int h) {
    // ... existing terminal render
    netmap_widget_.render(game(), r, x, y, w, h);
}
```

Also bind `N` (when no terminal input is in flight) to `netmap_widget_.open_widget()`.

- [ ] **Step 12.4 — Smoke test**

Run game; open PDA Hacking tab; type `netmap` (or press `N`). Verify the overlay appears with regional nodes. Arrow keys move cursor. `,` swaps to deep-Grid. `Esc` closes. With `ConsciousnessAnchor` unlocked (`:unlock-anchor`), deep-Grid zoom shows `Your.Anchor`.

- [ ] **Step 12.5 — Commit**

```bash
git add include/astra/grid_netmap_widget.h src/grid_netmap_widget.cpp \
        src/pda_hacking_tab.cpp CMakeLists.txt
git commit -m "feat(grid): GridNetmapWidget overlay (regional + deep-Grid zoom) (Task 12)"
```

---

## Task 13 — Sgr A\* rebirth flow

**Files:**
- Create: `include/astra/rebirth_sequence.h`
- Create: `src/rebirth_sequence.cpp`
- Modify: the navigation layer (location TBD — search for "warp_to_system" or "jump_to" handler)
- Modify: `src/dev_console.cpp` — `:rebirth` verb

**Goal:** Crossing the Sgr A\* event horizon shows a confirmation modal listing what survives, plays a 6-second cinematic on first crossing, deletes the galaxy save, applies `consciousness.dat` to a fresh galaxy.

- [ ] **Step 13.1 — Locate the jump entry point**

Run: `grep -rn "warp_to_system\|jump_to\|enter_system" src/ include/ --include='*.cpp' --include='*.h' | head -20`. Identify the function called when the player chooses to jump to a target system. The Sgr A\* hook will branch off the `target.id == 0` (Sgr A\* system id) case. Document the file and line in this checkbox before proceeding to 13.2.

- [ ] **Step 13.2 — Define `RebirthSequence`**

```cpp
// include/astra/rebirth_sequence.h
#pragma once

namespace astra {

class Game;

namespace rebirth_sequence {

// Returns true if the rebirth was confirmed and started.
// Returns false if user cancelled the modal.
bool begin(Game& game);

// Renders the cinematic / modal overlay if active. No-op otherwise.
void render(Game& game, class Renderer& r);

// Handles a key while the modal/cinematic is active.
// Returns true if consumed.
bool handle_key(Game& game, int key);

bool is_active(const Game& game);

} // namespace rebirth_sequence
} // namespace astra
```

- [ ] **Step 13.3 — Implement state machine**

```cpp
// src/rebirth_sequence.cpp
#include "astra/rebirth_sequence.h"
#include "astra/consciousness_save.h"
#include "astra/game.h"
#include "astra/save_file.h"
#include "astra/save_system.h"
#include "astra/renderer.h"

#include <chrono>
#include <thread>

namespace astra::rebirth_sequence {

namespace {

enum class Phase { Inactive, Confirm, Cinematic, Apply };

struct State {
    Phase phase = Phase::Inactive;
    int   cinematic_line_idx = 0;
    int   cinematic_tick = 0;        // ticks since cinematic start
};

State& state(Game& game) { return game.rebirth_state(); }

void render_confirm(Game& game, Renderer& r) {
    // Box, title, list of surviving items, [Enter]/[Esc] hints.
    // Build the list dynamically from consciousness.dat + skill state.
    // ... (omitted — straightforward layout code)
}

const char* k_cinematic_lines[] = {
    "The body unmakes itself at the event horizon.",
    "Spacetime curves into a single bright point.",
    "...consciousness uploads...",
    "...the galaxy collapses behind you...",
    "...knowledge persists through the singularity...",
    "                  [Press any key]",
};

void render_cinematic(Game& game, Renderer& r) {
    auto& s = state(game);
    int reveal_count = std::min(s.cinematic_line_idx + 1,
                                (int)(sizeof(k_cinematic_lines) / sizeof(*k_cinematic_lines)));
    int cy = 8;
    for (int i = 0; i < reveal_count; ++i) {
        r.draw_string(20, cy + i * 2, k_cinematic_lines[i],
                      (i == reveal_count - 1) ? Color::BrightCyan : Color::Cyan);
    }
}

void apply(Game& game) {
    // 1. Pre-rebirth write of consciousness.dat (latest in-memory state).
    ConsciousnessSave cs = game.consciousness();
    cs.rebirth_count++;
    cs.seen_first_rebirth = true;
    write_consciousness(cs);

    // 2. Delete current galaxy save.
    std::string fname = "save_" + std::to_string(game.world().seed());
    delete_save(fname);

    // 3. Re-seed a new galaxy.
    uint32_t new_seed = static_cast<uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    game.start_new_galaxy(new_seed);

    // 4. Re-apply consciousness on top of the new player.
    game.consciousness() = cs;
    if (cs.deep_grid_base) {
        // Stitch the base into the new galaxy's GridNetwork.
        game.world().grid_network.stitch_player_anchor(*cs.deep_grid_base, cs.consciousness_id);
    }

    game.log("You wake. The galaxy is new. Memory persists.");
}

} // namespace

bool begin(Game& game) {
    auto& s = state(game);
    s.phase = Phase::Confirm;
    return true;
}

void render(Game& game, Renderer& r) {
    auto& s = state(game);
    switch (s.phase) {
        case Phase::Inactive:  return;
        case Phase::Confirm:   render_confirm(game, r); return;
        case Phase::Cinematic: render_cinematic(game, r); return;
        case Phase::Apply:     return;
    }
}

bool handle_key(Game& game, int key) {
    auto& s = state(game);
    if (s.phase == Phase::Inactive) return false;

    if (s.phase == Phase::Confirm) {
        if (key == KEY_ENTER) {
            // First crossing? Run cinematic.
            if (!game.consciousness().seen_first_rebirth) {
                s.phase = Phase::Cinematic;
                s.cinematic_line_idx = 0;
                s.cinematic_tick = 0;
            } else {
                s.phase = Phase::Apply;
                apply(game);
                s.phase = Phase::Inactive;
            }
            return true;
        }
        if (key == KEY_ESCAPE) {
            s.phase = Phase::Inactive;
            return true;
        }
        return true;
    }

    if (s.phase == Phase::Cinematic) {
        constexpr int k_lines = sizeof(k_cinematic_lines) / sizeof(*k_cinematic_lines);
        if (s.cinematic_line_idx < k_lines - 1) {
            s.cinematic_line_idx++;
            return true;
        } else {
            s.phase = Phase::Apply;
            apply(game);
            s.phase = Phase::Inactive;
            return true;
        }
    }
    return false;
}

bool is_active(const Game& game) { return state(game).phase != Phase::Inactive; }

} // namespace astra::rebirth_sequence
```

For cinematic pacing, drop a line each time the player presses a key (manual reveal) — this is turn-based; a real wall-clock pacing would require the game's frame loop to drive it. Manual reveal is simpler and respects the game's keyboard-driven cadence.

- [ ] **Step 13.4 — Hook from the navigation layer**

In the function identified in Step 13.1, add the Sgr A\* branch:

```cpp
if (target_system.id == 0 /* Sgr A* */) {
    rebirth_sequence::begin(game);
    return;   // do NOT execute the normal jump
}
```

In the main game render loop, after normal rendering:

```cpp
rebirth_sequence::render(game, r);
```

In the main input handler, before normal input:

```cpp
if (rebirth_sequence::handle_key(game, key)) return;
```

- [ ] **Step 13.5 — Add `Game::consciousness()` and `start_new_galaxy()`**

`include/astra/game.h`:

```cpp
ConsciousnessSave& consciousness();
const ConsciousnessSave& consciousness() const;
RebirthState& rebirth_state();
void start_new_galaxy(uint32_t seed);
```

Implementation: `start_new_galaxy` re-runs the world generation pipeline that ordinarily fires on `New Game`, with the supplied seed.

- [ ] **Step 13.6 — Dev verb `:rebirth`**

```cpp
if (cmd == "rebirth") {
    rebirth_sequence::begin(game);
    return;
}
if (cmd == "rebirth-reset") {
    delete_consciousness();
    game.consciousness() = ConsciousnessSave{};
    game.log("consciousness.dat cleared.");
    return;
}
```

- [ ] **Step 13.7 — Smoke test**

`:unlock-anchor` to seed a base + assign consciousness_id. `:rebirth`. Confirm modal lists "Deep-Grid base", "consciousness id". Confirm — first crossing plays cinematic. After cinematic, verify new galaxy seed (different system layout, fresh ship), and that `:netmap` deep-Grid zoom *still* shows `Your.Anchor`.

- [ ] **Step 13.8 — Commit**

```bash
git add include/astra/rebirth_sequence.h src/rebirth_sequence.cpp \
        include/astra/game.h src/game.cpp src/dev_console.cpp \
        CMakeLists.txt
git commit -m "feat(grid): Sgr A* rebirth — modal + cinematic + galaxy reseed (Task 13)"
```

---

## Task 14 — Documentation updates

**Files:**
- Modify: `docs/mechanics.md` — Hacking section
- Modify: `docs/items.md` — Cyberdeck/Program/Implant tables
- Modify: `docs/roadmap.md` — Plan 4 line

**Goal:** All Plan 4 features land in the design docs.

- [ ] **Step 14.1 — `docs/mechanics.md`**

Append to the Hacking section:

- Soul Mirror channel: progress per turn, EP cost, commit threshold, damage interrupt rules.
- Sgr A\* rebirth: confirmation modal, cinematic, save scope deletion + apply.
- Per-build cross-rebirth survival matrix (re-listed verbatim from spec §4).

- [ ] **Step 14.2 — `docs/items.md`**

Add Implant family:

```
## Implants

| Name           | Slot type | Effects                  | Source                |
|----------------|-----------|--------------------------|-----------------------|
| Neural Backup  | Implant   | -1 Will; auto-syncs lore | T2+ deep-Grid drop    |
```

Add the two new T3 programs to the Program family table.

- [ ] **Step 14.3 — `docs/roadmap.md`**

Tick the D-layer line; add a "Plan 5 — Grid HUD redesign (deferred)" entry; add a "Plan 6 — Grid content expansion (LAN subnets)" entry.

- [ ] **Step 14.4 — Commit**

```bash
git add docs/mechanics.md docs/items.md docs/roadmap.md
git commit -m "docs(grid): mechanics, items, roadmap — Plan 4 D-layer (Task 14)"
```

---

## Self-review checklist

Run after all 14 tasks land, before merging:

- [ ] **Spec coverage:** Every checkbox in spec §12 is implemented.
- [ ] **No backcompat code:** v54 saves rejected; no migration shims.
- [ ] **File-size discipline:** `wc -l src/*.cpp` — no new file > 600 lines, none of the modified files have grown past 600.
- [ ] **Dev verbs in place:** `:rebirth`, `:spawn-implant`, `:unlock-anchor`, `:sync-soul`, `:dump-precursor`.
- [ ] **Survival matrix verified:** by playing through a `:rebirth` with each of the four build configurations (no Cat_Hacking; Cat_Hacking only; Neural Backup only; ConsciousnessAnchor capstone) — verify the right things survive.
- [ ] **Camera doesn't break Plan 3 sectors:** subnets render visually identical to v53.
- [ ] **`netmap` overlay regression-free:** opening + closing leaves the terminal in a clean state.
- [ ] **Save schema serialization round-trips:** save → quit → load — Player's implants and consciousness state survive.

## Closeout

After all tasks land + manual verification, invoke `superpowers:finishing-a-development-branch` for the merge protocol. Per `feedback_clean_commits.md`, expect a cherry-pick squash before merging if individual commits got noisy.

After merge, write the Plan 5 kickoff prompt under `.superpowers/` covering the Grid HUD redesign per spec §4's deferred-to-Plan-5 subsection.
