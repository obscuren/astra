#pragma once

#include "astra/grid_sector.h"
#include "astra/item.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace astra {

inline constexpr uint32_t CONSCIOUSNESS_SAVE_VERSION = 1;

struct LoreFragmentRef {
    std::string archive_id;        // e.g. "ARCH-Hangar7-12x4"
    uint32_t galaxy_seed_origin = 0;
    int32_t  world_tick_origin  = 0;
};

struct AiContact {
    uint32_t faction_id = 0;
    int32_t  reputation = 0;       // -100..+100
};

struct ConsciousnessSave {
    uint32_t version = CONSCIOUSNESS_SAVE_VERSION;
    uint64_t consciousness_id = 0;
    uint32_t rebirth_count = 0;
    bool     seen_first_rebirth = false;

    std::vector<LoreFragmentRef> lore_archive;
    int32_t                      grid_currency = 0;
    std::vector<AiContact>       ai_contacts;

    // Hacker-only — populated only with ConsciousnessAnchor capstone unlocked.
    // Body fields (GridSector + Items) are deferred to Task 9; v1 of this task
    // writes only the present-flags and counts.
    std::optional<GridSector> deep_grid_base;
    std::vector<Item>         signature_program_rack;
};

std::filesystem::path consciousness_save_path();

bool write_consciousness(const ConsciousnessSave& cs);

// Reads into out. Returns false if file missing or schema mismatch
// (in which case `out` is left default-constructed). Never throws.
bool read_consciousness(ConsciousnessSave& out);

// Dev verb only. Used by `:rebirth-reset`.
bool delete_consciousness();

} // namespace astra
