#pragma once

#include "astra/lore_types.h"
#include "astra/name_generator.h"

#include <random>
#include <string>

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
        bool has_predecessors,
        const std::vector<Civilization>& predecessors,
        const NameGenerator& namer);

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
