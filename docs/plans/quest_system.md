# Quest System — Full Technical Design

## Overview

Two quest types:
1. **Story quests** — code-defined, hand-crafted quest lines with world-modifying events
2. **Random quests** — template-based (kill/fetch/deliver/scout), generated from world state

A **QuestManager** class tracks active/completed quests, objectives, and rewards. Quests can trigger world modifications: spawning special dungeons, NPCs, items, and overworld POIs.

---

## Data Model (implemented in Phase 1)

### Quest struct (`include/astra/quest.h`)

```cpp
struct Quest {
    std::string id;                       // unique identifier
    std::string title;                    // "The Missing Hauler"
    std::string description;              // full description
    std::string giver_npc;                // NPC who gave it
    QuestStatus status;                   // Available/Active/Completed/Failed
    std::vector<QuestObjective> objectives;
    QuestReward reward;
    bool is_story = false;
    int accepted_tick = 0;
};
```

### QuestObjective

```cpp
enum class ObjectiveType { KillNpc, GoToLocation, CollectItem, TalkToNpc, DeliverItem };

struct QuestObjective {
    ObjectiveType type;
    std::string description;   // "Kill 3 Xytomorphs"
    int target_count = 1;
    int current_count = 0;
    std::string target_id;     // NPC role, item name, location name
};
```

### QuestReward

```cpp
struct QuestReward {
    int xp = 0;
    int credits = 0;
    int skill_points = 0;
    std::string item_name;         // specific item reward
    std::string faction_name;      // faction affected
    int reputation_change = 0;
};
```

---

## QuestManager (`include/astra/quest.h`)

Tracks all quest state. Game owns a `QuestManager quest_manager_`.

### Lifecycle
- `accept_quest(Quest, world_tick)` — marks active, records tick
- `complete_quest(quest_id, Player&)` — applies rewards, moves to completed
- `fail_quest(quest_id)` — marks failed, moves to completed

### Progress Tracking (called by game systems)
- `on_npc_killed(npc_role)` — increments KillNpc objectives
- `on_item_picked_up(item_name)` — increments CollectItem objectives
- `on_location_entered(location_name)` — completes GoToLocation objectives
- `on_npc_talked(npc_name)` — completes TalkToNpc objectives

### Integration Points
- **Combat** (`game_combat.cpp`): calls `on_npc_killed` after NPC death
- **Items** (`game_interaction.cpp`): calls `on_item_picked_up` after pickup
- **World transitions** (`game_world.cpp`): calls `on_location_entered` on map entry
- **Dialog** (`dialog_manager.cpp`): calls `on_npc_talked` when talking to quest NPCs

---

## Quest-Triggered World Modification

### The Problem

When a quest is accepted, the world needs to react:
- A specialized cave spawns with specific enemies
- A quest item appears in a dungeon
- A new POI appears on the overworld
- Quest-specific NPCs populate a location

### Architecture: QuestLocationMeta

Store quest-specific generation parameters alongside the location cache:

```cpp
struct QuestLocationMeta {
    std::string quest_id;
    int difficulty_override = -1;          // -1 = use default
    std::vector<std::string> npc_roles;    // specific NPCs to spawn
    std::vector<std::string> quest_items;  // items to place on ground
    Tile poi_type = Tile::Empty;           // overworld stamp to place
    bool remove_on_completion = false;     // clean up after quest done
};
```

Add to WorldManager:
```cpp
std::map<LocationKey, QuestLocationMeta> quest_locations_;
```

### Pipeline: Quest Accept → World Spawn

```
1. Player accepts quest via dialog
   ↓
2. StoryQuest::on_accepted(Game& game) called
   ↓
3. Quest logic determines WHERE to spawn content:
   - Pick a system/body/overworld tile
   - Or use a specific known location
   ↓
4. Register quest location:
   world_.quest_locations_[key] = QuestLocationMeta{...}
   ↓
5. Optionally modify overworld:
   - Place a new POI stamp on the overworld tile
   - Set tile to OW_CaveEntrance / OW_Ruins / new quest tile type
   ↓
6. When player enters that location:
   enter_detail_map() / enter_dungeon_from_detail() checks quest_locations_
   ↓
7. If quest metadata exists for this key:
   - Override MapProperties (difficulty, loot_tier)
   - Call quest-specific NPC spawner instead of debug_spawn()
   - Place quest items on the ground
   ↓
8. On quest completion:
   StoryQuest::on_completed(Game& game)
   - Remove quest_locations_ entry
   - Optionally clear location_cache to reset the area
```

### Injection Points in Existing Code

**Overworld modification** (`game_world.cpp` — enter_overworld_tile):
```cpp
// After overworld generation, check for quest POIs
for (auto& [key, meta] : world_.quest_locations_) {
    if (key matches current system/body && meta.poi_type != Tile::Empty) {
        // Place stamp on overworld at the target coordinates
    }
}
```

**Detail map generation** (`game_world.cpp` — enter_detail_map):
```cpp
auto it = world_.quest_locations_.find(detail_key);
if (it != world_.quest_locations_.end()) {
    auto& meta = it->second;
    if (meta.difficulty_override >= 0) props.difficulty = meta.difficulty_override;
    // After generation: spawn quest NPCs/items
}
```

**Dungeon generation** (`game_world.cpp` — enter_dungeon_from_detail):
```cpp
auto it = world_.quest_locations_.find(dungeon_key);
if (it != world_.quest_locations_.end()) {
    // Use quest-specific NPC spawner
    spawn_quest_npcs(map, npcs, player, it->second, rng);
    // Place quest items
    for (const auto& item_name : it->second.quest_items) {
        place_quest_item(map, ground_items, item_name, rng);
    }
}
```

### Quest NPC Spawner

```cpp
void spawn_quest_npcs(TileMap& map, std::vector<Npc>& npcs,
                      int px, int py,
                      const QuestLocationMeta& meta,
                      std::mt19937& rng) {
    std::vector<std::pair<int,int>> occupied = {{px, py}};
    for (const auto& role : meta.npc_roles) {
        Npc npc = create_quest_npc(role, rng);
        if (map.find_open_spot_other_room(px, py, npc.x, npc.y, occupied, &rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
        }
    }
}
```

---

## Story Quests (Code-Defined)

### Base Class

```cpp
class StoryQuest {
public:
    virtual ~StoryQuest() = default;
    virtual Quest create_quest() = 0;
    virtual void on_accepted(Game& game) {}
    virtual void on_completed(Game& game) {}
    virtual void on_failed(Game& game) {}
};
```

### Example: "The Missing Hauler"

```cpp
class MissingHaulerQuest : public StoryQuest {
    Quest create_quest() override {
        Quest q;
        q.id = "story_missing_hauler";
        q.title = "The Missing Hauler";
        q.description = "A cargo hauler went dark near the asteroid belt...";
        q.is_story = true;

        // Objective 1: travel to the derelict
        q.objectives.push_back({ObjectiveType::GoToLocation,
            "Find the missing hauler", 1, 0, "Derelict Hauler"});
        // Objective 2: recover the cargo
        q.objectives.push_back({ObjectiveType::CollectItem,
            "Recover the cargo manifest", 1, 0, "Cargo Manifest"});
        // Objective 3: return to station keeper
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report back to the Station Keeper", 1, 0, "Station Keeper"});

        q.reward.xp = 200;
        q.reward.credits = 100;
        q.reward.faction_name = "Stellari Conclave";
        q.reward.reputation_change = 10;
        return q;
    }

    void on_accepted(Game& game) override {
        // Spawn a derelict station dungeon on a nearby asteroid
        // 1. Find a suitable asteroid body in the current system
        // 2. Register QuestLocationMeta with derelict-specific NPCs
        // 3. Place marker on star chart
    }

    void on_completed(Game& game) override {
        // Clean up quest dungeon
        // Unlock follow-up quest
    }
};
```

### Story Quest Registry

```cpp
const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog();
StoryQuest* find_story_quest(const std::string& id);
```

---

## Random Quest Templates

### Template Types

| Template | Objective | Example |
|----------|-----------|---------|
| **Kill** | KillNpc × N | "Eliminate 3 Xytomorphs in the caves" |
| **Fetch** | CollectItem × N | "Retrieve 2 Power Cores" |
| **Deliver** | DeliverItem to NPC | "Bring cargo to the Arms Dealer" |
| **Scout** | GoToLocation | "Explore the asteroid surface" |

### Generation from World State

```cpp
Quest generate_random_quest(const WorldManager& world, std::mt19937& rng) {
    // 1. Pick template based on weights
    // 2. Fill targets from available world data:
    //    - Kill: enemy types present in nearby dungeons
    //    - Fetch: crafting materials or common item types
    //    - Deliver: pick an NPC from the current station
    //    - Scout: pick an unexplored body in the star chart
    // 3. Scale rewards by difficulty/distance
}
```

### NPC Quest Offering

NPCs with `QuestTrait` can offer random quests:
- Station Keeper: scout/deliver quests
- Merchants: fetch/deliver quests
- Military NPCs: kill quests

When player selects the quest dialog option:
1. QuestManager generates a random quest
2. Dialog shows quest description + accept/decline choices
3. On accept: `quest_manager_.accept_quest(quest)`
4. If story quest: `story_quest->on_accepted(game)` triggers world changes

---

## Save/Load

### Quest Serialization (`save_file.cpp`)

```cpp
// Write
w.write_u32(quest_manager.active_quests().size());
for (const auto& q : quest_manager.active_quests()) {
    w.write_string(q.id);
    w.write_string(q.title);
    w.write_string(q.description);
    w.write_string(q.giver_npc);
    w.write_u8(static_cast<uint8_t>(q.status));
    w.write_u32(q.objectives.size());
    for (const auto& obj : q.objectives) {
        w.write_u8(static_cast<uint8_t>(obj.type));
        w.write_string(obj.description);
        w.write_i32(obj.target_count);
        w.write_i32(obj.current_count);
        w.write_string(obj.target_id);
    }
    // ... reward fields, is_story, accepted_tick
}
// Same for completed quests

// QuestLocationMeta also needs serialization
w.write_u32(world.quest_locations().size());
for (const auto& [key, meta] : world.quest_locations()) {
    // Write key tuple + meta fields
}
```

### Load
- Read quests into quest_manager
- Read quest_locations into world_manager
- Story quests don't need re-triggering (world state is cached)

---

## Character Screen — Quests Tab (implemented in Phase 1)

Renders:
- Active quests with title, description, objectives (checkbox + progress bar)
- Reward summary per quest
- Completed quests section at bottom

---

## Implementation Phases

### Phase 1: Core ✅ (done)
- Quest/QuestObjective/QuestReward structs
- QuestManager with accept/complete/tracking
- on_npc_killed wired into combat
- Quests tab renders active quests
- Dev console quest commands

### Phase 2: Quest-Triggered World Modification ✅ (done)
- QuestLocationMeta struct in WorldManager
- Injection hooks in enter_dungeon_from_detail
- Quest-specific NPC spawner (create_npc_by_role)
- Quest item placement
- Visual quest markers on star chart (galaxy/region/local/system views)
- Quest marker glyph (SG_QuestMarker) on overworld
- BrightYellow color for quest markers

### Phase 3: Story Quest Framework ✅ (done)
- StoryQuest base class with on_accepted/on_completed
- Story quest registry
- First story quest: "The Missing Hauler"
- Dialog integration for quest acceptance + turn-in
- on_item_picked_up and on_npc_talked wired in

### Phase 4: Random Quest Generation ✅ (done)
- Template system: kill, fetch, deliver, scout
- generate_quest_for_role() picks template by NPC role
- Merchant and Drifter NPCs now offer quests via dialog
- Generic quest acceptance in dialog system (story + random)
- Dev console: quest deliver, quest scout commands

### Phase 5: Save/Load + Polish ✅ (done)
- Quest serialization (QUST section in save file, version 13)
- Quest location meta serialization (LocationKey + QuestLocationMeta)
- QuestManager::restore() for loading without re-applying rewards
- Quest completion notifications in game log (colored messages)

---

## File Map

| File | Purpose |
|------|---------|
| `include/astra/quest.h` | Quest structs, QuestManager, StoryQuest base ✅ |
| `src/quest.cpp` | QuestManager impl, random generators ✅ |
| `include/astra/world_manager.h` | QuestLocationMeta, quest_locations_ map |
| `src/game_world.cpp` | Injection hooks in enter_detail/dungeon |
| `src/game_combat.cpp` | on_npc_killed hook ✅ |
| `src/game_interaction.cpp` | on_item_picked_up hook |
| `src/dialog_manager.cpp` | Quest acceptance from dialog |
| `src/character_screen.cpp` | Quests tab rendering ✅ |
| `src/save_file.cpp` | Quest serialization ✅ |
| `src/quests/` | Individual story quest classes (future) |
