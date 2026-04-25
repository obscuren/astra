#pragma once

#include "astra/item.h"
#include "astra/loot_source.h"

#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace astra {

struct LootEntry {
    uint16_t    item_def_id     = 0;       // → build_by_def_id()
    std::string identifier;                 // stable string handle for `give item`
    Rarity      min_rarity      = Rarity::Common;
    Rarity      max_rarity      = Rarity::Common;
    int         default_weight  = 0;
    std::map<LootSource, int> source_weights;  // optional per-source override
    uint64_t    source_mask     = 0;
    Theme       theme           = Theme::None;
    int         min_level       = 1;
    Category    category        = Category::Junk;
};

struct StockManifestEntry {
    enum class Mode { Always, Random };
    Mode      mode           = Mode::Random;
    uint16_t  item_def_id    = 0;          // Always: which item; Random: ignored
    Category  category       = Category::Junk;  // Random: filter; Always: ignored
    int       quantity       = 1;
    int       min_reputation = 0;
};

// --- Public API ---------------------------------------------------------

// Pick + build a single loot item.
// `source` is the bitset of contexts the call is rolling for (typically a single bit).
// `forced_category` bypasses the per-source category roll when set.
// Returns std::nullopt if no entry matches the filter.
std::optional<Item> roll_loot(LootSource source,
                              int level,
                              std::mt19937& rng,
                              std::optional<Category> forced_category = std::nullopt);

// Build a merchant stock vector from a manifest.
std::vector<Item> assemble_stock(const std::vector<StockManifestEntry>& manifest,
                                 LootSource source,
                                 int faction_rep,
                                 int level,
                                 std::mt19937& rng);

// Find an entry by its string identifier (case-insensitive). For dev console.
const LootEntry* find_entry_by_identifier(std::string_view identifier);

// Read-only access to the full table (for dev-console list, debug overlays, etc).
const std::vector<LootEntry>& loot_table_all_entries();

// Startup self-check: returns false (and logs to stderr) if any entry's
// item_def_id is not handled by build_by_def_id. Call from main() in DEV builds.
bool verify_dispatch_coverage();

} // namespace astra
