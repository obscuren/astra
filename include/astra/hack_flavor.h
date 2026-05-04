#pragma once

#include <span>
#include <string>
#include <string_view>

namespace astra {

// Plan 7 §9 layer 3 — per-faction flavor pack.
// Phase A ships only the Civilian pack as the universal fallback. Corp
// and Cartel land in Phase B.
struct HackFlavorPack {
    const char*                  faction_name    = "Outpost";
    const char*                  root_user_name  = "root";
    std::span<const char* const> motd_lines;
    std::span<const char* const> log_lines;
    std::span<const char* const> user_names;
    std::span<const char* const> file_contents;
    std::span<const char* const> banner_chrome;
};

// Returns a flavor pack for the given faction string. `faction` is the same
// string used on Npc::faction (e.g. "Cartel", "Stellari Conclave", or empty
// for unaligned). Phase A ALWAYS returns the Civilian pack — Plan 7 §9 says
// Civilian is the safe default for `Faction::None`, future factions, and any
// pack not yet shipped.
const HackFlavorPack& flavor_for(std::string_view faction);

} // namespace astra
