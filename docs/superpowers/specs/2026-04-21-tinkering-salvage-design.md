# Tinkering Salvage — Design

Date: 2026-04-21
Status: Approved (pending implementation plan)

## Summary

Turns "scrap from kills" into a structured system with two tiers:

1. **Universal salvage** — any enemy has a low chance to drop a new *Spare Parts* item to the floor on kill. No skill required.
2. **Mechanical salvage** — new mechanical enemies can be disassembled on kill for *Spare Parts* + *Circuitry* straight into the player's inventory. Gated behind the `Cat_Tinkering` category unlock.

Also introduces a `CreatureFlag` bitfield on `Npc` for future-proof creature categorization (only `Mechanical` and `Biological` wired now).

## Scope

### In scope

- New items: `Spare Parts`, `Circuitry`
- New `CreatureFlag` enum (`uint64_t`) with `Mechanical` and `Biological`
- `uint64_t flags` field on `Npc`, persisted in save files
- Marking existing organic NPCs as `Biological`
- Three new mechanical enemy NPCs (Rust Hound, Sentry Drone, Archon Automaton)
- Drop logic in `src/game_combat.cpp`:
  - 5% Spare Parts drop on any kill (to floor)
  - 40% salvage roll on mechanical kills *if* `Cat_Tinkering` learned → 1–2 Spare Parts + 30% Circuitry directly to inventory, with colored log message

### Deferred / out of scope

- Ranged combat for Sentry Drone (ships as melee placeholder, with `TODO` comment)
- Additional `CreatureFlag` variants beyond Mechanical/Biological
- Balance passes on spawn tables for the new enemies
- Any UI changes to the character screen or tinkering screen

## Items

Two new items, both stackable junk/crafting resources.

### Spare Parts

- `ITEM_SPARE_PARTS = 47` in `include/astra/item_ids.h` (next free slot after `ITEM_VOID_MANTLE = 46`; IDs 33–46 are already taken by crafting materials, ship components, and energy shields)
- Name: `"Spare Parts"`, type `ItemType::Junk`, weight 1
- Description: `"Usable parts pulled from wreckage. Good for repairs."`
- Glyph: `~`, color dark-gray (matches existing junk visual language)
- Factory: `Item build_spare_parts()` in `src/item_defs.cpp`

### Circuitry

- `ITEM_CIRCUITRY = 48` in `include/astra/item_ids.h`
- Name: `"Circuitry"`, type `ItemType::Junk`, weight 1
- Description: `"Salvaged integrated circuits. Essential for advanced repair."`
- Glyph: `~`, color cyan (distinct from generic scrap)
- Factory: `Item build_circuitry()` in `src/item_defs.cpp`

Theme entries added to `src/terminal_theme.cpp`. Save-file name/id mapping added in `src/save_file.cpp` alongside existing junk entries. `display_name(Item&)` will return a colored name automatically via the existing theme hook.

## Creature Flags

New header `include/astra/creature_flags.h`:

```cpp
enum class CreatureFlag : uint64_t {
    None       = 0,
    Mechanical = 1ull << 0,
    Biological = 1ull << 1,
};

constexpr uint64_t operator|(CreatureFlag a, CreatureFlag b) {
    return uint64_t(a) | uint64_t(b);
}

inline bool has_flag(uint64_t flags, CreatureFlag f) {
    return (flags & uint64_t(f)) != 0;
}
```

`Npc` (in `include/astra/npc.h`) gains:

```cpp
uint64_t flags = 0;
```

Helpers `is_mechanical(const Npc&)` and `is_biological(const Npc&)` are thin wrappers over `has_flag`.

Every existing organic hostile NPC factory (xytomorph, drifter, pirate captain, archon remnant, void reaver, scavenger, etc.) sets `flags |= uint64_t(CreatureFlag::Biological)`. NPCs that do not participate in combat (merchants, station keepers) may be left at `0` — the flag is informational, not gating.

Save file:
- `save_file.cpp` writes/reads the `flags` field as a `uint64_t`
- Versioned read path required — older saves without the field default to `0` (existing NPCs will silently lose the Biological tag on load; acceptable since it has no gameplay effect outside this feature)

## Mechanical Enemies

Three new NPCs follow the existing pattern (one file per NPC under `src/npcs/`, matching `archon_remnant.cpp`). Each:

- Adds a new value to `NpcRole` in `include/astra/npc.h`
- Provides a factory function returning a fully configured `Npc`
- Sets `flags |= uint64_t(CreatureFlag::Mechanical)`
- Is registered in `create_npc` and `create_npc_by_role` dispatch

| File | NpcRole | Name | Tier | Combat | Notes |
|---|---|---|---|---|---|
| `src/npcs/rust_hound.cpp` | `RustHound` | "Rust Hound" | low | fast melee | scavenger-built quadruped drone |
| `src/npcs/sentry_drone.cpp` | `SentryDrone` | "Sentry Drone" | mid | melee placeholder | hostile station security; `// TODO: ranged attack` |
| `src/npcs/archon_automaton.cpp` | `ArchonAutomaton` | "Archon Automaton" | high | heavy melee | precursor relic; thematically tied to existing Archon lore |

Spawn-table wiring in `src/npc_spawner.cpp` adds these to appropriate biome/level buckets so they can be encountered during normal play. Actual spawn weighting can be tuned later; the goal here is "they exist and can appear."

## Drop Logic

All salvage logic lives in `src/game_combat.cpp`, in the same area as the existing loot-drop block (after the NPC dies). `generate_loot_drop` and `random_junk` are **not** modified — salvage is an independent path.

Pseudocode after a successful kill:

```cpp
if (is_mechanical(npc)) {
    if (player_has_skill(player_, SkillId::Cat_Tinkering)) {
        if (roll(rng) < 0.40f) {
            int spare_count = 1 + (roll(rng) < 0.50f ? 1 : 0); // 1-2
            for (int i = 0; i < spare_count; ++i)
                player_.inventory.push_back(build_spare_parts());

            bool got_circuitry = roll(rng) < 0.30f;
            if (got_circuitry)
                player_.inventory.push_back(build_circuitry());

            Item sp = build_spare_parts();
            std::string msg = "You salvage " + display_name(sp) + " from the " + npc.name + ".";
            if (got_circuitry) {
                Item cc = build_circuitry();
                msg = "You salvage " + display_name(sp) + " and " + display_name(cc) +
                      " from the " + npc.name + ".";
            }
            game.log(msg);
        }
    }
    // Mechanical enemies do not use the universal 5% path — no flesh to scavenge.
} else {
    // Universal path: ungated, independent of existing loot roll.
    if (roll(rng) < 0.05f) {
        Item sp = build_spare_parts();
        world_.ground_items().push_back({npc.x, npc.y, std::move(sp)});
    }
}
```

Stackable behavior follows existing inventory/pickup conventions — if a stack already exists, quantities merge through the normal add path.

### Rationale

- Mechanical enemies skipping the universal 5% floor drop keeps the thematic split clean: flesh → ground parts; machines → structured salvage (or nothing without the skill).
- Using the existing `display_name(Item&)` helper guarantees the log message matches the item's theme color, consistent with other combat log output and the user's "colored references" convention.
- Rates (5% / 40% / 30%) are first-pass values; they live in one place and are trivial to tune after playtest.

## Files Touched (preview)

New:
- `include/astra/creature_flags.h`
- `src/npcs/rust_hound.cpp`
- `src/npcs/sentry_drone.cpp`
- `src/npcs/archon_automaton.cpp`

Modified:
- `include/astra/item_ids.h`, `include/astra/item_defs.h`, `src/item_defs.cpp`
- `include/astra/npc.h`, `src/npc.cpp` (role dispatch)
- `src/npc_spawner.cpp`
- `src/npcs/*.cpp` (existing organic NPCs — add `Biological` flag)
- `src/game_combat.cpp`
- `src/save_file.cpp`
- `src/terminal_theme.cpp`
- `CMakeLists.txt` (new .cpp files)

## Testing

- Build with `-DDEV=ON` and smoke-test: kill a Xytomorph → occasional Spare Parts on ground.
- Spawn a Rust Hound via dev tools without Tinkering: no salvage message, no inventory additions.
- Learn `Cat_Tinkering`, kill a Rust Hound: occasional salvage log message with colored item names; inventory gains items.
- Save/load roundtrip: flags persist across save file reload.

## Follow-ups

- Ranged combat for Sentry Drone (separate feature).
- Additional `CreatureFlag` variants (Undead, Psionic, Synthetic, etc.) as needed.
- Consider scaling salvage yield by enemy tier / level once balance data exists.
