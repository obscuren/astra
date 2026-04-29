#include "astra/pda_screen.h"
#include "astra/character.h"
#include "astra/faction.h"

#include <string>

namespace astra {

void PdaScreen::draw_reputation(UIContext& ctx) {
    if (player_->reputation.empty()) {
        ctx.text({.x = 2, .y = 2, .content = "No faction standings.", .tag = UITag::TextDim});
        return;
    }

    int y = 2;
    for (int i = 0; i < static_cast<int>(player_->reputation.size()); ++i) {
        if (y >= ctx.height() - 4) break;
        const auto& f = player_->reputation[i];
        bool selected = (cursor_ == i);

        if (selected) ctx.put(1, y, '>', Color::Yellow);
        ctx.text({.x = 3, .y = y, .content = f.faction_name,
                  .tag = selected ? UITag::TextBright : UITag::TextDefault});

        auto tier = reputation_tier(f.reputation);
        std::string rep = std::string(reputation_tier_name(tier)) +
                          " (" + std::to_string(f.reputation) + ")";
        UITag rep_tag = f.reputation > 0 ? UITag::TextSuccess
                      : f.reputation < 0 ? UITag::TextDanger
                      : UITag::TextDim;
        ctx.text({.x = ctx.width() - 2 - static_cast<int>(rep.size()), .y = y,
                  .content = rep, .tag = rep_tag});

        // Faction description
        y++;
        const char* desc = faction_description(f.faction_name);
        if (desc[0] != '\0') {
            std::string desc_str(desc);
            int max_w = ctx.width() - 8;
            int dx = 5;
            while (!desc_str.empty() && y < ctx.height() - 3) {
                std::string line;
                if (static_cast<int>(desc_str.size()) <= max_w) {
                    line = desc_str;
                    desc_str.clear();
                } else {
                    auto pos = desc_str.rfind(' ', max_w);
                    if (pos == std::string::npos) pos = max_w;
                    line = desc_str.substr(0, pos);
                    desc_str = desc_str.substr(pos + 1);
                }
                ctx.text({.x = dx, .y = y, .content = line, .tag = UITag::TextDim});
                y++;
            }
        }

        // Flavor text based on tier
        std::string flavor;
        switch (tier) {
            case ReputationTier::Trusted:  flavor = "They consider you a trusted ally."; break;
            case ReputationTier::Liked:    flavor = "They view you with curiosity."; break;
            case ReputationTier::Neutral:  flavor = "They are indifferent toward you."; break;
            case ReputationTier::Disliked: flavor = "They are wary of you."; break;
            case ReputationTier::Hated:    flavor = "They are hostile toward you."; break;
        }
        ctx.text({.x = 5, .y = y, .content = flavor,
                  .tag = tier <= ReputationTier::Disliked ? UITag::TextDanger : UITag::TextDim});
        y += 2;
    }
}

} // namespace astra
