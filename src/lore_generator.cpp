#include "astra/lore_generator.h"

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

    int civ_count = 2 + static_cast<int>(rng() % 4);
    float cursor_bya = uniform(rng, 4.0f, 6.0f);

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

        generate_events(rng, civ, i, i > 0);
        generate_figures(rng, civ, namer);
        generate_artifacts(rng, civ, namer);

        // Advance cursor past this civ + silence gap.
        cursor_bya = civ.epoch_end_bya - uniform(rng, 0.1f, 0.6f);

        lore.civilizations.push_back(std::move(civ));
    }

    // Human epoch.
    lore.humanity = generate_human_epoch(rng, lore.civilizations);

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

// ── generate_events ─────────────────────────────────────────────────────────

void LoreGenerator::generate_events(
    std::mt19937& rng,
    Civilization& civ,
    int epoch_index,
    bool has_predecessors)
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

    auto push = [&](LoreEventType t, float frac, const std::string& d) {
        raw.push_back({t, frac, d});
    };

    // Emergence always first.
    push(LoreEventType::Emergence, 0.0f,
         civ.short_name + " consciousness emerges");

    float cursor = 0.05f;

    // Predecessor discovery.
    if (has_predecessors) {
        push(LoreEventType::RuinDiscovery, cursor,
             "Ancient ruins discovered by " + civ.short_name + " explorers");
        cursor += 0.05f;

        if (rng() % 2 == 0) {
            push(LoreEventType::Decipherment, cursor,
                 "Precursor inscriptions partially decoded");
            cursor += 0.05f;
        }

        push(LoreEventType::ReverseEngineering, cursor,
             "Precursor technology reverse-engineered");
        cursor += 0.05f;
    }

    // Expansion.
    push(LoreEventType::Colonization, cursor + 0.05f,
         civ.short_name + " expands to nearby systems");
    cursor += 0.10f;

    push(LoreEventType::HyperspaceRoute, cursor + 0.05f,
         "First hyperspace route established");
    cursor += 0.10f;

    // Mid-epoch events (1-3 random).
    int mid_count = uniform_int(rng, 1, 3);
    static const LoreEventType mid_types[] = {
        LoreEventType::MegastructureBuilt,
        LoreEventType::SgrADetection,
        LoreEventType::CivilWar,
        LoreEventType::ResourceWar,
        LoreEventType::ArtifactCreation,
        LoreEventType::ConvergenceDiscovery,
    };
    for (int i = 0; i < mid_count; ++i) {
        auto t = mid_types[rng() % 6];
        float frac = uniform(rng, cursor, 0.75f);
        std::string desc;
        switch (t) {
        case LoreEventType::MegastructureBuilt:
            desc = "Megastructure constructed in deep space";
            break;
        case LoreEventType::SgrADetection:
            desc = "Anomalous signals detected from Sgr A*";
            break;
        case LoreEventType::CivilWar:
            desc = "Internal factions fracture " + civ.short_name + " unity";
            break;
        case LoreEventType::ResourceWar:
            desc = "War erupts over dwindling resources";
            break;
        case LoreEventType::ArtifactCreation:
            desc = "Legendary artifact forged";
            break;
        case LoreEventType::ConvergenceDiscovery:
            desc = "Convergence point discovered near galactic core";
            break;
        default:
            desc = "Unknown event";
            break;
        }
        push(t, frac, desc);
    }

    // Collapse.
    push(collapse_event_type(civ.collapse_cause), 0.90f,
         civ.short_name + " civilization collapses — " + collapse_name(civ.collapse_cause));

    // Legacy events.
    if (rng() % 2 == 0) {
        push(LoreEventType::VaultSealed, 0.95f,
             "Knowledge vaults sealed against the coming silence");
    }
    if (rng() % 3 == 0) {
        push(LoreEventType::GuardianCreated, 0.97f,
             "Autonomous guardians activated to protect relics");
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

// ── format_history ──────────────────────────────────────────────────────────

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

    return out.str();
}

} // namespace astra
