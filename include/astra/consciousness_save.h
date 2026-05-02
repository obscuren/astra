#pragma once

#include "astra/grid_sector.h"
#include "astra/item.h"
#include "astra/sector_runtime_state.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace astra {

inline constexpr uint32_t CONSCIOUSNESS_SAVE_VERSION = 2;

struct LoreFragmentRef {
    std::string archive_id;        // e.g. "ARCH-Hangar7-12x4"
    uint32_t galaxy_seed_origin = 0;
    int32_t  world_tick_origin  = 0;
};

// v2 — Plan 5 deep-grid warp anchor (filled in Cut 3).
struct WarpAnchorRecord {
    uint16_t    galaxy_id = 0;
    uint32_t    region_seed = 0;
    std::string lan_display_name;
    int         nodes_total = 0;
    int         nodes_cracked = 0;
    bool        warpable = true;
};

// v2 — Plan 5 AI contact record (filled in Cut 4).
struct AiContactRecord {
    std::string id;            // e.g. "aria.heavens-above"
    std::string display_name;
    uint16_t    origin_galaxy_id = 0;
    // (Plan 7 expands this)
};

struct ConsciousnessSave {
    uint32_t version = CONSCIOUSNESS_SAVE_VERSION;
    uint64_t consciousness_id = 0;
    uint32_t rebirth_count = 0;
    bool     seen_first_rebirth = false;

    std::vector<LoreFragmentRef> lore_archive;
    int32_t                      grid_currency = 0;

    // Hacker-only — populated only with ConsciousnessAnchor capstone unlocked.
    // Empty sector (sec.w == 0) means "not yet anchored". When the player
    // takes the ConsciousnessAnchor capstone, this becomes the 60×40
    // hand-authored layout from make_deep_grid_base().
    GridSector              deep_grid_base;
    std::vector<Item>       signature_program_rack;

    // Plan 5 — runtime overlay applied to deep_grid_base on each jack-in.
    SectorRuntimeState            deep_grid_sector_state;

    // Plan 5 Cut 3 — populated on first ⊕ crack per LAN.
    std::vector<WarpAnchorRecord> warp_anchors;

    // Plan 5 Cut 4 — placeholder until that cut populates.
    std::vector<AiContactRecord>  ai_contacts;
};

std::filesystem::path consciousness_save_path();

bool write_consciousness(const ConsciousnessSave& cs);

// Reads into out. Returns false if file missing or schema mismatch
// (in which case `out` is left default-constructed). Never throws.
bool read_consciousness(ConsciousnessSave& out);

// Dev verb only. Used by `:rebirth-reset`.
bool delete_consciousness();

} // namespace astra
