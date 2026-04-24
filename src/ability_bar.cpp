#include "astra/ability_bar.h"

#include "astra/ability.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/player.h"
#include "astra/telegraph.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_set>

namespace astra::ability_bar {

int row_count(const std::vector<SkillId>& slots) {
    if (slots.empty()) return 1;
    const int n = static_cast<int>(slots.size());
    return (n + kSlotsPerRow - 1) / kSlotsPerRow;
}

std::optional<SkillId> slot_at(const std::vector<SkillId>& slots, int row, int col) {
    if (row < 0 || col < 0 || col >= kSlotsPerRow) return std::nullopt;
    const int idx = row * kSlotsPerRow + col;
    if (idx < 0 || idx >= static_cast<int>(slots.size())) return std::nullopt;
    return slots[idx];
}

// Player-based overloads (wired to ability_slots).
int row_count(const Player& player) {
    return row_count(player.ability_slots);
}

std::optional<SkillId> slot_at(const Player& player, int row, int col) {
    return slot_at(player.ability_slots, row, col);
}

bool assign_on_learn(Player& player, SkillId id) {
    // Only active abilities land on the bar. Category unlocks, passives,
    // and other non-ability SkillIds are silently ignored.
    if (find_ability(id) == nullptr) {
        return false;
    }
    auto& slots = player.ability_slots;
    if (std::find(slots.begin(), slots.end(), id) != slots.end()) {
        return false; // already present
    }
    if (static_cast<int>(slots.size()) >= kMaxRows * kSlotsPerRow) {
        return false; // bar full
    }
    slots.push_back(id);
    return true;
}

bool remove_and_compact(Player& player, SkillId id) {
    auto& slots = player.ability_slots;
    auto it = std::find(slots.begin(), slots.end(), id);
    if (it == slots.end()) return false;
    slots.erase(it);
    return true;
}

void page_up(int& visible_row, const Player& player) {
    const int n = row_count(player);
    if (n <= 1) return;
    visible_row = (visible_row - 1 + n) % n;
}

void page_down(int& visible_row, const Player& player) {
    const int n = row_count(player);
    if (n <= 1) return;
    visible_row = (visible_row + 1) % n;
}

void clamp_visible_row(int& visible_row, const Player& player) {
    const int n = row_count(player);
    if (visible_row < 0) visible_row = 0;
    if (visible_row >= n) visible_row = std::max(0, n - 1);
}

void validate_and_dedupe(Player& player) {
    auto& slots = player.ability_slots;

    // Drop entries the player no longer has, or that aren't active
    // abilities (category unlocks, passives, weapon-expertise tags, etc.
    // can land here from saves written before assign_on_learn filtered).
    std::erase_if(slots, [&](SkillId id) {
        return !player_has_skill(player, id) || find_ability(id) == nullptr;
    });

    // Drop duplicates, preserving first occurrence.
    std::unordered_set<uint32_t> seen;
    std::erase_if(slots, [&](SkillId id) {
        return !seen.insert(static_cast<uint32_t>(id)).second;
    });
}

void reconcile_from_learned(Player& player) {
    // For each learned skill that is an ability (has a catalog entry),
    // ensure it's on the bar.
    for (const auto& a : ability_catalog()) {
        if (player_has_skill(player, a->skill_id)) {
            assign_on_learn(player, a->skill_id);  // no-op if already present
        }
    }
}

bool use_slot(Game& game, int visible_row, int col) {
    auto slot = slot_at(game.player(), visible_row, col);
    if (!slot.has_value()) {
        game.log("No ability in that slot.");
        return false;
    }

    auto* ability = find_ability(*slot);
    if (!ability) return false;

    // Cooldown check
    if (has_effect(game.player().effects, ability->cooldown_effect)) {
        const auto* cd = find_effect(game.player().effects, ability->cooldown_effect);
        game.log(ability->name + " is on cooldown (" +
                 std::to_string(cd ? cd->remaining : 0) + " ticks).");
        return false;
    }

    // Weapon requirement
    if (ability->required_weapon != WeaponClass::None) {
        bool has_weapon = false;
        const auto& rh = game.player().equipment.right_hand;
        const auto& ms = game.player().equipment.missile;
        if (rh && rh->weapon_class == ability->required_weapon) has_weapon = true;
        if (ms && ms->weapon_class == ability->required_weapon) has_weapon = true;
        if (!has_weapon) {
            game.log(ability->name + " requires the right weapon equipped.");
            return false;
        }
    }

    // Adjacent target
    Npc* target = nullptr;
    if (ability->needs_adjacent_target) {
        int px = game.player().x, py = game.player().y;
        int best_dist = 999;
        for (auto& npc : game.world().npcs()) {
            if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
            int dx = std::abs(npc.x - px), dy = std::abs(npc.y - py);
            int dist = std::max(dx, dy);
            if (dist <= 1 && dist < best_dist) {
                target = &npc;
                best_dist = dist;
            }
        }
        if (!target) {
            game.log("No adjacent enemy to target.");
            return false;
        }
    }

    // Cooldown factory (local — old one was file-local in ability.cpp)
    auto make_cd = [](EffectId id, const std::string& name, int duration) {
        Effect e;
        e.id = id;
        e.name = name + " CD";
        e.color = Color::DarkGray;
        e.duration = duration;
        e.remaining = duration;
        e.show_in_bar = false;
        return e;
    };

    auto finalize = [ability, &game, &make_cd]() {
        add_effect(game.player().effects, make_cd(
            ability->cooldown_effect, ability->name,
            ability->effective_cooldown(game.player())));
        game.advance_world(ability->action_cost);
    };

    if (ability->telegraph.has_value()) {
        game.telegraph().begin(
            *ability->telegraph,
            game.player().x, game.player().y,
            [ability, finalize, &game](const TelegraphResult& res) {
                if (!ability->execute_telegraphed(game, res)) {
                    return;
                }
                finalize();
            });
        game.log(ability->name + ": pick a direction, Enter to dash, Esc to cancel.");
        return true;
    }

    if (!ability->execute(game, target)) {
        return false;
    }
    finalize();
    return true;
}

} // namespace astra::ability_bar
