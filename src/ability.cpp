#include "astra/ability.h"
#include "astra/game.h"

namespace astra {

static const std::vector<AbilityDef> s_abilities = {
    {SkillId::Jab, "Jab",
     "Quick off-hand strike for 50% damage.",
     3, EffectId::CooldownJab,
     true, WeaponClass::ShortBlade, 25},

    {SkillId::Cleave, "Cleave",
     "Strike all adjacent enemies.",
     5, EffectId::CooldownCleave,
     true, WeaponClass::LongBlade, 50},

    {SkillId::Quickdraw, "Quickdraw",
     "Fire at reduced action cost.",
     3, EffectId::CooldownQuickdraw,
     false, WeaponClass::Pistol, 25},

    {SkillId::Intimidate, "Intimidate",
     "Frighten an adjacent hostile, causing it to flee.",
     10, EffectId::CooldownIntimidate,
     true, WeaponClass::None, 50},
};

const std::vector<AbilityDef>& ability_catalog() {
    return s_abilities;
}

const AbilityDef* find_ability(SkillId id) {
    for (const auto& a : s_abilities) {
        if (a.skill_id == id) return &a;
    }
    return nullptr;
}

std::vector<SkillId> get_ability_bar(const Player& player) {
    std::vector<SkillId> bar;
    for (const auto& a : s_abilities) {
        if (player_has_skill(player, a.skill_id)) {
            bar.push_back(a.skill_id);
            if (bar.size() >= 5) break;
        }
    }
    return bar;
}

static Effect make_cooldown(EffectId id, const char* name, int duration) {
    Effect e;
    e.id = id;
    e.name = std::string(name) + " CD";
    e.color = Color::DarkGray;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    return e;
}

bool use_ability(int slot, Game& game) {
    auto bar = get_ability_bar(game.player());
    if (slot < 0 || slot >= static_cast<int>(bar.size())) {
        game.log("No ability in that slot.");
        return false;
    }

    const auto* ability = find_ability(bar[slot]);
    if (!ability) return false;

    // Check cooldown
    if (has_effect(game.player().effects, ability->cooldown_effect)) {
        const auto* cd = find_effect(game.player().effects, ability->cooldown_effect);
        game.log(std::string(ability->name) + " is on cooldown (" +
                 std::to_string(cd ? cd->remaining : 0) + " ticks).");
        return false;
    }

    // Check weapon requirement
    if (ability->required_weapon != WeaponClass::None) {
        bool has_weapon = false;
        const auto& rh = game.player().equipment.right_hand;
        const auto& ms = game.player().equipment.missile;
        if (rh && rh->weapon_class == ability->required_weapon) has_weapon = true;
        if (ms && ms->weapon_class == ability->required_weapon) has_weapon = true;
        if (!has_weapon) {
            game.log(std::string(ability->name) + " requires the right weapon equipped.");
            return false;
        }
    }

    // Find adjacent hostile target (if needed)
    Npc* target = nullptr;
    if (ability->needs_adjacent_target) {
        int px = game.player().x, py = game.player().y;
        int best_dist = 999;
        for (auto& npc : game.world().npcs()) {
            if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
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

    // Execute ability
    if (ability->skill_id == SkillId::Jab) {
        int damage = game.player().effective_attack() / 2;
        if (damage < 1) damage = 1;
        damage = apply_damage_effects(target->effects, damage);
        if (damage > 0) {
            target->hp -= damage;
            if (target->hp < 0) target->hp = 0;
            game.log("You jab " + target->display_name() + " for " +
                     std::to_string(damage) + " damage!");
        }
    }
    else if (ability->skill_id == SkillId::Cleave) {
        int px = game.player().x, py = game.player().y;
        int hits = 0;
        for (auto& npc : game.world().npcs()) {
            if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
            int dx = std::abs(npc.x - px), dy = std::abs(npc.y - py);
            if (std::max(dx, dy) <= 1) {
                int damage = game.player().effective_attack();
                if (damage < 1) damage = 1;
                damage = apply_damage_effects(npc.effects, damage);
                if (damage > 0) {
                    npc.hp -= damage;
                    if (npc.hp < 0) npc.hp = 0;
                    hits++;
                    game.log("Cleave hits " + npc.display_name() + " for " +
                             std::to_string(damage) + "!");
                }
            }
        }
        if (hits == 0) game.log("Cleave hits nothing.");
    }
    else if (ability->skill_id == SkillId::Quickdraw) {
        // Quickdraw fires at the combat system's current target
        auto* tnpc = game.combat().target_npc();
        if (!tnpc || !tnpc->alive()) {
            game.log("No target for Quickdraw. Use [t] to target first.");
            return false;
        }
        int damage = game.player().effective_attack();
        if (damage < 1) damage = 1;
        damage = apply_damage_effects(tnpc->effects, damage);
        if (damage > 0) {
            tnpc->hp -= damage;
            if (tnpc->hp < 0) tnpc->hp = 0;
            game.log("Quickdraw hits " + tnpc->display_name() + " for " +
                     std::to_string(damage) + "!");
        }
    }
    else if (ability->skill_id == SkillId::Intimidate) {
        // Apply flee effect to target — for now just damage with a message
        // Full flee behavior will come later
        game.log("You intimidate " + target->display_name() + "!");
        game.log(target->display_name() + " cowers in fear.");
        // TODO: apply flee effect when flee behavior is implemented
    }

    // Apply cooldown
    add_effect(game.player().effects, make_cooldown(
        ability->cooldown_effect, ability->name, ability->cooldown_ticks));

    // Advance world
    game.advance_world(ability->action_cost);

    return true;
}

} // namespace astra
