# Quest System Analysis & Recommendations

**Date:** 2026-04-04  
**Status:** Planning document — no implementation yet

## Current State

The quest system has solid bones: QuestManager lifecycle, StoryQuest virtual base, 6 objective types, distance-scaled rewards, QuestLocationMeta for world modification, star chart markers, and full save/load. Two story quests and four random templates exist.

The hardcoded approach is intentional — single binary, no external data files. This document works within that constraint.

## The Problem With Current "Random" Quests

The random quest generator picks from fixed arrays and rolls dice on counts. Every kill quest targets Xytomorphs. Every fetch quest wants Power Cores or Circuit Boards. After a few hours of play, the player has seen every possible combination. The quests feel like Mad Libs with a small word bank.

True randomness requires the system to compose quests from world state rather than templates.

---

## Recommendation 1: World-Driven Quest Generation

**Priority:** High  
**Effort:** Medium  
**Impact:** Transforms quest variety from ~20 combinations to effectively infinite

### Concept

Instead of picking from hardcoded arrays, the quest generator should query the actual world state:

- **Kill targets** — scan NPCs that exist on nearby bodies. If there are Xytomorphs on asteroid Krell-4, generate a kill quest for Xytomorphs on Krell-4. If a derelict station has hostile drifters, generate a bounty quest for them.
- **Fetch items** — scan what items are available in the game's item database, filtered by what makes sense for the quest giver's role. A merchant wants trade goods. An engineer wants components.
- **Scout locations** — already partially world-driven (uses real body names). Extend to reference specific POI types discovered on overworld maps.
- **Delivery targets** — pick a real NPC on a real station in a nearby system as the recipient. The player must actually find and talk to them.

### Key Insight

The world already has all the data needed. Star systems have bodies with biomes, danger levels, and NPC populations. The item database has categories. NPC roles are enumerated. The quest generator just needs to read this data instead of ignoring it.

### Sketch

```
generate_kill_quest(world_state, nearby_systems):
    for each nearby system:
        for each body with hostiles:
            candidate = {system, body, npc_role, estimated_count}
    pick candidate weighted by distance/danger
    build quest targeting that specific location and NPC type
```

This means a kill quest might say "Clear the Xytomorph nest on Krell-4 in the Vega system" — a real place with real enemies the player can navigate to.

---

## Recommendation 2: Quest Composition System

**Priority:** High  
**Effort:** Medium  
**Impact:** Enables multi-step procedural quests

### Concept

Currently all random quests have exactly one objective. Story quests have multiple objectives but are hand-authored. A composition system would let the generator chain objectives into multi-step quests procedurally.

### Quest Patterns (composable templates)

**Investigate Pattern:**
1. GoToLocation (travel to the site)
2. KillNpc or CollectItem (deal with what's there)
3. TalkToNpc (report back)

**Supply Run Pattern:**
1. CollectItem (gather materials — could be bought or found)
2. DeliverItem to NPC at destination
3. TalkToNpc (confirm delivery)

**Bounty Pattern:**
1. GoToLocation (find the target)
2. KillNpc (eliminate them)
3. CollectItem (proof of kill — trophy, dog tag, etc.)
4. TalkToNpc (collect bounty)

**Escort/Courier Pattern:**
1. TalkToNpc (pick up cargo/info at origin)
2. GoToLocation (travel to destination)
3. TalkToNpc (deliver at destination)

### Implementation Approach

Define patterns as sequences of objective templates. Each template has a slot type (location, npc_role, item_name) that gets filled by world-state queries during generation. The existing objective types already support everything — the generator just needs to produce multi-objective quests.

```cpp
struct QuestPattern {
    const char* name_template;  // "Bounty: {npc_role} on {body_name}"
    struct Step {
        ObjectiveType type;
        const char* desc_template;  // "Eliminate {count} {npc_role}"
        // slot bindings: which world-query fills target_id, count, etc.
    };
    std::vector<Step> steps;
};
```

No external files — patterns are constexpr arrays compiled into the binary.

---

## Recommendation 3: Contextual Quest Flavor

**Priority:** Medium  
**Effort:** Low  
**Impact:** Makes identical quest structures feel different

### Concept

The same "kill 3 hostiles" quest should read differently depending on who gives it and why. A Station Keeper frames it as security. A Drifter frames it as revenge. A Merchant frames it as protecting trade routes.

### Implementation

Each NPC role gets a set of quest flavor pools — intro text, objective descriptions, completion text. These are small string arrays (still compiled in, no data files) but provide narrative variety.

```cpp
struct QuestFlavor {
    const char* title_templates[4];   // "Clear {location}", "Purge {location}", etc.
    const char* intro_lines[3];       // "These creatures are bad for business..."
    const char* complete_lines[3];    // "The route is safe again. Well done."
};
```

Even 3-4 variants per field creates combinatorial variety. With world-driven targets (Rec 1) and multi-step patterns (Rec 2), the same underlying quest type produces hundreds of distinct-feeling quests.

---

## Recommendation 4: NPC Role Quest Expansion

**Priority:** Medium  
**Effort:** Low  
**Impact:** More quest sources, more natural interactions

### Current State

Only 3 NPC roles offer quests: Station Keeper, Merchant, Drifter. But the game has many more roles: Engineer, Commander, Medic, ArmsDealer, Astronomer, Nova.

### Proposed Role-Quest Mappings

| Role | Quest Types | Flavor |
|------|------------|--------|
| Station Keeper | Scout, Deliver, Story | Security and administration |
| Merchant | Fetch, Deliver | Trade and supply |
| Drifter | Kill, Scout | Survival and rumors |
| **Engineer** | Fetch (components), Scout (salvage sites) | Technical recovery |
| **Commander** | Kill (bounties), Scout (recon) | Military operations |
| **Medic** | Fetch (medical supplies), Deliver (medicine to outpost) | Humanitarian |
| **ArmsDealer** | Kill (field test weapons), Fetch (rare materials) | Combat and arms trade |
| **Astronomer** | Scout (specific stellar phenomena), GoTo (observation points) | Scientific exploration |

### Implementation

Extend the role-to-quest-type mapping in `generate_quest_for_role()`. Each new role just needs its own case with appropriate quest type weights. The quest patterns and world-driven generation handle the rest.

---

## Recommendation 5: Story Quest Chains

**Priority:** High  
**Effort:** Medium  
**Impact:** Narrative depth, the backbone of Astra's story progression

### Concept

The most important quests in Astra should be hand-tailored multi-quest story lines. Quest A completes and unlocks Quest B, which unlocks Quest C, and so on. Each quest in a chain builds on the previous — same characters, escalating stakes, evolving narrative.

These are always hand-authored. They are the primary vehicle for telling Astra's story, revealing world lore, and driving the player toward Sgr A*. Random quests fill the gaps, but story chains are the spine.

### Examples

**The Hauler Arc (extending the existing quest):**
1. The Missing Hauler — investigate derelict, find cargo manifest
2. Track the Signal — the manifest reveals a pirate transmission source
3. The Pirate Outpost — assault the base, discover they're excavating ancient ruins
4. What They Found — descend into the ruins, encounter precursor technology

**The ARIA Arc (ship AI storyline):**
1. Getting Airborne — repair ship systems (existing tutorial)
2. ARIA's Memory — ARIA recovers fragmented data about her origin
3. The Ghost Ship — track down the vessel ARIA was originally installed on
4. Core Directive — ARIA reveals her true purpose, player makes a choice

**The Beacon Arc (main story toward Sgr A*):**
1. The First Beacon — discover an ancient navigation beacon
2. The Network — find connecting beacons, learn the pattern
3. ... (escalating toward galactic center)

### Data Model

```cpp
struct Quest {
    // ... existing fields ...
    std::string chain_id;               // groups quests in a storyline (e.g. "hauler_arc")
    int chain_index = 0;                // position in chain (1, 2, 3...)
    int chain_length = 0;               // total quests in chain (for UI: "Part 2 of 4")
    std::string prerequisite_quest_id;  // must be completed before this quest appears
    std::string next_quest_id;          // quest to make available on completion
};
```

### StoryQuest Chain Pattern

The existing `StoryQuest` base class needs a small extension:

```cpp
class StoryQuest {
public:
    virtual Quest create_quest() = 0;
    virtual void on_accepted(Game& game) {}
    virtual void on_completed(Game& game) {}
    virtual void on_failed(Game& game) {}
    
    // Chain support
    virtual std::string chain_id() const { return ""; }
    virtual std::string next_quest_id() const { return ""; }
};
```

When a chained story quest completes, the system automatically makes the next quest in the chain available — either immediately offered by the same NPC, or discoverable at a new location set up by `on_completed()`.

### Chain Lifecycle

```
Quest A available (no prereq, or game start)
  → Player accepts and completes Quest A
  → on_completed() sets up the world for Quest B
  → Quest B becomes available (from same NPC, or new NPC, or location-triggered)
  → Player accepts and completes Quest B
  → on_completed() sets up Quest C
  → ... continues until chain ends
```

### UI Integration

- Quest log shows chain context: "The Missing Hauler (Part 1 of 4 — The Hauler Arc)"
- Completed chain quests remain visible in journal under the chain heading
- Star chart markers show the next chain quest's location when available

### Key Design Rule

Story chains are never procedurally generated. Every quest in a chain is a hand-written `StoryQuest` subclass with authored dialog, custom world modifications, and deliberate narrative beats. The randomness comes from *which* systems and bodies the chain uses (selected during `on_accepted()`), not from the story itself.

### Secondary: Procedural Mini-Chains

As a separate, lower-priority feature, the random quest generator could occasionally produce 2-3 step mini-chains (kill → scout → clear). These are not story quests — they're procedural content that mimics chain structure. This is nice-to-have, not essential.

---

## Recommendation 6: Quest Failure & Consequences

**Priority:** Low  
**Effort:** Low  
**Impact:** Stakes and tension

### Current State

`fail_quest()` exists but nothing calls it. Quests are immortal — they sit in the active list forever with no pressure.

### Proposed Failure Conditions

- **Expiration:** Some quests have a tick deadline. "Deliver this medical supply within 500 ticks." The generator sets `expiry_tick = accepted_tick + N` based on distance.
- **Kill target death:** If the specific NPC a bounty targets dies before the player reaches them (killed by another NPC, despawned), the quest fails.
- **Quest giver death:** If the NPC who gave the quest dies, the quest fails (no one to turn in to). This adds weight to protecting friendly NPCs.

### Consequences

- Failed quests reduce faction reputation slightly (-2 to -5)
- Failed quests are logged in journal with reason
- Some NPCs remember failures and are less likely to offer quests again (reputation gate)

---

## Recommendation 7: Reward Variety

**Priority:** Low  
**Effort:** Low  
**Impact:** More interesting quest motivation

### Current State

Rewards are XP + credits + optional faction rep. The `skill_points` field exists but is never used. No item rewards.

### Proposed Additions

- **Item rewards:** Quest completion grants a specific item. The generator picks from loot tables appropriate to quest difficulty. Could be a weapon, component, or rare material.
- **Skill point rewards:** Higher-difficulty quests award 10-25 SP, giving an alternative to grinding levels.
- **Ship component rewards:** Engineer quests could reward ship upgrades directly.
- **Map reveals:** Astronomer quests could reveal unexplored systems on the star chart.
- **Unique rewards:** Story quest chains culminate in unique items not available anywhere else.

### Data Model Addition

```cpp
struct QuestReward {
    // ... existing fields ...
    std::string item_def_id;    // item to grant (empty = none)
    int item_count = 0;
};
```

---

## Recommendation 8: DeliverItem Objective (Wire Up Dead Code)

**Priority:** Low  
**Effort:** Low  
**Impact:** Enables courier/trade quests properly

### Current State

`ObjectiveType::DeliverItem` is defined in the enum but never generated and has no tracking callback. Deliver quests currently use `TalkToNpc` as a workaround.

### Proposed Implementation

Add `on_item_delivered(const std::string& item_name, const std::string& npc_role)` callback to QuestManager. Triggered when the player gives an item to an NPC during dialog. This enables proper courier quests where the player must carry a specific item to a specific NPC.

The dialog system already has trade/give mechanics. The callback just needs to be wired in.

---

## Implementation Priority Order

If implementing these, this order maximizes value at each step:

1. **Story Quest Chains (Rec 5)** — the narrative backbone. Enables multi-quest story arcs that drive the player forward. Must be in place before writing more story content.
2. **World-Driven Generation (Rec 1)** — biggest bang for random quests. Makes every procedural quest feel unique because targets and locations are real.
3. **NPC Role Expansion (Rec 4)** — low effort, more quest sources. Works with existing generation.
4. **Contextual Flavor (Rec 3)** — low effort, narrative variety. String arrays.
5. **Quest Composition (Rec 2)** — multi-step random quests feel more substantial. Builds on Rec 1.
6. **Reward Variety (Rec 7)** — more interesting motivation, unique items for chain endings.
7. **Quest Failure (Rec 6)** — adds stakes once there are enough quests to lose.
8. **DeliverItem (Rec 8)** — wire up existing dead code.

---

## What Not To Do

- **Don't add external data files** — the single-binary approach is a feature, not a limitation. Compile-time quest content is simpler to test and deploy.
- **Don't build a scripting engine** — Lua/embedded scripting adds complexity for content that can be expressed as compiled patterns + world queries.
- **Don't over-abstract** — the current StoryQuest virtual base is the right level of abstraction. Don't add a quest DSL or visual editor.
- **Don't optimize prematurely** — the O(N*M) objective scanning is fine with <50 active quests. Only optimize if profiling shows it matters.

---

## Summary

The quest system's architecture is sound. The weakness is that random quests don't use the rich world state that already exists. The single biggest improvement is making the generator query actual world data (systems, bodies, NPCs, items) instead of picking from tiny hardcoded arrays. Combined with composable multi-step patterns and NPC role expansion, this would create a system where every playthrough produces genuinely different quest content without any external data files.
