#include "astra/character.h"
#include "astra/player.h"

namespace astra {

ReputationTier reputation_tier(int reputation) {
    if (reputation <= -50) return ReputationTier::Hated;
    if (reputation <= -10) return ReputationTier::Disliked;
    if (reputation < 10)   return ReputationTier::Neutral;
    if (reputation < 50)   return ReputationTier::Liked;
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

} // namespace astra
