#include "astra/combat_system.h"
#include "astra/animation.h"
#include "astra/game.h"
#include "astra/item_gen.h"

#include <algorithm>
#include <array>

namespace astra {

static int sign(int v) { return (v > 0) - (v < 0); }

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
}

static bool roll_percent(std::mt19937& rng, int chance) {
    if (chance <= 0) return false;
    return std::uniform_int_distribution<int>(1, 100)(rng) <= chance;
}

static int npc_dodge_chance(const Npc& npc) {
    int chance = npc.level + (npc.elite ? 5 : 0);
    return std::min(chance, 25);
}

void CombatSystem::process_npc_turn(Npc& npc, Game& game) {
    if (!npc.alive()) return;

    // Displaced NPCs try to return to their original position
    if (npc.return_x >= 0 && npc.return_y >= 0) {
        int rx = npc.return_x, ry = npc.return_y;
        npc.return_x = -1;
        npc.return_y = -1;
        // Only move back if the spot is free
        if (game.world().map().passable(rx, ry) &&
            !(game.player().x == rx && game.player().y == ry) &&
            !game.tile_occupied(rx, ry)) {
            npc.x = rx;
            npc.y = ry;
        }
        return;
    }

    if (npc.disposition == Disposition::Friendly || npc.quickness == 0)
        return;

    if (npc.disposition == Disposition::Hostile) {
        int dist = chebyshev_dist(npc.x, npc.y, game.player().x, game.player().y);

        // Fleeing — move away from player instead of attacking
        if (has_effect(npc.effects, EffectId::Flee)) {
            int dx = sign(npc.x - game.player().x);
            int dy = sign(npc.y - game.player().y);
            // Try away diagonal, then each cardinal fallback
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
            return; // cornered, skip turn
        }

        // Adjacent — attack
        if (dist <= 1) {
            // Player dodge check
            int dodge_chance = std::min(game.player().effective_dodge() * 2, 50);
            if (roll_percent(game.world().rng(), dodge_chance)) {
                game.log("You dodge " + npc.display_name() + "'s attack!");
                return;
            }
            int raw_damage = npc.attack_damage();
            int defense = game.player().effective_defense();
            int damage = raw_damage - defense;
            if (damage < 1) damage = 1;
            damage = apply_damage_effects(game.player().effects, damage);
            if (damage <= 0) {
                game.log(npc.display_name() + " strikes you but deals no damage.");
                return;
            }
            game.player().hp -= damage;
            if (game.player().hp < 0) game.player().hp = 0;
            game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
            game.log(npc.display_name() + " strikes you for " +
                std::to_string(damage) + " damage!");
            if (game.player().hp <= 0) {
                game.set_death_message("Slain by " + npc.display_name());
            }
            return;
        }

        // Within detection range — chase
        if (dist <= 8) {
            int dx = sign(game.player().x - npc.x);
            int dy = sign(game.player().y - npc.y);

            // Try diagonal, then each cardinal fallback
            struct { int x, y; } candidates[] = {
                {dx, dy}, {dx, 0}, {0, dy}
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
            return; // all blocked, skip turn
        }

        // Fall through to wander
    }

    // Wander: try random cardinal directions
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
    // Dodge check
    if (roll_percent(game.world().rng(), npc_dodge_chance(npc))) {
        game.log(npc.display_name() + " dodges your attack!");
        return;
    }

    int damage = game.player().effective_attack();
    // Weapon expertise bonus
    const auto& weapon = game.player().equipment.right_hand;
    if (weapon) {
        if (weapon->weapon_class == WeaponClass::ShortBlade
            && player_has_skill(game.player(), SkillId::ShortBladeExpertise))
            damage += 1;
        if (weapon->weapon_class == WeaponClass::LongBlade
            && player_has_skill(game.player(), SkillId::LongBladeExpertise))
            damage += 1;
    }
    if (damage < 1) damage = 1;

    // Critical hit check
    bool is_crit = false;
    int crit_chance = std::clamp((game.player().attributes.luck - 8) * 2 + 3, 0, 30);
    if (roll_percent(game.world().rng(), crit_chance)) {
        damage = damage + (damage + 1) / 2;
        is_crit = true;
    }

    damage = apply_damage_effects(npc.effects, damage);
    if (damage <= 0) {
        game.log("Your attack has no effect on " + npc.display_name() + ".");
        return;
    }
    npc.hp -= damage;
    if (npc.hp < 0) npc.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, npc.x, npc.y);
    if (is_crit) {
        game.log("CRITICAL HIT! You strike " + npc.display_name() + " for " +
            std::to_string(damage) + " damage!");
    } else {
        game.log("You strike " + npc.display_name() + " for " +
            std::to_string(damage) + " damage!");
    }
    if (!npc.alive()) {
        game.log(npc.display_name() + " is destroyed!");
        game.player().kills++;
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
        if (std::uniform_int_distribution<int>(0, 1)(game.world().rng()) == 0) {
            Item loot = generate_loot_drop(game.world().rng(), npc.level);
            game.log("Dropped: " + loot.name);
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
        if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
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
                game.log("Targeted: " + found->display_name());
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

    // Dodge check (ranged — ammo already consumed)
    if (roll_percent(game.world().rng(), npc_dodge_chance(*target_npc_))) {
        game.log(target_npc_->display_name() + " dodges your shot!");
        game.advance_world(ActionCost::shoot);
        return;
    }

    // Damage = effective attack (includes STR modifier + all equipment)
    int damage = game.player().effective_attack();
    // Ranged weapon expertise bonus
    const auto& rw2 = game.player().equipment.missile;
    if (rw2) {
        if (rw2->weapon_class == WeaponClass::Pistol
            && player_has_skill(game.player(), SkillId::SteadyHand))
            damage += 1;
        if (rw2->weapon_class == WeaponClass::Rifle
            && player_has_skill(game.player(), SkillId::Marksman))
            damage += 1;
    }
    if (damage < 1) damage = 1;

    // Critical hit check
    bool is_crit = false;
    int crit_chance = std::clamp((game.player().attributes.luck - 8) * 2 + 3, 0, 30);
    if (roll_percent(game.world().rng(), crit_chance)) {
        damage = damage + (damage + 1) / 2;
        is_crit = true;
    }

    damage = apply_damage_effects(target_npc_->effects, damage);
    if (damage <= 0) {
        game.log("Your shot has no effect on " + target_npc_->display_name() + ".");
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
    game.log(hit_msg + target_npc_->display_name() + " for " +
        std::to_string(damage) + " damage. [" +
        std::to_string(rd.current_charge) + "/" +
        std::to_string(rd.charge_capacity) + "]");

    if (!target_npc_->alive()) {
        game.log(target_npc_->display_name() + " is destroyed!");
        game.player().kills++;
        game.quests().on_npc_killed(target_npc_->role);
        int xp = target_npc_->xp_reward();
        if (xp > 0) {
            game.player().xp += xp;
            game.log("You gain " + std::to_string(xp) + " XP.");
            check_level_up(game);
        }
        // Loot drop (50% chance)
        if (std::uniform_int_distribution<int>(0, 1)(game.world().rng()) == 0) {
            Item loot = generate_loot_drop(game.world().rng(), target_npc_->level);
            game.log("Dropped: " + loot.name);
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
        game.log(weapon->name + " is fully charged.");
        return;
    }

    for (int i = 0; i < static_cast<int>(game.player().inventory.items.size()); ++i) {
        if (game.player().inventory.items[i].type == ItemType::Battery) {
            int added = std::min(5, rd.charge_capacity - rd.current_charge);
            rd.current_charge += added;
            game.log("Reloaded " + weapon->name + ". (+" + std::to_string(added) +
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
