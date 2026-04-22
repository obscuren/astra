#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

class Game;

// ─── Effect primitives ────────────────────────────────────────────
// Thin wrappers around existing subsystems (faction.h, world_manager,
// playback_viewer). Scenarios compose these; they do not reach into
// subsystems directly. This is how the effect vocabulary accumulates.

// Shift the player's standing with a named faction by delta.
void shift_faction_standing(Game& game, const std::string& faction, int delta);

// Set or clear a named world flag.
void set_world_flag(Game& game, const std::string& flag, bool value);

// Display an incoming transmission modal. Blocks input until dismissed.
// Implemented via the existing playback_viewer AudioLog style.
void open_transmission(Game& game,
                       const std::string& header,
                       const std::vector<std::string>& lines);

// Inject ambient NPC spawns into a LocationKey via QuestLocationMeta.
// The next time the player enters this location, the named NPC roles
// spawn as ambient encounter.
void inject_location_encounter(Game& game,
                               uint32_t system_id,
                               int body_index,
                               bool is_station,
                               const std::vector<std::string>& npc_roles,
                               const std::string& source_tag);

} // namespace astra
