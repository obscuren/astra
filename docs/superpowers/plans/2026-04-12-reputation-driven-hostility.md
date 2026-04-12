# Reputation-Driven Hostility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace hardcoded NPC Disposition with reputation-derived hostility so NPCs fight each other and the player based on faction standings.

**Architecture:** Remove the `Disposition` enum entirely. Add a faction standings table and `is_hostile()` / `is_hostile_to_player()` query functions. All hostility checks become reputation queries. NPC AI gains target selection that scans for hostile NPCs and the player using the same logic.

**Tech Stack:** C++20, CMake, terminal renderer

**Spec:** `docs/superpowers/specs/2026-04-12-reputation-driven-hostility-design.md`

---

### Task 1: Faction Registry & Standings Table

**Files:**
- Create: `include/astra/faction.h`
- Modify: `src/faction.cpp`
- Modify: `include/astra/character.h:43-49` (update tier thresholds in comments)

This task creates the faction registry with string constants, faction descriptions (for the reputation tab), the inter-faction standings table, and the `is_hostile()` query functions.

- [ ] **Step 1: Create `include/astra/faction.h`**

```cpp
#pragma once

#include <string>
#include <vector>

namespace astra {

struct Player; // forward declare

// ─── Faction string constants ───────────────────────────────────
// Use these everywhere instead of raw string literals.

inline constexpr const char* Faction_StellariConclave = "Stellari Conclave";
inline constexpr const char* Faction_KrethMiningGuild = "Kreth Mining Guild";
inline constexpr const char* Faction_VeldraniAccord   = "Veldrani Accord";
inline constexpr const char* Faction_SylphariWanderers = "Sylphari Wanderers";
inline constexpr const char* Faction_TerranFederation = "Terran Federation";
inline constexpr const char* Faction_XytomorphHive    = "Xytomorph Hive";
inline constexpr const char* Faction_VoidReavers      = "Void Reavers";
inline constexpr const char* Faction_ArchonRemnants   = "Archon Remnants";
inline constexpr const char* Faction_Feral            = "Feral";
inline constexpr const char* Faction_DriftCollective   = "The Drift Collective";

// ─── Faction metadata ───────────────────────��───────────────────

struct FactionInfo {
    const char* name;
    const char* description;
};

// Returns all factions in display order.
const std::vector<FactionInfo>& all_factions();

// ─── Inter-faction standings ──────────────────────────────────���─

// Look up the default standing between two factions.
// Returns 0 if either faction is empty or unknown.
int default_faction_standing(const std::string& a, const std::string& b);

// ─── Hostility queries ──────────────────────────────���───────────

// True if faction_a considers faction_b hostile (standing <= -300).
// Returns false if either faction is empty or they are the same.
bool is_hostile(const std::string& faction_a, const std::string& faction_b);

// True if the NPC's faction considers the player hostile.
// Uses the player's reputation vector (player→faction standing).
bool is_hostile_to_player(const std::string& npc_faction, const Player& player);

// Return the faction description string (for UI). Empty if unknown.
const char* faction_description(const std::string& faction);

} // namespace astra
```

- [ ] **Step 2: Update reputation tier thresholds in `include/astra/character.h`**

Change the comments on lines 43-49 from old range to new:

```cpp
enum class ReputationTier : int8_t {
    Hated    = -2,   // rep <= -300
    Disliked = -1,   // rep -299 to -60
    Neutral  =  0,   // rep -59 to 59
    Liked    =  1,   // rep 60 to 299
    Trusted  =  2,   // rep >= 300
};
```

- [ ] **Step 3: Update `src/faction.cpp` with new thresholds, standings table, and query functions**

Replace the entire file. The new version keeps the existing functions (`reputation_tier`, `reputation_tier_name`, `reputation_price_pct`, `reputation_for`) but with updated thresholds, and adds the faction registry, standings table, and hostility queries.

```cpp
#include "astra/faction.h"
#include "astra/character.h"
#include "astra/player.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace astra {

// ─── Reputation tier (new -600/+600 scale) ──────────────────────

ReputationTier reputation_tier(int reputation) {
    if (reputation <= -300) return ReputationTier::Hated;
    if (reputation <= -60)  return ReputationTier::Disliked;
    if (reputation < 60)    return ReputationTier::Neutral;
    if (reputation < 300)   return ReputationTier::Liked;
    return ReputationTier::Trusted;
}

const char* reputation_tier_name(ReputationTier tier) {
    switch (tier) {
        case ReputationTier::Hated:    return "Hated";
        case ReputationTier::Disliked: return "Disliked";
        case ReputationTier::Neutral:  return "Neutral";
        case ReputationTier::Liked:    return "Liked";
        case ReputationTier::Trusted:  return "Trusted";
    }
    return "Unknown";
}

int reputation_price_pct(int reputation) {
    switch (reputation_tier(reputation)) {
        case ReputationTier::Hated:    return 30;
        case ReputationTier::Disliked: return 15;
        case ReputationTier::Neutral:  return 0;
        case ReputationTier::Liked:    return -10;
        case ReputationTier::Trusted:  return -20;
    }
    return 0;
}

int reputation_for(const Player& player, const std::string& faction) {
    if (faction.empty()) return 0;
    for (const auto& fs : player.reputation) {
        if (fs.faction_name == faction) return fs.reputation;
    }
    return 0;
}

// ─── Faction registry ───────────────────────────────────────────

const std::vector<FactionInfo>& all_factions() {
    static const std::vector<FactionInfo> factions = {
        {Faction_StellariConclave,  "Station operators, scientists, and diplomats of the Stellari race. "
                                     "They maintain the network of ancient space stations."},
        {Faction_KrethMiningGuild,  "Stocky, mineral-skinned traders and resource extractors. "
                                     "They control most of the galaxy's commerce routes."},
        {Faction_VeldraniAccord,    "Tall, blue-skinned diplomats and traders. "
                                     "Known for brokering peace between warring factions."},
        {Faction_SylphariWanderers, "Wispy, luminescent nomads who drift between the stars. "
                                     "They value freedom and the natural order above all."},
        {Faction_TerranFederation,  "Humanity's government — adaptable, resourceful, and numerous. "
                                     "They are the galaxy's generalists."},
        {Faction_XytomorphHive,     "Chitinous alien predators driven by a hive intelligence. "
                                     "Hostile to all other life."},
        {Faction_VoidReavers,       "Pirates and raiders who prey on trade routes. "
                                     "They respect only strength."},
        {Faction_ArchonRemnants,    "Automated defense constructs left by an ancient civilization. "
                                     "Their programming has degraded into hostility."},
        {Faction_Feral,             "Wild space fauna — cave beasts, void creatures, and other "
                                     "non-intelligent predators found throughout the galaxy."},
        {Faction_DriftCollective,   "Scavengers and nomads with no homeworld. "
                                     "They trade with anyone and fight only when provoked."},
    };
    return factions;
}

const char* faction_description(const std::string& faction) {
    for (const auto& f : all_factions()) {
        if (f.name == faction) return f.description;
    }
    return "";
}

// ─── Inter-faction standings table ──────────────────────��───────
//
// Symmetric pairs: (faction_a, faction_b, standing).
// To tweak any relationship, change the number here.
// Pairs not listed default to 0 (Neutral).

struct FactionPair {
    const char* a;
    const char* b;
    int standing;
};

// clang-format off
static const FactionPair default_standings[] = {
    // Race-aligned inter-faction relations
    {Faction_StellariConclave,  Faction_VeldraniAccord,    100},
    {Faction_StellariConclave,  Faction_SylphariWanderers,  50},
    {Faction_StellariConclave,  Faction_TerranFederation,   50},
    {Faction_KrethMiningGuild,  Faction_TerranFederation,   50},
    {Faction_KrethMiningGuild,  Faction_SylphariWanderers, -50},
    {Faction_VeldraniAccord,    Faction_SylphariWanderers, 100},
    {Faction_VeldraniAccord,    Faction_TerranFederation,  100},
    {Faction_VeldraniAccord,    Faction_DriftCollective,    50},
    {Faction_SylphariWanderers, Faction_DriftCollective,    50},

    // Everyone vs Xytomorphs
    {Faction_StellariConclave,  Faction_XytomorphHive,    -400},
    {Faction_KrethMiningGuild,  Faction_XytomorphHive,    -400},
    {Faction_VeldraniAccord,    Faction_XytomorphHive,    -400},
    {Faction_SylphariWanderers, Faction_XytomorphHive,    -400},
    {Faction_TerranFederation,  Faction_XytomorphHive,    -400},
    {Faction_DriftCollective,   Faction_XytomorphHive,    -400},
    {Faction_VoidReavers,       Faction_XytomorphHive,    -400},
    {Faction_ArchonRemnants,    Faction_XytomorphHive,    -400},
    {Faction_Feral,             Faction_XytomorphHive,    -400},

    // Everyone vs Void Reavers
    {Faction_StellariConclave,  Faction_VoidReavers,      -350},
    {Faction_KrethMiningGuild,  Faction_VoidReavers,      -350},
    {Faction_VeldraniAccord,    Faction_VoidReavers,      -300},
    {Faction_SylphariWanderers, Faction_VoidReavers,      -350},
    {Faction_TerranFederation,  Faction_VoidReavers,      -350},
    {Faction_DriftCollective,   Faction_VoidReavers,      -300},

    // Everyone vs Archon Remnants
    {Faction_StellariConclave,  Faction_ArchonRemnants,   -400},
    {Faction_KrethMiningGuild,  Faction_ArchonRemnants,   -400},
    {Faction_VeldraniAccord,    Faction_ArchonRemnants,   -400},
    {Faction_SylphariWanderers, Faction_ArchonRemnants,   -400},
    {Faction_TerranFederation,  Faction_ArchonRemnants,   -400},
    {Faction_DriftCollective,   Faction_ArchonRemnants,   -400},
    {Faction_VoidReavers,       Faction_ArchonRemnants,   -400},

    // Everyone vs Feral
    {Faction_StellariConclave,  Faction_Feral,            -400},
    {Faction_KrethMiningGuild,  Faction_Feral,            -400},
    {Faction_VeldraniAccord,    Faction_Feral,            -400},
    {Faction_SylphariWanderers, Faction_Feral,            -400},
    {Faction_TerranFederation,  Faction_Feral,            -400},
    {Faction_DriftCollective,   Faction_Feral,            -400},
    {Faction_VoidReavers,       Faction_Feral,            -400},
    {Faction_ArchonRemnants,    Faction_Feral,            -400},

    // Hostile factions vs each other
    {Faction_XytomorphHive,     Faction_VoidReavers,      -400},
    {Faction_XytomorphHive,     Faction_ArchonRemnants,   -400},
    {Faction_XytomorphHive,     Faction_Feral,            -400},
    {Faction_VoidReavers,       Faction_Feral,            -400},
    {Faction_ArchonRemnants,    Faction_Feral,            -400},
};
// clang-format on

int default_faction_standing(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return 0;
    if (a == b) return 600; // same faction = max friendly
    for (const auto& p : default_standings) {
        if ((a == p.a && b == p.b) || (a == p.b && b == p.a))
            return p.standing;
    }
    return 0; // unknown pair = Neutral
}

// ─── Hostility queries ──────────────────────────────────────────

static constexpr int hostile_threshold = -300;

bool is_hostile(const std::string& faction_a, const std::string& faction_b) {
    if (faction_a.empty() || faction_b.empty()) return false;
    if (faction_a == faction_b) return false;
    return default_faction_standing(faction_a, faction_b) <= hostile_threshold;
}

bool is_hostile_to_player(const std::string& npc_faction, const Player& player) {
    if (npc_faction.empty()) return false;
    int rep = reputation_for(player, npc_faction);
    return rep <= hostile_threshold;
}

} // namespace astra
```

- [ ] **Step 4: Build and verify compilation**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | head -50`

Expected: Compiles (faction.cpp changes are backward-compatible since old functions still exist with new thresholds).

- [ ] **Step 5: Commit**

```bash
git add include/astra/faction.h include/astra/character.h src/faction.cpp
git commit -m "feat(faction): add faction registry, standings table, and hostility queries

New -600/+600 reputation scale. 10 factions with inter-faction standings.
is_hostile() and is_hostile_to_player() derive hostility from reputation."
```

---

### Task 2: Remove Disposition Enum & Field

**Files:**
- Modify: `include/astra/npc.h:13-17, 45` (remove enum and field)
- Modify: `include/astra/map_editor.h:65` (remove disposition from NpcTemplate)
- Modify: `src/map_editor.cpp:57-67` (remove disposition from palette)
- Modify: all `src/npcs/*.cpp` files (remove disposition assignments)

- [ ] **Step 1: Remove Disposition enum and field from `include/astra/npc.h`**

Delete lines 13-17 (the `Disposition` enum) entirely.

Remove line 45: `Disposition disposition = Disposition::Neutral;`

- [ ] **Step 2: Remove disposition from `include/astra/map_editor.h`**

Change the NpcTemplate struct (line 61-66) to remove the `disposition` field:

```cpp
    struct NpcTemplate {
        std::string name;
        NpcRole npc_role;
        std::string role;
    };
```

- [ ] **Step 3: Remove disposition from `src/map_editor.cpp` palette**

Update `init_npc_palette()` (lines 57-67) to remove the disposition values:

```cpp
void MapEditor::init_npc_palette() {
    npc_palette_ = {
        {"Guard",     NpcRole::Civilian,      "guard"},
        {"Merchant",  NpcRole::Merchant,      "merchant"},
        {"Engineer",  NpcRole::Engineer,      "engineer"},
        {"Medic",     NpcRole::Medic,         "medic"},
        {"Commander", NpcRole::Commander,     "commander"},
        {"Civilian",  NpcRole::Civilian,      "civilian"},
        {"Hostile",   NpcRole::Xytomorph,     "hostile"},
    };
    npc_cursor_ = 0;
}
```

Also find any place in `map_editor.cpp` that sets `npc.disposition` from the palette template and remove that assignment. Search for `disposition` in the file.

- [ ] **Step 4: Remove `npc.disposition = ...` from all NPC builder files**

Remove the disposition assignment line from each file:

| File | Line to remove |
|------|---------------|
| `src/npcs/hub_npcs.cpp` | Lines 13, 56, 98, 177, 223, 265 — remove `npc.disposition = Disposition::Friendly;` or `Neutral` |
| `src/npcs/xytomorph.cpp` | Line 12 — remove `npc.disposition = Disposition::Hostile;` |
| `src/npcs/station_keeper.cpp` | Line 13 — remove `npc.disposition = Disposition::Friendly;` |
| `src/npcs/merchant.cpp` | Line 13 — remove `npc.disposition = Disposition::Neutral;` |
| `src/npcs/drifter.cpp` | Line 12 — remove `npc.disposition = Disposition::Neutral;` |
| `src/npcs/nova.cpp` | Line 13 — remove `npc.disposition = Disposition::Friendly;` |
| `src/npcs/civilian.cpp` | Line 65 — remove `npc.disposition = Disposition::Friendly;` |
| `src/npcs/scavenger.cpp` | Line 12 — remove `npc.disposition = Disposition::Neutral;` |
| `src/npcs/prospector.cpp` | Line 12 — remove `npc.disposition = Disposition::Neutral;` |

- [ ] **Step 5: DO NOT BUILD YET — this will break compilation because many .cpp files still reference `Disposition`. That's expected. Continue to Task 3-7 which fix all references. Build after all references are updated.**

- [ ] **Step 6: Commit (stage only, don't push)**

```bash
git add include/astra/npc.h include/astra/map_editor.h src/map_editor.cpp src/npcs/*.cpp
git commit -m "refactor(npc): remove Disposition enum and field

Hostility is now derived from reputation, never stored.
Remaining Disposition references will be fixed in following commits."
```

---

### Task 3: Update Game Interaction (Disposition → Hostility Query)

**Files:**
- Modify: `src/game_interaction.cpp:104, 229, 248, 342`

All four `Disposition` references must become `is_hostile_to_player()` calls.

- [ ] **Step 1: Add include at top of `src/game_interaction.cpp`**

Add `#include "astra/faction.h"` to the includes if not already present.

- [ ] **Step 2: Update bump-to-attack (line 104)**

Change:
```cpp
if (npc.disposition == Disposition::Hostile) {
```
To:
```cpp
if (is_hostile_to_player(npc.faction, game.player())) {
```

Note: the surrounding code references `*this` as the Game — check whether the function is a member of `Game`. If so, use `player()` or `player_`. Match the existing pattern in the file.

- [ ] **Step 3: Update hostile interaction refusal (line 229)**

Change:
```cpp
if (target->disposition == Disposition::Hostile) {
```
To:
```cpp
if (is_hostile_to_player(target->faction, game.player())) {
```

Again, match the variable names used in context — it might be `player_` if inside a Game method.

- [ ] **Step 4: Update is_interactable check (line 248)**

Change:
```cpp
if (npc.x == tx && npc.y == ty && npc.disposition != Disposition::Hostile) return true;
```
To:
```cpp
if (npc.x == tx && npc.y == ty && !is_hostile_to_player(npc.faction, player_)) return true;
```

Check the function signature to determine whether `player_` or `game.player()` is the right accessor.

- [ ] **Step 5: Update auto-walk hostile check (line 342)**

Change:
```cpp
if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
```
To:
```cpp
if (!npc.alive() || !is_hostile_to_player(npc.faction, player_)) continue;
```

- [ ] **Step 6: Commit**

```bash
git add src/game_interaction.cpp
git commit -m "refactor(interaction): replace Disposition checks with is_hostile_to_player()"
```

---

### Task 4: Update Game Combat — NPC AI with NPC-vs-NPC

**Files:**
- Modify: `src/game_combat.cpp` (process_npc_turn, attack_npc, begin_targeting)
- Modify: `include/astra/combat_system.h` (add attack_npc_vs_npc declaration)

This is the biggest task. The NPC AI needs to scan for hostile NPCs and fight them, not just chase the player.

- [ ] **Step 1: Add include to `src/game_combat.cpp`**

Add `#include "astra/faction.h"` to the includes.

- [ ] **Step 2: Add `attack_npc_vs_npc` declaration to `include/astra/combat_system.h`**

Add after line 23 (`attack_npc`):

```cpp
    void attack_npc_vs_npc(Npc& attacker, Npc& defender, Game& game);
```

- [ ] **Step 3: Add helper function to find nearest hostile entity in `src/game_combat.cpp`**

Add before `process_npc_turn`:

```cpp
// Find the nearest entity (NPC or player) that is hostile to the given NPC.
// Returns a pointer to the target NPC, or nullptr if the player is the target.
// Sets out_target_is_player=true if the player is the best target.
// Returns nullptr with out_target_is_player=false if no hostile entity is in range.
struct HostileTarget {
    Npc* npc = nullptr;       // non-null if target is an NPC
    bool is_player = false;   // true if target is the player
    int distance = 9999;
};

static HostileTarget find_nearest_hostile(Npc& self, Game& game) {
    HostileTarget best;
    const int detection_range = 8;

    // Check other NPCs
    for (auto& other : game.world().npcs()) {
        if (&other == &self || !other.alive()) continue;
        if (!is_hostile(self.faction, other.faction)) continue;
        int d = chebyshev_dist(self.x, self.y, other.x, other.y);
        if (d <= detection_range && d < best.distance) {
            best.npc = &other;
            best.is_player = false;
            best.distance = d;
        }
    }

    // Check player
    if (is_hostile_to_player(self.faction, game.player())) {
        int d = chebyshev_dist(self.x, self.y, game.player().x, game.player().y);
        if (d <= detection_range && d < best.distance) {
            best.npc = nullptr;
            best.is_player = true;
            best.distance = d;
        }
    }

    return best;
}
```

- [ ] **Step 4: Implement `attack_npc_vs_npc` in `src/game_combat.cpp`**

Add after `attack_npc`:

```cpp
void CombatSystem::attack_npc_vs_npc(Npc& attacker, Npc& defender, Game& game) {
    // Dodge check (same as player dodge vs NPC)
    if (roll_percent(game.world().rng(), npc_dodge_chance(defender))) {
        game.log(defender.display_name() + " dodges " + attacker.display_name() + "'s attack!");
        return;
    }

    int damage = attacker.attack_damage();
    // Simple defense: level-based
    int defense = defender.level;
    damage -= defense;
    if (damage < 1) damage = 1;

    damage = apply_damage_effects(defender.effects, damage);
    if (damage <= 0) {
        game.log(attacker.display_name() + "'s attack has no effect on " +
                 defender.display_name() + ".");
        return;
    }
    defender.hp -= damage;
    if (defender.hp < 0) defender.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, defender.x, defender.y);
    game.log(attacker.display_name() + " strikes " + defender.display_name() +
             " for " + std::to_string(damage) + " damage!");
    if (!defender.alive()) {
        game.log(defender.display_name() + " is destroyed by " + attacker.display_name() + "!");
    }
}
```

- [ ] **Step 5: Rewrite `process_npc_turn` in `src/game_combat.cpp`**

Replace the entire function (lines 27-137) with:

```cpp
void CombatSystem::process_npc_turn(Npc& npc, Game& game) {
    if (!npc.alive()) return;

    // Displaced NPCs try to return to their original position
    if (npc.return_x >= 0 && npc.return_y >= 0) {
        int rx = npc.return_x, ry = npc.return_y;
        npc.return_x = -1;
        npc.return_y = -1;
        if (game.world().map().passable(rx, ry) &&
            !(game.player().x == rx && game.player().y == ry) &&
            !game.tile_occupied(rx, ry)) {
            npc.x = rx;
            npc.y = ry;
        }
        return;
    }

    // Stationary NPCs (shopkeepers, quest givers, etc.)
    if (npc.quickness == 0) return;

    // Fleeing — move away from player (Intimidate effect)
    if (has_effect(npc.effects, EffectId::Flee)) {
        int dx = sign(npc.x - game.player().x);
        int dy = sign(npc.y - game.player().y);
        struct { int x, y; } candidates[] = {
            {dx, dy}, {dx, 0}, {0, dy}, {-dy, dx}, {dy, -dx}
        };
        for (auto [cx, cy] : candidates) {
            if (cx == 0 && cy == 0) continue;
            int nx = npc.x + cx;
            int ny = npc.y + cy;
            if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
                npc.x = nx;
                npc.y = ny;
                return;
            }
        }
        return; // cornered, skip turn
    }

    // Find nearest hostile target
    auto target = find_nearest_hostile(npc, game);

    if (target.is_player) {
        int dist = target.distance;

        // Adjacent — attack player
        if (dist <= 1) {
            int dodge_chance = std::min(game.player().effective_dodge() * 2, 50);
            if (roll_percent(game.world().rng(), dodge_chance)) {
                game.log("You dodge " + npc.display_name() + "'s attack!");
                return;
            }
            int raw_damage = npc.attack_damage();
            int defense = game.player().effective_defense();
            int damage = raw_damage - defense;
            if (damage < 1) damage = 1;
            damage = apply_damage_effects(game.player().effects, damage);
            if (damage <= 0) {
                game.log(npc.display_name() + " strikes you but deals no damage.");
                return;
            }
            game.player().hp -= damage;
            if (game.player().hp < 0) game.player().hp = 0;
            game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
            game.log(npc.display_name() + " strikes you for " +
                     std::to_string(damage) + " damage!");
            if (game.player().hp <= 0) {
                game.set_death_message("Slain by " + npc.display_name());
            }
            return;
        }

        // Chase player
        int dx = sign(game.player().x - npc.x);
        int dy = sign(game.player().y - npc.y);
        struct { int x, y; } candidates[] = {{dx, dy}, {dx, 0}, {0, dy}};
        for (auto [cx, cy] : candidates) {
            if (cx == 0 && cy == 0) continue;
            int nx = npc.x + cx;
            int ny = npc.y + cy;
            if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
                npc.x = nx;
                npc.y = ny;
                return;
            }
        }
        return; // blocked
    }

    if (target.npc) {
        int dist = target.distance;

        // Adjacent — attack NPC
        if (dist <= 1) {
            attack_npc_vs_npc(npc, *target.npc, game);
            return;
        }

        // Chase NPC
        int dx = sign(target.npc->x - npc.x);
        int dy = sign(target.npc->y - npc.y);
        struct { int x, y; } candidates[] = {{dx, dy}, {dx, 0}, {0, dy}};
        for (auto [cx, cy] : candidates) {
            if (cx == 0 && cy == 0) continue;
            int nx = npc.x + cx;
            int ny = npc.y + cy;
            if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
                npc.x = nx;
                npc.y = ny;
                return;
            }
        }
        return; // blocked
    }

    // No hostile target — wander
    std::array<std::pair<int,int>, 4> dirs = {{{0,-1},{0,1},{-1,0},{1,0}}};
    std::shuffle(dirs.begin(), dirs.end(), game.world().rng());
    for (auto [dx, dy] : dirs) {
        int nx = npc.x + dx;
        int ny = npc.y + dy;
        if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
            npc.x = nx;
            npc.y = ny;
            return;
        }
    }
}
```

Key changes:
- Removed `Disposition::Friendly` early return — now uses `quickness == 0` for stationary NPCs
- Hostile check uses `find_nearest_hostile()` which scans both NPCs and player
- NPC-vs-NPC combat via `attack_npc_vs_npc()`
- Player deprioritized by distance (nearest target wins, regardless of type)

- [ ] **Step 6: Update `begin_targeting` (line 214)**

Change:
```cpp
if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
```
To:
```cpp
if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
```

- [ ] **Step 7: Add reputation penalty on kill to `attack_npc` (after line 184)**

After `game.player().kills++;` (line 184), add:

```cpp
        // Reputation penalty for killing a faction NPC
        if (!npc.faction.empty()) {
            for (auto& fs : game.player().reputation) {
                if (fs.faction_name == npc.faction) {
                    fs.reputation = std::max(fs.reputation - 30, -600);
                    game.log("Your reputation with " + npc.faction + " decreased.");
                    break;
                }
            }
        }
```

Add the same block to `shoot_target` after `game.player().kills++;` (line 395).

- [ ] **Step 8: Commit**

```bash
git add include/astra/combat_system.h src/game_combat.cpp
git commit -m "feat(combat): reputation-driven NPC AI with NPC-vs-NPC combat

NPCs scan for hostile entities (player and other NPCs) using is_hostile().
NPC-vs-NPC combat uses same damage formula. Killing faction NPCs costs -30 rep."
```

---

### Task 5: Update Rendering, Minimap, Abilities, Dev Commands

**Files:**
- Modify: `src/game_rendering.cpp:1211-1214`
- Modify: `src/minimap.cpp:223`
- Modify: `src/ability.cpp:52, 212`
- Modify: `src/game.cpp:418-425` (kill hostiles dev command)
- Modify: `src/dev_console.cpp:432` (killall command)

- [ ] **Step 1: Add `#include "astra/faction.h"` to each file that doesn't have it**

Add the include to `game_rendering.cpp`, `minimap.cpp`, `ability.cpp`, `game.cpp`, and `dev_console.cpp`.

- [ ] **Step 2: Update rendering colors in `src/game_rendering.cpp` (lines 1211-1214)**

Replace the switch block:
```cpp
        switch (combat_.target_npc()->disposition) {
            case Disposition::Hostile:  tc = Color::Red; break;
            case Disposition::Neutral:  tc = Color::Yellow; break;
            case Disposition::Friendly: tc = Color::Green; break;
        }
```
With:
```cpp
        if (is_hostile_to_player(combat_.target_npc()->faction, player_)) {
            tc = Color::Red;
        } else {
            auto tier = reputation_tier(reputation_for(player_, combat_.target_npc()->faction));
            tc = (tier <= ReputationTier::Disliked) ? Color::Yellow : Color::Green;
        }
```

- [ ] **Step 3: Update minimap in `src/minimap.cpp` (line 223)**

Change:
```cpp
bool hostile = (npc.disposition == Disposition::Hostile);
```
To:
```cpp
bool hostile = is_hostile_to_player(npc.faction, player);
```

Check how `player` is accessed in the minimap draw function. It may be passed as a parameter or accessed via a member. Match the existing pattern. The function likely receives a `const Player&` — use that.

- [ ] **Step 4: Update ability targeting in `src/ability.cpp`**

At line 52 (CleaveAbility), change:
```cpp
if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
```
To:
```cpp
if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
```

At line 212 (general ability target filter), change:
```cpp
if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
```
To:
```cpp
if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
```

- [ ] **Step 5: Update dev kill hostiles in `src/game.cpp` (lines 418-425)**

Change:
```cpp
if (npc.alive() && npc.disposition == Disposition::Hostile) {
```
To:
```cpp
if (npc.alive() && is_hostile_to_player(npc.faction, player_)) {
```

- [ ] **Step 6: Update dev console killall in `src/dev_console.cpp` (line 432)**

Change:
```cpp
== Disposition::Hostile
```
To:
```cpp
is_hostile_to_player(npc.faction, player)
```

Check the exact variable names used in the console handler for the player reference.

- [ ] **Step 7: Build and verify compilation**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -30`

Expected: Clean compilation. All `Disposition::` references should now be gone.

- [ ] **Step 8: Verify no remaining Disposition references**

Run: `grep -rn "Disposition" include/ src/ --include="*.cpp" --include="*.h"`

Expected: Zero results.

- [ ] **Step 9: Commit**

```bash
git add src/game_rendering.cpp src/minimap.cpp src/ability.cpp src/game.cpp src/dev_console.cpp
git commit -m "refactor: replace all remaining Disposition checks with reputation queries"
```

---

### Task 6: Update Save/Load & Player Initialization

**Files:**
- Modify: `include/astra/save_file.h:62` (bump version)
- Modify: `src/save_file.cpp:567, 1171, 511-515, 1082-1087`
- Modify: `src/game.cpp:599-601, 867-870` (player reputation init)

- [ ] **Step 1: Bump save version in `include/astra/save_file.h`**

Change line 62:
```cpp
    uint32_t version = 25;   // was 24: added faction to NPC, scaled reputation 6x
```

- [ ] **Step 2: Update `write_npc` in `src/save_file.cpp`**

At line 567, replace:
```cpp
    w.write_u8(static_cast<uint8_t>(npc.disposition));
```
With:
```cpp
    w.write_string(npc.faction);  // v25: faction instead of disposition
```

- [ ] **Step 3: Update `read_npc` in `src/save_file.cpp`**

At line 1171, replace:
```cpp
    npc.disposition = static_cast<Disposition>(r.read_u8());
```
With version-gated logic:
```cpp
    if (data.version >= 25) {
        npc.faction = r.read_string();
    } else {
        // Legacy: read and discard disposition byte
        r.read_u8();
        // Faction will be set by NPC role rebuild below
    }
```

After the NPC is fully read (after interaction traits), add legacy faction assignment for old saves:
```cpp
    // Legacy: assign faction from role for pre-v25 saves
    if (data.version < 25 && npc.faction.empty()) {
        switch (npc.npc_role) {
            case NpcRole::StationKeeper:
            case NpcRole::Medic:
            case NpcRole::Commander:
            case NpcRole::Astronomer:
            case NpcRole::Engineer:
                npc.faction = Faction_StellariConclave;
                break;
            case NpcRole::Merchant:
            case NpcRole::FoodMerchant:
            case NpcRole::ArmsDealer:
                npc.faction = Faction_KrethMiningGuild;
                break;
            case NpcRole::Xytomorph:
                npc.faction = Faction_XytomorphHive;
                break;
            default:
                break; // unaligned
        }
    }
```

- [ ] **Step 4: Scale reputation values on legacy load**

In the reputation read block (around line 1082-1087), add scaling after reading:

```cpp
        uint32_t rep_count = r.read_u32();
        p.reputation.resize(rep_count);
        for (uint32_t i = 0; i < rep_count; ++i) {
            p.reputation[i].faction_name = r.read_string();
            p.reputation[i].reputation = r.read_i32();
        }
        // Scale old reputation values to new range
        if (data.version < 25) {
            for (auto& fs : p.reputation) {
                fs.reputation = std::clamp(fs.reputation * 6, -600, 600);
            }
        }
```

- [ ] **Step 5: Update player reputation initialization in `src/game.cpp`**

Replace dev mode init (lines 599-601):
```cpp
    player_.reputation = {
        {Faction_StellariConclave,  10},
        {Faction_KrethMiningGuild,  0},
        {Faction_VeldraniAccord,    0},
        {Faction_SylphariWanderers, 0},
        {Faction_TerranFederation,  0},
        {Faction_XytomorphHive,     -400},
        {Faction_VoidReavers,       -350},
        {Faction_ArchonRemnants,    -400},
        {Faction_Feral,             -400},
        {Faction_DriftCollective,   0},
    };
```

Replace normal mode init (lines 867-870) with a function that looks up the player's race and assigns the +100 bonus to their race faction:

```cpp
    // Initialize reputation for all factions
    auto race_faction = [](Race r) -> const char* {
        switch (r) {
            case Race::Stellari:   return Faction_StellariConclave;
            case Race::Kreth:      return Faction_KrethMiningGuild;
            case Race::Veldrani:   return Faction_VeldraniAccord;
            case Race::Sylphari:   return Faction_SylphariWanderers;
            case Race::Human:      return Faction_TerranFederation;
            case Race::Xytomorph:  return Faction_XytomorphHive;
        }
        return "";
    };
    const char* own_faction = race_faction(player_.race);

    for (const auto& fi : all_factions()) {
        int starting_rep = default_faction_standing(
            own_faction, fi.name);
        // Bonus for own race's faction
        if (fi.name == std::string(own_faction)) {
            starting_rep = 100;
        }
        // Clamp to range
        starting_rep = std::clamp(starting_rep, -600, 600);
        player_.reputation.push_back({fi.name, starting_rep});
    }
```

Add `#include "astra/faction.h"` to `src/game.cpp` includes.

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -30`

Expected: Clean compilation.

- [ ] **Step 7: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp src/game.cpp
git commit -m "feat(save): bump to v25, serialize NPC faction, scale reputation 6x

NPC faction now persisted instead of disposition. Legacy saves get faction
assigned by role and reputation scaled to new -600/+600 range.
Player starting reputation uses faction standings table + race bonus."
```

---

### Task 7: Update Character Screen — Faction Descriptions

**Files:**
- Modify: `src/character_screen.cpp:2450-2488`

- [ ] **Step 1: Add include**

Add `#include "astra/faction.h"` to the includes of `src/character_screen.cpp`.

- [ ] **Step 2: Rewrite `draw_reputation` to include faction descriptions**

Replace the function (lines 2450-2488):

```cpp
void CharacterScreen::draw_reputation(UIContext& ctx) {
    if (player_->reputation.empty()) {
        ctx.text({.x = 2, .y = 2, .content = "No faction standings.", .tag = UITag::TextDim});
        return;
    }

    int y = 2;
    for (int i = 0; i < static_cast<int>(player_->reputation.size()); ++i) {
        if (y >= ctx.height() - 4) break;
        const auto& f = player_->reputation[i];
        bool selected = (cursor_ == i);

        if (selected) ctx.put(1, y, '>', Color::Yellow);
        ctx.text({.x = 3, .y = y, .content = f.faction_name,
                  .tag = selected ? UITag::TextBright : UITag::TextDefault});

        auto tier = reputation_tier(f.reputation);
        std::string rep = std::string(reputation_tier_name(tier)) +
                          " (" + std::to_string(f.reputation) + ")";
        UITag rep_tag = f.reputation > 0 ? UITag::TextSuccess
                      : f.reputation < 0 ? UITag::TextDanger
                      : UITag::TextDim;
        ctx.text({.x = ctx.width() - 2 - static_cast<int>(rep.size()), .y = y,
                  .content = rep, .tag = rep_tag});

        // Faction description
        y++;
        const char* desc = faction_description(f.faction_name);
        if (desc[0] != '\0') {
            // Word-wrap description to fit panel width
            std::string desc_str(desc);
            int max_w = ctx.width() - 8;
            int dx = 5;
            while (!desc_str.empty() && y < ctx.height() - 3) {
                std::string line;
                if (static_cast<int>(desc_str.size()) <= max_w) {
                    line = desc_str;
                    desc_str.clear();
                } else {
                    auto pos = desc_str.rfind(' ', max_w);
                    if (pos == std::string::npos) pos = max_w;
                    line = desc_str.substr(0, pos);
                    desc_str = desc_str.substr(pos + 1);
                }
                ctx.text({.x = dx, .y = y, .content = line, .tag = UITag::TextDim});
                y++;
            }
        }

        // Flavor text based on tier
        std::string flavor;
        switch (tier) {
            case ReputationTier::Trusted:  flavor = "They consider you a trusted ally."; break;
            case ReputationTier::Liked:    flavor = "They view you with curiosity."; break;
            case ReputationTier::Neutral:  flavor = "They are indifferent toward you."; break;
            case ReputationTier::Disliked: flavor = "They are wary of you."; break;
            case ReputationTier::Hated:    flavor = "They are hostile toward you."; break;
        }
        ctx.text({.x = 5, .y = y, .content = flavor,
                  .tag = tier <= ReputationTier::Disliked ? UITag::TextDanger : UITag::TextDim});
        y += 2;
    }
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -20`

Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add src/character_screen.cpp
git commit -m "feat(ui): show faction descriptions in reputation tab"
```

---

### Task 8: Update Formulas Doc & Verify

**Files:**
- Modify: `docs/formulas.md:72-77` (update reputation thresholds)

- [ ] **Step 1: Update reputation thresholds in `docs/formulas.md`**

Replace lines 72-77:

```
- Faction reputation modifier (`faction_pct`):
  - Hated (rep <= -300): +30%
  - Disliked (rep -299 to -60): +15%
  - Neutral (rep -59 to 59): 0%
  - Liked (rep 60 to 299): -10%
  - Trusted (rep >= 300): -20%
```

- [ ] **Step 2: Add combat reputation section to `docs/formulas.md`**

Add after the Loot Drops section (after line 51):

```markdown
### Combat Reputation

- **Kill faction NPC**: -30 reputation with victim's faction (clamped to -600)
- From Neutral (0), takes 10 kills to reach Hated (≤ -300)
- Killing unaligned, Feral, Xytomorph, Archon, or Void Reaver NPCs: no reputation change (for now)
```

- [ ] **Step 3: Full build + run smoke test**

Run: `cmake -B build -DDEV=ON && cmake --build build && ./build/astra --term`

Verify:
- Game starts without crash
- NPCs display correctly
- Character screen reputation tab shows all 10 factions with descriptions
- Xytomorphs are still hostile (red) and attack on sight

- [ ] **Step 4: Commit**

```bash
git add docs/formulas.md
git commit -m "docs: update formulas for new reputation scale and combat rep penalty"
```

---

### Task 9: Update NPC Spawner — New Faction Assignments

**Files:**
- Modify: `src/npc_spawner.cpp` (verify existing faction assignments use constants)
- Modify: NPC builders as needed for new faction NPCs

- [ ] **Step 1: Update NPC spawner to use faction constants**

In `src/npc_spawner.cpp`, add `#include "astra/faction.h"` and verify all faction string literals are replaced with constants (e.g., `"Stellari Conclave"` → `Faction_StellariConclave`).

- [ ] **Step 2: Update NPC builder faction strings to use constants**

In each builder file under `src/npcs/`, replace raw string faction assignments:
- `"Stellari Conclave"` → `Faction_StellariConclave`
- `"Kreth Mining Guild"` → `Faction_KrethMiningGuild`
- `"Xytomorph Hive"` → `Faction_XytomorphHive`

Add `#include "astra/faction.h"` to each builder file.

- [ ] **Step 3: Assign factions to currently unaligned NPCs**

Review NPCs that currently have empty faction strings and assign appropriate factions:
- `drifter.cpp`: `npc.faction = Faction_DriftCollective;`
- `scavenger.cpp`: `npc.faction = Faction_DriftCollective;`
- `prospector.cpp`: `npc.faction = Faction_KrethMiningGuild;`
- `civilian.cpp`: assign based on the NPC's race or leave unaligned
- `nova.cpp`: `npc.faction = Faction_StellariConclave;` (or leave unaligned — check context)

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -20`

- [ ] **Step 5: Commit**

```bash
git add src/npc_spawner.cpp src/npcs/*.cpp
git commit -m "refactor(npcs): use faction constants, assign factions to unaligned NPCs"
```

---

### Task 10: Final Verification & Cleanup

- [ ] **Step 1: Grep for any remaining raw faction strings**

Run: `grep -rn '"Stellari Conclave"\|"Kreth Mining Guild"\|"Xytomorph Hive"' src/ include/ --include="*.cpp" --include="*.h"`

Replace any remaining raw strings with constants from `faction.h`. Exception: save_file.cpp legacy migration code may still need raw strings for backward compatibility — that's fine.

- [ ] **Step 2: Grep for any remaining Disposition references**

Run: `grep -rn "Disposition" src/ include/ --include="*.cpp" --include="*.h"`

Expected: Zero results.

- [ ] **Step 3: Full build**

Run: `cmake -B build -DDEV=ON && cmake --build build`

Expected: Clean compilation, zero warnings related to our changes.

- [ ] **Step 4: Run the game and test these scenarios**

1. Start new game — verify 10 factions in reputation tab with descriptions
2. Walk into Xytomorph — should trigger combat (they're Hated)
3. Kill Xytomorph — no reputation change message
4. Use dev console: `give rep Stellari Conclave -400` — verify Stellari NPCs turn red and hostile
5. Use dev console: `give rep Stellari Conclave 0` — verify Stellari NPCs turn back to friendly
6. If two hostile factions are on same map, verify they fight each other

- [ ] **Step 5: Commit any remaining fixes**

```bash
git add -A
git commit -m "fix: cleanup remaining faction string literals and edge cases"
```
