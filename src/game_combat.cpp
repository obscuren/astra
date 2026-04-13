#include "astra/combat_system.h"
#include "astra/animation.h"
#include "astra/dice.h"
#include "astra/display_name.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/item_gen.h"

#include <algorithm>
#include <array>

namespace astra {

static int sign(int v) { return (v > 0) - (v < 0); }

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
}

static int roll_d20(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(1, 20)(rng);
}

static int roll_d10(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(1, 10)(rng);
}

static int weapon_skill_bonus(const Player& player, WeaponClass wc) {
    switch (wc) {
        case WeaponClass::ShortBlade:
            return player_has_skill(player, SkillId::ShortBladeExpertise) ? 2 : 0;
        case WeaponClass::LongBlade:
            return player_has_skill(player, SkillId::LongBladeExpertise) ? 2 : 0;
        case WeaponClass::Pistol:
            return player_has_skill(player, SkillId::SteadyHand) ? 2 : 0;
        case WeaponClass::Rifle:
            return player_has_skill(player, SkillId::Marksman) ? 2 : 0;
        default: return 0;
    }
}

static int apply_resistance(int damage, DamageType type, const Resistances& res) {
    int pct = 0;
    switch (type) {
        case DamageType::Kinetic:    pct = res.kinetic; break;
        case DamageType::Plasma:     pct = res.heat; break;
        case DamageType::Electrical: pct = res.electrical; break;
        case DamageType::Cryo:       pct = res.cold; break;
        case DamageType::Acid:       pct = res.acid; break;
    }
    if (pct <= 0) return damage;
    return std::max(0, damage - damage * pct / 100);
}

static int shield_absorb(int damage, DamageType type, const TypeAffinity& affinity) {
    int bonus_pct = affinity.for_type(type);
    if (bonus_pct > 0) {
        return std::max(1, damage * 100 / (100 + bonus_pct));
    }
    return damage;
}

struct PenetrationResult {
    int total_damage = 0;
    int penetrations = 0;
};

static PenetrationResult roll_penetration(std::mt19937& rng, int str_mod,
                                           int effective_av, const Dice& damage_dice) {
    PenetrationResult result;
    int natural = roll_d10(rng);
    if (natural == 1) return result;
    int pv = natural + str_mod;
    if (natural == 10 || pv > effective_av) {
        result.penetrations = 1;
        result.total_damage = damage_dice.roll(rng);
        if (natural != 10) {
            int excess = pv - effective_av;
            while (excess >= 4) {
                result.penetrations++;
                result.total_damage += damage_dice.roll(rng);
                excess -= 4;
            }
        }
    }
    return result;
}

struct HostileTarget {
    Npc* npc = nullptr;
    bool is_player = false;
    int distance = 9999;
};

static HostileTarget find_nearest_hostile(Npc& self, Game& game) {
    HostileTarget best;
    const int detection_range = 8;

    for (auto& other : game.world().npcs()) {
        if (&other == &self || !other.alive()) continue;
        if (!is_hostile(self.faction, other.faction)) continue;
        int d = chebyshev_dist(self.x, self.y, other.x, other.y);
        if (d <= detection_range && d < best.distance) {
            best.npc = &other;
            best.is_player = false;
            best.distance = d;
        }
    }

    if (is_hostile_to_player(self.faction, game.player())) {
        int d = chebyshev_dist(self.x, self.y, game.player().x, game.player().y);
        if (d <= detection_range && d < best.distance) {
            best.npc = nullptr;
            best.is_player = true;
            best.distance = d;
        }
    }

    return best;
}

void CombatSystem::attack_npc_vs_npc(Npc& attacker, Npc& defender, Game& game) {
    auto& rng = game.world().rng();

    // Attack roll: 1d20 + attacker.level/2 vs defender.dv
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log(display_name(defender) + " dodges " + display_name(attacker) + "'s attack!");
        return;
    }
    int attack_roll = natural + attacker.level / 2;
    if (natural != 20 && attack_roll < defender.dv) {
        game.log(display_name(defender) + " dodges " + display_name(attacker) + "'s attack!");
        return;
    }

    // Determine damage dice
    Dice dmg_dice = attacker.damage_dice;
    if (dmg_dice.empty()) dmg_dice = Dice::make(1, 3);
    DamageType dtype = attacker.damage_type;

    // Penetration: 1d10 + attacker.level/3 vs defender.av + affinity
    int effective_av = defender.av + defender.type_affinity.for_type(dtype);
    auto pen = roll_penetration(rng, attacker.level / 3, effective_av, dmg_dice);
    if (pen.total_damage <= 0) {
        game.log(display_name(attacker) + "'s attack has no effect on " + display_name(defender) + ".");
        return;
    }

    int damage = apply_damage_effects(defender.effects, pen.total_damage);
    if (damage <= 0) {
        game.log(display_name(attacker) + "'s attack has no effect on " + display_name(defender) + ".");
        return;
    }
    defender.hp -= damage;
    if (defender.hp < 0) defender.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, defender.x, defender.y);
    game.log(display_name(attacker) + " strikes " + display_name(defender) +
             " for " + std::to_string(damage) + " " + display_name(dtype) + " damage!");
    if (!defender.alive()) {
        game.log(display_name(defender) + " is destroyed by " + display_name(attacker) + "!");
    }
}

void CombatSystem::process_npc_turn(Npc& npc, Game& game) {
    if (!npc.alive()) return;

    if (npc.return_x >= 0 && npc.return_y >= 0) {
        int rx = npc.return_x, ry = npc.return_y;
        npc.return_x = -1;
        npc.return_y = -1;
        if (game.world().map().passable(rx, ry) &&
            !(game.player().x == rx && game.player().y == ry) &&
            !game.tile_occupied(rx, ry)) {
            npc.x = rx;
            npc.y = ry;
        }
        return;
    }

    if (npc.quickness == 0) return;

    if (has_effect(npc.effects, EffectId::Flee)) {
        int dx = sign(npc.x - game.player().x);
        int dy = sign(npc.y - game.player().y);
        struct { int x, y; } candidates[] = {
            {dx, dy}, {dx, 0}, {0, dy}, {-dy, dx}, {dy, -dx}
        };
        for (auto [cx, cy] : candidates) {
            if (cx == 0 && cy == 0) continue;
            int nx = npc.x + cx;
            int ny = npc.y + cy;
            if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
                npc.x = nx;
                npc.y = ny;
                return;
            }
        }
        return;
    }

    auto target = find_nearest_hostile(npc, game);

    if (target.is_player) {
        int dist = target.distance;
        if (dist <= 1) {
            auto& rng = game.world().rng();

            // Attack roll: 1d20 + npc.level/2 vs player.effective_dv()
            int natural = roll_d20(rng);
            if (natural == 1) {
                game.log("You dodge " + display_name(npc) + "'s attack!");
                return;
            }
            int attack_roll = natural + npc.level / 2;
            if (natural != 20 && attack_roll < game.player().effective_dv()) {
                game.log("You dodge " + display_name(npc) + "'s attack!");
                return;
            }

            Dice dmg_dice = npc.damage_dice;
            if (dmg_dice.empty()) dmg_dice = Dice::make(1, 3);
            DamageType dtype = npc.damage_type;

            // Shield check
            if (game.player().shield_hp > 0) {
                // Penetrate shield as AV=0
                auto pen = roll_penetration(rng, npc.level / 3, 0, dmg_dice);
                if (pen.total_damage <= 0) {
                    game.log(display_name(npc) + "'s attack is absorbed by your shield.");
                    return;
                }
                int absorbed = shield_absorb(pen.total_damage, dtype, game.player().shield_affinity);
                game.player().shield_hp -= absorbed;
                if (game.player().shield_hp < 0) game.player().shield_hp = 0;
                game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
                game.log(display_name(npc) + " hits your shield for " +
                         std::to_string(absorbed) + " " + display_name(dtype) + " damage. [Shield " +
                         std::to_string(game.player().shield_hp) + "/" +
                         std::to_string(game.player().shield_max_hp) + "]");
                return;
            }

            // Penetration: 1d10 + npc.level/3 vs player.effective_av(dtype)
            int eff_av = game.player().effective_av(dtype);
            auto pen = roll_penetration(rng, npc.level / 3, eff_av, dmg_dice);
            if (pen.total_damage <= 0) {
                game.log(display_name(npc) + " strikes you but deals no damage.");
                return;
            }

            int damage = apply_resistance(pen.total_damage, dtype, game.player().resistances);
            damage = apply_damage_effects(game.player().effects, damage);
            if (damage <= 0) {
                game.log(display_name(npc) + " strikes you but deals no damage.");
                return;
            }
            game.player().hp -= damage;
            if (game.player().hp < 0) game.player().hp = 0;
            game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
            game.log(display_name(npc) + " strikes you for " +
                     std::to_string(damage) + " " + display_name(dtype) + " damage!");
            if (game.player().hp <= 0) {
                game.set_death_message("Slain by " + display_name(npc));
            }
            return;
        }
        int dx = sign(game.player().x - npc.x);
        int dy = sign(game.player().y - npc.y);
        struct { int x, y; } candidates[] = {{dx, dy}, {dx, 0}, {0, dy}};
        for (auto [cx, cy] : candidates) {
            if (cx == 0 && cy == 0) continue;
            int nx = npc.x + cx;
            int ny = npc.y + cy;
            if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
                npc.x = nx;
                npc.y = ny;
                return;
            }
        }
        return;
    }

    if (target.npc) {
        int dist = target.distance;
        if (dist <= 1) {
            attack_npc_vs_npc(npc, *target.npc, game);
            return;
        }
        int dx = sign(target.npc->x - npc.x);
        int dy = sign(target.npc->y - npc.y);
        struct { int x, y; } candidates[] = {{dx, dy}, {dx, 0}, {0, dy}};
        for (auto [cx, cy] : candidates) {
            if (cx == 0 && cy == 0) continue;
            int nx = npc.x + cx;
            int ny = npc.y + cy;
            if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
                npc.x = nx;
                npc.y = ny;
                return;
            }
        }
        return;
    }

    std::array<std::pair<int,int>, 4> dirs = {{{0,-1},{0,1},{-1,0},{1,0}}};
    std::shuffle(dirs.begin(), dirs.end(), game.world().rng());
    for (auto [dx, dy] : dirs) {
        int nx = npc.x + dx;
        int ny = npc.y + dy;
        if (game.world().map().passable(nx, ny) && !game.tile_occupied(nx, ny)) {
            npc.x = nx;
            npc.y = ny;
            return;
        }
    }
}

void CombatSystem::attack_npc(Npc& npc, Game& game) {
    auto& rng = game.world().rng();

    // Determine weapon and damage dice
    const auto& weapon = game.player().equipment.right_hand;
    Dice dmg_dice = Dice::make(1, 3); // unarmed
    DamageType dtype = DamageType::Kinetic;
    WeaponClass wc = WeaponClass::None;
    if (weapon && !weapon->damage_dice.empty()) {
        dmg_dice = weapon->damage_dice;
        dtype = weapon->damage_type;
        wc = weapon->weapon_class;
    }

    // Attack roll: 1d20 + (AGI-10)/2 + weapon_skill_bonus vs npc.dv
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log(display_name(npc) + " dodges your attack!");
        return;
    }
    int agi_mod = (game.player().attributes.agility - 10) / 2;
    int attack_roll = natural + agi_mod + weapon_skill_bonus(game.player(), wc);
    if (natural != 20 && attack_roll < npc.dv) {
        game.log(display_name(npc) + " dodges your attack! (roll " +
                 std::to_string(attack_roll) + " vs DV " + std::to_string(npc.dv) + ")");
        return;
    }
    game.log("Attack roll: " + std::to_string(attack_roll) + " vs DV " + std::to_string(npc.dv) +
             (natural == 20 ? " (nat 20!)" : ""));

    // Critical hit check
    bool is_crit = false;
    int crit_chance = std::clamp((game.player().attributes.luck - 8) * 2 + 3, 0, 30);
    if (std::uniform_int_distribution<int>(1, 100)(rng) <= crit_chance) {
        is_crit = true;
    }

    int damage = 0;
    if (is_crit) {
        // Auto-penetrate, roll damage dice twice
        damage = dmg_dice.roll(rng) + dmg_dice.roll(rng);
    } else {
        // Penetration: 1d10 + (STR-10)/2 vs npc.av + npc.type_affinity
        int str_mod = (game.player().attributes.strength - 10) / 2;
        int effective_av = npc.av + npc.type_affinity.for_type(dtype);
        auto pen = roll_penetration(rng, str_mod, effective_av, dmg_dice);
        damage = pen.total_damage;
    }

    if (damage <= 0) {
        game.log("Your attack has no effect on " + display_name(npc) + ".");
        return;
    }

    damage = apply_damage_effects(npc.effects, damage);
    if (damage <= 0) {
        game.log("Your attack has no effect on " + display_name(npc) + ".");
        return;
    }
    npc.hp -= damage;
    if (npc.hp < 0) npc.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, npc.x, npc.y);
    if (is_crit) {
        game.log("CRITICAL HIT! You strike " + display_name(npc) + " for " +
            std::to_string(damage) + " " + display_name(dtype) + " damage!");
    } else {
        game.log("You strike " + display_name(npc) + " for " +
            std::to_string(damage) + " " + display_name(dtype) + " damage!");
    }
    if (!npc.alive()) {
        game.log(display_name(npc) + " is destroyed!");
        game.player().kills++;
        // Reputation penalty for killing a faction NPC
        if (!npc.faction.empty()) {
            for (auto& fs : game.player().reputation) {
                if (fs.faction_name == npc.faction) {
                    fs.reputation = std::max(fs.reputation - 30, -600);
                    game.log("Your reputation with " + npc.faction + " decreased.");
                    break;
                }
            }
        }
        game.quests().on_npc_killed(npc.role);
        int xp = npc.xp_reward();
        if (xp > 0) {
            game.player().xp += xp;
            game.log("You gain " + std::to_string(xp) + " XP.");
            check_level_up(game);
        }
        int credits = npc.level * 2 + (npc.elite ? 5 : 0);
        if (credits > 0) {
            game.player().money += credits;
            game.log("You salvage " + std::to_string(credits) + "$.");
        }
        // Loot drop (50% chance)
        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
            Item loot = generate_loot_drop(rng, npc.level);
            game.log("Dropped: " + display_name(loot));
            game.world().ground_items().push_back({npc.x, npc.y, std::move(loot)});
        }
    }
}

void CombatSystem::begin_targeting(Game& game) {
    targeting_ = true;
    blink_phase_ = 0;

    // Find nearest visible hostile NPC
    Npc* nearest = nullptr;
    int best_dist = 9999;
    for (auto& npc : game.world().npcs()) {
        if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
        if (game.world().visibility().get(npc.x, npc.y) != Visibility::Visible) continue;
        int d = chebyshev_dist(game.player().x, game.player().y, npc.x, npc.y);
        if (d < best_dist) {
            best_dist = d;
            nearest = &npc;
        }
    }

    if (nearest) {
        target_x_ = nearest->x;
        target_y_ = nearest->y;
    } else {
        target_x_ = game.player().x;
        target_y_ = game.player().y;
    }

    game.log("Targeting mode. Move cursor, [Enter] confirm, [Esc] cancel.");
}

void CombatSystem::handle_targeting_input(int key, Game& game) {
    auto try_move_cursor = [&](int dx, int dy) {
        // Scan up to 20 tiles in direction to skip walls/unexplored gaps
        for (int i = 1; i <= 20; ++i) {
            int nx = target_x_ + dx * i;
            int ny = target_y_ + dy * i;
            if (nx < 0 || nx >= game.world().map().width() || ny < 0 || ny >= game.world().map().height()) return;
            if (game.world().map().passable(nx, ny) && game.world().visibility().get(nx, ny) == Visibility::Visible) {
                target_x_ = nx;
                target_y_ = ny;
                return;
            }
        }
    };
    switch (key) {
        case 'k': case KEY_UP:    try_move_cursor( 0, -1); break;
        case 'j': case KEY_DOWN:  try_move_cursor( 0,  1); break;
        case 'h': case KEY_LEFT:  try_move_cursor(-1,  0); break;
        case 'l': case KEY_RIGHT: try_move_cursor( 1,  0); break;
        case '\n': case '\r': {
            // Check for alive NPC at cursor
            Npc* found = nullptr;
            for (auto& npc : game.world().npcs()) {
                if (npc.alive() && npc.x == target_x_ && npc.y == target_y_) {
                    found = &npc;
                    break;
                }
            }
            if (found) {
                target_npc_ = found;
                targeting_ = false;
                game.log("Targeted: " + found->label());
            } else {
                game.log("No target there.");
            }
            break;
        }
        case '\033': // Escape
            targeting_ = false;
            target_npc_ = nullptr;
            game.log("Targeting cancelled.");
            break;
        default:
            break;
    }
}

void CombatSystem::shoot_target(Game& game) {
    // Check weapon equipped
    auto& weapon = game.player().equipment.missile;
    if (!weapon || !weapon->ranged) {
        game.log("No ranged weapon equipped.");
        return;
    }

    if (!target_npc_ || !target_npc_->alive()) {
        target_npc_ = nullptr;
        game.log("No target selected. Press [t] to target.");
        return;
    }

    if (game.world().visibility().get(target_npc_->x, target_npc_->y) != Visibility::Visible) {
        game.log("Target not visible.");
        return;
    }

    // Check range
    auto& rd = *weapon->ranged;
    int dist = chebyshev_dist(game.player().x, game.player().y, target_npc_->x, target_npc_->y);
    int effective_range = rd.max_range;
    if (weapon->weapon_class == WeaponClass::Rifle
        && player_has_skill(game.player(), SkillId::Marksman))
        effective_range += 2;
    if (dist > effective_range) {
        game.log("Target out of range (" + std::to_string(dist) + "/" +
            std::to_string(effective_range) + ").");
        return;
    }

    // Check charge — auto-reload if empty
    if (rd.current_charge < rd.charge_per_shot) {
        // Try auto-reload from inventory
        bool reloaded = false;
        for (int i = 0; i < static_cast<int>(game.player().inventory.items.size()); ++i) {
            if (game.player().inventory.items[i].type == ItemType::Battery) {
                int added = std::min(5, rd.charge_capacity - rd.current_charge);
                rd.current_charge += added;
                game.log("Auto-reload: +" + std::to_string(added) + " charge.");
                auto& cell = game.player().inventory.items[i];
                if (cell.stackable && cell.stack_count > 1) {
                    --cell.stack_count;
                } else {
                    game.player().inventory.items.erase(game.player().inventory.items.begin() + i);
                }
                reloaded = true;
                break;
            }
        }
        if (!reloaded) {
            game.log("Weapon empty. No energy cells to reload.");
            return;
        }
        if (rd.current_charge < rd.charge_per_shot) {
            game.log("Not enough charge to fire.");
            return;
        }
    }

    // Consume charge
    rd.current_charge -= rd.charge_per_shot;

    auto& rng = game.world().rng();

    // Determine weapon dice
    Dice dmg_dice = weapon->damage_dice;
    if (dmg_dice.empty()) dmg_dice = Dice::make(1, 3);
    DamageType dtype = weapon->damage_type;
    WeaponClass wc = weapon->weapon_class;

    // Attack roll: 1d20 + (AGI-10)/2 + weapon_skill_bonus vs npc.dv
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log(display_name(*target_npc_) + " dodges your shot!");
        game.advance_world(ActionCost::shoot);
        return;
    }
    int agi_mod = (game.player().attributes.agility - 10) / 2;
    int attack_roll = natural + agi_mod + weapon_skill_bonus(game.player(), wc);
    if (natural != 20 && attack_roll < target_npc_->dv) {
        game.log(display_name(*target_npc_) + " dodges your shot! (roll " +
                 std::to_string(attack_roll) + " vs DV " + std::to_string(target_npc_->dv) + ")");
        game.advance_world(ActionCost::shoot);
        return;
    }

    // Critical hit check
    bool is_crit = false;
    int crit_chance = std::clamp((game.player().attributes.luck - 8) * 2 + 3, 0, 30);
    if (std::uniform_int_distribution<int>(1, 100)(rng) <= crit_chance) {
        is_crit = true;
    }

    int damage = 0;
    if (is_crit) {
        damage = dmg_dice.roll(rng) + dmg_dice.roll(rng);
    } else {
        int str_mod = (game.player().attributes.strength - 10) / 2;
        int effective_av = target_npc_->av + target_npc_->type_affinity.for_type(dtype);
        auto pen = roll_penetration(rng, str_mod, effective_av, dmg_dice);
        damage = pen.total_damage;
    }

    if (damage <= 0) {
        game.log("Your shot has no effect on " + display_name(*target_npc_) + ".");
        game.advance_world(ActionCost::shoot);
        return;
    }

    damage = apply_damage_effects(target_npc_->effects, damage);
    if (damage <= 0) {
        game.log("Your shot has no effect on " + display_name(*target_npc_) + ".");
        game.advance_world(ActionCost::shoot);
        return;
    }
    target_npc_->hp -= damage;
    if (target_npc_->hp < 0) target_npc_->hp = 0;
    // Projectile travel + damage flash
    game.animations().spawn_effect_line(anim_projectile,
        game.player().x, game.player().y,
        target_npc_->x, target_npc_->y);
    game.animations().spawn_effect(anim_damage_flash, target_npc_->x, target_npc_->y);
    std::string hit_msg = is_crit ? "CRITICAL HIT! You shoot " : "You shoot ";
    game.log(hit_msg + display_name(*target_npc_) + " for " +
        std::to_string(damage) + " " + display_name(dtype) + " damage. [" +
        std::to_string(rd.current_charge) + "/" +
        std::to_string(rd.charge_capacity) + "]");

    if (!target_npc_->alive()) {
        game.log(display_name(*target_npc_) + " is destroyed!");
        game.player().kills++;
        // Reputation penalty for killing a faction NPC
        if (!target_npc_->faction.empty()) {
            for (auto& fs : game.player().reputation) {
                if (fs.faction_name == target_npc_->faction) {
                    fs.reputation = std::max(fs.reputation - 30, -600);
                    game.log("Your reputation with " + target_npc_->faction + " decreased.");
                    break;
                }
            }
        }
        game.quests().on_npc_killed(target_npc_->role);
        int xp = target_npc_->xp_reward();
        if (xp > 0) {
            game.player().xp += xp;
            game.log("You gain " + std::to_string(xp) + " XP.");
            check_level_up(game);
        }
        // Loot drop (50% chance)
        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
            Item loot = generate_loot_drop(rng, target_npc_->level);
            game.log("Dropped: " + display_name(loot));
            game.world().ground_items().push_back({target_npc_->x, target_npc_->y, std::move(loot)});
        }
        target_npc_ = nullptr;
    }

    game.advance_world(ActionCost::shoot);
}

void CombatSystem::reload_weapon(Game& game) {
    auto& weapon = game.player().equipment.missile;
    if (!weapon || !weapon->ranged) {
        game.log("No ranged weapon equipped.");
        return;
    }

    auto& rd = *weapon->ranged;
    if (rd.current_charge >= rd.charge_capacity) {
        game.log(weapon->label() + " is fully charged.");
        return;
    }

    for (int i = 0; i < static_cast<int>(game.player().inventory.items.size()); ++i) {
        if (game.player().inventory.items[i].type == ItemType::Battery) {
            int added = std::min(5, rd.charge_capacity - rd.current_charge);
            rd.current_charge += added;
            game.log("Reloaded " + weapon->label() + ". (+" + std::to_string(added) +
                " charge, " + std::to_string(rd.current_charge) + "/" +
                std::to_string(rd.charge_capacity) + ")");
            auto& cell = game.player().inventory.items[i];
            if (cell.stackable && cell.stack_count > 1) {
                --cell.stack_count;
            } else {
                game.player().inventory.items.erase(game.player().inventory.items.begin() + i);
            }
            game.advance_world(ActionCost::wait);
            return;
        }
    }

    game.log("No energy cells to reload.");
}



void CombatSystem::reload_shield(Game& game) {
    auto& shield = game.player().equipment.shield;
    if (!shield || shield->shield_capacity <= 0) {
        game.log("No energy shield equipped.");
        return;
    }
    if (game.player().shield_hp >= game.player().shield_max_hp) {
        game.log("Shield is at full charge.");
        return;
    }
    for (int i = 0; i < static_cast<int>(game.player().inventory.items.size()); ++i) {
        if (game.player().inventory.items[i].type == ItemType::Battery) {
            int added = std::min(5, game.player().shield_max_hp - game.player().shield_hp);
            game.player().shield_hp += added;
            game.log("Shield recharged +" + std::to_string(added) + " HP. (" +
                     std::to_string(game.player().shield_hp) + "/" +
                     std::to_string(game.player().shield_max_hp) + ")");
            auto& cell = game.player().inventory.items[i];
            if (cell.stackable && cell.stack_count > 1) {
                --cell.stack_count;
            } else {
                game.player().inventory.items.erase(game.player().inventory.items.begin() + i);
            }
            game.advance_world(ActionCost::wait);
            return;
        }
    }
    game.log("No energy cells to recharge shield.");
}

void CombatSystem::remove_dead_npcs(Game& game) {
    // Nullify target_npc_ if it died
    if (target_npc_ && !target_npc_->alive()) {
        target_npc_ = nullptr;
    }
    // Close dialog if interacting NPC died
    if (game.dialog().interacting_npc() && !game.dialog().interacting_npc()->alive()) {
        game.dialog().close();
    }
    game.world().npcs().erase(
        std::remove_if(game.world().npcs().begin(), game.world().npcs().end(),
                        [](const Npc& n) { return !n.alive(); }),
        game.world().npcs().end());
}

// --- Level-up rewards (easy to balance) ---
static constexpr int attr_points_per_level = 2;
static constexpr int skill_points_per_level = 50;
static constexpr float xp_scale_factor = 1.5f;

void CombatSystem::check_level_up(Game& game) {
    while (game.player().xp >= game.player().max_xp) {
        game.player().xp -= game.player().max_xp;
        game.player().level++;
        game.player().max_xp = static_cast<int>(game.player().max_xp * xp_scale_factor);
        game.player().attribute_points += attr_points_per_level;
        game.player().skill_points += skill_points_per_level;

        // Heal to full on level up
        game.player().max_hp = game.player().effective_max_hp();
        game.player().hp = game.player().max_hp;

        game.animations().spawn_effect(anim_level_up, game.player().x, game.player().y);
        game.log("LEVEL UP! You are now level " + std::to_string(game.player().level) + ".");
        game.log("  +" + std::to_string(attr_points_per_level) + " attribute points, +"
            + std::to_string(skill_points_per_level) + " SP.");
    }
}


void CombatSystem::reset() {
    targeting_ = false;
    target_x_ = 0;
    target_y_ = 0;
    blink_phase_ = 0;
    target_npc_ = nullptr;
}

} // namespace astra
