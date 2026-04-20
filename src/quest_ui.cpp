#include "astra/quest_ui.h"

#include "astra/display_name.h"
#include "astra/quest.h"
#include "astra/renderer.h"

namespace astra {

std::string format_quest_body(const Quest& q) {
    std::string s;
    if (!q.description.empty()) s += q.description + "\n\n";

    if (!q.objectives.empty()) {
        s += colored("Objectives:", Color::DarkGray) + "\n";
        for (const auto& obj : q.objectives) {
            s += "  \xe2\x80\xa2 " + obj.description + "\n";
        }
        s += "\n";
    }

    const auto& r = q.reward;
    bool has_reward = r.xp > 0 || r.credits > 0 || r.skill_points > 0
                   || !r.items.empty() || !r.factions.empty();
    if (has_reward) {
        s += colored("Rewards:", Color::DarkGray) + "\n";
        for (const auto& it : r.items) {
            s += "  " + display_name(it) + "\n";
        }
        if (r.xp > 0)
            s += "  " + colored(std::to_string(r.xp) + " XP", Color::Cyan) + "\n";
        if (r.credits > 0)
            s += "  " + colored(std::to_string(r.credits) + "$", Color::Yellow) + "\n";
        if (r.skill_points > 0)
            s += "  " + colored(std::to_string(r.skill_points) + " SP", Color::Cyan) + "\n";
        for (const auto& fr : r.factions) {
            if (fr.faction_name.empty() || fr.reputation_change == 0) continue;
            std::string sign = fr.reputation_change > 0 ? "+" : "";
            s += "  " + colored(sign + std::to_string(fr.reputation_change)
                                + " reputation with " + fr.faction_name, Color::Green)
               + "\n";
        }
    }
    return s;
}

} // namespace astra
