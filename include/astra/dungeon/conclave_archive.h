#pragma once

#include "astra/dungeon_recipe.h"

#include <vector>

namespace astra {

// Three-level Precursor ruin for the Stellar Signal Stage 4 Conclave
// Archive quest. Level 1 Conclave Sentries, Level 2 Heavy Sentries +
// Archon Remnants, Level 3 Archon Sentinel boss + Nova's resonance
// crystal in the back chamber.
std::vector<DungeonLevelSpec> build_conclave_archive_levels();

} // namespace astra
