#include "astra/character.h"
#include "astra/faction.h"
#include "astra/player.h"

#include <algorithm>
#include <cstring>

namespace astra {

// ─── Reputation tier helpers ────────────────────────────────────

ReputationTier reputation_tier(int reputation) {
    if (reputation <= -300) return ReputationTier::Hated;
    if (reputation <= -60)  return ReputationTier::Disliked;
    if (reputation < 60)    return ReputationTier::Neutral;
    if (reputation < 300)   return ReputationTier::Liked;
    return ReputationTier::Trusted;
}

const char* reputation_tier_name(ReputationTier tier) {
    switch (tier) {
        case ReputationTier::Hated:    return "Hated";
        case ReputationTier::Disliked: return "Disliked";
        case ReputationTier::Neutral:  return "Neutral";
        case ReputationTier::Liked:    return "Liked";
        case ReputationTier::Trusted:  return "Trusted";
    }
    return "Unknown";
}

int reputation_price_pct(int reputation) {
    switch (reputation_tier(reputation)) {
        case ReputationTier::Hated:    return 30;
        case ReputationTier::Disliked: return 15;
        case ReputationTier::Neutral:  return 0;
        case ReputationTier::Liked:    return -10;
        case ReputationTier::Trusted:  return -20;
    }
    return 0;
}

int reputation_for(const Player& player, const std::string& faction) {
    if (faction.empty()) return 0;
    for (const auto& fs : player.reputation) {
        if (fs.faction_name == faction) return fs.reputation;
    }
    return 0;
}

// ─── Faction registry ───────────────────────────────────────────

const std::vector<FactionInfo>& all_factions() {
    static const std::vector<FactionInfo> factions = {
        {Faction_StellariConclave,
         "Station operators, scientists, and diplomats of the Stellari race. "
         "They maintain the network of ancient space stations."},
        {Faction_KrethMiningGuild,
         "Stocky, mineral-skinned traders and resource extractors. "
         "They control most of the galaxy's commerce routes."},
        {Faction_VeldraniAccord,
         "Tall, blue-skinned diplomats and traders. "
         "Known for brokering peace between warring factions."},
        {Faction_SylphariWanderers,
         "Wispy, luminescent nomads who drift between the stars. "
         "They value freedom and the natural order above all."},
        {Faction_TerranFederation,
         "Humanity's government — adaptable, resourceful, and numerous. "
         "They are the galaxy's generalists."},
        {Faction_XytomorphHive,
         "Chitinous alien predators driven by a hive intelligence. "
         "Hostile to all other life."},
        {Faction_VoidReavers,
         "Pirates and raiders who prey on trade routes. "
         "They respect only strength."},
        {Faction_ArchonRemnants,
         "Automated defense constructs left by an ancient civilization. "
         "Their programming has degraded into hostility."},
        {Faction_Feral,
         "Wild space fauna — cave beasts, void creatures, and other "
         "non-intelligent predators found throughout the galaxy."},
        {Faction_DriftCollective,
         "Scavengers and nomads with no homeworld. "
         "They trade with anyone and fight only when provoked."},
    };
    return factions;
}

const char* faction_description(const std::string& faction) {
    for (const auto& f : all_factions()) {
        if (faction == f.name) return f.description;
    }
    return "";
}

// ─── Inter-faction standings table ──────────────────────────────
// Symmetric pairs: (faction_a, faction_b, standing).
// To tweak any relationship, change the number here.
// Pairs not listed default to 0 (Neutral).

struct FactionPair {
    const char* a;
    const char* b;
    int standing;
};

// clang-format off
static const FactionPair default_standings[] = {
    // Race-aligned inter-faction relations
    {Faction_StellariConclave,  Faction_VeldraniAccord,    100},
    {Faction_StellariConclave,  Faction_SylphariWanderers,  50},
    {Faction_StellariConclave,  Faction_TerranFederation,   50},
    {Faction_KrethMiningGuild,  Faction_TerranFederation,   50},
    {Faction_KrethMiningGuild,  Faction_SylphariWanderers, -50},
    {Faction_VeldraniAccord,    Faction_SylphariWanderers, 100},
    {Faction_VeldraniAccord,    Faction_TerranFederation,  100},
    {Faction_VeldraniAccord,    Faction_DriftCollective,    50},
    {Faction_SylphariWanderers, Faction_DriftCollective,    50},

    // Everyone vs Xytomorphs
    {Faction_StellariConclave,  Faction_XytomorphHive,    -400},
    {Faction_KrethMiningGuild,  Faction_XytomorphHive,    -400},
    {Faction_VeldraniAccord,    Faction_XytomorphHive,    -400},
    {Faction_SylphariWanderers, Faction_XytomorphHive,    -400},
    {Faction_TerranFederation,  Faction_XytomorphHive,    -400},
    {Faction_DriftCollective,   Faction_XytomorphHive,    -400},
    {Faction_VoidReavers,       Faction_XytomorphHive,    -400},
    {Faction_ArchonRemnants,    Faction_XytomorphHive,    -400},
    {Faction_Feral,             Faction_XytomorphHive,    -400},

    // Everyone vs Void Reavers
    {Faction_StellariConclave,  Faction_VoidReavers,      -350},
    {Faction_KrethMiningGuild,  Faction_VoidReavers,      -350},
    {Faction_VeldraniAccord,    Faction_VoidReavers,      -300},
    {Faction_SylphariWanderers, Faction_VoidReavers,      -350},
    {Faction_TerranFederation,  Faction_VoidReavers,      -350},
    {Faction_DriftCollective,   Faction_VoidReavers,      -300},

    // Everyone vs Archon Remnants
    {Faction_StellariConclave,  Faction_ArchonRemnants,   -400},
    {Faction_KrethMiningGuild,  Faction_ArchonRemnants,   -400},
    {Faction_VeldraniAccord,    Faction_ArchonRemnants,   -400},
    {Faction_SylphariWanderers, Faction_ArchonRemnants,   -400},
    {Faction_TerranFederation,  Faction_ArchonRemnants,   -400},
    {Faction_DriftCollective,   Faction_ArchonRemnants,   -400},
    {Faction_VoidReavers,       Faction_ArchonRemnants,   -400},

    // Everyone vs Feral
    {Faction_StellariConclave,  Faction_Feral,            -400},
    {Faction_KrethMiningGuild,  Faction_Feral,            -400},
    {Faction_VeldraniAccord,    Faction_Feral,            -400},
    {Faction_SylphariWanderers, Faction_Feral,            -400},
    {Faction_TerranFederation,  Faction_Feral,            -400},
    {Faction_DriftCollective,   Faction_Feral,            -400},
    {Faction_VoidReavers,       Faction_Feral,            -400},
    {Faction_ArchonRemnants,    Faction_Feral,            -400},

    // Hostile factions vs each other
    {Faction_XytomorphHive,     Faction_VoidReavers,      -400},
    {Faction_XytomorphHive,     Faction_ArchonRemnants,   -400},
    {Faction_XytomorphHive,     Faction_Feral,            -400},
    {Faction_VoidReavers,       Faction_Feral,            -400},
    {Faction_ArchonRemnants,    Faction_Feral,            -400},
};
// clang-format on

int default_faction_standing(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return 0;
    if (a == b) return 600; // same faction = max friendly
    for (const auto& p : default_standings) {
        if ((a == p.a && b == p.b) || (a == p.b && b == p.a))
            return p.standing;
    }
    return 0; // unknown pair = Neutral
}

// ─── Hostility queries ──────────────────────────────────────────

static constexpr int hostile_threshold = -300;

bool is_hostile(const std::string& faction_a, const std::string& faction_b) {
    if (faction_a.empty() || faction_b.empty()) return false;
    if (faction_a == faction_b) return false;
    return default_faction_standing(faction_a, faction_b) <= hostile_threshold;
}

bool is_hostile_to_player(const std::string& npc_faction, const Player& player) {
    if (npc_faction.empty()) return false;
    int rep = reputation_for(player, npc_faction);
    return rep <= hostile_threshold;
}

int modify_faction_standing(Player& player, const std::string& faction, int delta) {
    for (auto& fs : player.reputation) {
        if (fs.faction_name == faction) {
            fs.reputation = std::clamp(fs.reputation + delta, -1000, 1000);
            return fs.reputation;
        }
    }
    FactionStanding fs;
    fs.faction_name = faction;
    fs.reputation = std::clamp(delta, -1000, 1000);
    player.reputation.push_back(fs);
    return fs.reputation;
}

} // namespace astra
