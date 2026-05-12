#include "astra/lan.h"

#include "astra/consciousness_save.h"
#include "astra/hackable.h"
#include "astra/ip.h"
#include "astra/npc.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace astra {

LanFlavour infer_flavour(const WorldManager& world) {
    // TODO(Cut 2): swap to MapKind once World tracks per-map kinds.
    // Cut 1: derive from MapType + biome + location_name.
    const TileMap& m = world.map();

    // Precursor LANs are flagged via the "Precursor" location-name marker
    // (see OW_PrecursorArchive descent and crashed-ship POIs).
    const std::string& loc = m.location_name();
    if (loc.find("Precursor") != std::string::npos) {
        return LanFlavour::Precursor;
    }
    if (loc.find("Crashed") != std::string::npos
     || loc.find("crashed") != std::string::npos) {
        return LanFlavour::Precursor;
    }

    switch (m.map_type()) {
        case MapType::SpaceStation:
        case MapType::DerelictStation:
        case MapType::Starship:
        case MapType::DetailMap:           // settlements treated as Station
        case MapType::Overworld:
            return LanFlavour::Station;
        case MapType::Asteroid:
        case MapType::Rocky:
        case MapType::Lava:
        case MapType::Nebula:
            return LanFlavour::Asteroid;
    }
    return LanFlavour::Station;
}

bool infer_connected(LanFlavour f) {
    // Spec §3 Q5: Station + Asteroid + Precursor connected; Dungeon isolated.
    return f != LanFlavour::Dungeon;
}


// Plan 5 Cut 4 Task 38: slugify a region name for AI contact id generation.
// Lowercases alphanumerics, converts spaces/underscores/hyphens to hyphens,
// skips other characters.
std::string slugify(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '_' || c == '-') {
            if (out.empty() || out.back() != '-') {
                out.push_back('-');
            }
        }
        // else: skip
    }
    // Trim trailing hyphen if present.
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

// Plan 5 hostname: per-device dotted name for nmap output.
// Format: "<short-tag>-<host_octet>.<region-slug>.lan"
std::string lan_hostname(const Hackable& h, const LanMetadata& meta) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s-%u.%s.lan",
                  tag_summary(h.tags),
                  static_cast<unsigned>(h.ip & 0xFF),
                  slugify(meta.region_label).c_str());
    return buf;
}

} // namespace astra
