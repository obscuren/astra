#pragma once

#include "astra/hackable.h"
#include "astra/rect.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class LanFlavour : uint8_t { Station, Asteroid, Dungeon, Precursor };

// Lowercases alphanumerics, converts spaces / underscores / hyphens to a single
// '-' and strips everything else. Trailing hyphens are trimmed. Used both for
// AI contact ids and for nmap hostname construction.
std::string slugify(const std::string& s);

struct LanZone {
    std::string name;
    Rect        extents = {};
    int         tier = 1;
};

class WorldManager;

// LAN bookkeeping survives the GridNetwork delete because it still names the
// player's region and seeds per-LAN content. The multi-region node-graph
// fields (lan_root, has_deep_grid_edge, runtime state buckets) are gone;
// per-target netspace generation replaces them in Phase 1+.
struct LanMetadata {
    std::string     region_label;
    std::string     display_name;
    LanFlavour      flavour = LanFlavour::Station;
    int             security_tier = 1;
    bool            connected = false;
    uint32_t        gen_seed = 0;
    uint32_t        subnet_base = 0;
    std::vector<LanZone>           zones;
    uint64_t        last_visited_tick = 0;
    int             nodes_total = 0;
    int             nodes_cracked = 0;
    int             ice_killed = 0;
    int             lore_extracted = 0;
    uint16_t        origin_galaxy_id = 0;
};

// Build a per-device hostname for nmap-style output. Format:
//   <short-tag>-<host_octet>.<region-slug>.lan
std::string lan_hostname(const Hackable& h, const LanMetadata& meta);

// Infer flavour from the active map's kind/biome/location.
LanFlavour infer_flavour(const WorldManager& world);

// Whether a LAN of this flavour gets a Deep-Grid edge.
bool       infer_connected(LanFlavour f);

} // namespace astra
