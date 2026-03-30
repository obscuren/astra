# Plan: Wayfinding Skill Category

## Context

The "getting lost" mechanic has flat 15% chance and fixed regain rate with no way to improve. Adding a Wayfinding skill category gives players agency over navigation — terrain lore skills reduce lost chance on specific terrain, Compass Sense improves bearings recovery, and terrain familiarity halves travel time on known terrain. Camp Making hooks into future cooking/rest systems.

---

## Skill Tree

**Category: Wayfinding** (unlock cost: 75 SP)
- *"Knowledge of navigation, terrain, and survival in the wild. Reduces the chance of getting lost and improves overland travel."*

| Skill | Type | SP | Req | Effect |
|-------|------|-----|-----|--------|
| Camp Making | Active | 50 | INT 12 | Set up camp — rest + cook (ability implemented next) |
| Compass Sense | Passive | 50 | INT 13 | Halves the bearings grace period (30→15 moves) and doubles ramp rate |
| Terrain Lore: Plains | Passive | 50 | INT 12 | -50% lost chance on Plains/Desert tiles, halves travel ticks |
| Terrain Lore: Forest | Passive | 50 | INT 13 | -50% lost chance on Forest/Fungal tiles, halves travel ticks |
| Terrain Lore: Wetlands | Passive | 75 | INT 14 | -50% lost chance on Swamp/River/Lake tiles, halves travel ticks |
| Terrain Lore: Mountains | Passive | 75 | INT 15 | -50% lost chance on Mountains/Crater tiles, halves travel ticks |
| Terrain Lore: Tundra | Passive | 75 | INT 15 | -50% lost chance on Ice/Lava tiles, halves travel ticks |

Category unlock itself gives: -10% base lost chance (15%→~13%).

---

## Phase 1: Add SkillIds

### `include/astra/skill_defs.h`
Add to the SkillId enum:
```cpp
// Wayfinding (900+)
Cat_Wayfinding = 9,
CampMaking = 900,
CompassSense = 901,
LorePlains = 902,
LoreForest = 903,
LoreWetlands = 904,
LoreMountains = 905,
LoreTundra = 906,
```

---

## Phase 2: Register in Catalog

### `src/skill_defs.cpp`
Add new category to the `skill_catalog()` static vector:
```cpp
{SkillId::Cat_Wayfinding, "Wayfinding",
 "Knowledge of navigation, terrain, and survival in the wild. "
 "Reduces the chance of getting lost and improves overland travel.", 75, {
    {SkillId::CampMaking, "Camp Making",
     "Set up a makeshift camp with a fire for resting and cooking.",
     false, 50, 12, "Intelligence"},
    {SkillId::CompassSense, "Compass Sense",
     "Your natural sense of direction is sharpened. Grace period before "
     "regaining bearings is halved and recovery rate doubled.",
     true, 50, 13, "Intelligence"},
    {SkillId::LorePlains, "Terrain Lore: Plains",
     "Familiarity with open terrain. 50% less likely to get lost on "
     "plains and desert. Travel time halved.",
     true, 50, 12, "Intelligence"},
    {SkillId::LoreForest, "Terrain Lore: Forest",
     "You read forest trails instinctively. 50% less likely to get lost "
     "in forests and fungal growth. Travel time halved.",
     true, 50, 13, "Intelligence"},
    {SkillId::LoreWetlands, "Terrain Lore: Wetlands",
     "Navigating bogs and waterways comes naturally. 50% less likely to "
     "get lost in swamps, rivers, and lakeside. Travel time halved.",
     true, 75, 14, "Intelligence"},
    {SkillId::LoreMountains, "Terrain Lore: Mountains",
     "Highland pathfinding expertise. 50% less likely to get lost in "
     "mountains and craters. Travel time halved.",
     true, 75, 15, "Intelligence"},
    {SkillId::LoreTundra, "Terrain Lore: Tundra",
     "Survival knowledge for extreme environments. 50% less likely to "
     "get lost on ice fields and volcanic terrain. Travel time halved.",
     true, 75, 15, "Intelligence"},
}},
```

---

## Phase 3: Hook into Lost Mechanic

### `src/game_world.cpp` — `get_lost_chance()`

Currently returns flat 15%. Add terrain-aware skill checks:

```cpp
int Game::get_lost_chance(Tile terrain) const {
    int base = 15;
    // Category unlock gives small flat reduction
    if (player_has_skill(player_, SkillId::Cat_Wayfinding)) base -= 2;
    // Terrain lore skills halve chance for matching terrain
    bool has_lore = false;
    switch (terrain) {
        case Tile::OW_Plains: case Tile::OW_Desert:
            has_lore = player_has_skill(player_, SkillId::LorePlains); break;
        case Tile::OW_Forest: case Tile::OW_Fungal:
            has_lore = player_has_skill(player_, SkillId::LoreForest); break;
        case Tile::OW_Swamp: case Tile::OW_River: case Tile::OW_Lake:
            has_lore = player_has_skill(player_, SkillId::LoreWetlands); break;
        case Tile::OW_Mountains: case Tile::OW_Crater:
            has_lore = player_has_skill(player_, SkillId::LoreMountains); break;
        case Tile::OW_IceField: case Tile::OW_LavaFlow:
            has_lore = player_has_skill(player_, SkillId::LoreTundra); break;
        default: break;
    }
    if (has_lore) base /= 2;
    return std::max(base, 1);
}
```

### `src/game_world.cpp` — `regain_chance()`

Currently: 30-move grace, 1% per 3 moves, cap 25%. With Compass Sense:

```cpp
int Game::regain_chance() const {
    int grace = 30;
    int ramp_divisor = 3;
    int cap = 25;
    if (player_has_skill(player_, SkillId::CompassSense)) {
        grace = 15;        // halved
        ramp_divisor = 2;  // faster ramp (~1.5x)
        cap = 40;          // higher cap
    }
    if (lost_moves_ < grace) return 0;
    int chance = (lost_moves_ - grace) / ramp_divisor;
    return std::min(chance, cap);
}
```

---

## Phase 4: Hook into Travel Time

### `src/game_interaction.cpp` — overworld movement (try_move)

Currently `advance_world(15)`. With terrain lore:

```cpp
int travel_cost = 15;
Tile stepped = world_.map().get(nx, ny);
// Terrain lore halves travel time
bool has_lore = false;
switch (stepped) { /* same terrain→skill mapping as get_lost_chance */ }
if (has_lore) travel_cost /= 2;
advance_world(travel_cost);
```

### `src/game_world.cpp` — `transition_detail_edge()`

Zone transitions have 5 (intra-tile) or 15 (cross-tile) tick cost. Apply same terrain lore halving to cross-tile transitions. Intra-tile stays at 5 (already fast).

Extract the terrain-to-lore check into a shared helper to avoid duplication:

### `include/astra/skill_defs.h` or inline in game
```cpp
bool has_terrain_lore(const Player& player, Tile terrain);
```

---

## Phase 5: Update Formulas Doc

### `docs/formulas.md`
Add Wayfinding section documenting all modifiers.

---

## Files Modified

| File | Changes |
|------|---------|
| `include/astra/skill_defs.h` | Add 7 new SkillIds (Cat_Wayfinding + 6 skills) |
| `src/skill_defs.cpp` | Register Wayfinding category in catalog |
| `src/game_world.cpp` | Modify get_lost_chance(), regain_chance(), transition_detail_edge() |
| `src/game_interaction.cpp` | Terrain lore halves overworld travel ticks |
| `docs/formulas.md` | Document Wayfinding modifiers |

---

## Verification

1. Learn Terrain Lore: Plains → walk on plains → lost chance reduced (test with `give sp 500`)
2. Get lost → learn Compass Sense → bearings regain faster (grace 15 instead of 30)
3. Walk on forest without Forest Lore → 15 ticks. Learn it → 7 ticks
4. Category unlock alone gives small lost chance reduction
5. Camp Making shows in ability bar but does nothing yet (placeholder)
6. Skills appear in character screen Skills tab with correct INT requirements
7. Save/load preserves learned wayfinding skills (existing SkillId serialization handles this)
