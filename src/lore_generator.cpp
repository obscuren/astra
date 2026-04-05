#include "astra/lore_generator.h"
#include "astra/narrative_templates.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>

namespace astra {

// ── Enum-to-string helpers ──────────────────────────────────────────────────

static const char* architecture_name(Architecture a) {
    switch (a) {
    case Architecture::Crystalline:  return "crystalline";
    case Architecture::Organic:      return "organic";
    case Architecture::Geometric:    return "geometric";
    case Architecture::VoidCarved:   return "void-carved";
    case Architecture::LightWoven:   return "light-woven";
    }
    return "unknown";
}

static const char* tech_name(TechStyle t) {
    switch (t) {
    case TechStyle::Gravitational:       return "gravitational";
    case TechStyle::BioMechanical:       return "bio-mechanical";
    case TechStyle::QuantumLattice:      return "quantum-lattice";
    case TechStyle::HarmonicResonance:   return "harmonic-resonance";
    case TechStyle::PhaseShifting:       return "phase-shifting";
    }
    return "unknown";
}

static const char* philosophy_name(Philosophy p) {
    switch (p) {
    case Philosophy::Expansionist:   return "expansionist";
    case Philosophy::Contemplative:  return "contemplative";
    case Philosophy::Predatory:      return "predatory";
    case Philosophy::Symbiotic:      return "symbiotic";
    case Philosophy::Transcendent:   return "transcendent";
    }
    return "unknown";
}

static const char* collapse_name(CollapseCause c) {
    switch (c) {
    case CollapseCause::War:                return "war";
    case CollapseCause::Transcendence:      return "transcendence";
    case CollapseCause::Plague:             return "plague";
    case CollapseCause::ResourceExhaustion: return "resource exhaustion";
    case CollapseCause::SgrAObsession:      return "Sgr A* obsession";
    case CollapseCause::Unknown:            return "unknown";
    }
    return "unknown";
}

static const char* sgra_name(SgrARelation r) {
    switch (r) {
    case SgrARelation::ReachedAndVanished: return "reached and vanished";
    case SgrARelation::TriedAndFailed:     return "tried and failed";
    case SgrARelation::BuiltToward:        return "built toward";
    case SgrARelation::FledFrom:           return "fled from";
    case SgrARelation::Unaware:            return "unaware";
    }
    return "unknown";
}

static const char* predecessor_name(PredecessorRelation r) {
    switch (r) {
    case PredecessorRelation::Revered:   return "revered";
    case PredecessorRelation::Exploited: return "exploited";
    case PredecessorRelation::Feared:    return "feared";
    case PredecessorRelation::Ignored:   return "ignored";
    }
    return "unknown";
}

static const char* event_type_name(LoreEventType t) {
    switch (t) {
    case LoreEventType::Emergence:              return "Emergence";
    case LoreEventType::RuinDiscovery:          return "Ruin Discovery";
    case LoreEventType::Decipherment:           return "Decipherment";
    case LoreEventType::ReverseEngineering:     return "Reverse Engineering";
    case LoreEventType::Colonization:           return "Colonization";
    case LoreEventType::HyperspaceRoute:        return "Hyperspace Route";
    case LoreEventType::MegastructureBuilt:     return "Megastructure Built";
    case LoreEventType::FirstContact:           return "First Contact";
    case LoreEventType::CivilWar:               return "Civil War";
    case LoreEventType::ResourceWar:            return "Resource War";
    case LoreEventType::BorderConflict:         return "Border Conflict";
    case LoreEventType::ArtifactWar:            return "Artifact War";
    case LoreEventType::SgrADetection:          return "Sgr A* Detection";
    case LoreEventType::ConvergenceDiscovery:   return "Convergence Discovery";
    case LoreEventType::PrecursorBreakthrough:  return "Precursor Breakthrough";
    case LoreEventType::ArtifactCreation:       return "Artifact Creation";
    case LoreEventType::Transcendence:          return "Transcendence";
    case LoreEventType::SelfDestruction:        return "Self-Destruction";
    case LoreEventType::Consumption:            return "Consumption";
    case LoreEventType::Fragmentation:          return "Fragmentation";
    case LoreEventType::CollapseUnknown:        return "Collapse (Unknown)";
    case LoreEventType::VaultSealed:            return "Vault Sealed";
    case LoreEventType::GuardianCreated:        return "Guardian Created";
    case LoreEventType::ArtifactScattered:      return "Artifact Scattered";
    case LoreEventType::WarningEncoded:         return "Warning Encoded";
    // System-scale
    case LoreEventType::ColonyFounded:          return "Colony Founded";
    case LoreEventType::Terraforming:           return "Terraforming";
    case LoreEventType::OrbitalConstruction:    return "Orbital Construction";
    case LoreEventType::SystemBattle:           return "System Battle";
    case LoreEventType::Plague:                 return "Plague";
    case LoreEventType::ScientificBreakthrough: return "Breakthrough";
    case LoreEventType::CulturalRenaissance:    return "Renaissance";
    case LoreEventType::FactionSchism:          return "Faction Schism";
    case LoreEventType::TradeRoute:             return "Trade Route";
    case LoreEventType::MiningDisaster:         return "Mining Disaster";
    // Planet-scale
    case LoreEventType::VaultDiscovered:        return "Vault Discovered";
    case LoreEventType::UndergroundCity:        return "Underground City";
    case LoreEventType::WeaponTestSite:         return "Weapon Test";
    case LoreEventType::SacredSite:             return "Sacred Site";
    case LoreEventType::PrisonColony:           return "Prison Colony";
    case LoreEventType::LastStand:              return "Last Stand";
    case LoreEventType::AlienBiology:           return "Alien Biology";
    case LoreEventType::SurfaceScared:          return "Surface Scarred";
    case LoreEventType::AbandonedOutpost:       return "Abandoned Outpost";
    case LoreEventType::CrashSite:              return "Crash Site";
    }
    return "unknown";
}

static const char* archetype_name(FigureArchetype a) {
    switch (a) {
    case FigureArchetype::Founder:   return "The Founder";
    case FigureArchetype::Conqueror: return "The Conqueror";
    case FigureArchetype::Sage:      return "The Sage";
    case FigureArchetype::Traitor:   return "The Traitor";
    case FigureArchetype::Explorer:  return "The Explorer";
    case FigureArchetype::Last:      return "The Last";
    case FigureArchetype::Builder:   return "The Builder";
    }
    return "unknown";
}

static const char* category_name(ArtifactCategory c) {
    switch (c) {
    case ArtifactCategory::Weapon:          return "weapon";
    case ArtifactCategory::NavigationTool:  return "navigation tool";
    case ArtifactCategory::KnowledgeStore:  return "knowledge store";
    case ArtifactCategory::Key:             return "key";
    case ArtifactCategory::Anomaly:         return "anomaly";
    }
    return "unknown";
}

static const char* style_name(RecordStyle s) {
    switch (s) {
    case RecordStyle::Official:      return "Official";
    case RecordStyle::Personal:      return "Personal";
    case RecordStyle::Scientific:    return "Scientific";
    case RecordStyle::Legend:        return "Legend";
    case RecordStyle::Transmission:  return "Transmission";
    }
    return "Unknown";
}

static const char* reliability_name(RecordReliability r) {
    switch (r) {
    case RecordReliability::Verified:    return "verified";
    case RecordReliability::Disputed:    return "disputed";
    case RecordReliability::Myth:        return "myth";
    case RecordReliability::Propaganda:  return "propaganda";
    }
    return "unknown";
}

// ── Collapse-cause-to-event-type mapping ────────────────────────────────────

static LoreEventType collapse_event_type(CollapseCause cause) {
    switch (cause) {
    case CollapseCause::War:                return LoreEventType::SelfDestruction;
    case CollapseCause::Transcendence:      return LoreEventType::Transcendence;
    case CollapseCause::Plague:             return LoreEventType::Consumption;
    case CollapseCause::ResourceExhaustion: return LoreEventType::Fragmentation;
    case CollapseCause::SgrAObsession:      return LoreEventType::Transcendence;
    case CollapseCause::Unknown:            return LoreEventType::CollapseUnknown;
    }
    return LoreEventType::CollapseUnknown;
}

// ── Utility ─────────────────────────────────────────────────────────────────

static float uniform(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

static int uniform_int(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

// ── generate ────────────────────────────────────────────────────────────────

WorldLore LoreGenerator::generate(unsigned game_seed) {
    WorldLore lore;
    lore.seed = game_seed;

    std::mt19937 rng(game_seed ^ 0x4C4F5245u);

    // 4-8 precursor civilizations across billions of years
    int civ_count = 4 + static_cast<int>(rng() % 5);
    // Start deep — 5-8 billion years ago to fit 4-8 civilizations
    float cursor_bya = uniform(rng, 5.0f, 8.0f);

    // Track used phoneme pools to avoid duplicates.
    std::vector<int> used_pools;

    for (int i = 0; i < civ_count; ++i) {
        // Pick a unique phoneme pool.
        int pool_idx;
        do {
            pool_idx = static_cast<int>(rng() % phoneme_pool_count);
        } while (std::find(used_pools.begin(), used_pools.end(), pool_idx) != used_pools.end()
                 && static_cast<int>(used_pools.size()) < phoneme_pool_count);
        used_pools.push_back(pool_idx);

        auto civ = generate_civilization(rng, i, cursor_bya, lore.civilizations);

        NameGenerator namer(static_cast<PhonemePool>(pool_idx));
        civ.phoneme_pool = static_cast<PhonemePool>(pool_idx);

        // Name: "The X Y" pattern from civilization().
        civ.name = namer.civilization(rng);
        // Extract short_name: second word (the middle word of "The X Y").
        {
            auto first_space = civ.name.find(' ');
            if (first_space != std::string::npos) {
                auto second_space = civ.name.find(' ', first_space + 1);
                if (second_space != std::string::npos) {
                    civ.short_name = civ.name.substr(first_space + 1,
                                                     second_space - first_space - 1);
                } else {
                    civ.short_name = civ.name.substr(first_space + 1);
                }
            } else {
                civ.short_name = civ.name;
            }
        }

        generate_events(rng, civ, i, i > 0, lore.civilizations, namer);
        generate_figures(rng, civ, namer);
        generate_artifacts(rng, civ, namer);
        NarrativeGenerator::generate(rng, civ, namer, lore.civilizations);

        // Advance cursor: sometimes silence gap, sometimes overlap (contemporaneous)
        // 25% chance of overlap with previous civilization
        if (rng() % 4 == 0 && i > 0) {
            // Overlap: next civ starts during this civ's late period
            cursor_bya = civ.epoch_start_bya - (civ.epoch_start_bya - civ.epoch_end_bya) * 0.6f;
            // Add first contact event to the previous civilization
            auto& prev = lore.civilizations.back();
            prev.events.push_back({
                LoreEventType::FirstContact,
                cursor_bya,
                prev.short_name + " detects signals from a younger species emerging nearby"
            });
        } else {
            // Normal silence gap
            cursor_bya = civ.epoch_end_bya - uniform(rng, 0.05f, 0.4f);
        }

        lore.civilizations.push_back(std::move(civ));
    }

    // Human epoch.
    lore.humanity = generate_human_epoch(rng, lore.civilizations);

    // Generate race origins.
    generate_race_origins(rng, lore);

    // Count beacons from megastructure and hyperspace events.
    int total = 0;
    int active = 0;
    for (auto& civ : lore.civilizations) {
        for (auto& ev : civ.events) {
            if (ev.type == LoreEventType::MegastructureBuilt ||
                ev.type == LoreEventType::HyperspaceRoute) {
                ++total;
                // ~66% still active.
                if (rng() % 3 != 0) ++active;
            }
        }
    }
    lore.total_beacons = total;
    lore.active_beacons = active;
    lore.generated = true;

    return lore;
}

// ── generate_civilization ───────────────────────────────────────────────────

Civilization LoreGenerator::generate_civilization(
    std::mt19937& rng,
    int epoch_index,
    float epoch_start_bya,
    const std::vector<Civilization>& predecessors)
{
    Civilization civ;
    civ.epoch_start_bya = epoch_start_bya;

    float duration = uniform(rng, 0.1f, 0.8f);
    civ.epoch_end_bya = epoch_start_bya - duration;

    civ.architecture = static_cast<Architecture>(rng() % 5);
    civ.tech_style   = static_cast<TechStyle>(rng() % 5);
    civ.philosophy   = static_cast<Philosophy>(rng() % 5);
    civ.collapse_cause = static_cast<CollapseCause>(rng() % 6);

    if (epoch_index == 0) {
        // Primordials: no predecessor, biased SgrA relation.
        civ.predecessor_relation = PredecessorRelation::Ignored;
        int sgra_roll = static_cast<int>(rng() % 3);
        if (sgra_roll == 0)
            civ.sgra_relation = SgrARelation::ReachedAndVanished;
        else if (sgra_roll == 1)
            civ.sgra_relation = SgrARelation::BuiltToward;
        else
            civ.sgra_relation = static_cast<SgrARelation>(rng() % 5);
    } else {
        civ.predecessor_relation = static_cast<PredecessorRelation>(rng() % 4);
        civ.sgra_relation = static_cast<SgrARelation>(rng() % 5);
    }

    civ.homeworld_system_id = rng();

    return civ;
}

// ── generate_events (phased) ────────────────────────────────────────────────

void LoreGenerator::generate_events(
    std::mt19937& rng,
    Civilization& civ,
    int epoch_index,
    bool has_predecessors,
    const std::vector<Civilization>& predecessors,
    const NameGenerator& namer)
{
    float start = civ.epoch_start_bya;
    float end   = civ.epoch_end_bya;
    float span  = start - end;

    // Collect events in order; we'll assign times proportionally at the end.
    struct RawEvent {
        LoreEventType type;
        float fraction; // 0..1 through epoch
        std::string desc;
    };
    std::vector<RawEvent> raw;
    raw.reserve(40);

    auto push = [&](LoreEventType t, float frac, const std::string& d) {
        raw.push_back({t, frac, d});
    };

    // Get predecessor name for cross-references
    std::string pred_name = "an ancient species";
    std::string pred_short = "precursor";
    if (!predecessors.empty()) {
        const auto& prev = predecessors.back();
        pred_name = prev.name;
        pred_short = prev.short_name;
    }

    // Helper: generate a place name with a system id
    auto make_place = [&]() -> std::string { return namer.place(rng); };

    // ────────────────────────────────────────────────────────────────────────
    // Phase 1 (0.00-0.10): EMERGENCE — homeworld only
    // ────────────────────────────────────────────────────────────────────────
    {
        static const char* emergence_contemplative[] = {
            " consciousness stirs in the deep voids",
            " awakens to the whisper of starlight",
            " achieves collective awareness across a single world",
        };
        static const char* emergence_predatory[] = {
            " claws its way to sentience through ruthless competition",
            " emerges as the apex predator of a hostile world",
            " achieves dominance, then curiosity, then ambition",
        };
        static const char* emergence_default[] = {
            " consciousness emerges on their homeworld",
            " achieves sentience and begins to question the stars",
            " takes its first steps toward the void",
        };

        const char** emer_pool;
        switch (civ.philosophy) {
            case Philosophy::Contemplative:
            case Philosophy::Transcendent:
                emer_pool = emergence_contemplative; break;
            case Philosophy::Predatory:
            case Philosophy::Expansionist:
                emer_pool = emergence_predatory; break;
            default:
                emer_pool = emergence_default; break;
        }
        push(LoreEventType::Emergence, uniform(rng, 0.00f, 0.05f),
             civ.short_name + emer_pool[rng() % 3]);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 2 (0.10-0.20): EARLY DEVELOPMENT — still on homeworld
    // ────────────────────────────────────────────────────────────────────────
    {
        // Scientific breakthrough (planetary scale, on homeworld)
        {
            std::string desc;
            switch (rng() % 4) {
                case 0: desc = civ.short_name + " scientists unify the fundamental forces — a new era of physics begins"; break;
                case 1: desc = std::string("Breakthrough in ") + tech_name(civ.tech_style) + " theory on the " + civ.short_name + " homeworld"; break;
                case 2: desc = "First successful fusion reactor powers " + civ.short_name + " cities — the energy crisis ends"; break;
                default: desc = civ.short_name + " mathematicians discover the equations underlying void-space geometry"; break;
            }
            push(LoreEventType::ScientificBreakthrough, uniform(rng, 0.10f, 0.15f), desc);
        }

        // Cultural renaissance (homeworld)
        {
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "A great renaissance sweeps the " + civ.short_name + " homeworld — art, philosophy, and science flourish"; break;
                case 1: desc = "The " + civ.short_name + " Golden Age begins — a flowering of culture unlike anything before"; break;
                default: desc = civ.short_name + " philosophers articulate the drive toward the stars, igniting a cultural awakening"; break;
            }
            push(LoreEventType::CulturalRenaissance, uniform(rng, 0.15f, 0.20f), desc);
        }

        // Optional: alien biology discovered on homeworld (30%)
        if (rng() % 10 < 3) {
            push(LoreEventType::AlienBiology, uniform(rng, 0.12f, 0.18f),
                 "Deep ocean expeditions on the " + civ.short_name + " homeworld discover organisms of astonishing complexity — possibly a second genesis");
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 3 (0.20-0.35): SPACE AGE — expanding to nearby systems
    // ────────────────────────────────────────────────────────────────────────
    {
        // Orbital construction (homeworld orbit)
        {
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "First orbital station assembled above the " + civ.short_name + " homeworld — the space age begins"; break;
                case 1: desc = "Orbital shipyards above the homeworld begin constructing the first " + civ.short_name + " fleet"; break;
                default: desc = "A ring station is assembled in homeworld orbit — population reaches millions"; break;
            }
            push(LoreEventType::OrbitalConstruction, uniform(rng, 0.20f, 0.25f), desc);
        }

        // Colony founded (first off-world)
        {
            std::string place = make_place();
            push(LoreEventType::ColonyFounded, uniform(rng, 0.25f, 0.30f),
                 "First " + civ.short_name + " colony established on " + place + " — a new world for a young species");
        }

        // Predecessor discovery — only possible once they've left the homeworld
        if (has_predecessors) {
            static const char* ruin_templates[] = {
                "Explorers unearth the shattered spires of %s on a barren moon",
                "A mining expedition breaches a sealed %s vault deep underground",
                "%s ruins discovered beneath the ice of a frozen world",
                "An expedition stumbles upon %s structures, impossibly ancient",
                "The unmistakable geometry of %s architecture found on three worlds simultaneously",
            };
            std::string ruin_desc = ruin_templates[rng() % 5];
            auto pos = ruin_desc.find("%s");
            if (pos != std::string::npos)
                ruin_desc.replace(pos, 2, pred_short);
            push(LoreEventType::RuinDiscovery, uniform(rng, 0.28f, 0.33f), ruin_desc);

            if (rng() % 2 == 0) {
                static const char* decipher_templates[] = {
                    "Partial translation of %s data crystals reveals star charts pointing inward",
                    "%s inscriptions decoded — references to a 'convergence' at the galactic heart",
                    "A %s beacon activates when touched, broadcasting coordinates toward Sgr A*",
                };
                std::string dec_desc = decipher_templates[rng() % 3];
                pos = dec_desc.find("%s");
                if (pos != std::string::npos)
                    dec_desc.replace(pos, 2, pred_short);
                push(LoreEventType::Decipherment, uniform(rng, 0.30f, 0.35f), dec_desc);
            }

            // Relationship colors the reverse-engineering description
            std::string re_desc;
            switch (civ.predecessor_relation) {
                case PredecessorRelation::Revered:
                    re_desc = "Following " + pred_short + " designs with reverence, " +
                        civ.short_name + " rebuilds their technology";
                    break;
                case PredecessorRelation::Exploited:
                    re_desc = civ.short_name + " strip-mines " + pred_short +
                        " ruins, extracting technology without understanding its purpose";
                    break;
                case PredecessorRelation::Feared:
                    re_desc = "Despite the unsettling nature of " + pred_short +
                        " tech, " + civ.short_name + " cautiously reverse-engineers key systems";
                    break;
                default:
                    re_desc = pred_short + " technology adapted for " + civ.short_name + " use";
                    break;
            }
            push(LoreEventType::ReverseEngineering, uniform(rng, 0.32f, 0.35f), re_desc);
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 4 (0.35-0.55): INTERSTELLAR EXPANSION
    // ────────────────────────────────────────────────────────────────────────
    {
        // Colonization (many systems)
        {
            int sys_count = 5 + uniform_int(rng, 0, 30);
            static const char* expansion_templates[] = {
                "%s colonies established across %d nearby star systems",
                "The first %s generation ships depart, seeding %d systems",
                "%s expansion wave reaches %d systems within a millennium",
            };
            std::string exp_desc = expansion_templates[rng() % 3];
            auto pos = exp_desc.find("%s");
            if (pos != std::string::npos) exp_desc.replace(pos, 2, civ.short_name);
            pos = exp_desc.find("%d");
            if (pos != std::string::npos) exp_desc.replace(pos, 2, std::to_string(sys_count));
            push(LoreEventType::Colonization, uniform(rng, 0.35f, 0.40f), exp_desc);
        }

        // Hyperspace route
        {
            int net_count = 3 + uniform_int(rng, 0, 15);
            static const char* hyperspace_templates[] = {
                "First stable hyperspace conduit links the inner colonies",
                "The %s hyperspace network connects %d systems, enabling rapid expansion",
                "A breakthrough in void-folding allows instantaneous transit between key worlds",
            };
            std::string hs_desc = hyperspace_templates[rng() % 3];
            auto pos = hs_desc.find("%s");
            if (pos != std::string::npos) hs_desc.replace(pos, 2, civ.short_name);
            pos = hs_desc.find("%d");
            if (pos != std::string::npos) hs_desc.replace(pos, 2, std::to_string(net_count));
            push(LoreEventType::HyperspaceRoute, uniform(rng, 0.40f, 0.45f), hs_desc);
        }

        // Colony founded events (2-3, with place names)
        int colony_count = uniform_int(rng, 2, 3);
        for (int i = 0; i < colony_count; ++i) {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "Colony established on " + place + " — becomes a major " + civ.short_name + " settlement"; break;
                case 1: desc = civ.short_name + " settlers land on " + place + " — a thriving outpost within a generation"; break;
                default: desc = place + " claimed and colonized — its rich resources fuel " + civ.short_name + " expansion"; break;
            }
            push(LoreEventType::ColonyFounded, uniform(rng, 0.42f, 0.55f), desc);
        }

        // Trade routes between named places
        {
            std::string place1 = make_place();
            std::string place2 = make_place();
            switch (rng() % 2) {
                case 0: push(LoreEventType::TradeRoute, uniform(rng, 0.45f, 0.55f),
                    "The " + place1 + "-" + place2 + " corridor becomes the most vital trade route in " + civ.short_name + " space"); break;
                default: push(LoreEventType::TradeRoute, uniform(rng, 0.45f, 0.55f),
                    "Merchant guilds establish the " + place1 + " Exchange — rare materials from " + std::to_string(uniform_int(rng, 5, 30)) + " systems pass through"); break;
            }
        }

        // Terraforming project
        if (rng() % 2 == 0) {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = place + " terraformed into a garden world — oceans and forests bloom on a barren rock"; break;
                case 1: desc = "Centuries-long terraforming project transforms " + place + " into a habitable paradise"; break;
                default: desc = civ.short_name + " engineers reshape " + place + "'s atmosphere — the first children are born under open sky"; break;
            }
            push(LoreEventType::Terraforming, uniform(rng, 0.45f, 0.55f), desc);
        }

        // Mining operation
        if (rng() % 2 == 0) {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "Deep core mining on " + place + " triggers seismic collapse — thousands trapped"; break;
                case 1: desc = "Asteroid mining accident near " + place + " scatters debris across shipping lanes"; break;
                default: desc = "Excavation on " + place + " breaches a " + pred_short + " containment chamber — unknown substance released"; break;
            }
            push(LoreEventType::MiningDisaster, uniform(rng, 0.45f, 0.55f), desc);
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 5 (0.55-0.70): PEAK CIVILIZATION
    // ────────────────────────────────────────────────────────────────────────
    {
        // Megastructure built
        {
            static const char* mega_templates[] = {
                "A void-gate large enough to swallow moons is constructed at the galactic rim",
                "An orbital ring encircles a gas giant — the largest structure ever built",
                "A beacon array spanning three systems begins broadcasting toward the core",
                "Construction of a Dyson lattice begins around a dying star",
                "A space station the size of a small world is completed after centuries of work",
            };
            push(LoreEventType::MegastructureBuilt, uniform(rng, 0.55f, 0.62f),
                 mega_templates[rng() % 5]);
        }

        // Scientific breakthrough
        {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 4) {
                case 0: desc = "Researchers on " + place + " achieve controlled singularity containment — power generation leaps forward"; break;
                case 1: desc = "A " + civ.short_name + " physicist on " + place + " proves faster-than-light communication is possible"; break;
                case 2: desc = "The " + place + " Observatory detects structured signals from deep within Sgr A*"; break;
                default: desc = std::string("Breakthrough in ") + tech_name(civ.tech_style) + " technology at " + place + " research station"; break;
            }
            push(LoreEventType::ScientificBreakthrough, uniform(rng, 0.58f, 0.65f), desc);
        }

        // Cultural renaissance
        {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = place + " becomes the cultural heart of " + civ.short_name + " civilization — artists, philosophers, and dreamers gather"; break;
                case 1: desc = "The " + place + " Renaissance — a flowering of art, science, and exploration that defines the era"; break;
                default: desc = "A new philosophical movement on " + place + " redefines " + civ.short_name + "'s relationship with the void"; break;
            }
            push(LoreEventType::CulturalRenaissance, uniform(rng, 0.60f, 0.68f), desc);
        }

        // Sgr A* detection
        {
            std::string desc;
            switch (rng() % 4) {
                case 0: desc = "Deep-space sensors detect anomalous gravitational waves from the galactic center"; break;
                case 1: desc = "A " + pred_short + " beacon fragment reveals coordinates pointing to Sgr A*"; break;
                case 2: desc = "Astronomers observe impossible light patterns emanating from Sgr A*"; break;
                default: desc = civ.short_name + " physicists prove that Sgr A* is not merely a black hole"; break;
            }
            push(LoreEventType::SgrADetection, uniform(rng, 0.58f, 0.65f), desc);
        }

        // Artifact creation
        {
            std::string desc;
            switch (rng() % 4) {
                case 0: desc = "The greatest " + civ.short_name + " artificers forge a weapon of terrible power"; break;
                case 1: desc = "A navigation device is created that can sense the beacon network across light-years"; break;
                case 2: desc = "A knowledge crystal is encoded with the sum of " + civ.short_name + " understanding"; break;
                default: desc = "An artifact of unknown purpose is constructed — it hums when pointed at the core"; break;
            }
            push(LoreEventType::ArtifactCreation, uniform(rng, 0.62f, 0.70f), desc);
        }

        // Convergence discovery
        {
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = civ.short_name + " scholars realize all precursor beacon paths converge at Sgr A*"; break;
                case 1: desc = "Analysis of " + pred_short + " records reveals the convergence pattern — everything points inward"; break;
                default: desc = "The mathematical proof is undeniable: every civilization's journey ends at the center"; break;
            }
            push(LoreEventType::ConvergenceDiscovery, uniform(rng, 0.63f, 0.70f), desc);
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 6 (0.70-0.85): TENSIONS & CONFLICT
    // ────────────────────────────────────────────────────────────────────────
    {
        // Civil war / faction schism
        {
            std::string desc;
            switch (rng() % 4) {
                case 0: desc = "The outer colonies declare independence — the " + civ.short_name + " civil war begins"; break;
                case 1: desc = "Ideological schism splits " + civ.short_name + " into two hostile factions"; break;
                case 2: desc = "A dispute over " + pred_short + " artifacts escalates into open warfare"; break;
                default: desc = "The " + civ.short_name + " leadership fractures over the question of Sgr A*"; break;
            }
            push(LoreEventType::CivilWar, uniform(rng, 0.70f, 0.75f), desc);
        }

        // Resource war
        {
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "Rare element deposits trigger conflict between " + civ.short_name + " factions"; break;
                case 1: desc = "Control of hyperspace fuel sources becomes the defining struggle of the era"; break;
                default: desc = "Wars over " + pred_short + " technology caches devastate entire systems"; break;
            }
            push(LoreEventType::ResourceWar, uniform(rng, 0.72f, 0.78f), desc);
        }

        // System battles (1-2, with place names)
        int battle_count = uniform_int(rng, 1, 2);
        for (int i = 0; i < battle_count; ++i) {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 4) {
                case 0: desc = "The Battle of " + place + " — " + std::to_string(uniform_int(rng, 20, 200)) + " ships destroyed in the largest engagement of the epoch"; break;
                case 1: desc = "Rebel fleet ambushes loyalist forces above " + place + " — the system changes hands"; break;
                case 2: desc = "Siege of " + place + " lasts " + std::to_string(uniform_int(rng, 3, 50)) + " years before the defenders surrender"; break;
                default: desc = place + " system ravaged by running battles — debris field makes navigation hazardous for millennia"; break;
            }
            push(LoreEventType::SystemBattle, uniform(rng, 0.73f, 0.82f), desc);
        }

        // Plague (50%)
        if (rng() % 2 == 0) {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "A pathogen from " + place + "'s biosphere devastates three neighboring colonies"; break;
                case 1: desc = "Quarantine declared across the " + place + " system — millions perish before a cure is found"; break;
                default: desc = "Biological contamination from a breached " + pred_short + " facility on " + place + " triggers a pandemic"; break;
            }
            push(LoreEventType::Plague, uniform(rng, 0.74f, 0.82f), desc);
        }

        // Weapon test site (40%)
        if (rng() % 5 < 2) {
            std::string place = make_place();
            push(LoreEventType::WeaponTestSite, uniform(rng, 0.75f, 0.83f),
                 civ.short_name + " tests a devastating weapon on " + place + "'s far side — the crater is visible from orbit for eternity");
        }

        // Prison colony (30%)
        if (rng() % 10 < 3) {
            std::string place = make_place();
            push(LoreEventType::PrisonColony, uniform(rng, 0.73f, 0.82f),
                 place + " is converted into an exile world — dissidents and war criminals are marooned on its surface");
        }

        // Sacred site (40%)
        if (rng() % 5 < 2) {
            std::string place = make_place();
            push(LoreEventType::SacredSite, uniform(rng, 0.72f, 0.80f),
                 place + " is declared sacred ground after a " + civ.short_name + " mystic experiences a vision of the convergence");
        }

        // Faction schism (50%)
        if (rng() % 2 == 0) {
            std::string place = make_place();
            std::string desc;
            switch (rng() % 3) {
                case 0: desc = "The " + place + " Secession — an entire sector declares independence from " + civ.short_name + " central authority"; break;
                case 1: desc = "Rival factions on " + place + " establish competing governments — the system becomes a divided world"; break;
                default: desc = "Religious movement on " + place + " rejects " + civ.short_name + " orthodoxy, founding a breakaway sect"; break;
            }
            push(LoreEventType::FactionSchism, uniform(rng, 0.74f, 0.83f), desc);
        }

        // Inter-civilization interaction (30% chance if predecessors exist and overlapping epochs)
        if (has_predecessors && rng() % 10 < 3) {
            std::string inter_desc;
            switch (rng() % 4) {
                case 0: inter_desc = "A dormant " + pred_short + " defense system awakens, destroying two " + civ.short_name + " colony ships"; break;
                case 1: inter_desc = civ.short_name + " expedition finds a sealed " + pred_short + " vault containing a dire warning about Sgr A*"; break;
                case 2: inter_desc = "A " + pred_short + " guardian construct is captured alive — it speaks of the convergence in a dead language"; break;
                default: inter_desc = civ.short_name + " engineers accidentally activate a " + pred_short + " weapon, sterilizing an entire moon"; break;
            }
            push(LoreEventType::PrecursorBreakthrough, uniform(rng, 0.70f, 0.82f), inter_desc);
        }
    }

    // ── Contemporaneous civilization interactions ──
    // Check if any predecessor's epoch overlaps with ours
    for (size_t pi = 0; pi < predecessors.size(); ++pi) {
        const auto& other = predecessors[pi];
        if (other.epoch_end_bya < civ.epoch_start_bya &&
            other.epoch_end_bya > civ.epoch_end_bya) {
            float overlap_start = std::min(civ.epoch_start_bya, other.epoch_start_bya);
            float overlap_end = std::max(civ.epoch_end_bya, other.epoch_end_bya);
            float overlap_frac_start = (start - overlap_start) / span;
            float overlap_frac_end = (start - overlap_end) / span;
            if (overlap_frac_start < 0.0f) overlap_frac_start = 0.1f;
            if (overlap_frac_end > 0.9f) overlap_frac_end = 0.85f;

            int interaction_count = uniform_int(rng, 1, 3);
            for (int ic = 0; ic < interaction_count; ++ic) {
                float frac = uniform(rng, overlap_frac_start, overlap_frac_end);
                std::string desc;
                switch (rng() % 8) {
                    case 0:
                        desc = "First contact between " + civ.short_name + " and " + other.short_name + " — cautious diplomatic exchanges begin";
                        push(LoreEventType::FirstContact, frac, desc);
                        break;
                    case 1:
                        desc = civ.short_name + " and " + other.short_name + " forge a trade alliance — technology and culture flow between species";
                        push(LoreEventType::TradeRoute, frac, desc);
                        break;
                    case 2:
                        desc = "Border skirmish between " + civ.short_name + " and " + other.short_name + " fleets in a contested system";
                        push(LoreEventType::BorderConflict, frac, desc);
                        break;
                    case 3: {
                        std::string place = make_place();
                        desc = "The " + place + " Accords — " + civ.short_name + " and " + other.short_name + " establish a shared research station";
                        push(LoreEventType::ScientificBreakthrough, frac, desc);
                        break;
                    }
                    case 4:
                        desc = "War erupts between " + civ.short_name + " and " + other.short_name + " over a " + pred_short + " artifact cache";
                        push(LoreEventType::ArtifactWar, frac, desc);
                        break;
                    case 5:
                        desc = civ.short_name + " missionaries attempt to convert " + other.short_name + " populations — met with fascination and resistance";
                        push(LoreEventType::CulturalRenaissance, frac, desc);
                        break;
                    case 6:
                        desc = "Joint " + civ.short_name + "-" + other.short_name + " expedition reaches deeper toward Sgr A* than either could alone";
                        push(LoreEventType::ConvergenceDiscovery, frac, desc);
                        break;
                    default:
                        desc = other.short_name + " refugees flee to " + civ.short_name + " space as their civilization fragments";
                        push(LoreEventType::ColonyFounded, frac, desc);
                        break;
                }
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 7 (0.85-0.90): DECLINE
    // ────────────────────────────────────────────────────────────────────────
    {
        // Abandoned outposts (1-2)
        int outpost_count = uniform_int(rng, 1, 2);
        for (int i = 0; i < outpost_count; ++i) {
            std::string place = make_place();
            push(LoreEventType::AbandonedOutpost, uniform(rng, 0.85f, 0.88f),
                 "A " + civ.short_name + " research outpost on " + place + " is abandoned after contact is lost — rescue teams find only silence");
        }

        // Last stand (50%)
        if (rng() % 2 == 0) {
            std::string place = make_place();
            push(LoreEventType::LastStand, uniform(rng, 0.86f, 0.89f),
                 "The last loyalist garrison makes their stand on " + place + " — they hold for " + std::to_string(uniform_int(rng, 1, 20)) + " years before falling");
        }

        // Underground city (40%) — sheltering from collapse
        if (rng() % 5 < 2) {
            std::string place = make_place();
            push(LoreEventType::UndergroundCity, uniform(rng, 0.85f, 0.89f),
                 "An immense underground city is carved into " + place + "'s mantle — it houses " + std::to_string(uniform_int(rng, 1, 50)) + " million souls sheltering from the collapse");
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Phase 8 (0.90-1.00): COLLAPSE & LEGACY
    // ────────────────────────────────────────────────────────────────────────
    {
        // Collapse event
        std::string collapse_desc;
        switch (civ.collapse_cause) {
            case CollapseCause::Transcendence:
                collapse_desc = "In a single galactic heartbeat, " + civ.short_name +
                    " abandons physical form and streams toward Sgr A* as pure energy";
                break;
            case CollapseCause::War:
                collapse_desc = "The final " + civ.short_name +
                    " war consumes everything — worlds burn, stations fall silent, the void reclaims all";
                break;
            case CollapseCause::Plague:
                collapse_desc = "A pathogen — perhaps released from a " + pred_short +
                    " vault — sweeps through " + civ.short_name + " space, leaving only empty cities";
                break;
            case CollapseCause::ResourceExhaustion:
                collapse_desc = civ.short_name +
                    " expansion outpaces supply — worlds are stripped bare, colonies wither, the network fragments";
                break;
            case CollapseCause::SgrAObsession:
                collapse_desc = "Consumed by the mystery of Sgr A*, " + civ.short_name +
                    " pours everything into reaching the center — and in reaching, is unmade";
                break;
            default:
                collapse_desc = civ.short_name +
                    " vanishes from the galaxy. No records survive to explain why. Only ruins remain.";
                break;
        }
        push(collapse_event_type(civ.collapse_cause), 0.92f, collapse_desc);

        // Vault sealed (50%)
        if (rng() % 2 == 0) {
            push(LoreEventType::VaultSealed, 0.95f,
                 "In their final days, " + civ.short_name +
                 " seals knowledge vaults across " + std::to_string(2 + rng() % 8) +
                 " systems — a message to whoever comes next");
        }

        // Guardian created (33%)
        if (rng() % 3 == 0) {
            push(LoreEventType::GuardianCreated, 0.97f,
                 "Autonomous " + civ.short_name +
                 " guardians are activated with a single directive: protect the path to the center");
        }

        // Crash sites (40%) — ships fleeing the collapse
        if (rng() % 5 < 2) {
            std::string place = make_place();
            push(LoreEventType::CrashSite, uniform(rng, 0.93f, 0.99f),
                 "A " + civ.short_name + " capital ship crashes on " + place + " during a hyperspace malfunction — wreckage spans kilometers");
        }
    }

    // Sort by fraction and assign real times.
    std::sort(raw.begin(), raw.end(),
              [](auto& a, auto& b) { return a.fraction < b.fraction; });

    civ.events.reserve(raw.size());
    for (auto& r : raw) {
        LoreEvent ev;
        ev.type = r.type;
        ev.time_bya = start - r.fraction * span;
        ev.description = std::move(r.desc);
        ev.system_id = rng();
        civ.events.push_back(std::move(ev));
    }
}

// ── generate_figures ────────────────────────────────────────────────────────

void LoreGenerator::generate_figures(
    std::mt19937& rng,
    Civilization& civ,
    const NameGenerator& namer)
{
    int count = uniform_int(rng, 3, 6);
    civ.figures.reserve(count);

    // Always include Founder and Last.
    std::vector<FigureArchetype> archetypes = {
        FigureArchetype::Founder,
        FigureArchetype::Last,
    };

    static const FigureArchetype fill_types[] = {
        FigureArchetype::Conqueror,
        FigureArchetype::Sage,
        FigureArchetype::Traitor,
        FigureArchetype::Explorer,
        FigureArchetype::Builder,
    };

    while (static_cast<int>(archetypes.size()) < count) {
        archetypes.push_back(fill_types[rng() % 5]);
    }

    for (auto arch : archetypes) {
        KeyFigure fig;
        fig.name = namer.name(rng);
        fig.title = namer.title(rng, arch);
        fig.archetype = arch;
        fig.system_id = rng();

        // Achievement by archetype.
        switch (arch) {
        case FigureArchetype::Founder:
            fig.achievement = "United the first " + civ.short_name + " colonies";
            break;
        case FigureArchetype::Conqueror:
            fig.achievement = "Conquered twelve systems in a single campaign";
            break;
        case FigureArchetype::Sage:
            fig.achievement = "Deciphered the precursor convergence theorem";
            break;
        case FigureArchetype::Traitor:
            fig.achievement = "Betrayed the central council, fracturing the alliance";
            break;
        case FigureArchetype::Explorer:
            fig.achievement = "Charted the first route toward the galactic core";
            break;
        case FigureArchetype::Last:
            fig.achievement = "Sealed the final vault before the silence";
            break;
        case FigureArchetype::Builder:
            fig.achievement = "Designed the great megastructure at the void's edge";
            break;
        }

        // Fate varies by archetype and collapse cause.
        switch (arch) {
        case FigureArchetype::Founder:
            fig.fate = "remembered in every ruin";
            break;
        case FigureArchetype::Conqueror:
            fig.fate = (civ.collapse_cause == CollapseCause::War)
                ? "killed in the final battle" : "vanished beyond the frontier";
            break;
        case FigureArchetype::Sage:
            fig.fate = (civ.collapse_cause == CollapseCause::Transcendence)
                ? "transcended with the last of the enlightened"
                : "entombed with their writings";
            break;
        case FigureArchetype::Traitor:
            fig.fate = "erased from official records";
            break;
        case FigureArchetype::Explorer:
            fig.fate = (civ.sgra_relation == SgrARelation::ReachedAndVanished)
                ? "entered Sgr A* and was never seen again"
                : "lost in uncharted space";
            break;
        case FigureArchetype::Last:
            fig.fate = "fate unknown — last transmission unfinished";
            break;
        case FigureArchetype::Builder:
            fig.fate = "merged with their creation";
            break;
        }

        civ.figures.push_back(std::move(fig));
    }
}

// ── generate_artifacts ──────────────────────────────────────────────────────

void LoreGenerator::generate_artifacts(
    std::mt19937& rng,
    Civilization& civ,
    const NameGenerator& namer)
{
    int count = uniform_int(rng, 1, 3);
    civ.artifacts.reserve(count);

    for (int i = 0; i < count; ++i) {
        LoreArtifact art;
        art.category = static_cast<ArtifactCategory>(rng() % 5);
        art.name = namer.artifact(rng, art.category);
        art.system_id = rng();

        // Link to a random figure if possible.
        if (!civ.figures.empty()) {
            int fig_idx = static_cast<int>(rng() % civ.figures.size());
            art.figure_index = fig_idx;
            art.origin_text = "Forged by " + civ.figures[fig_idx].name
                            + " of the " + civ.short_name + ".";
        } else {
            art.origin_text = "Created by the " + civ.short_name
                            + " in their final epoch.";
        }

        // Effect by category.
        switch (art.category) {
        case ArtifactCategory::Weapon:
            art.effect_text = "+3 attack power when wielded in combat";
            break;
        case ArtifactCategory::NavigationTool:
            art.effect_text = "Reveals hidden hyperspace routes on the star chart";
            break;
        case ArtifactCategory::KnowledgeStore:
            art.effect_text = "Unlocks precursor research topics when studied";
            break;
        case ArtifactCategory::Key:
            art.effect_text = "Opens sealed vaults left by the " + civ.short_name;
            break;
        case ArtifactCategory::Anomaly:
            art.effect_text = "Emits unpredictable energy — effects vary per use";
            break;
        }

        civ.artifacts.push_back(std::move(art));
    }
}

// generate_records replaced by NarrativeGenerator::generate()

// ── generate_human_epoch ────────────────────────────────────────────────────

HumanHistory LoreGenerator::generate_human_epoch(
    std::mt19937& rng,
    const std::vector<Civilization>& civilizations)
{
    HumanHistory h;
    h.arrival_bya      = 0.01f;   // 10,000 years ago
    h.golden_age_start = 0.005f;  // 5,000 years ago
    h.fracture_bya     = 0.001f;  // 1,000 years ago

    h.events.push_back({LoreEventType::Emergence, h.arrival_bya,
        "Humanity arrives at The Heavens Above", 0});

    h.events.push_back({LoreEventType::RuinDiscovery, 0.008f,
        "First precursor ruins discovered in the outer systems", 0});

    h.events.push_back({LoreEventType::ReverseEngineering, h.golden_age_start,
        "Precursor hyperspace technology reverse-engineered", 0});

    h.events.push_back({LoreEventType::Colonization, 0.003f,
        "Human colonies spread across dozens of systems", 0});

    h.events.push_back({LoreEventType::Fragmentation, h.fracture_bya,
        "The great fracture — human factions splinter", 0});

    // Generate 2-4 faction names.
    static const char* adjectives[] = {
        "Crimson", "Iron", "Azure", "Obsidian", "Stellar",
        "Void", "Solar", "Gilded", "Silent", "Radiant",
    };
    static const char* templates[] = {
        "Compact", "Sovereignty", "Dominion", "Confederacy",
        "Alliance", "Republic", "Collective", "Accord",
    };

    int faction_count = uniform_int(rng, 2, 4);
    for (int i = 0; i < faction_count; ++i) {
        std::string faction = std::string(adjectives[rng() % 10])
                            + " " + templates[rng() % 8];
        h.faction_names.push_back(std::move(faction));
    }

    return h;
}

// ── generate_race_origins ──────────────────────────────────────────────────

void LoreGenerator::generate_race_origins(std::mt19937& rng, WorldLore& lore) {
    if (lore.civilizations.size() < 2) return;

    // ── Stellari — descendants of one of the first two precursor civilizations ──
    {
        RaceOrigin origin;
        origin.race_name = "Stellari";
        // Always from epoch 0 or 1 (the earliest civilizations)
        origin.ancestor_civ_index = (lore.civilizations.size() > 1 && rng() % 2 == 0) ? 1 : 0;
        const auto& ancestor = lore.civilizations[origin.ancestor_civ_index];

        // They split off during the ancestor's late period or survived the collapse
        origin.origin_bya = ancestor.epoch_end_bya + uniform(rng, 0.01f, 0.1f);

        static const char* stellari_origins[] = {
            "When %s fell, a splinter colony survived in the deep void between stars. "
            "Over billions of years they evolved, their bodies drinking starlight, their minds expanding beyond flesh. "
            "They became the Stellari — luminous inheritors of the oldest knowledge in the galaxy.",

            "A faction of %s refused to follow their civilization into collapse. "
            "They retreated to a hidden system near a stable star and waited. "
            "Generations became epochs. Epochs became eons. They forgot their name but not their purpose — "
            "to tend the beacon network, to keep the path open. The galaxy calls them Stellari now.",

            "The Stellari do not speak of their origins to outsiders. But their genetic markers "
            "are unmistakable — they carry sequences identical to %s biological samples found in the deepest ruins. "
            "They are what remains of the first great civilization, changed beyond recognition but still carrying the flame.",
        };

        std::string text = stellari_origins[rng() % 3];
        auto pos = text.find("%s");
        if (pos != std::string::npos) text.replace(pos, 2, ancestor.short_name);
        pos = text.find("%s");
        if (pos != std::string::npos) text.replace(pos, 2, ancestor.short_name);
        origin.origin_text = text;

        // Starting lore fragment
        origin.starting_fragment.title = "Fragment of the Old Memory";
        origin.starting_fragment.style = RecordStyle::Legend;
        origin.starting_fragment.reliability = RecordReliability::Myth;
        origin.starting_fragment.source = "Stellari oral tradition";
        origin.starting_fragment.body =
            "Before the silence, before the long dark, we were part of something greater. "
            "The " + ancestor.name + " built the first roads between stars. "
            "We walked those roads. We remember their songs, though the words have changed. "
            "The beacons still call to us — a hum in our luminescence, a pull toward the center. "
            "We are the last thread of a tapestry woven across billions of years. "
            "Whatever waits at Sgr A*, our ancestors knew its name.";

        lore.race_origins.push_back(std::move(origin));
    }

    // ── Sylphari — emerged during a silence, possibly from precursor terraforming ──
    {
        RaceOrigin origin;
        origin.race_name = "Sylphari";

        // Pick a mid-era civilization whose terraforming might have produced them
        int mid = static_cast<int>(lore.civilizations.size()) / 2;
        if (mid < 1) mid = 1;
        origin.ancestor_civ_index = mid;
        const auto& ancestor = lore.civilizations[mid];

        // They emerged in the silence after that civilization
        origin.origin_bya = ancestor.epoch_end_bya - uniform(rng, 0.1f, 0.5f);

        static const char* sylphari_origins[] = {
            "On a world terraformed by %s engineers and then abandoned, life took an unexpected path. "
            "The luminescent organisms left in the modified biosphere evolved sentience over millions of years. "
            "The Sylphari emerged — wispy, ethereal, made of light as much as matter — "
            "carrying fragments of %s bioengineering in every cell.",

            "The Sylphari have no creation myth because they remember their own beginning. "
            "They awoke in a garden world shaped by %s hands, long after those hands had turned to dust. "
            "They grew among the ruins, their bioluminescence feeding on the residual energy of %s technology. "
            "To the Sylphari, the precursor ruins are not alien — they are the nursery where their species was born.",

            "Whether the %s intended to create the Sylphari is a matter of debate. "
            "Their terraforming records suggest a world-scale biological experiment — "
            "adaptive organisms designed to maintain ecosystem balance after the builders departed. "
            "The Sylphari evolved far beyond that original purpose, developing consciousness, culture, and a deep "
            "affinity for the living worlds their creators left behind.",
        };

        std::string text = sylphari_origins[rng() % 3];
        // Replace all %s
        for (;;) {
            auto pos = text.find("%s");
            if (pos == std::string::npos) break;
            text.replace(pos, 2, ancestor.short_name);
        }
        origin.origin_text = text;

        origin.starting_fragment.title = "The Garden and the Silence";
        origin.starting_fragment.style = RecordStyle::Personal;
        origin.starting_fragment.reliability = RecordReliability::Verified;
        origin.starting_fragment.source = "Sylphari ancestral memory";
        origin.starting_fragment.body =
            "We were born in a garden that someone else planted. "
            "The " + ancestor.short_name + " shaped our world — the luminescent groves, the singing rivers, "
            "the air that tastes of ozone and possibility. They left before we opened our eyes. "
            "But we feel them in the soil, in the hum of the old machines buried beneath the roots. "
            "When we wander the stars now, we are looking for the gardeners. We want to understand why they left. "
            "And perhaps, if we follow the beacons far enough inward, we will find them waiting.";

        lore.race_origins.push_back(std::move(origin));
    }

    // ── Veldrani — contemporaneous with humans, traders and diplomats ──
    {
        RaceOrigin origin;
        origin.race_name = "Veldrani";
        origin.ancestor_civ_index = -1; // no precursor link
        origin.origin_bya = 0.012f; // slightly before humans

        origin.origin_text =
            "The Veldrani evolved on a water-rich world in the outer rim, developing spaceflight "
            "roughly two millennia before humanity. Their tall, blue-skinned physiology adapted to "
            "a high-gravity aquatic environment. First contact with humans was peaceful — the Veldrani "
            "had already established trade networks across a dozen systems and welcomed new partners. "
            "Their diplomatic traditions and mercantile culture made them natural intermediaries "
            "in the fractured post-Fracture galaxy.";

        origin.starting_fragment.title = "The Trader's Creed";
        origin.starting_fragment.style = RecordStyle::Official;
        origin.starting_fragment.reliability = RecordReliability::Verified;
        origin.starting_fragment.source = "Veldrani Merchant Guild Charter";
        origin.starting_fragment.body =
            "We sail between the stars as our ancestors sailed between the islands — with an open hold "
            "and an open hand. Every species we meet is a new port, every culture a new cargo. "
            "The Veldrani do not conquer. We trade. We connect. We remember every debt and every kindness. "
            "In a galaxy of ruins and silence, we are the thread that binds what remains.";

        lore.race_origins.push_back(std::move(origin));
    }

    // ── Kreth — contemporaneous with humans, engineers and miners ──
    {
        RaceOrigin origin;
        origin.race_name = "Kreth";
        origin.ancestor_civ_index = -1;
        origin.origin_bya = 0.008f; // slightly after humans

        origin.origin_text =
            "The Kreth evolved deep underground on a mineral-rich world, their rocky, dense bodies "
            "adapted to crushing pressures and toxic atmospheres. They reached the surface only after "
            "millennia of tunneling, and the stars were a revelation. Their engineering prowess — "
            "honed by generations of building in the deep — made them invaluable allies when the "
            "galactic factions began rebuilding after the Fracture. Kreth-built stations are renowned "
            "for their durability, and Kreth mining operations extract resources others cannot reach.";

        origin.starting_fragment.title = "The Deep Remembers";
        origin.starting_fragment.style = RecordStyle::Personal;
        origin.starting_fragment.reliability = RecordReliability::Verified;
        origin.starting_fragment.source = "Kreth oral history";
        origin.starting_fragment.body =
            "We came from the deep. Not the deep of space — the deep of stone, of pressure, of darkness "
            "so complete that light was a legend. When we finally broke through to the surface and saw "
            "the sky for the first time, our elders wept. Not from beauty — from vertigo. "
            "So much emptiness above us, where there should have been rock. "
            "We've learned to love the void since then. But we still build like we're underground — "
            "thick walls, strong joints, nothing wasted. The stars are just another tunnel to dig through.";

        lore.race_origins.push_back(std::move(origin));
    }

    // ── Xytomorph — bioweapon or containment breach from a precursor era ──
    {
        RaceOrigin origin;
        origin.race_name = "Xytomorph";

        // Pick a mid-to-late precursor civilization as the source
        int source_idx = std::min(static_cast<int>(lore.civilizations.size()) - 1,
                                  1 + static_cast<int>(rng() % (lore.civilizations.size() - 1)));
        origin.ancestor_civ_index = source_idx;
        const auto& source = lore.civilizations[source_idx];
        origin.origin_bya = source.epoch_end_bya;

        static const char* xyto_origins[] = {
            "The Xytomorphs were never meant to exist. Deep in %s weapons laboratories, "
            "bioengineers designed an adaptive predator — a living weapon that could survive any environment "
            "and consume any organic matter. When %s collapsed, the containment protocols failed. "
            "The Xytomorphs spread through the ruins like a plague, evolving, adapting, breeding in the dark. "
            "Billions of years later, they infest every derelict station and abandoned ruin in the galaxy.",

            "Whether the %s created the Xytomorphs deliberately or accidentally is debated by xenobiologists. "
            "Their chitinous bodies contain trace elements of %s biotechnology — engineered enzymes, "
            "synthetic neural pathways, adaptive camouflage that no natural evolution could produce. "
            "They were designed to be perfect survivors. They succeeded beyond any specification.",

            "The first Xytomorph specimens were found in a sealed %s laboratory complex, "
            "dormant in stasis pods that had maintained power for billions of years. "
            "When the pods were breached by explorers, the creatures awakened hungry. "
            "They have been spreading ever since, nesting in ruins, breeding in the dark places "
            "where %s technology still hums with residual energy.",
        };

        std::string text = xyto_origins[rng() % 3];
        for (;;) {
            auto pos = text.find("%s");
            if (pos == std::string::npos) break;
            text.replace(pos, 2, source.short_name);
        }
        origin.origin_text = text;

        // Xytomorphs don't get a starting fragment (hostile, not playable in standard mode)
        origin.starting_fragment.title = "Xenobiology Report: Xytomorph";
        origin.starting_fragment.style = RecordStyle::Scientific;
        origin.starting_fragment.reliability = RecordReliability::Verified;
        origin.starting_fragment.source = "Galactic Xenobiology Institute";
        origin.starting_fragment.body =
            "Classification: Xytomorph (common designation). Highly aggressive chitinous predator. "
            "Traces of " + source.short_name + " bioengineering confirmed in genome analysis. "
            "Adaptive physiology allows survival in vacuum, extreme temperature, and toxic atmospheres. "
            "Exercise extreme caution. Do not approach nesting sites without military escort.";

        lore.race_origins.push_back(std::move(origin));
    }

    // ── Human — no precursor link, standard origin ──
    {
        RaceOrigin origin;
        origin.race_name = "Human";
        origin.ancestor_civ_index = -1;
        origin.origin_bya = lore.humanity.arrival_bya;

        origin.origin_text =
            "Humanity arrived among the stars ten thousand years ago, the latest species to look up "
            "and wonder. They found a galaxy littered with ruins, haunted by the echoes of civilizations "
            "that had risen and fallen before Earth's sun was born. Humans reverse-engineered hyperspace "
            "technology from precursor wreckage and expanded rapidly — too rapidly. The golden age ended "
            "in the Fracture, and now the scattered human factions pick through the bones of giants.";

        origin.starting_fragment.title = "Orientation Brief: New Recruits";
        origin.starting_fragment.style = RecordStyle::Official;
        origin.starting_fragment.reliability = RecordReliability::Verified;
        origin.starting_fragment.source = "Stellari Conclave Orientation Office";
        origin.starting_fragment.body =
            "Welcome to The Heavens Above. You are standing on a station orbiting Jupiter, "
            "built by a species that vanished before our sun ignited. Let that sink in. "
            "The galaxy is old, and we are very, very young. "
            "Your starship is docked at Bay 7. Your mission: survive, explore, and maybe — "
            "just maybe — figure out what all those ancient civilizations were pointing at "
            "when they aimed their beacons toward the center of the galaxy. Good luck, commander.";

        lore.race_origins.push_back(std::move(origin));
    }
}

std::string LoreGenerator::format_history(const WorldLore& lore) {
    std::ostringstream out;

    out << "=== WORLD LORE (seed " << lore.seed << ") ===\n";
    out << "Civilizations: " << lore.civilizations.size() << "\n";

    if (!lore.civilizations.empty()) {
        out << "Timeline: "
            << lore.civilizations.front().epoch_start_bya << " Bya -> "
            << lore.civilizations.back().epoch_end_bya << " Bya\n";
    }

    out << "Beacons: " << lore.active_beacons << " active / "
        << lore.total_beacons << " total\n";

    for (size_t i = 0; i < lore.civilizations.size(); ++i) {
        auto& civ = lore.civilizations[i];
        out << "\n--- Epoch " << i << ": " << civ.name << " ---\n";
        out << "  Architecture: " << architecture_name(civ.architecture) << "\n";
        out << "  Tech style:   " << tech_name(civ.tech_style) << "\n";
        out << "  Philosophy:   " << philosophy_name(civ.philosophy) << "\n";
        out << "  Collapse:     " << collapse_name(civ.collapse_cause) << "\n";
        out << "  Sgr A*:       " << sgra_name(civ.sgra_relation) << "\n";

        if (i > 0) {
            out << "  Predecessors: " << predecessor_name(civ.predecessor_relation) << "\n";
        }

        out << "  Timeline:     "
            << civ.epoch_start_bya << " - " << civ.epoch_end_bya << " Bya\n";

        if (!civ.events.empty()) {
            out << "  Events:\n";
            for (auto& ev : civ.events) {
                out << "    [" << ev.time_bya << " Bya] "
                    << event_type_name(ev.type) << " — " << ev.description << "\n";
            }
        }

        if (!civ.figures.empty()) {
            out << "  Figures:\n";
            for (auto& fig : civ.figures) {
                out << "    " << fig.name << " (" << archetype_name(fig.archetype)
                    << ") — " << fig.achievement << " [" << fig.fate << "]\n";
            }
        }

        if (!civ.artifacts.empty()) {
            out << "  Artifacts:\n";
            for (auto& art : civ.artifacts) {
                out << "    " << art.name << " [" << category_name(art.category)
                    << "] — " << art.effect_text << "\n";
            }
        }

        if (!civ.records.empty()) {
            out << "  LORE RECORDS: (" << civ.records.size() << ")\n";
            for (const auto& rec : civ.records) {
                out << "    [" << style_name(rec.style) << "] " << rec.title << "\n";
                // Print body indented, wrapping at ~70 chars
                std::string body = rec.body;
                size_t pos = 0;
                while (pos < body.size()) {
                    size_t end_pos = std::min(pos + 70, body.size());
                    if (end_pos < body.size()) {
                        size_t space = body.rfind(' ', end_pos);
                        if (space > pos) end_pos = space;
                    }
                    out << "      " << body.substr(pos, end_pos - pos) << "\n";
                    pos = end_pos;
                    if (pos < body.size() && body[pos] == ' ') ++pos;
                }
                out << "\n";
            }
        }
    }

    out << "\n--- Human Epoch ---\n";
    out << "  Arrival:    " << lore.humanity.arrival_bya << " Bya\n";
    out << "  Golden age: " << lore.humanity.golden_age_start << " Bya\n";
    out << "  Fracture:   " << lore.humanity.fracture_bya << " Bya\n";

    if (!lore.humanity.events.empty()) {
        out << "  Events:\n";
        for (auto& ev : lore.humanity.events) {
            out << "    [" << ev.time_bya << " Bya] "
                << event_type_name(ev.type) << " — " << ev.description << "\n";
        }
    }

    if (!lore.humanity.faction_names.empty()) {
        out << "  Factions:\n";
        for (auto& f : lore.humanity.faction_names) {
            out << "    - " << f << "\n";
        }
    }

    // Race origins
    if (!lore.race_origins.empty()) {
        out << "\n--- Race Origins ---\n\n";
        for (const auto& ro : lore.race_origins) {
            out << "  " << ro.race_name;
            if (ro.ancestor_civ_index >= 0 &&
                ro.ancestor_civ_index < static_cast<int>(lore.civilizations.size())) {
                out << " (descended from " << lore.civilizations[ro.ancestor_civ_index].short_name << ")";
            }
            out << "  [" << ro.origin_bya << " Bya]\n";
            // Word-wrap origin text
            std::string text = ro.origin_text;
            size_t pos = 0;
            while (pos < text.size()) {
                size_t end = std::min(pos + 70, text.size());
                if (end < text.size()) {
                    size_t space = text.rfind(' ', end);
                    if (space > pos) end = space;
                }
                out << "    " << text.substr(pos, end - pos) << "\n";
                pos = end;
                if (pos < text.size() && text[pos] == ' ') ++pos;
            }
            out << "\n";
        }
    }

    return out.str();
}

} // namespace astra
