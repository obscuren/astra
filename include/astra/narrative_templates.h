#pragma once

#include "astra/lore_types.h"
#include "astra/name_generator.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

// Context passed to the narrative generator for slot filling.
struct NarrativeContext {
    std::string civ;            // short name: "Aelithae"
    std::string civ_full;       // full name: "The Aelithae Convergence"
    std::string pred;           // predecessor short name
    std::string tech;           // technology style name
    std::string arch;           // architecture style name
    std::string phil;           // philosophy name
    std::string collapse;       // collapse cause name
    // Populated per-record:
    std::string place;          // a generated place name
    std::string place2;         // a second place name (for trade routes, etc.)
    std::string figure;         // a figure name + title
    std::string figure_name;    // just the name
    std::string artifact;       // artifact name
    int count = 0;              // a random number for templates
    int years = 0;              // a time span
};

class NarrativeGenerator {
public:
    // Generate all lore records for a civilization.
    static void generate(std::mt19937& rng,
                         Civilization& civ,
                         const NameGenerator& namer,
                         const std::vector<Civilization>& predecessors);

private:
    // Generate a single record for a specific event.
    static LoreRecord generate_event_record(
        std::mt19937& rng,
        const NarrativeContext& ctx,
        const LoreEvent& event,
        int event_index,
        RecordStyle style);

    // Generate a record about a key figure.
    static LoreRecord generate_figure_record(
        std::mt19937& rng,
        const NarrativeContext& ctx,
        const KeyFigure& figure,
        int figure_index,
        const Civilization& civ);

    // Generate a record about an artifact.
    static LoreRecord generate_artifact_record(
        std::mt19937& rng,
        const NarrativeContext& ctx,
        const LoreArtifact& artifact,
        int artifact_index,
        const Civilization& civ);

    // Generate a contradictory pair for an event.
    static std::pair<LoreRecord, LoreRecord> generate_contradiction(
        std::mt19937& rng,
        const NarrativeContext& ctx,
        const LoreEvent& event,
        int event_index);

    // Pick a weighted style appropriate for an event type.
    static RecordStyle pick_style(std::mt19937& rng, LoreEventType type);

    // Generate a title for a record.
    static std::string generate_title(std::mt19937& rng,
                                       RecordStyle style,
                                       const NarrativeContext& ctx,
                                       const std::string& subject);

    // Generate a source attribution.
    static std::string generate_source(std::mt19937& rng,
                                        RecordStyle style,
                                        const NarrativeContext& ctx);

    // Build narrative body from composable fragments.
    static std::string compose_body(std::mt19937& rng,
                                     RecordStyle style,
                                     LoreEventType type,
                                     const NarrativeContext& ctx,
                                     const std::string& event_desc,
                                     int length_class); // 0=short, 1=medium, 2=long
};

} // namespace astra
