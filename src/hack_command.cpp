#include "astra/hack_command.h"

#include "astra/player.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace astra {

bool ParsedArgs::has_flag(std::string_view name) const {
    for (const auto& a : argv) {
        if (a == name) return true;
        // long-form --flag=value
        if (a.size() > name.size() && a.compare(0, name.size(), name) == 0 &&
            a[name.size()] == '=') return true;
    }
    return false;
}

std::string_view ParsedArgs::value_of(std::string_view name) const {
    for (size_t i = 0; i < argv.size(); ++i) {
        const auto& a = argv[i];
        if (a.size() > name.size() &&
            a.compare(0, name.size(), name) == 0 &&
            a[name.size()] == '=') {
            return std::string_view(a).substr(name.size() + 1);
        }
        if (a == name && i + 1 < argv.size()) {
            return argv[i + 1];
        }
    }
    return {};
}

ParsedArgs parse_command_line(const std::string& line) {
    ParsedArgs out;
    out.raw = line;
    // trim trailing whitespace
    while (!out.raw.empty() &&
           (out.raw.back() == ' ' || out.raw.back() == '\t')) {
        out.raw.pop_back();
    }
    std::istringstream iss(out.raw);
    std::string tok;
    while (iss >> tok) {
        if (tok == "-h" || tok == "--help") out.wants_help = true;
        out.argv.push_back(std::move(tok));
    }
    return out;
}

ScaledCost scaled_cost(const HackCommand& cmd, const Player& player) {
    int int_mod = (player.attributes.intelligence - 10) / 2;
    // Skill factor — Cat_Hacking rank reduces cost mildly. Specific skill
    // refinements (RootKit/ColdHands) ship in Phase B; Phase A keeps the
    // hook in place by exposing scaled_cost() as the single source.
    double skill_factor = 1.0;
    // (Phase A: no per-skill reductions yet. Phase B wires RootKit and
    //  ColdHands into command-specific selectors.)

    double turns_mul = std::max(0.25, 1.0 - 0.05 * int_mod);
    double heat_mul  = std::max(0.25, 1.0 - 0.04 * int_mod);

    ScaledCost s;
    s.turns = std::max(1, static_cast<int>(std::round(cmd.base_turns * turns_mul * skill_factor)));
    if (cmd.base_turns == 0) s.turns = 0;
    s.heat = std::max(0, static_cast<int>(std::round(cmd.base_heat * heat_mul * skill_factor)));
    s.detection = std::max(0, cmd.base_detection); // ColdHands wires into this in Phase B
    return s;
}

bool is_player_root(const Hackable& target, bool shell_tier_root) {
    // True if the shell session is at root tier OR the device has no Locked
    // tag (root is the default tier on those).
    if (shell_tier_root) return true;
    if (!has_tag(target.tags, HackTag::Locked)) return true;
    if (target.escalated) return true;
    return false;
}

HackCommandRegistry& HackCommandRegistry::get() {
    static HackCommandRegistry r;
    return r;
}

void HackCommandRegistry::add(const HackCommand* cmd) {
    if (!cmd) return;
    // Avoid double-add on hot-rebuild static-init quirks.
    for (const auto* c : commands_) if (c == cmd) return;
    commands_.push_back(cmd);
}

const HackCommand* HackCommandRegistry::find(std::string_view name) const {
    for (const auto* c : commands_) {
        if (c && name == c->name) return c;
    }
    return nullptr;
}

std::vector<const HackCommand*>
HackCommandRegistry::commands_for(HackTagMask tags, bool is_root) const {
    std::vector<const HackCommand*> out;
    for (const auto* c : commands_) {
        if (!c) continue;
        if (c->required_tag != HackTag::None) {
            if (!has_tag(tags, c->required_tag)) continue;
        }
        if (c->requires_root && !is_root) continue;
        out.push_back(c);
    }
    return out;
}

std::vector<const HackCommand*>
HackCommandRegistry::universals(bool is_root) const {
    std::vector<const HackCommand*> out;
    for (const auto* c : commands_) {
        if (!c) continue;
        if (c->required_tag != HackTag::None) continue;
        if (c->requires_root && !is_root) continue;
        out.push_back(c);
    }
    return out;
}

void register_hack_command(const HackCommand* cmd) {
    HackCommandRegistry::get().add(cmd);
}

} // namespace astra
