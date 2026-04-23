#include "astra/ability.h"
#include "astra/dice.h"
#include "astra/display_name.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/world_constants.h"

namespace astra {

// ── Concrete abilities ──────────────────────────────────────────────

class JabAbility : public Ability {
public:
    JabAbility() {
        skill_id = SkillId::Jab;
        name = "Jab";
        description = "Quick off-hand strike for 50% damage.";
        cooldown_ticks = 3;
        cooldown_effect = EffectId::CooldownJab;
        needs_adjacent_target = true;
        required_weapon = WeaponClass::ShortBlade;
        action_cost = 25;
    }

    bool execute(Game& game, Npc* target) override {
        const auto& weapon = game.player().equipment.right_hand;
        Dice dmg_dice = Dice::make(1, 3);
        DamageType dtype = DamageType::Kinetic;
        if (weapon && !weapon->damage_dice.empty()) {
            dmg_dice = weapon->damage_dice;
            dtype = weapon->damage_type;
        }
        int damage = std::max(1, dmg_dice.roll(game.world().rng()) / 2);
        damage = apply_damage_effects(target->effects, damage);
        if (damage > 0) {
            target->hp -= damage;
            if (target->hp < 0) target->hp = 0;
            game.log("You jab " + display_name(*target) + " for " +
                     std::to_string(damage) + " " + display_name(dtype) + " damage!");
        }
        return true;
    }
};

class CleaveAbility : public Ability {
public:
    CleaveAbility() {
        skill_id = SkillId::Cleave;
        name = "Cleave";
        description = "Strike all adjacent enemies.";
        cooldown_ticks = 5;
        cooldown_effect = EffectId::CooldownCleave;
        needs_adjacent_target = true;
        required_weapon = WeaponClass::LongBlade;
        action_cost = 50;
    }

    bool execute(Game& game, Npc* /*target*/) override {
        const auto& weapon = game.player().equipment.right_hand;
        Dice dmg_dice = Dice::make(1, 3);
        DamageType dtype = DamageType::Kinetic;
        if (weapon && !weapon->damage_dice.empty()) {
            dmg_dice = weapon->damage_dice;
            dtype = weapon->damage_type;
        }
        int px = game.player().x, py = game.player().y;
        int hits = 0;
        for (auto& npc : game.world().npcs()) {
            if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
            int dx = std::abs(npc.x - px), dy = std::abs(npc.y - py);
            if (std::max(dx, dy) <= 1) {
                int damage = dmg_dice.roll(game.world().rng());
                if (damage < 1) damage = 1;
                damage = apply_damage_effects(npc.effects, damage);
                if (damage > 0) {
                    npc.hp -= damage;
                    if (npc.hp < 0) npc.hp = 0;
                    hits++;
                    game.log("Cleave hits " + npc.label() + " for " +
                             std::to_string(damage) + " " + display_name(dtype) + "!");
                }
            }
        }
        if (hits == 0) game.log("Cleave hits nothing.");
        return true;
    }
};

class QuickdrawAbility : public Ability {
public:
    QuickdrawAbility() {
        skill_id = SkillId::Quickdraw;
        name = "Quickdraw";
        description = "Fire at reduced action cost.";
        cooldown_ticks = 3;
        cooldown_effect = EffectId::CooldownQuickdraw;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::Pistol;
        action_cost = 25;
    }

    bool execute(Game& game, Npc* /*target*/) override {
        auto* tnpc = game.combat().target_npc();
        if (!tnpc || !tnpc->alive()) {
            game.log("No target for Quickdraw. Use [t] to target first.");
            return false;
        }
        const auto& weapon = game.player().equipment.missile;
        Dice dmg_dice = Dice::make(1, 3);
        DamageType dtype = DamageType::Kinetic;
        if (weapon && !weapon->damage_dice.empty()) {
            dmg_dice = weapon->damage_dice;
            dtype = weapon->damage_type;
        }
        int damage = dmg_dice.roll(game.world().rng());
        if (damage < 1) damage = 1;
        damage = apply_damage_effects(tnpc->effects, damage);
        if (damage > 0) {
            tnpc->hp -= damage;
            if (tnpc->hp < 0) tnpc->hp = 0;
            game.log("Quickdraw hits " + display_name(*tnpc) + " for " +
                     std::to_string(damage) + " " + display_name(dtype) + "!");
        }
        return true;
    }
};

class IntimidateAbility : public Ability {
public:
    IntimidateAbility() {
        skill_id = SkillId::Intimidate;
        name = "Intimidate";
        description = "Frighten an adjacent hostile, causing it to flee.";
        cooldown_ticks = 10;
        cooldown_effect = EffectId::CooldownIntimidate;
        needs_adjacent_target = true;
        required_weapon = WeaponClass::None;
        action_cost = 50;
    }

    bool execute(Game& game, Npc* target) override {
        int duration = 3 + (game.player().attributes.willpower - 10) / 2;
        if (duration < 2) duration = 2;
        add_effect(target->effects, make_flee_ge(duration));
        game.log("You intimidate " + display_name(*target) + "!");
        game.log(display_name(*target) + " flees in fear! (" +
                 std::to_string(duration) + " ticks)");
        return true;
    }
};

class CampMakingAbility : public Ability {
public:
    CampMakingAbility() {
        skill_id = SkillId::CampMaking;
        name = "Camp Making";
        description = "Build a campfire on an adjacent tile. "
                      "Grants Cozy (2x regen) within 6 tiles.";
        cooldown_ticks = world::camp_making_cooldown_ticks;
        cooldown_effect = EffectId::CooldownCampMaking;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::None;
        action_cost = world::camp_making_action_cost;
    }

    bool execute(Game& game, Npc* /*target*/) override {
        auto& map = game.world().map();
        const int px = game.player().x;
        const int py = game.player().y;

        // 8-neighbour scan in a fixed order. Pick the first adjacent tile
        // that is passable and has no fixture already on it.
        static constexpr int dx8[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
        static constexpr int dy8[8] = {-1, -1, -1,  0, 0,  1, 1, 1};

        int target_x = -1, target_y = -1;
        for (int i = 0; i < 8; ++i) {
            int tx = px + dx8[i], ty = py + dy8[i];
            if (tx < 0 || tx >= map.width() || ty < 0 || ty >= map.height()) continue;
            if (!map.passable(tx, ty)) continue;
            if (map.fixture_id(tx, ty) >= 0) continue;
            target_x = tx;
            target_y = ty;
            break;
        }

        if (target_x < 0) {
            game.log("No space to build a camp.");
            return false;
        }

        FixtureData fd = make_fixture(FixtureType::Campfire);
        fd.spawn_tick = game.world().world_tick();
        map.add_fixture(target_x, target_y, std::move(fd));

        game.log("You build a crackling campfire.");
        return true;
    }
};

// ── Catalog ─────────────────────────────────────────────────────────

static std::vector<std::unique_ptr<Ability>> build_catalog() {
    std::vector<std::unique_ptr<Ability>> cat;
    cat.push_back(std::make_unique<JabAbility>());
    cat.push_back(std::make_unique<CleaveAbility>());
    cat.push_back(std::make_unique<QuickdrawAbility>());
    cat.push_back(std::make_unique<IntimidateAbility>());
    cat.push_back(std::make_unique<CampMakingAbility>());
    return cat;
}

const std::vector<std::unique_ptr<Ability>>& ability_catalog() {
    static auto catalog = build_catalog();
    return catalog;
}

Ability* find_ability(SkillId id) {
    for (const auto& a : ability_catalog()) {
        if (a->skill_id == id) return a.get();
    }
    return nullptr;
}

std::vector<SkillId> get_ability_bar(const Player& player) {
    std::vector<SkillId> bar;
    for (const auto& a : ability_catalog()) {
        if (player_has_skill(player, a->skill_id)) {
            bar.push_back(a->skill_id);
            if (bar.size() >= 5) break;
        }
    }
    return bar;
}

// ── Use ability ─────────────────────────────────────────────────────

static Effect make_cooldown(EffectId id, const std::string& name, int duration) {
    Effect e;
    e.id = id;
    e.name = name + " CD";
    e.color = Color::DarkGray;
    e.duration = duration;
    e.remaining = duration;
    // Cooldowns are not shown in the effect bar — the ability-bar UI
    // already surfaces remaining cooldown per slot.
    e.show_in_bar = false;
    return e;
}

bool use_ability(int slot, Game& game) {
    auto bar = get_ability_bar(game.player());
    if (slot < 0 || slot >= static_cast<int>(bar.size())) {
        game.log("No ability in that slot.");
        return false;
    }

    auto* ability = find_ability(bar[slot]);
    if (!ability) return false;

    // Check cooldown
    if (has_effect(game.player().effects, ability->cooldown_effect)) {
        const auto* cd = find_effect(game.player().effects, ability->cooldown_effect);
        game.log(ability->name + " is on cooldown (" +
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
            game.log(ability->name + " requires the right weapon equipped.");
            return false;
        }
    }

    // Find adjacent hostile target (if needed)
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

    // Execute
    if (!ability->execute(game, target)) {
        return false;
    }

    // Apply cooldown
    add_effect(game.player().effects, make_cooldown(
        ability->cooldown_effect, ability->name, ability->cooldown_ticks));

    // Advance world
    game.advance_world(ability->action_cost);

    return true;
}

} // namespace astra
