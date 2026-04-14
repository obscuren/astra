# World Lore Generation — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate billions of years of procedural galactic history (epochs, civilizations, events, key figures, legendary artifacts) from the world seed, display it in a dev-mode history viewer, and persist it across save/load.

**Architecture:** A `LoreGenerator` produces a `WorldLore` data structure from the game seed before galaxy generation. `WorldLore` contains `Epoch` records, each with a `Civilization`, timeline of `LoreEvent`s, `KeyFigure`s, and `LoreArtifact`s. A `NameGenerator` uses curated phoneme pools to produce consistent names per civilization. The dev console gains a `history` command that renders the full timeline. `WorldLore` is stored in `WorldManager` and serialized with saves.

**Tech Stack:** C++20, seed-based deterministic generation via `std::mt19937`

**Spec:** `docs/world_lore_generation.md`

**Phase scope:** Core lore engine + dev viewer. Galaxy integration (tiers, POI placement, dungeon theming) is Phase 2.

---

## File Structure

| File | Responsibility |
|------|---------------|
| `include/astra/lore_types.h` | All lore data structures: Epoch, Civilization, LoreEvent, KeyFigure, LoreArtifact, WorldLore |
| `include/astra/name_generator.h` | Phoneme pool definitions and name generation API |
| `src/name_generator.cpp` | Curated syllable banks, name composition logic |
| `include/astra/lore_generator.h` | LoreGenerator class declaration |
| `src/lore_generator.cpp` | Epoch simulation: civilization generation, event chains, figure/artifact creation |
| `src/lore_serialization.cpp` | WorldLore binary read/write for save files |
| Modify: `include/astra/world_manager.h` | Add `WorldLore` member + accessor |
| Modify: `src/game.cpp` | Call lore generation before galaxy generation |
| Modify: `src/dev_console.cpp` | Add `history` command |
| Modify: `src/save_system.cpp` | Serialize/deserialize WorldLore |
| Modify: `include/astra/save_file.h` | Add WorldLore to SaveData, bump version |
| Modify: `CMakeLists.txt` | Add new source files |

---

### Task 1: Lore Data Structures

Define all the types that represent generated history. No logic — just structs and enums.

**Files:**
- Create: `include/astra/lore_types.h`

- [ ] **Step 1: Create lore_types.h**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

// ── Phoneme pool identity ──
enum class PhonemePool : uint8_t {
    Sharp,      // crystalline/sharp: Keth, Vor, Thex
    Flowing,    // flowing/ancient: Ael, Vyn, Osa
    Guttural,   // guttural/deep: Groth, Durr, Khar
    Ethereal,   // ethereal/light: Phi, Sei, Lua
    Harmonic,   // harmonic/resonant: Zyn, Mael, Thal
    Staccato,   // clicking/staccato: Kix, Tuk, Prel
};

static constexpr int phoneme_pool_count = 6;

// ── Cultural traits ──
enum class Architecture : uint8_t {
    Crystalline, Organic, Geometric, VoidCarved, LightWoven,
};

enum class TechStyle : uint8_t {
    Gravitational, BioMechanical, QuantumLattice, HarmonicResonance, PhaseShifting,
};

enum class Philosophy : uint8_t {
    Expansionist, Contemplative, Predatory, Symbiotic, Transcendent,
};

// ── Collapse cause ──
enum class CollapseCause : uint8_t {
    War,
    Transcendence,
    Plague,
    ResourceExhaustion,
    SgrAObsession,
    Unknown,
};

// ── Relationship to predecessors ──
enum class PredecessorRelation : uint8_t {
    Revered, Exploited, Feared, Ignored,
};

// ── Relationship to Sgr A* ──
enum class SgrARelation : uint8_t {
    ReachedAndVanished,
    TriedAndFailed,
    BuiltToward,
    FledFrom,
    Unaware,
};

// ── Event types ──
enum class LoreEventType : uint8_t {
    Emergence,
    RuinDiscovery,
    Decipherment,
    ReverseEngineering,
    Colonization,
    HyperspaceRoute,
    MegastructureBuilt,
    FirstContact,
    CivilWar,
    ResourceWar,
    BorderConflict,
    ArtifactWar,
    SgrADetection,
    ConvergenceDiscovery,
    PrecursorBreakthrough,
    ArtifactCreation,
    Transcendence,
    SelfDestruction,
    Consumption,
    Fragmentation,
    CollapseUnknown,
    VaultSealed,
    GuardianCreated,
    ArtifactScattered,
    WarningEncoded,
};

// ── Key figure archetypes ──
enum class FigureArchetype : uint8_t {
    Founder,
    Conqueror,
    Sage,
    Traitor,
    Explorer,
    Last,
    Builder,
};

// ── Artifact categories ──
enum class ArtifactCategory : uint8_t {
    Weapon,
    NavigationTool,
    KnowledgeStore,
    Key,
    Anomaly,
};

// ── A single event in the timeline ──
struct LoreEvent {
    LoreEventType type;
    float time_bya;             // billions of years ago
    std::string description;    // e.g. "First Aelithae consciousness on Lerimund Prime"
    uint32_t system_id = 0;     // associated star system (0 = none)
};

// ── A notable historical figure ──
struct KeyFigure {
    std::string name;
    std::string title;          // e.g. "Archon", "the Builder"
    FigureArchetype archetype;
    std::string achievement;    // one sentence
    uint32_t system_id = 0;     // associated location
    int artifact_index = -1;    // index into civilization's artifacts (-1 = none)
    std::string fate;           // e.g. "entered Sgr A*", "died at the Spire", "unknown"
};

// ── A legendary artifact ──
struct LoreArtifact {
    std::string name;
    ArtifactCategory category;
    std::string origin_text;    // 1-2 sentences on who made it and why
    std::string effect_text;    // gameplay effect description
    uint32_t system_id = 0;     // where it ended up
    int body_index = -1;        // which body (-1 = unknown)
    int figure_index = -1;      // which figure created/carried it
};

// ── A civilization and its epoch ──
struct Civilization {
    std::string name;           // e.g. "The Aelithae Convergence"
    std::string short_name;     // e.g. "Aelithae"
    PhonemePool phoneme_pool;
    Architecture architecture;
    TechStyle tech_style;
    Philosophy philosophy;

    PredecessorRelation predecessor_relation;
    SgrARelation sgra_relation;
    CollapseCause collapse_cause;

    float epoch_start_bya;      // billions of years ago
    float epoch_end_bya;
    uint32_t homeworld_system_id = 0;

    std::vector<LoreEvent> events;
    std::vector<KeyFigure> figures;
    std::vector<LoreArtifact> artifacts;
};

// ── Human epoch ──
struct HumanHistory {
    float arrival_bya;          // when humanity arrived (~0.01 = 10,000 years ago)
    float golden_age_start;
    float fracture_bya;
    std::vector<LoreEvent> events;
    // Factions generated from fracture event
    std::vector<std::string> faction_names;
};

// ── The complete world lore ──
struct WorldLore {
    unsigned seed = 0;
    std::vector<Civilization> civilizations;  // index 0 = Primordials
    HumanHistory humanity;

    // Summary stats for dev log
    int total_beacons = 0;
    int active_beacons = 0;

    bool generated = false;
};

} // namespace astra
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build (header-only, no new cpp files yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/lore_types.h
git commit -m "Add lore data structures — epochs, civilizations, events, figures, artifacts"
```

---

### Task 2: Phoneme-Based Name Generator

Create the syllable pools and name composition logic. Each pool has a distinct linguistic feel. Names for civilizations, figures, locations, and artifacts are all generated from the same pool for consistency.

**Files:**
- Create: `include/astra/name_generator.h`
- Create: `src/name_generator.cpp`
- Modify: `CMakeLists.txt` (add `src/name_generator.cpp` after `src/minimap.cpp`)

- [ ] **Step 1: Create name_generator.h**

```cpp
#pragma once

#include "astra/lore_types.h"

#include <random>
#include <string>

namespace astra {

class NameGenerator {
public:
    explicit NameGenerator(PhonemePool pool);

    // Generate a personal name (1-3 syllables): "Vynosar", "Kethzan"
    std::string name(std::mt19937& rng) const;

    // Generate a place name (2-3 syllables + optional suffix): "Lerimund", "Thexcyra"
    std::string place(std::mt19937& rng) const;

    // Generate a civilization name: "The Aelithae Convergence"
    std::string civilization(std::mt19937& rng) const;

    // Generate an artifact name: "The Ithaemund Lens", "Beacon of Grothkhar"
    std::string artifact(std::mt19937& rng, ArtifactCategory category) const;

    // Generate a title: "Archon", "Warden", "Keeper"
    std::string title(std::mt19937& rng, FigureArchetype archetype) const;

private:
    PhonemePool pool_;
};

} // namespace astra
```

- [ ] **Step 2: Create name_generator.cpp**

```cpp
#include "astra/name_generator.h"

namespace astra {

// ── Syllable pools ──
// Each pool has onsets (consonant clusters), nuclei (vowels), and codas.
// Names are built: onset+nucleus (+coda for heavier syllables).

struct SyllablePool {
    const char* syllables[40];
    int count;
    const char* polity_suffixes[6];
    int polity_count;
};

static const SyllablePool pools[] = {
    // Sharp (crystalline)
    {{"Keth", "Vor", "Thex", "Zan", "Cyr", "Qal", "Drex", "Kryn",
      "Vex", "Zar", "Kyr", "Thul", "Rex", "Pyx", "Xen", "Cryx",
      "Zel", "Tyr", "Nyx", "Kren", "Vyr", "Zex", "Ax", "Kor",
      "Syx", "Dex", "Fyx", "Gyr", "Hex", "Jex", "Lex", "Myx",
      "Prex", "Ryx", "Styx", "Trex", "Wrex", "Bryx", "Klyx", "Skyr"}, 40,
     {"Dominion", "Collective", "Accord", "Lattice", "Matrix", "Nexus"}, 6},

    // Flowing (ancient)
    {{"Ael", "Vyn", "Osa", "Leri", "Ithae", "Myr", "Aelu", "Elyn",
      "Ova", "Syl", "Thae", "Yra", "Ael", "Ila", "Uma", "Rae",
      "Wyn", "Fae", "Nae", "Lyra", "Eira", "Ari", "Elu", "Ova",
      "Sel", "Tha", "Vel", "Wae", "Xae", "Yel", "Zae", "Mael",
      "Rael", "Sael", "Tael", "Vael", "Wael", "Nael", "Kael", "Dael"}, 40,
     {"Convergence", "Ascendancy", "Communion", "Lumen", "Eternity", "Resonance"}, 6},

    // Guttural (deep)
    {{"Groth", "Durr", "Khar", "Mog", "Zhul", "Brek", "Grum", "Drak",
      "Krug", "Murg", "Zhor", "Brug", "Gark", "Durn", "Khor", "Mork",
      "Zug", "Brog", "Gul", "Druk", "Kreg", "Murg", "Zhag", "Brek",
      "Gor", "Dur", "Krog", "Mog", "Zhur", "Brug", "Grak", "Drum",
      "Khar", "Murg", "Zhor", "Brak", "Gul", "Drek", "Kruk", "Mog"}, 40,
     {"Horde", "Dominion", "Throng", "Maw", "Forge", "Bastion"}, 6},

    // Ethereal (light)
    {{"Phi", "Sei", "Lua", "Wen", "Tia", "Nev", "Zhi", "Rei",
      "Mua", "Fen", "Lia", "Sev", "Phi", "Dei", "Kua", "Yen",
      "Via", "Hev", "Shi", "Bei", "Nua", "Len", "Ria", "Tev",
      "Ahi", "Cei", "Dua", "Gen", "Jia", "Kev", "Ohi", "Pei",
      "Qua", "Sen", "Uia", "Vev", "Whi", "Xei", "Yua", "Zen"}, 40,
     {"Chorus", "Whisper", "Drift", "Veil", "Shimmer", "Radiance"}, 6},

    // Harmonic (resonant)
    {{"Zyn", "Mael", "Thal", "Orn", "Vel", "Ryn", "Kael", "Shal",
      "Dyn", "Nael", "Phal", "Urn", "Hel", "Wyn", "Bael", "Chal",
      "Fyn", "Gael", "Jhal", "Lorn", "Mel", "Syn", "Thael", "Vhal",
      "Ayn", "Dael", "Ehal", "Horn", "Iel", "Kyn", "Lael", "Mhal",
      "Nyn", "Oael", "Pyn", "Rael", "Sael", "Tyn", "Uael", "Whal"}, 40,
     {"Harmony", "Symphony", "Resonance", "Accord", "Chorus", "Cadence"}, 6},

    // Staccato (clicking)
    {{"Kix", "Tuk", "Prel", "Chak", "Rik", "Tok", "Drik", "Klak",
      "Pik", "Suk", "Trel", "Vrak", "Bik", "Guk", "Krel", "Nrak",
      "Zik", "Fuk", "Hrel", "Jrak", "Lik", "Muk", "Orel", "Qrak",
      "Wik", "Xuk", "Yrel", "Trik", "Chik", "Duk", "Frel", "Grak",
      "Hik", "Juk", "Krel", "Lrak", "Mik", "Nuk", "Prek", "Srak"}, 40,
     {"Swarm", "Cluster", "Hive", "Pack", "Network", "Array"}, 6},
};

NameGenerator::NameGenerator(PhonemePool pool) : pool_(pool) {}

std::string NameGenerator::name(std::mt19937& rng) const {
    const auto& p = pools[static_cast<int>(pool_)];
    // 1-3 syllables, bias toward 2
    int syllable_count = 2;
    int roll = rng() % 10;
    if (roll < 2) syllable_count = 1;
    else if (roll >= 8) syllable_count = 3;

    std::string result;
    for (int i = 0; i < syllable_count; ++i) {
        result += p.syllables[rng() % p.count];
    }
    // Capitalize first letter, lowercase rest
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(result[0]));
        for (size_t i = 1; i < result.size(); ++i)
            result[i] = static_cast<char>(std::tolower(result[i]));
    }
    return result;
}

std::string NameGenerator::place(std::mt19937& rng) const {
    const auto& p = pools[static_cast<int>(pool_)];
    // Places are 2-3 syllables
    int syllable_count = (rng() % 3 == 0) ? 3 : 2;

    std::string result;
    for (int i = 0; i < syllable_count; ++i) {
        result += p.syllables[rng() % p.count];
    }
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(result[0]));
        for (size_t i = 1; i < result.size(); ++i)
            result[i] = static_cast<char>(std::tolower(result[i]));
    }

    // 30% chance of a suffix like " Prime", " Major"
    static const char* place_suffixes[] = {
        " Prime", " Major", " Reach", " Deep", " Spire",
    };
    if (rng() % 10 < 3) {
        result += place_suffixes[rng() % 5];
    }
    return result;
}

std::string NameGenerator::civilization(std::mt19937& rng) const {
    const auto& p = pools[static_cast<int>(pool_)];
    // "The [name] [polity]"
    std::string civ_name;
    int syllable_count = (rng() % 3 == 0) ? 3 : 2;
    for (int i = 0; i < syllable_count; ++i) {
        civ_name += p.syllables[rng() % p.count];
    }
    if (!civ_name.empty()) {
        civ_name[0] = static_cast<char>(std::toupper(civ_name[0]));
        for (size_t i = 1; i < civ_name.size(); ++i)
            civ_name[i] = static_cast<char>(std::tolower(civ_name[i]));
    }

    std::string polity = p.polity_suffixes[rng() % p.polity_count];
    return "The " + civ_name + " " + polity;
}

std::string NameGenerator::artifact(std::mt19937& rng, ArtifactCategory category) const {
    std::string base = name(rng);

    static const char* weapon_types[] = {"Blade", "Lance", "Edge", "Fang", "Shard"};
    static const char* nav_types[] = {"Lens", "Compass", "Chart", "Key", "Beacon"};
    static const char* knowledge_types[] = {"Codex", "Engram", "Tablet", "Archive", "Crystal"};
    static const char* key_types[] = {"Seal", "Key", "Cipher", "Gate", "Sigil"};
    static const char* anomaly_types[] = {"Anomaly", "Paradox", "Void", "Echo", "Fracture"};

    const char** types;
    int count = 5;
    switch (category) {
        case ArtifactCategory::Weapon:        types = weapon_types; break;
        case ArtifactCategory::NavigationTool: types = nav_types; break;
        case ArtifactCategory::KnowledgeStore: types = knowledge_types; break;
        case ArtifactCategory::Key:            types = key_types; break;
        case ArtifactCategory::Anomaly:        types = anomaly_types; break;
    }

    // "The [Name] [Type]" or "[Type] of [Name]"
    if (rng() % 2 == 0) {
        return "The " + base + " " + types[rng() % count];
    } else {
        return std::string(types[rng() % count]) + " of " + base;
    }
}

std::string NameGenerator::title(std::mt19937& rng, FigureArchetype archetype) const {
    static const char* founder_titles[] = {"Progenitor", "First Speaker", "Awakener", "Prime", "Origin"};
    static const char* conqueror_titles[] = {"Warlord", "Subjugator", "Commander", "Overlord", "Imperator"};
    static const char* sage_titles[] = {"Archon", "Sage", "Oracle", "Luminary", "Seer"};
    static const char* traitor_titles[] = {"the Betrayer", "the Fallen", "the Heretic", "the Defector", "the Lost"};
    static const char* explorer_titles[] = {"Pathfinder", "Voidwalker", "Wayfinder", "Navigator", "Seeker"};
    static const char* last_titles[] = {"the Last", "the Final", "Remnant", "Endkeeper", "the Lone"};
    static const char* builder_titles[] = {"Architect", "Forgemaster", "Artificer", "Shaper", "the Builder"};

    const char** titles;
    switch (archetype) {
        case FigureArchetype::Founder:   titles = founder_titles; break;
        case FigureArchetype::Conqueror: titles = conqueror_titles; break;
        case FigureArchetype::Sage:      titles = sage_titles; break;
        case FigureArchetype::Traitor:   titles = traitor_titles; break;
        case FigureArchetype::Explorer:  titles = explorer_titles; break;
        case FigureArchetype::Last:      titles = last_titles; break;
        case FigureArchetype::Builder:   titles = builder_titles; break;
    }
    return titles[rng() % 5];
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

After `src/minimap.cpp`, add:
```
    src/name_generator.cpp
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/name_generator.h src/name_generator.cpp CMakeLists.txt
git commit -m "Add phoneme-based name generator with 6 syllable pools"
```

---

### Task 3: Lore Generator — Epoch Simulation

The core simulation that produces the full history from a seed. Generates 2-5 precursor civilizations, each with events, figures, and artifacts. Then generates the human epoch.

**Files:**
- Create: `include/astra/lore_generator.h`
- Create: `src/lore_generator.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create lore_generator.h**

```cpp
#pragma once

#include "astra/lore_types.h"
#include "astra/name_generator.h"

#include <random>

namespace astra {

class LoreGenerator {
public:
    // Generate complete world lore from a game seed.
    static WorldLore generate(unsigned game_seed);

    // Format the lore as a human-readable string (for dev log).
    static std::string format_history(const WorldLore& lore);

private:
    static Civilization generate_civilization(
        std::mt19937& rng,
        int epoch_index,
        float epoch_start_bya,
        const std::vector<Civilization>& predecessors);

    static void generate_events(
        std::mt19937& rng,
        Civilization& civ,
        int epoch_index,
        bool has_predecessors);

    static void generate_figures(
        std::mt19937& rng,
        Civilization& civ,
        const NameGenerator& namer);

    static void generate_artifacts(
        std::mt19937& rng,
        Civilization& civ,
        const NameGenerator& namer);

    static HumanHistory generate_human_epoch(
        std::mt19937& rng,
        const std::vector<Civilization>& civilizations);
};

} // namespace astra
```

- [ ] **Step 2: Create lore_generator.cpp**

```cpp
#include "astra/lore_generator.h"

#include <algorithm>
#include <sstream>

namespace astra {

// ── Trait name lookups for format_history ──
static const char* architecture_name(Architecture a) {
    static const char* names[] = {
        "crystalline", "organic", "geometric", "void-carved", "light-woven"};
    return names[static_cast<int>(a)];
}

static const char* tech_name(TechStyle t) {
    static const char* names[] = {
        "gravitational", "bio-mechanical", "quantum-lattice",
        "harmonic-resonance", "phase-shifting"};
    return names[static_cast<int>(t)];
}

static const char* philosophy_name(Philosophy p) {
    static const char* names[] = {
        "expansionist", "contemplative", "predatory", "symbiotic", "transcendent"};
    return names[static_cast<int>(p)];
}

static const char* collapse_name(CollapseCause c) {
    static const char* names[] = {
        "war", "transcendence", "plague",
        "resource exhaustion", "Sgr A* obsession", "unknown"};
    return names[static_cast<int>(c)];
}

static const char* sgra_name(SgrARelation r) {
    static const char* names[] = {
        "reached and vanished", "tried and failed",
        "built toward", "fled from", "unaware"};
    return names[static_cast<int>(r)];
}

static const char* predecessor_name(PredecessorRelation r) {
    static const char* names[] = {"revered", "exploited", "feared", "ignored"};
    return names[static_cast<int>(r)];
}

static const char* event_type_name(LoreEventType t) {
    static const char* names[] = {
        "Emergence", "Ruin Discovery", "Decipherment", "Reverse Engineering",
        "Colonization", "Hyperspace Route", "Megastructure Built", "First Contact",
        "Civil War", "Resource War", "Border Conflict", "Artifact War",
        "Sgr A* Detection", "Convergence Discovery", "Precursor Breakthrough",
        "Artifact Creation", "Transcendence", "Self-Destruction", "Consumption",
        "Fragmentation", "Collapse (Unknown)", "Vault Sealed", "Guardian Created",
        "Artifact Scattered", "Warning Encoded"};
    return names[static_cast<int>(t)];
}

static const char* archetype_name(FigureArchetype a) {
    static const char* names[] = {
        "The Founder", "The Conqueror", "The Sage", "The Traitor",
        "The Explorer", "The Last", "The Builder"};
    return names[static_cast<int>(a)];
}

static const char* category_name(ArtifactCategory c) {
    static const char* names[] = {
        "weapon", "navigation tool", "knowledge store", "key", "anomaly"};
    return names[static_cast<int>(c)];
}

// ── Main generation ──

WorldLore LoreGenerator::generate(unsigned game_seed) {
    WorldLore lore;
    lore.seed = game_seed;

    std::mt19937 rng(game_seed ^ 0x4C4F5245u); // "LORE"

    // Number of precursor civilizations: 2-5
    int civ_count = 2 + static_cast<int>(rng() % 4);

    // Epoch timing: Primordials start 4-6 billion years ago
    float timeline_start = 4.0f + static_cast<float>(rng() % 20) / 10.0f;

    // Used phoneme pools (avoid repeats)
    std::vector<int> available_pools;
    for (int i = 0; i < phoneme_pool_count; ++i) available_pools.push_back(i);

    float epoch_start = timeline_start;

    for (int i = 0; i < civ_count; ++i) {
        // Pick a unique phoneme pool
        int pool_idx_idx = rng() % available_pools.size();
        int pool_idx = available_pools[pool_idx_idx];
        available_pools.erase(available_pools.begin() + pool_idx_idx);
        if (available_pools.empty()) {
            // Refill if we run out (unlikely with 6 pools and max 5 civs)
            for (int j = 0; j < phoneme_pool_count; ++j) available_pools.push_back(j);
        }

        auto civ = generate_civilization(rng, i, epoch_start, lore.civilizations);

        // Override phoneme pool with our unique selection
        civ.phoneme_pool = static_cast<PhonemePool>(pool_idx);

        // Generate names using the assigned pool
        NameGenerator namer(civ.phoneme_pool);
        std::string civ_name = namer.civilization(rng);
        civ.name = civ_name;
        // Extract short name (second word of "The X Y")
        {
            size_t first_space = civ_name.find(' ');
            size_t second_space = civ_name.find(' ', first_space + 1);
            if (first_space != std::string::npos && second_space != std::string::npos)
                civ.short_name = civ_name.substr(first_space + 1, second_space - first_space - 1);
            else if (first_space != std::string::npos)
                civ.short_name = civ_name.substr(first_space + 1);
            else
                civ.short_name = civ_name;
        }

        // Generate events, figures, artifacts
        generate_events(rng, civ, i, i > 0);
        generate_figures(rng, civ, namer);
        generate_artifacts(rng, civ, namer);

        // Next epoch starts after a silence period
        float silence = 0.1f + static_cast<float>(rng() % 50) / 100.0f; // 0.1-0.6 Bya
        epoch_start = civ.epoch_end_bya - silence;
        if (epoch_start < 0.02f) epoch_start = 0.02f;

        lore.civilizations.push_back(std::move(civ));
    }

    // Human epoch
    lore.humanity = generate_human_epoch(rng, lore.civilizations);

    // Count beacons
    for (const auto& civ : lore.civilizations) {
        for (const auto& e : civ.events) {
            if (e.type == LoreEventType::MegastructureBuilt ||
                e.type == LoreEventType::HyperspaceRoute) {
                lore.total_beacons++;
                if (rng() % 3 != 0) lore.active_beacons++; // ~66% still active
            }
        }
    }

    lore.generated = true;
    return lore;
}

Civilization LoreGenerator::generate_civilization(
    std::mt19937& rng, int epoch_index, float epoch_start_bya,
    const std::vector<Civilization>& predecessors) {

    Civilization civ;

    // Epoch duration: 0.1 - 0.8 billion years
    float duration = 0.1f + static_cast<float>(rng() % 70) / 100.0f;
    civ.epoch_start_bya = epoch_start_bya;
    civ.epoch_end_bya = epoch_start_bya - duration;
    if (civ.epoch_end_bya < 0.01f) civ.epoch_end_bya = 0.01f;

    // Cultural traits
    civ.architecture = static_cast<Architecture>(rng() % 5);
    civ.tech_style = static_cast<TechStyle>(rng() % 5);
    civ.philosophy = static_cast<Philosophy>(rng() % 5);
    civ.collapse_cause = static_cast<CollapseCause>(rng() % 6);

    // Relationships
    if (epoch_index == 0) {
        civ.predecessor_relation = PredecessorRelation::Ignored; // no predecessors
        // Primordials are more likely to reach Sgr A* or build toward it
        int sgra_roll = rng() % 3;
        civ.sgra_relation = (sgra_roll == 0) ? SgrARelation::ReachedAndVanished
                          : (sgra_roll == 1) ? SgrARelation::BuiltToward
                          : SgrARelation::TriedAndFailed;
    } else {
        civ.predecessor_relation = static_cast<PredecessorRelation>(rng() % 4);
        civ.sgra_relation = static_cast<SgrARelation>(rng() % 5);
    }

    // Homeworld system ID — generated from seed (will be mapped to real systems in Phase 2)
    civ.homeworld_system_id = rng();

    return civ;
}

void LoreGenerator::generate_events(
    std::mt19937& rng, Civilization& civ, int epoch_index, bool has_predecessors) {

    float t = civ.epoch_start_bya;
    float end = civ.epoch_end_bya;
    float span = t - end;

    // Always starts with emergence
    civ.events.push_back({LoreEventType::Emergence, t, "First consciousness"});
    t -= span * 0.05f;

    // If has predecessors, discover their ruins
    if (has_predecessors) {
        civ.events.push_back({LoreEventType::RuinDiscovery, t, "Discovered predecessor ruins"});
        t -= span * 0.05f;
        if (rng() % 2 == 0) {
            civ.events.push_back({LoreEventType::Decipherment, t, "Deciphered ancient records"});
            t -= span * 0.05f;
        }
        civ.events.push_back({LoreEventType::ReverseEngineering, t, "Reverse-engineered precursor technology"});
        t -= span * 0.05f;
    }

    // Expansion phase
    civ.events.push_back({LoreEventType::Colonization, t, "First interstellar colony established"});
    t -= span * 0.1f;
    civ.events.push_back({LoreEventType::HyperspaceRoute, t, "Hyperspace route network established"});
    t -= span * 0.1f;

    // Mid-epoch: 1-3 random events
    int mid_events = 1 + static_cast<int>(rng() % 3);
    for (int i = 0; i < mid_events && t > end + span * 0.3f; ++i) {
        LoreEventType type;
        std::string desc;
        int roll = rng() % 6;
        switch (roll) {
            case 0:
                type = LoreEventType::MegastructureBuilt;
                desc = "Megastructure constructed";
                break;
            case 1:
                type = LoreEventType::SgrADetection;
                desc = "Sgr A* gravitational anomaly detected";
                break;
            case 2:
                type = LoreEventType::CivilWar;
                desc = "Internal schism erupted";
                break;
            case 3:
                type = LoreEventType::ResourceWar;
                desc = "Resource conflict over precursor artifacts";
                break;
            case 4:
                type = LoreEventType::ArtifactCreation;
                desc = "Legendary artifact forged";
                break;
            default:
                type = LoreEventType::ConvergenceDiscovery;
                desc = "Convergence pattern recognized — all paths lead to center";
                break;
        }
        civ.events.push_back({type, t, desc});
        t -= span * 0.1f;
    }

    // Collapse phase
    LoreEventType collapse_type;
    std::string collapse_desc;
    switch (civ.collapse_cause) {
        case CollapseCause::Transcendence:
            collapse_type = LoreEventType::Transcendence;
            collapse_desc = "Civilization transcended physical form";
            break;
        case CollapseCause::War:
            collapse_type = LoreEventType::SelfDestruction;
            collapse_desc = "Destroyed in final war";
            break;
        case CollapseCause::Plague:
            collapse_type = LoreEventType::Fragmentation;
            collapse_desc = "Scattered by pandemic";
            break;
        case CollapseCause::SgrAObsession:
            collapse_type = LoreEventType::Consumption;
            collapse_desc = "Consumed by Sgr A* obsession";
            break;
        default:
            collapse_type = LoreEventType::CollapseUnknown;
            collapse_desc = "Vanished — cause unknown";
            break;
    }
    civ.events.push_back({collapse_type, end, collapse_desc});

    // Legacy events (after collapse)
    if (rng() % 2 == 0) {
        civ.events.push_back({LoreEventType::VaultSealed, end,
            "Knowledge vaults sealed before collapse"});
    }
    if (rng() % 3 == 0) {
        civ.events.push_back({LoreEventType::GuardianCreated, end,
            "Automated guardians activated to protect ruins"});
    }
}

void LoreGenerator::generate_figures(
    std::mt19937& rng, Civilization& civ, const NameGenerator& namer) {

    // 3-6 figures per civilization
    int count = 3 + static_cast<int>(rng() % 4);

    // Always include Founder and Last
    std::vector<FigureArchetype> archetypes = {
        FigureArchetype::Founder, FigureArchetype::Last
    };

    // Fill remaining with random archetypes
    FigureArchetype all_archetypes[] = {
        FigureArchetype::Conqueror, FigureArchetype::Sage,
        FigureArchetype::Traitor, FigureArchetype::Explorer,
        FigureArchetype::Builder
    };
    for (int i = 2; i < count; ++i) {
        archetypes.push_back(all_archetypes[rng() % 5]);
    }

    static const char* fates_known[] = {
        "died in battle", "perished in the collapse",
        "sealed themselves in a vault", "disappeared without trace",
        "executed for treason"
    };
    static const char* fates_transcend[] = {
        "entered Sgr A*", "transcended physical form",
        "became one with the beacon network"
    };

    for (auto archetype : archetypes) {
        KeyFigure fig;
        // Use a local copy of rng to keep names deterministic
        fig.name = namer.name(rng);
        fig.title = namer.title(rng, archetype);
        fig.archetype = archetype;
        fig.system_id = rng(); // placeholder, mapped in Phase 2

        switch (archetype) {
            case FigureArchetype::Founder:
                fig.achievement = "Established " + civ.short_name + " civilization";
                break;
            case FigureArchetype::Conqueror:
                fig.achievement = "Expanded territory across " + std::to_string(3 + rng() % 20) + " systems";
                break;
            case FigureArchetype::Sage:
                fig.achievement = "Made the breakthrough that revealed the convergence pattern";
                break;
            case FigureArchetype::Traitor:
                fig.achievement = "Caused the great schism that weakened " + civ.short_name;
                break;
            case FigureArchetype::Explorer:
                fig.achievement = "Charted the deepest reaches toward Sgr A*";
                break;
            case FigureArchetype::Last:
                fig.achievement = "Led " + civ.short_name + " through their final days";
                break;
            case FigureArchetype::Builder:
                fig.achievement = "Chief architect of the beacon network";
                break;
        }

        // Fate
        if (civ.collapse_cause == CollapseCause::Transcendence && archetype == FigureArchetype::Last)
            fig.fate = fates_transcend[rng() % 3];
        else if (archetype == FigureArchetype::Traitor)
            fig.fate = "executed for treason";
        else if (archetype == FigureArchetype::Last)
            fig.fate = "fate unknown";
        else
            fig.fate = fates_known[rng() % 5];

        civ.figures.push_back(std::move(fig));
    }
}

void LoreGenerator::generate_artifacts(
    std::mt19937& rng, Civilization& civ, const NameGenerator& namer) {

    // 1-3 artifacts per civilization
    int count = 1 + static_cast<int>(rng() % 3);

    ArtifactCategory categories[] = {
        ArtifactCategory::Weapon, ArtifactCategory::NavigationTool,
        ArtifactCategory::KnowledgeStore, ArtifactCategory::Key,
        ArtifactCategory::Anomaly
    };

    for (int i = 0; i < count; ++i) {
        LoreArtifact art;
        ArtifactCategory cat = categories[rng() % 5];
        art.category = cat;
        art.name = namer.artifact(rng, cat);
        art.system_id = rng(); // placeholder
        art.body_index = -1;

        // Link to a figure if possible
        if (!civ.figures.empty()) {
            art.figure_index = static_cast<int>(rng() % civ.figures.size());
            art.origin_text = "Created by " + civ.figures[art.figure_index].name
                + " " + civ.figures[art.figure_index].title
                + " during the " + civ.short_name + " epoch.";
        } else {
            art.origin_text = "Origin lost to time.";
        }

        switch (cat) {
            case ArtifactCategory::Weapon:
                art.effect_text = "+" + std::to_string(3 + rng() % 5) + " attack, unique combat ability";
                break;
            case ArtifactCategory::NavigationTool:
                art.effect_text = "Reveals beacon locations on star chart, +" + std::to_string(1 + rng() % 3) + " view radius";
                break;
            case ArtifactCategory::KnowledgeStore:
                art.effect_text = "Grants " + std::to_string(50 + (rng() % 5) * 25) + " XP, reveals epoch history";
                break;
            case ArtifactCategory::Key:
                art.effect_text = "Unlocks sealed vaults of " + civ.short_name + " origin";
                break;
            case ArtifactCategory::Anomaly:
                art.effect_text = "Unpredictable effects near Sgr A*, +" + std::to_string(1 + rng() % 3) + " to all stats";
                break;
        }

        civ.artifacts.push_back(std::move(art));
    }
}

HumanHistory LoreGenerator::generate_human_epoch(
    std::mt19937& rng, const std::vector<Civilization>& civilizations) {

    HumanHistory h;
    h.arrival_bya = 0.01f; // 10,000 years ago
    h.golden_age_start = 0.005f; // 5,000 years ago
    h.fracture_bya = 0.001f; // 1,000 years ago

    h.events.push_back({LoreEventType::Emergence, h.arrival_bya,
        "Humanity reaches the stars"});
    h.events.push_back({LoreEventType::RuinDiscovery, h.arrival_bya - 0.001f,
        "First alien ruins discovered"});
    h.events.push_back({LoreEventType::ReverseEngineering, h.golden_age_start,
        "Hyperspace technology reverse-engineered from precursor ruins"});
    h.events.push_back({LoreEventType::Colonization, h.golden_age_start - 0.001f,
        "Golden age of expansion begins"});
    h.events.push_back({LoreEventType::Fragmentation, h.fracture_bya,
        "The Fracture — civilization fragments into factions"});

    // Generate 2-4 faction names
    int faction_count = 2 + static_cast<int>(rng() % 3);
    static const char* faction_templates[] = {
        "Conclave", "Guild", "Federation", "Alliance", "Syndicate",
        "Collective", "Order", "Republic", "Union", "Authority"
    };
    static const char* faction_adjectives[] = {
        "Stellari", "Outer Rim", "Core", "Free", "United",
        "Iron", "Solar", "Void", "New", "Crimson"
    };

    for (int i = 0; i < faction_count; ++i) {
        std::string faction = std::string(faction_adjectives[rng() % 10])
            + " " + faction_templates[rng() % 10];
        h.faction_names.push_back(faction);
    }

    return h;
}

// ── Dev history formatter ──

std::string LoreGenerator::format_history(const WorldLore& lore) {
    std::ostringstream out;

    out << "WORLD HISTORY — Seed: " << lore.seed << "\n";
    out << "Civilizations: " << lore.civilizations.size() << "\n";
    if (!lore.civilizations.empty()) {
        out << "Timeline: " << lore.civilizations[0].epoch_start_bya
            << " - " << lore.humanity.fracture_bya << " Bya\n";
    }
    out << "Beacons: " << lore.active_beacons << " active / "
        << lore.total_beacons << " total\n\n";

    for (size_t i = 0; i < lore.civilizations.size(); ++i) {
        const auto& civ = lore.civilizations[i];
        out << std::string(50, '=') << "\n";
        out << "EPOCH " << i << ": " << civ.name << "\n";
        out << "  Span: " << civ.epoch_start_bya << " - "
            << civ.epoch_end_bya << " Bya\n";
        out << "  Aesthetic: " << architecture_name(civ.architecture)
            << " architecture, " << tech_name(civ.tech_style) << " technology\n";
        out << "  Philosophy: " << philosophy_name(civ.philosophy) << "\n";
        out << "  Collapse: " << collapse_name(civ.collapse_cause) << "\n";
        if (i > 0)
            out << "  Predecessors: " << predecessor_name(civ.predecessor_relation) << "\n";
        out << "  Sgr A*: " << sgra_name(civ.sgra_relation) << "\n";
        out << std::string(50, '=') << "\n\n";

        out << "  TIMELINE:\n";
        for (const auto& e : civ.events) {
            out << "    " << e.time_bya << " Bya  "
                << event_type_name(e.type) << " -- " << e.description << "\n";
        }
        out << "\n";

        out << "  KEY FIGURES:\n";
        for (const auto& f : civ.figures) {
            out << "    " << f.name << " " << f.title
                << " (" << archetype_name(f.archetype) << ")\n";
            out << "      " << f.achievement << "\n";
            out << "      Fate: " << f.fate << "\n\n";
        }

        out << "  ARTIFACTS:\n";
        for (const auto& a : civ.artifacts) {
            out << "    " << a.name << " (" << category_name(a.category) << ")\n";
            out << "      " << a.origin_text << "\n";
            out << "      Effect: " << a.effect_text << "\n\n";
        }
    }

    out << std::string(50, '=') << "\n";
    out << "PRESENT DAY: Humanity\n";
    out << std::string(50, '=') << "\n\n";

    out << "  TIMELINE:\n";
    for (const auto& e : lore.humanity.events) {
        out << "    " << e.time_bya << " Bya  "
            << event_type_name(e.type) << " -- " << e.description << "\n";
    }
    out << "\n";

    out << "  FACTIONS:\n";
    for (const auto& f : lore.humanity.faction_names) {
        out << "    " << f << "\n";
    }

    return out.str();
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

After `src/name_generator.cpp`, add:
```
    src/lore_generator.cpp
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/lore_generator.h src/lore_generator.cpp CMakeLists.txt
git commit -m "Add lore generator — epoch simulation with events, figures, artifacts"
```

---

### Task 4: Wire Lore into Game and WorldManager

Add `WorldLore` to `WorldManager`, generate it during `new_game()` before galaxy generation, and store it for later use.

**Files:**
- Modify: `include/astra/world_manager.h`
- Modify: `src/game.cpp`

- [ ] **Step 1: Add WorldLore to WorldManager**

In `include/astra/world_manager.h`, add the include after the existing includes:

```cpp
#include "astra/lore_types.h"
```

Add member and accessors near the seed/rng accessors (around line 93):

```cpp
    WorldLore& lore() { return lore_; }
    const WorldLore& lore() const { return lore_; }
```

Add the member variable in the private section:

```cpp
    WorldLore lore_;
```

- [ ] **Step 2: Generate lore in new_game()**

In `src/game.cpp`, add the include near the top:

```cpp
#include "astra/lore_generator.h"
```

In `Game::new_game(const CreationResult& cr)`, find the line that generates the galaxy (around line 647):
```cpp
world_.navigation() = generate_galaxy(world_.seed());
```

Add lore generation **before** that line:

```cpp
    // Generate world lore before galaxy (lore-first principle)
    world_.lore() = LoreGenerator::generate(world_.seed());
```

Do the same in the dev-mode `Game::new_game()` (around line 541), before the galaxy generation line:

```cpp
    world_.lore() = LoreGenerator::generate(world_.seed());
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/world_manager.h src/game.cpp
git commit -m "Wire lore generation into new game flow — generates before galaxy"
```

---

### Task 5: Dev Console History Command

Add a `history` command to the dev console that dumps the formatted lore timeline to the message log.

**Files:**
- Modify: `src/dev_console.cpp`

- [ ] **Step 1: Add include**

At the top of `src/dev_console.cpp`, add:

```cpp
#include "astra/lore_generator.h"
```

- [ ] **Step 2: Add history command**

In `DevConsole::execute_command()`, find the help text (around line 112) and add:

```cpp
        log("  history             - show world lore history");
```

Then find where the command dispatch ends (look for the last `else if` block before the "Unknown command" fallback). Add before the unknown command handler:

```cpp
    } else if (verb == "history") {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        std::string history = LoreGenerator::format_history(game.world().lore());
        // Split into lines and log each one
        std::istringstream stream(history);
        std::string line;
        while (std::getline(stream, line)) {
            log(line);
        }
```

Also add at the top of the file if not present:

```cpp
#include <sstream>
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 4: Test manually**

Run: `./build/astra-dev`

1. Start a new game (dev mode)
2. Open dev console (`:`)
3. Type `history`
4. Verify the full lore timeline appears in the message log

Expected: Epoch headers, civilization names, events, figures, artifacts all rendered.

- [ ] **Step 5: Commit**

```bash
git add src/dev_console.cpp
git commit -m "Add dev console 'history' command — dumps full world lore timeline"
```

---

### Task 6: Lore Serialization (Save/Load)

Persist `WorldLore` across saves. Create a dedicated serialization file and integrate with the save system.

**Files:**
- Create: `src/lore_serialization.cpp`
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`
- Modify: `src/save_system.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add WorldLore to SaveData**

In `include/astra/save_file.h`, add the include:

```cpp
#include "astra/lore_types.h"
```

In the `SaveData` struct, add after the existing fields:

```cpp
    WorldLore lore;
```

Bump version:

```cpp
    uint32_t version = 21;  // was 20
```

- [ ] **Step 2: Create lore_serialization.cpp**

```cpp
#include "astra/lore_types.h"
#include "astra/save_file.h"

namespace astra {

// Forward declarations from save_file.cpp
class BinaryWriter;
class BinaryReader;

void write_lore(BinaryWriter& w, const WorldLore& lore) {
    w.write_u32(lore.seed);
    w.write_u8(lore.generated ? 1 : 0);
    if (!lore.generated) return;

    // Civilizations
    w.write_u32(static_cast<uint32_t>(lore.civilizations.size()));
    for (const auto& civ : lore.civilizations) {
        w.write_string(civ.name);
        w.write_string(civ.short_name);
        w.write_u8(static_cast<uint8_t>(civ.phoneme_pool));
        w.write_u8(static_cast<uint8_t>(civ.architecture));
        w.write_u8(static_cast<uint8_t>(civ.tech_style));
        w.write_u8(static_cast<uint8_t>(civ.philosophy));
        w.write_u8(static_cast<uint8_t>(civ.predecessor_relation));
        w.write_u8(static_cast<uint8_t>(civ.sgra_relation));
        w.write_u8(static_cast<uint8_t>(civ.collapse_cause));
        w.write_float(civ.epoch_start_bya);
        w.write_float(civ.epoch_end_bya);
        w.write_u32(civ.homeworld_system_id);

        // Events
        w.write_u32(static_cast<uint32_t>(civ.events.size()));
        for (const auto& e : civ.events) {
            w.write_u8(static_cast<uint8_t>(e.type));
            w.write_float(e.time_bya);
            w.write_string(e.description);
            w.write_u32(e.system_id);
        }

        // Figures
        w.write_u32(static_cast<uint32_t>(civ.figures.size()));
        for (const auto& f : civ.figures) {
            w.write_string(f.name);
            w.write_string(f.title);
            w.write_u8(static_cast<uint8_t>(f.archetype));
            w.write_string(f.achievement);
            w.write_u32(f.system_id);
            w.write_i32(f.artifact_index);
            w.write_string(f.fate);
        }

        // Artifacts
        w.write_u32(static_cast<uint32_t>(civ.artifacts.size()));
        for (const auto& a : civ.artifacts) {
            w.write_string(a.name);
            w.write_u8(static_cast<uint8_t>(a.category));
            w.write_string(a.origin_text);
            w.write_string(a.effect_text);
            w.write_u32(a.system_id);
            w.write_i32(a.body_index);
            w.write_i32(a.figure_index);
        }
    }

    // Human history
    w.write_float(lore.humanity.arrival_bya);
    w.write_float(lore.humanity.golden_age_start);
    w.write_float(lore.humanity.fracture_bya);
    w.write_u32(static_cast<uint32_t>(lore.humanity.events.size()));
    for (const auto& e : lore.humanity.events) {
        w.write_u8(static_cast<uint8_t>(e.type));
        w.write_float(e.time_bya);
        w.write_string(e.description);
        w.write_u32(e.system_id);
    }
    w.write_u32(static_cast<uint32_t>(lore.humanity.faction_names.size()));
    for (const auto& f : lore.humanity.faction_names) {
        w.write_string(f);
    }

    w.write_i32(lore.total_beacons);
    w.write_i32(lore.active_beacons);
}

WorldLore read_lore(BinaryReader& r) {
    WorldLore lore;
    lore.seed = r.read_u32();
    lore.generated = r.read_u8() != 0;
    if (!lore.generated) return lore;

    uint32_t civ_count = r.read_u32();
    lore.civilizations.resize(civ_count);
    for (auto& civ : lore.civilizations) {
        civ.name = r.read_string();
        civ.short_name = r.read_string();
        civ.phoneme_pool = static_cast<PhonemePool>(r.read_u8());
        civ.architecture = static_cast<Architecture>(r.read_u8());
        civ.tech_style = static_cast<TechStyle>(r.read_u8());
        civ.philosophy = static_cast<Philosophy>(r.read_u8());
        civ.predecessor_relation = static_cast<PredecessorRelation>(r.read_u8());
        civ.sgra_relation = static_cast<SgrARelation>(r.read_u8());
        civ.collapse_cause = static_cast<CollapseCause>(r.read_u8());
        civ.epoch_start_bya = r.read_float();
        civ.epoch_end_bya = r.read_float();
        civ.homeworld_system_id = r.read_u32();

        uint32_t event_count = r.read_u32();
        civ.events.resize(event_count);
        for (auto& e : civ.events) {
            e.type = static_cast<LoreEventType>(r.read_u8());
            e.time_bya = r.read_float();
            e.description = r.read_string();
            e.system_id = r.read_u32();
        }

        uint32_t fig_count = r.read_u32();
        civ.figures.resize(fig_count);
        for (auto& f : civ.figures) {
            f.name = r.read_string();
            f.title = r.read_string();
            f.archetype = static_cast<FigureArchetype>(r.read_u8());
            f.achievement = r.read_string();
            f.system_id = r.read_u32();
            f.artifact_index = r.read_i32();
            f.fate = r.read_string();
        }

        uint32_t art_count = r.read_u32();
        civ.artifacts.resize(art_count);
        for (auto& a : civ.artifacts) {
            a.name = r.read_string();
            a.category = static_cast<ArtifactCategory>(r.read_u8());
            a.origin_text = r.read_string();
            a.effect_text = r.read_string();
            a.system_id = r.read_u32();
            a.body_index = r.read_i32();
            a.figure_index = r.read_i32();
        }
    }

    lore.humanity.arrival_bya = r.read_float();
    lore.humanity.golden_age_start = r.read_float();
    lore.humanity.fracture_bya = r.read_float();
    uint32_t h_event_count = r.read_u32();
    lore.humanity.events.resize(h_event_count);
    for (auto& e : lore.humanity.events) {
        e.type = static_cast<LoreEventType>(r.read_u8());
        e.time_bya = r.read_float();
        e.description = r.read_string();
        e.system_id = r.read_u32();
    }
    uint32_t faction_count = r.read_u32();
    lore.humanity.faction_names.resize(faction_count);
    for (auto& f : lore.humanity.faction_names) {
        f = r.read_string();
    }

    lore.total_beacons = r.read_i32();
    lore.active_beacons = r.read_i32();

    return lore;
}

} // namespace astra
```

- [ ] **Step 3: Integrate with save_file.cpp**

In `src/save_file.cpp`, add the declarations at the top (after existing includes):

```cpp
// Lore serialization (defined in lore_serialization.cpp)
void write_lore(BinaryWriter& w, const WorldLore& lore);
WorldLore read_lore(BinaryReader& r);
```

Find the save function where it writes the last section before the end. Add after the last write:

```cpp
    write_lore(w, data.lore);
```

Find the load function. After the last read (and inside the version check for v21+), add:

```cpp
    if (data.version >= 21) {
        data.lore = read_lore(r);
    }
```

- [ ] **Step 4: Integrate with save_system.cpp**

In the save system's `build_save_data()` function, add:

```cpp
    data.lore = game.world().lore();
```

In the load/restore function, after restoring navigation, add:

```cpp
    game.world().lore() = data.lore;
```

- [ ] **Step 5: Add to CMakeLists.txt**

After `src/lore_generator.cpp`, add:
```
    src/lore_serialization.cpp
```

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 7: Test save/load cycle**

1. Start a new dev game
2. Type `:history` — note the civilization names
3. Save the game
4. Quit and reload
5. Type `:history` — verify same civilizations appear

- [ ] **Step 8: Commit**

```bash
git add src/lore_serialization.cpp include/astra/save_file.h src/save_file.cpp src/save_system.cpp CMakeLists.txt
git commit -m "Add lore save/load — WorldLore persisted across saves (version 21)"
```

---

### Task 7: Update Documentation

- [ ] **Step 1: Update roadmap**

In `docs/roadmap.md`, under "World Generation", check off the first item and mark the dev log:

From:
```
- [ ] **Procedural world lore** — billions of years of layered history, precursor civilizations, beacon network toward Sgr A*
- [ ] **Phoneme-based naming** — procedural civilization/figure/artifact names from curated syllable pools
```
To:
```
- [x] **Procedural world lore** — billions of years of layered history, precursor civilizations, beacon network toward Sgr A*
- [x] **Phoneme-based naming** — procedural civilization/figure/artifact names from curated syllable pools
- [x] **Developer history log** — full timeline dump in dev mode
```

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs: mark lore generator phase 1 as implemented"
```

---

### Task 8: Visual Lore Boot Sequence

During new game startup, display lore generation as a visual boot log that follows the existing boot sequence. As each epoch is generated, display the civilization name, time span, and key traits scrolling on screen. This reuses the `BootSequence` class pattern from `src/boot_sequence.cpp`.

**Files:**
- Modify: `include/astra/boot_sequence.h`
- Modify: `src/boot_sequence.cpp`
- Modify: `src/game.cpp`

- [ ] **Step 1: Add lore boot method to BootSequence**

In `include/astra/boot_sequence.h`, add the include:

```cpp
#include "astra/lore_types.h"
```

Add a new public method after `play()`:

```cpp
    // Play the lore generation boot sequence. Shows epochs scrolling on screen.
    // Returns false if user pressed a key to skip.
    bool play_lore(const WorldLore& lore);
```

- [ ] **Step 2: Implement play_lore in boot_sequence.cpp**

In `src/boot_sequence.cpp`, add at the top:

```cpp
#include "astra/lore_types.h"

#include <sstream>
```

Add the following helper and method at the end of the file, before the closing `} // namespace astra`:

```cpp
static std::string format_bya(float bya) {
    std::ostringstream out;
    out.precision(1);
    out << std::fixed << bya;
    return out.str();
}

bool BootSequence::play_lore(const WorldLore& lore) {
    if (!lore.generated) return true;

    int w = renderer_->get_width();
    int h = renderer_->get_height();
    int margin = 4;

    // Build the boot lines for lore
    struct LoreLine {
        std::string text;
        Color color;
        int delay_ms;
    };

    std::vector<LoreLine> lines;
    lines.push_back({"", Color::Default, 100});
    lines.push_back({"Scanning deep time archives...", Color::DarkGray, 300});

    for (size_t i = 0; i < lore.civilizations.size(); ++i) {
        const auto& civ = lore.civilizations[i];
        std::string line = "  Epoch " + std::to_string(i) + ": "
            + civ.name + " ("
            + format_bya(civ.epoch_start_bya) + " - "
            + format_bya(civ.epoch_end_bya) + " Bya) ... [OK]";
        lines.push_back({line, Color::Green, 200});
    }

    lines.push_back({"  Human arrival: "
        + format_bya(lore.humanity.arrival_bya) + " Bya",
        Color::Cyan, 200});
    lines.push_back({"", Color::Default, 100});
    lines.push_back({"Loading beacon network... "
        + std::to_string(lore.total_beacons) + " beacons ("
        + std::to_string(lore.active_beacons) + " active)",
        Color::DarkGray, 250});
    lines.push_back({"Galactic history compiled.", Color::White, 500});

    // Render using the same scrolling pattern as the main boot sequence
    int total_lines = static_cast<int>(lines.size());
    int start_y = (h - total_lines) / 2;
    if (start_y < 2) start_y = 2;

    for (int i = 0; i < total_lines; ++i) {
        const auto& line = lines[i];

        renderer_->clear();

        for (int j = 0; j <= i; ++j) {
            const auto& prev = lines[j];
            if (prev.text.empty()) continue;

            int y = start_y + j;
            if (y < 0 || y >= h) continue;

            // Split at bracket for colored status tags
            auto bracket = prev.text.rfind('[');
            if (bracket != std::string::npos && bracket > 0) {
                std::string prefix = prev.text.substr(0, bracket);
                for (int c = 0; c < static_cast<int>(prefix.size()); ++c) {
                    if (margin + c < w)
                        renderer_->draw_char(margin + c, y, prefix[c], Color::DarkGray);
                }
                std::string tag = prev.text.substr(bracket);
                for (int c = 0; c < static_cast<int>(tag.size()); ++c) {
                    if (margin + static_cast<int>(bracket) + c < w)
                        renderer_->draw_char(margin + static_cast<int>(bracket) + c, y,
                                             tag[c], prev.color);
                }
            } else {
                for (int c = 0; c < static_cast<int>(prev.text.size()); ++c) {
                    if (margin + c < w)
                        renderer_->draw_char(margin + c, y, prev.text[c], prev.color);
                }
            }
        }

        // Blinking cursor
        int cursor_y = start_y + i;
        int cursor_x = margin;
        if (!line.text.empty()) {
            cursor_x = margin + static_cast<int>(line.text.size()) + 1;
        }
        if (cursor_x < w && cursor_y < h) {
            renderer_->draw_char(cursor_x, cursor_y, '_', Color::DarkGray);
        }

        renderer_->present();
        if (delay(line.delay_ms)) return false;
    }

    // Brief pause at end
    if (delay(400)) return false;

    return true;
}
```

- [ ] **Step 3: Call play_lore during new game startup**

In `src/game.cpp`, find the location where the boot sequence plays during new game creation. After the existing `boot.play()` call, and after lore generation, add:

```cpp
    boot.play_lore(world_.lore());
```

This should go after `world_.lore() = LoreGenerator::generate(world_.seed());` and before `world_.navigation() = generate_galaxy(world_.seed());`.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 5: Test manually**

Run: `./build/astra-dev`

1. Start a new game
2. After the standard boot sequence, lore epochs should scroll on screen
3. Pressing any key should skip the lore boot

- [ ] **Step 6: Commit**

```bash
git add include/astra/boot_sequence.h src/boot_sequence.cpp src/game.cpp
git commit -m "Add visual lore boot sequence — epochs scroll during new game startup"
```

---

### Task 9: Galaxy Integration — System Tier Assignment

After lore generation and galaxy generation, annotate systems with significance tiers (0-3) based on lore events. Add a `lore_tier` field to `StarSystem` and a function to map placeholder lore system IDs to real generated systems.

**Files:**
- Modify: `include/astra/star_chart.h`
- Modify: `include/astra/lore_types.h`
- Create: `src/lore_galaxy_integration.cpp`
- Modify: `src/game.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add lore_tier to StarSystem**

In `include/astra/star_chart.h`, add a field to the `StarSystem` struct after `bodies_generated`:

```cpp
    uint8_t lore_tier = 0;  // 0=mundane, 1=touched, 2=significant, 3=pivotal
```

- [ ] **Step 2: Add LoreAnnotation to lore_types.h**

In `include/astra/lore_types.h`, add the following struct before the `WorldLore` struct:

```cpp
// ── Per-system lore annotation ──
struct LoreAnnotation {
    uint8_t tier = 0;                // 0-3 significance
    int civilization_index = -1;     // which civilization (-1 = none)
    std::string event_summary;       // what happened here
    uint32_t real_system_id = 0;     // mapped real system
};
```

Add a lookup map to `WorldLore`, after the `active_beacons` field:

```cpp
    // Per-system lore annotations, keyed by real system ID
    std::vector<LoreAnnotation> annotations;
```

- [ ] **Step 3: Create lore_galaxy_integration.cpp**

```cpp
#include "astra/lore_types.h"
#include "astra/star_chart.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace astra {

// Find the real system closest to a pseudo-random hash position.
// Uses the hash to pick a deterministic galactic position, then finds the
// nearest system in the generated galaxy.
static uint32_t map_lore_id_to_system(uint32_t lore_id,
                                       const NavigationData& nav) {
    if (nav.systems.empty()) return 0;

    // Use the lore ID as a seed to pick a rough galactic position
    float angle = static_cast<float>(lore_id & 0xFFFF) / 65536.0f * 6.2832f;
    float radius = static_cast<float>((lore_id >> 16) & 0xFFFF) / 65536.0f * 180.0f;
    float target_gx = std::cos(angle) * radius;
    float target_gy = std::sin(angle) * radius;

    // Find nearest real system (skip Sgr A* at index 0 and Sol at index 1)
    uint32_t best_id = nav.systems[0].id;
    float best_dist = 1e18f;

    for (size_t i = 2; i < nav.systems.size(); ++i) {
        const auto& sys = nav.systems[i];
        float dx = sys.gx - target_gx;
        float dy = sys.gy - target_gy;
        float d = dx * dx + dy * dy;
        if (d < best_dist) {
            best_dist = d;
            best_id = sys.id;
        }
    }
    return best_id;
}

void assign_lore_tiers(WorldLore& lore, NavigationData& nav) {
    if (!lore.generated) return;

    // Build system index for fast lookup
    std::unordered_map<uint32_t, size_t> id_to_index;
    for (size_t i = 0; i < nav.systems.size(); ++i) {
        id_to_index[nav.systems[i].id] = i;
    }

    // Track which systems are assigned and their annotations
    std::unordered_map<uint32_t, LoreAnnotation> annotation_map;

    // Used system IDs (avoid assigning two civilizations to the same system)
    std::unordered_set<uint32_t> used_systems;

    auto set_tier = [&](uint32_t sys_id, uint8_t tier, int civ_idx,
                        const std::string& summary) {
        auto it = id_to_index.find(sys_id);
        if (it == id_to_index.end()) return;

        auto& sys = nav.systems[it->second];
        if (tier > sys.lore_tier) {
            sys.lore_tier = tier;
        }

        auto& ann = annotation_map[sys_id];
        if (tier > ann.tier) {
            ann.tier = tier;
            ann.civilization_index = civ_idx;
            ann.event_summary = summary;
            ann.real_system_id = sys_id;
        }
    };

    for (size_t ci = 0; ci < lore.civilizations.size(); ++ci) {
        auto& civ = lore.civilizations[ci];

        // Map homeworld to a real system (Tier 3)
        uint32_t hw_id = map_lore_id_to_system(civ.homeworld_system_id, nav);
        // Avoid collisions — if already used, shift to next closest
        while (used_systems.count(hw_id)) {
            hw_id = map_lore_id_to_system(hw_id + 1, nav);
        }
        used_systems.insert(hw_id);
        civ.homeworld_system_id = hw_id; // update to real ID
        set_tier(hw_id, 3, static_cast<int>(ci),
                 civ.short_name + " homeworld");

        // Map event system IDs (Tier 2 for significant events)
        for (auto& event : civ.events) {
            if (event.system_id == 0) {
                // Assign a system based on event type
                if (event.type == LoreEventType::MegastructureBuilt ||
                    event.type == LoreEventType::HyperspaceRoute ||
                    event.type == LoreEventType::Colonization ||
                    event.type == LoreEventType::SgrADetection) {
                    event.system_id = map_lore_id_to_system(
                        hw_id ^ static_cast<uint32_t>(event.type) * 7919u, nav);
                }
            }
            if (event.system_id != 0) {
                uint32_t real_id = map_lore_id_to_system(event.system_id, nav);
                event.system_id = real_id;
                set_tier(real_id, 2, static_cast<int>(ci), event.description);
            }
        }

        // Map figure system IDs (Tier 2)
        for (auto& fig : civ.figures) {
            if (fig.system_id != 0) {
                uint32_t real_id = map_lore_id_to_system(fig.system_id, nav);
                fig.system_id = real_id;
                set_tier(real_id, 2, static_cast<int>(ci),
                         fig.name + " " + fig.title + " — " + fig.achievement);
            }
        }

        // Map artifact system IDs (Tier 2)
        for (auto& art : civ.artifacts) {
            if (art.system_id != 0) {
                uint32_t real_id = map_lore_id_to_system(art.system_id, nav);
                art.system_id = real_id;
                set_tier(real_id, 2, static_cast<int>(ci),
                         art.name + " located here");
            }
        }
    }

    // Mark nearby systems as Tier 1 (touched)
    // For each Tier 2-3 system, mark systems within a radius as Tier 1
    std::vector<std::pair<uint32_t, LoreAnnotation>> tier_2_3;
    for (const auto& [sys_id, ann] : annotation_map) {
        if (ann.tier >= 2) tier_2_3.push_back({sys_id, ann});
    }

    for (const auto& [sys_id, ann] : tier_2_3) {
        auto it = id_to_index.find(sys_id);
        if (it == id_to_index.end()) continue;
        const auto& center = nav.systems[it->second];

        for (size_t i = 0; i < nav.systems.size(); ++i) {
            auto& sys = nav.systems[i];
            if (sys.lore_tier > 0) continue; // already assigned

            float dx = sys.gx - center.gx;
            float dy = sys.gy - center.gy;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < 15.0f) {
                sys.lore_tier = 1;
                auto& nearby_ann = annotation_map[sys.id];
                if (nearby_ann.tier == 0) {
                    nearby_ann.tier = 1;
                    nearby_ann.civilization_index = ann.civilization_index;
                    nearby_ann.event_summary = "Near " + ann.event_summary;
                    nearby_ann.real_system_id = sys.id;
                }
            }
        }
    }

    // Build final annotation list
    lore.annotations.clear();
    for (const auto& [sys_id, ann] : annotation_map) {
        lore.annotations.push_back(ann);
    }
}

} // namespace astra
```

- [ ] **Step 4: Add declaration to lore_types.h**

At the bottom of `include/astra/lore_types.h`, before the closing `} // namespace astra`, add a forward declaration:

```cpp
// Forward declaration — defined in star_chart.h
struct NavigationData;

// Assign lore significance tiers to real galaxy systems.
// Call after both lore and galaxy generation.
void assign_lore_tiers(WorldLore& lore, NavigationData& nav);
```

- [ ] **Step 5: Wire into game.cpp**

In `src/game.cpp`, find the location after galaxy generation:

```cpp
world_.navigation() = generate_galaxy(world_.seed());
```

Add immediately after:

```cpp
    // Assign lore tiers to generated systems
    assign_lore_tiers(world_.lore(), world_.navigation());
```

Do this in both `new_game()` variants (dev mode and creation screen).

- [ ] **Step 6: Add to CMakeLists.txt**

After `src/lore_serialization.cpp`, add:

```
    src/lore_galaxy_integration.cpp
```

- [ ] **Step 7: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 8: Test with dev console**

Run: `./build/astra-dev`

1. Start a new game
2. Open dev console (`:`)
3. Type `history` — verify that system IDs in the output now reference real system IDs (small numbers matching the generated galaxy)

- [ ] **Step 9: Commit**

```bash
git add include/astra/star_chart.h include/astra/lore_types.h src/lore_galaxy_integration.cpp src/game.cpp CMakeLists.txt
git commit -m "Add lore tier assignment — map lore to real galaxy systems (tiers 0-3)"
```

---

### Task 10: System Lore Annotations

Add a lookup function to retrieve per-system lore annotations, enabling downstream features (star chart markers, dungeon theming, POI placement) to query what happened at any system.

**Files:**
- Modify: `include/astra/lore_types.h`
- Modify: `src/lore_galaxy_integration.cpp`

- [ ] **Step 1: Add annotation lookup to lore_types.h**

In `include/astra/lore_types.h`, add after the `assign_lore_tiers` declaration:

```cpp
// Look up the lore annotation for a system. Returns nullptr if none.
const LoreAnnotation* find_lore_annotation(const WorldLore& lore, uint32_t system_id);

// Get the civilization that built at this system. Returns nullptr if none.
const Civilization* lore_civilization_at(const WorldLore& lore, uint32_t system_id);
```

- [ ] **Step 2: Implement lookups in lore_galaxy_integration.cpp**

Add at the end of `src/lore_galaxy_integration.cpp`, before the closing `} // namespace astra`:

```cpp
const LoreAnnotation* find_lore_annotation(const WorldLore& lore, uint32_t system_id) {
    for (const auto& ann : lore.annotations) {
        if (ann.real_system_id == system_id) return &ann;
    }
    return nullptr;
}

const Civilization* lore_civilization_at(const WorldLore& lore, uint32_t system_id) {
    const auto* ann = find_lore_annotation(lore, system_id);
    if (!ann || ann->civilization_index < 0) return nullptr;
    if (ann->civilization_index >= static_cast<int>(lore.civilizations.size()))
        return nullptr;
    return &lore.civilizations[ann->civilization_index];
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/lore_types.h src/lore_galaxy_integration.cpp
git commit -m "Add lore annotation lookup — query civilization and history per system"
```

---

### Task 11: Star Chart Lore Markers

Show lore-significant systems on the star chart viewer with visual markers. Tier 2-3 systems display a distinct indicator. When the player has the cursor on a significant system, its lore annotation appears in the info panel.

This follows the existing quest marker pattern: populate a list of system IDs in `GalaxyMapDesc`, then render markers in `terminal_renderer_galaxy.cpp`.

**Files:**
- Modify: `include/astra/galaxy_map_desc.h`
- Modify: `src/star_chart_viewer.cpp`
- Modify: `src/terminal_renderer_galaxy.cpp`

- [ ] **Step 1: Add lore marker fields to GalaxyMapDesc**

In `include/astra/galaxy_map_desc.h`, add after the quest marker fields (around line 55):

```cpp
    // Lore markers (Tier 2-3 systems)
    std::vector<uint32_t> lore_system_ids;

    // Lore annotation for currently selected system (empty = none)
    std::string lore_annotation_text;
```

- [ ] **Step 2: Populate lore markers in StarChartViewer**

In `src/star_chart_viewer.cpp`, add the include:

```cpp
#include "astra/lore_types.h"
```

In the `build_desc()` method (or wherever `GalaxyMapDesc` is constructed — near the quest marker population around line 570), add after the quest marker block:

```cpp
    // Lore markers
    if (world_) {
        const auto& lore = world_->lore();
        if (lore.generated) {
            for (const auto& ann : lore.annotations) {
                if (ann.tier >= 2) {
                    lore_system_ids.push_back(ann.real_system_id);
                }
            }
            desc.lore_system_ids = std::move(lore_system_ids);

            // Annotation text for selected system
            if (cursor_index_ >= 0 &&
                cursor_index_ < static_cast<int>(nav_->systems.size())) {
                const auto& sys = nav_->systems[cursor_index_];
                if (sys.discovered) {
                    const auto* ann = find_lore_annotation(lore, sys.id);
                    if (ann && ann->tier >= 1) {
                        std::string text;
                        if (ann->civilization_index >= 0 &&
                            ann->civilization_index < static_cast<int>(lore.civilizations.size())) {
                            text = lore.civilizations[ann->civilization_index].short_name;
                            text += " — ";
                        }
                        text += ann->event_summary;
                        desc.lore_annotation_text = std::move(text);
                    }
                }
            }
        }
    }
```

- [ ] **Step 3: Render lore markers in terminal_renderer_galaxy.cpp**

In `src/terminal_renderer_galaxy.cpp`, add a helper after `is_quest_body`:

```cpp
static bool is_lore_system(const GalaxyMapDesc& desc, uint32_t sys_id) {
    for (auto id : desc.lore_system_ids)
        if (id == sys_id) return true;
    return false;
}
```

In each of the three zoom-level render functions (`render_galaxy_zoom`, `render_local_zoom`, and the system-level renderer), add a lore marker pass after the quest marker pass. Follow the same pattern — iterate systems, check `is_lore_system`, and draw a marker:

In `render_galaxy_zoom`, after the quest marker block (around line 85):

```cpp
    // Lore markers (Tier 2-3 systems, diamond glyph)
    for (const auto& sys : desc.systems) {
        if (!sys.discovered) continue;
        if (!is_lore_system(desc, sys.id)) continue;
        if (is_quest_system(desc, sys.id)) continue; // quest takes priority
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;
        if (sx + 1 < mw) ctx.put(sx + 1, sy, '*', Color::BrightCyan);
    }
```

In `render_local_zoom`, after its quest marker block (around line 192):

```cpp
    // Lore markers
    for (const auto& sys : desc.systems) {
        if (!sys.discovered) continue;
        if (!is_lore_system(desc, sys.id)) continue;
        if (is_quest_system(desc, sys.id)) continue;
        int sx = to_screen_x(sys.gx, view_left, view_w, mw);
        int sy = to_screen_y(sys.gy, view_top, view_h, mh);
        if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) continue;
        if (sy > 0) ctx.put(sx, sy - 1, '*', Color::BrightCyan);
    }
```

- [ ] **Step 4: Display lore annotation in info panel**

In the star chart viewer's info panel rendering (wherever the selected system's name and details are displayed), add after the existing system info:

```cpp
    if (!desc.lore_annotation_text.empty()) {
        // Draw lore annotation below system info
        renderer_->draw_string(info_x, info_y, desc.lore_annotation_text, Color::Cyan);
        info_y++;
    }
```

The exact location depends on the info panel rendering — check the `render_info_panel` or equivalent function in `src/star_chart_viewer.cpp`.

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 6: Test manually**

Run: `./build/astra-dev`

1. Start a new game
2. Open star chart (`m`)
3. Look for cyan `*` markers on discovered systems
4. Navigate to a marked system — verify lore annotation appears in the info panel

- [ ] **Step 7: Commit**

```bash
git add include/astra/galaxy_map_desc.h src/star_chart_viewer.cpp src/terminal_renderer_galaxy.cpp
git commit -m "Add star chart lore markers — Tier 2-3 systems show cyan indicators"
```

---

### Task 12: Lore-Driven POI Placement on Overworld

When generating overworld maps for lore-significant bodies, increase POI density based on the system's lore tier. Tier 1 gets extra ruins, Tier 2 gets a cluster of related POIs, Tier 3 gets a dominant lore feature.

**Files:**
- Modify: `include/astra/map_properties.h`
- Modify: `src/game_world.cpp`
- Modify: `src/generators/overworld_generator.cpp`

- [ ] **Step 1: Add lore_tier to MapProperties**

In `include/astra/map_properties.h`, add a field after `body_danger_level` in the overworld generation context section (around line 45):

```cpp
    uint8_t lore_tier = 0;  // 0=mundane, 1-3 from lore system
```

- [ ] **Step 2: Pass lore_tier during overworld generation**

In `src/game_world.cpp`, find where overworld `MapProperties` are populated before calling the generator. This is in the landing/travel code around line 1060, where `ow_seed` is computed. After setting `props.body_danger_level`:

```cpp
            // Pass lore tier to overworld generator
            props.lore_tier = target_sys.lore_tier;
```

- [ ] **Step 3: Increase POI density in overworld generator**

In `src/generators/overworld_generator.cpp`, in the `place_features` method, modify the ruins placement block (around line 659). Replace:

```cpp
    // --- Ruins ---
    {
        int count = 0;
        if (props_->body_danger_level >= 3) {
            count = std::uniform_int_distribution<int>(1, 4)(rng);
        } else if (pct(rng) < 30) {
            count = std::uniform_int_distribution<int>(1, 2)(rng);
        }
        if (count > 0) {
            // On asteroids/airless, only 1x1 stamps
            int max_idx = (asteroid || airless) ? 1 : -1;
            place_n(ruin_stamps, ruin_stamp_count, count, max_idx);
        }
    }
```

With:

```cpp
    // --- Ruins ---
    {
        int count = 0;
        if (props_->body_danger_level >= 3) {
            count = std::uniform_int_distribution<int>(1, 4)(rng);
        } else if (pct(rng) < 30) {
            count = std::uniform_int_distribution<int>(1, 2)(rng);
        }

        // Lore tier increases ruin density
        if (props_->lore_tier >= 3) {
            // Tier 3: dominant lore feature — dense ruins
            count = std::max(count, 4) + std::uniform_int_distribution<int>(1, 3)(rng);
        } else if (props_->lore_tier >= 2) {
            // Tier 2: cluster of related POIs
            count = std::max(count, 2) + std::uniform_int_distribution<int>(1, 2)(rng);
        } else if (props_->lore_tier >= 1) {
            // Tier 1: a few extra ruins
            count += std::uniform_int_distribution<int>(1, 2)(rng);
        }

        if (count > 0) {
            // On asteroids/airless, only 1x1 stamps
            int max_idx = (asteroid || airless) ? 1 : -1;
            place_n(ruin_stamps, ruin_stamp_count, count, max_idx);
        }
    }
```

Also add extra crashed ships for Tier 2+ systems. After the crashed ships block (around line 679), add:

```cpp
    // Extra crashed ships on lore-significant bodies
    if (props_->lore_tier >= 2) {
        int extra = std::uniform_int_distribution<int>(1, 2)(rng);
        int max_idx = (asteroid) ? 1 : -1;
        place_n(crashed_ship_stamps, crashed_ship_stamp_count, extra, max_idx);
    }
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/map_properties.h src/game_world.cpp src/generators/overworld_generator.cpp
git commit -m "Add lore-driven POI density — higher tier systems get more ruins and wrecks"
```

---

### Task 13: Civilization-Themed Dungeon Entry Text

When the player enters a dungeon on a lore-significant body, display the originating civilization's name and epoch in the entry message. The entry text system already exists in `src/game_world.cpp` at the `enter_dungeon_from_detail()` function (line 557).

**Files:**
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Add lore include**

At the top of `src/game_world.cpp`, add:

```cpp
#include "astra/lore_types.h"
```

- [ ] **Step 2: Add lore-themed entry text**

In `Game::enter_dungeon_from_detail()` (around line 557), find the switch statement for `ow_tile` and the `Tile::OW_Ruins` case (around line 585):

```cpp
        case Tile::OW_Ruins:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Corroded;
            enter_msg = "You enter the ancient ruins.";
            break;
```

Replace the `enter_msg` line with a lore-aware message:

```cpp
        case Tile::OW_Ruins:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Corroded;
            {
                const auto* civ = lore_civilization_at(
                    world_.lore(), world_.navigation().current_system_id);
                if (civ) {
                    std::ostringstream oss;
                    oss.precision(1);
                    oss << std::fixed;
                    oss << "You enter ruins of " << civ->short_name
                        << " origin, dating to " << civ->epoch_start_bya
                        << " billion years ago.";
                    lore_entry_msg_ = oss.str();
                    enter_msg = lore_entry_msg_.c_str();
                } else {
                    enter_msg = "You enter the ancient ruins.";
                }
            }
            break;
```

- [ ] **Step 3: Add lore_entry_msg_ member**

In `include/astra/game.h`, add a member variable in the private section:

```cpp
    std::string lore_entry_msg_;  // temp storage for lore-aware dungeon entry text
```

- [ ] **Step 4: Add sstream include**

At the top of `src/game_world.cpp`, add if not present:

```cpp
#include <sstream>
```

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 6: Test manually**

Run: `./build/astra-dev`

1. Start a new game
2. Type `:history` to note which systems have Tier 2-3
3. Travel to a lore-significant system, land on a body with ruins
4. Enter the ruins (press `>` on an `OW_Ruins` tile)
5. Verify entry text shows civilization name and epoch

- [ ] **Step 7: Commit**

```bash
git add src/game_world.cpp include/astra/game.h
git commit -m "Add civilization-themed dungeon entry text — ruins show origin era"
```

---

### Task 14: Archaeology Skill Category

Add the Archaeology skill category to the skill system with the skills defined in the spec: Ruin Reader, Artifact Identification, Excavation, Cultural Attunement, Precursor Linguist, and Beacon Sense. Follow the exact pattern of `Cat_Wayfinding` and its skills in `src/skill_defs.cpp`.

The category unlock base effect (civilization identification on dungeon entry) is already implemented by Task 13 — this task adds the skill tree and wires the base effect to require the category unlock.

**Files:**
- Modify: `include/astra/skill_defs.h`
- Modify: `src/skill_defs.cpp`
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Add Archaeology skill IDs**

In `include/astra/skill_defs.h`, add the category and skill IDs. After `Cartographer = 908`:

```cpp

    // Archaeology
    Cat_Archaeology = 10,
    RuinReader = 1000,
    ArtifactIdentification = 1001,
    Excavation = 1002,
    CulturalAttunement = 1003,
    PrecursorLinguist = 1004,
    BeaconSense = 1005,
```

- [ ] **Step 2: Add Archaeology category to skill_catalog()**

In `src/skill_defs.cpp`, add a new category entry after the `Cat_Wayfinding` block (before the closing `};` of the catalog vector, around line 139):

```cpp
        {SkillId::Cat_Archaeology, "Archaeology",
         "Knowledge of ancient civilizations and their ruins. "
         "Unlocks identification of which civilization built a ruin on dungeon entry.", 75, {
            {SkillId::RuinReader, "Ruin Reader",
             "Lore fragments found in ruins reveal more detail. Full text is "
             "displayed instead of partial fragments.",
             true, 50, 12, "Intelligence"},
            {SkillId::ArtifactIdentification, "Artifact Identification",
             "Unidentified ancient items are automatically identified on pickup. "
             "Saves time and resources otherwise spent on identification scrolls.",
             true, 75, 13, "Intelligence"},
            {SkillId::Excavation, "Excavation",
             "Search a ruin tile for hidden caches. Chance to find lore fragments "
             "or buried items beneath the surface.",
             false, 50, 12, "Intelligence"},
            {SkillId::CulturalAttunement, "Cultural Attunement",
             "Bonus to using artifacts from civilizations you have studied "
             "extensively. Effect strength scales with ruins explored.",
             true, 75, 14, "Intelligence"},
            {SkillId::PrecursorLinguist, "Precursor Linguist",
             "Read ancient inscriptions found on dungeon walls. Unlocks sealed "
             "doors and reveals hidden vault locations.",
             true, 100, 15, "Intelligence"},
            {SkillId::BeaconSense, "Beacon Sense",
             "Beacon network nodes glow on the star chart before you visit "
             "the system. Reveals the path toward Sgr A*.",
             true, 100, 16, "Intelligence"},
        }},
```

- [ ] **Step 3: Gate civilization identification on category unlock**

In `src/game_world.cpp`, in the `enter_dungeon_from_detail()` function, modify the lore entry text from Task 13 to check for the Archaeology skill. Update the `Tile::OW_Ruins` case:

Replace the lore_civilization_at block with:

```cpp
                const auto* civ = lore_civilization_at(
                    world_.lore(), world_.navigation().current_system_id);
                if (civ && player_has_skill(player_, SkillId::Cat_Archaeology)) {
                    std::ostringstream oss;
                    oss.precision(1);
                    oss << std::fixed;
                    oss << "You enter ruins of " << civ->short_name
                        << " origin, dating to " << civ->epoch_start_bya
                        << " billion years ago.";
                    lore_entry_msg_ = oss.str();
                    enter_msg = lore_entry_msg_.c_str();
                } else if (civ) {
                    enter_msg = "You enter ancient ruins of unknown origin.";
                } else {
                    enter_msg = "You enter the ancient ruins.";
                }
```

Add the include at the top of `src/game_world.cpp` if not present:

```cpp
#include "astra/skill_defs.h"
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 5: Test manually**

Run: `./build/astra-dev`

1. Start a new game
2. Open the skill tree (`k`)
3. Verify the "Archaeology" category appears with 6 skills
4. Navigate to a lore-significant system and enter ruins
5. Without the skill: entry text says "unknown origin"
6. Use `:skill Cat_Archaeology` (if available) to unlock, then re-enter ruins
7. With the skill: entry text shows civilization name and epoch

- [ ] **Step 6: Commit**

```bash
git add include/astra/skill_defs.h src/skill_defs.cpp src/game_world.cpp
git commit -m "Add Archaeology skill category — 6 skills for lore interaction"
```

---

### Task 15: Update Documentation

Update `docs/roadmap.md` to check off all implemented items from the world lore section that are now complete.

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update roadmap**

In `docs/roadmap.md`, under "World Generation", update the lore-related items:

From:
```
- [ ] **Procedural world lore** — billions of years of layered history, precursor civilizations, beacon network toward Sgr A*
- [ ] **Phoneme-based naming** — procedural civilization/figure/artifact names from curated syllable pools
- [ ] **Lore-driven galaxy shaping** — history determines system significance tiers, POI placement, dungeon theming
- [ ] **Legendary artifact generation** — unique items tied to historical figures and events
```

To:
```
- [x] **Procedural world lore** — billions of years of layered history, precursor civilizations, beacon network toward Sgr A*
- [x] **Phoneme-based naming** — procedural civilization/figure/artifact names from curated syllable pools
- [x] **Developer history log** — full timeline dump in dev mode
- [x] **Lore boot sequence** — visual epoch display during new game startup
- [x] **Lore-driven galaxy shaping** — history determines system significance tiers, POI placement, dungeon theming
- [x] **Star chart lore markers** — significant systems marked on star chart
- [x] **Archaeology skill category** — 6 skills for interacting with ruins and lore
- [ ] **Legendary artifact generation** — unique items tied to historical figures and events
```

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs: mark world lore phase 1+2 features as implemented"
```
