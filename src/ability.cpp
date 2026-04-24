#include "astra/ability.h"
#include "astra/dice.h"
#include "astra/display_name.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/player.h"
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

class TumbleAbility : public Ability {
public:
    TumbleAbility() {
        skill_id = SkillId::Tumble;
        name = "Tumble";
        description = "Dash up to 3 tiles, ignoring anything in between.";
        cooldown_ticks = 25;
        cooldown_effect = EffectId::CooldownTumble;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::None;
        action_cost = 50;
        telegraph = TelegraphSpec{
            .shape = TelegraphShape::Line,
            .range = 3,
            .width = 1,
            .diagonals = true,
            .stop_at_wall = true,
            .stop_at_enemy = false,
            .require_walkable_dest = true,
        };
    }

    bool execute(Game&, Npc*) override { return false; }

    bool execute_telegraphed(Game& game, const TelegraphResult& res) override {
        if (res.dest_x < 0 || res.dest_y < 0) return false;
        if (res.dest_x == game.player().x && res.dest_y == game.player().y) return false;
        for (const auto& npc : game.world().npcs()) {
            if (npc.alive() && npc.x == res.dest_x && npc.y == res.dest_y) {
                game.log("Landing blocked by " + npc.label() + ".");
                return false;
            }
        }
        game.player().x = res.dest_x;
        game.player().y = res.dest_y;
        game.refresh_view();
        game.log("You tumble to a new position.");
        return true;
    }
};

class AdrenalineRushAbility : public Ability {
public:
    AdrenalineRushAbility() {
        skill_id = SkillId::AdrenalineRush;
        name = "Adrenaline Rush";
        description = "+2 DV and +25% quickness for 3 ticks.";
        cooldown_ticks = 40;
        cooldown_effect = EffectId::CooldownAdrenaline;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::None;
        action_cost = 25;
    }

    bool execute(Game& game, Npc* /*target*/) override {
        add_effect(game.player().effects, make_adrenaline_rush_ge(3));
        game.log("Adrenaline floods your system!");
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

    int effective_cooldown(const Player& player) const override {
        if (player_has_skill(player, SkillId::AdvancedFireMaking)) {
            return (cooldown_ticks * 60) / 100;   // -40%
        }
        return cooldown_ticks;
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

// ── Default virtual implementations ─────────────────────────────────

int Ability::effective_cooldown(const Player& /*player*/) const {
    return cooldown_ticks;
}

// ── Catalog ─────────────────────────────────────────────────────────

static std::vector<std::unique_ptr<Ability>> build_catalog() {
    std::vector<std::unique_ptr<Ability>> cat;
    cat.push_back(std::make_unique<JabAbility>());
    cat.push_back(std::make_unique<CleaveAbility>());
    cat.push_back(std::make_unique<QuickdrawAbility>());
    cat.push_back(std::make_unique<IntimidateAbility>());
    cat.push_back(std::make_unique<TumbleAbility>());
    cat.push_back(std::make_unique<AdrenalineRushAbility>());
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

} // namespace astra
