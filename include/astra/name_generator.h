#pragma once

#include "astra/lore_types.h"

#include <random>
#include <string>

namespace astra {

class NameGenerator {
public:
    explicit NameGenerator(PhonemePool pool);

    std::string name(std::mt19937& rng) const;
    std::string place(std::mt19937& rng) const;
    std::string civilization(std::mt19937& rng) const;
    std::string artifact(std::mt19937& rng, ArtifactCategory category) const;
    std::string title(std::mt19937& rng, FigureArchetype archetype) const;

private:
    PhonemePool pool_;
};

} // namespace astra
