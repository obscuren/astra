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
    // Galactic-scale
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
    // System-scale
    ColonyFounded,
    Terraforming,
    OrbitalConstruction,
    SystemBattle,
    Plague,
    ScientificBreakthrough,
    CulturalRenaissance,
    FactionSchism,
    TradeRoute,
    MiningDisaster,
    // Planet-scale
    VaultDiscovered,
    UndergroundCity,
    WeaponTestSite,
    SacredSite,
    PrisonColony,
    LastStand,
    AlienBiology,
    SurfaceScared,
    AbandonedOutpost,
    CrashSite,
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

// ── Narrative styles ──
enum class RecordStyle : uint8_t {
    Official,       // dry, authoritative third-person
    Personal,       // first-person, emotional
    Legend,         // poetic, mythic third-person
    Scientific,     // clinical, precise
    Transmission,   // urgent, fragmented
};

enum class RecordReliability : uint8_t {
    Verified,
    Disputed,
    Myth,
    Propaganda,
};

// ── A discoverable lore text ──
struct LoreRecord {
    std::string title;
    std::string body;           // multi-sentence generated narrative
    RecordStyle style;
    RecordReliability reliability;
    std::string source;         // who wrote/recorded it
    int event_index = -1;       // which event this references
    int figure_index = -1;
    int artifact_index = -1;
    uint32_t system_id = 0;     // where this can be found
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

    std::string trait_summary;  // e.g. "AGG:15 CUR:8 IND:20 ..."

    PredecessorRelation predecessor_relation;
    SgrARelation sgra_relation;
    CollapseCause collapse_cause;

    float epoch_start_bya;      // billions of years ago
    float epoch_end_bya;
    uint32_t homeworld_system_id = 0;

    std::vector<LoreEvent> events;
    std::vector<KeyFigure> figures;
    std::vector<LoreArtifact> artifacts;
    std::vector<LoreRecord> records;
};

// ── Human epoch ──
struct HumanHistory {
    float arrival_bya;          // when humanity arrived (~0.01 = 10,000 years ago)
    float golden_age_start;
    float fracture_bya;
    std::vector<LoreEvent> events;
    std::vector<std::string> faction_names;
    std::vector<LoreRecord> records;
};

// ── Race origin in the lore ──
struct RaceOrigin {
    std::string race_name;
    int ancestor_civ_index = -1;     // which civilization they descend from (-1 = no precursor link)
    std::string origin_text;         // narrative description of their origin
    float origin_bya = 0.0f;         // when they emerged/split off
    LoreRecord starting_fragment;    // lore record the player starts with if playing this race
};

// ── Sim system snapshot (for galaxy integration) ──
struct LoreSystemData {
    uint32_t sim_id = 0;            // ID in the simulation
    float gx = 0, gy = 0;          // simulated position
    std::vector<int> ruin_civ_ids;  // civ indices that left ruins
    bool has_megastructure = false;
    int megastructure_builder = -1;
    bool beacon = false;
    bool battle_site = false;
    bool weapon_test_site = false;
    bool plague_origin = false;
    bool terraformed = false;
    int terraformed_by = -1;
    int lore_tier = 0;
};

// ── The complete world lore ──
struct WorldLore {
    unsigned seed = 0;
    std::vector<Civilization> civilizations;  // index 0 = Primordials
    HumanHistory humanity;
    std::vector<RaceOrigin> race_origins;     // origin stories for playable races
    std::vector<LoreSystemData> sim_systems;  // simulation system snapshots for galaxy mapping

    int total_beacons = 0;
    int active_beacons = 0;

    bool generated = false;
};

} // namespace astra
