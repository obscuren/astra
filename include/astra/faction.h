#pragma once

#include <string>
#include <vector>

namespace astra {

struct Player; // forward declare

// ─── Faction string constants ───────────────────────────────────
// Use these everywhere instead of raw string literals.

inline constexpr const char* Faction_StellariConclave = "Stellari Conclave";
inline constexpr const char* Faction_KrethMiningGuild = "Kreth Mining Guild";
inline constexpr const char* Faction_VeldraniAccord   = "Veldrani Accord";
inline constexpr const char* Faction_SylphariWanderers = "Sylphari Wanderers";
inline constexpr const char* Faction_TerranFederation = "Terran Federation";
inline constexpr const char* Faction_XytomorphHive    = "Xytomorph Hive";
inline constexpr const char* Faction_VoidReavers      = "Void Reavers";
inline constexpr const char* Faction_ArchonRemnants   = "Archon Remnants";
inline constexpr const char* Faction_Feral            = "Feral";
inline constexpr const char* Faction_DriftCollective   = "The Drift Collective";

// ─── Faction metadata ───────────────────────────────────────────

struct FactionInfo {
    const char* name;
    const char* description;
};

// Returns all factions in display order.
const std::vector<FactionInfo>& all_factions();

// ─── Inter-faction standings ────────────────────────────────────

// Look up the default standing between two factions.
// Returns 0 if either faction is empty or unknown.
int default_faction_standing(const std::string& a, const std::string& b);

// ─── Hostility queries ──────────────────────────────────────────

// True if faction_a considers faction_b hostile (standing <= -300).
// Returns false if either faction is empty or they are the same.
bool is_hostile(const std::string& faction_a, const std::string& faction_b);

// True if the NPC's faction considers the player hostile.
// Uses the player's reputation vector (player->faction standing).
bool is_hostile_to_player(const std::string& npc_faction, const Player& player);

// Add or subtract from the player's standing with a named faction.
// If the faction isn't in the reputation vector yet, it is appended.
// Clamps to [-1000, 1000]. Returns the new standing value.
int modify_faction_standing(Player& player, const std::string& faction, int delta);

// Return the faction description string (for UI). Empty if unknown.
const char* faction_description(const std::string& faction);

} // namespace astra
