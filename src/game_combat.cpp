#include "astra/combat_system.h"
#include "astra/soul_mirror.h"
#include "astra/animation.h"
#include "astra/energy.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/display_name.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/loot_table.h"
#include "astra/noise_event.h"
#include "astra/skill_defs.h"
#include "astra/tilemap.h"
#include "astra/grid_combat.h"
#include "astra/vulnerability.h"

#include <algorithm>
#include <array>
#include <vector>

namespace astra {

namespace {
constexpr int kSwiftnessDv = 5;
constexpr int kSidestepDv  = 2;
}  // namespace

static int sign(int v) { return (v > 0) - (v < 0); }

// Check and fire the Adrenal Pump implant.
// Called after player HP is reduced. If the player has the implant and
// hasn't triggered it this combat, and HP just fell below 30% of max,
// apply a +1 quickness buff for 5 turns and log.
static void check_adrenal_pump(Game& game) {
    auto& p = game.player();
    if (p.hp <= 0) return;
    auto im = p.implant_modifiers();
    if (!im.has_adrenal_pump) return;
    if (p.adrenal_pump_triggered_this_combat) return;
    int threshold = p.effective_max_hp() * 30 / 100;
    if (p.hp >= threshold) return;
    add_effect(p.effects, make_adrenal_pump_ge(5));
    p.adrenal_pump_triggered_this_combat = true;
    game.log(colored("Adrenal Pump fires", Color::Yellow) +
             " \xe2\x80\x94 adrenaline floods your system!");
}

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
}

// Bresenham line-of-sight from (x0,y0) to (x1,y1). Endpoints are excluded
// (attacker and target tiles are creatures, not obstacles). Returns false
// if any intervening tile is opaque (walls, closed doors, blocks_vision
// fixtures). Used by NPC ranged attacks.
static bool los_clear(const TileMap& map, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
        if (x == x1 && y == y1) break;
        if (map.opaque(x, y)) return false;
    }
    return true;
}

static int roll_d20(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(1, 20)(rng);
}

static int roll_d10(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(1, 10)(rng);
}

// Pick a destination tile for a corpse-drop item:
//   1. orthogonal-walkable tiles first (N, E, S, W)
//   2. diagonal-walkable tiles (NE, SE, SW, NW)
//   3. fall back to the death tile (corpse glyph wins on render; the
//      item still picks up via stand-on)
//
// "Walkable" means passable AND no fixture AND no existing ground item.
static std::pair<int,int> find_loot_drop_tile(
    const TileMap& map,
    const std::vector<GroundItem>& ground_items,
    int cx, int cy)
{
    auto free = [&](int x, int y) -> bool {
        if (!map.passable(x, y)) return false;
        if (map.fixture_id(x, y) >= 0) return false;
        for (const auto& gi : ground_items) {
            if (gi.x == x && gi.y == y) return false;
        }
        return true;
    };

    constexpr int orth[4][2] = { {0,-1}, {1,0}, {0,1}, {-1,0} };
    for (auto& d : orth) {
        int x = cx + d[0], y = cy + d[1];
        if (free(x, y)) return {x, y};
    }
    constexpr int diag[4][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
    for (auto& d : diag) {
        int x = cx + d[0], y = cy + d[1];
        if (free(x, y)) return {x, y};
    }
    return {cx, cy};
}

static void apply_salvage_on_kill(Game& game, Npc& npc, std::mt19937& rng) {
    if (is_mechanical(npc)) {
        // Gated: requires Cat_Tinkering. Mechanical kills do NOT roll the
        // universal 5% floor-drop — machines have no flesh to scavenge.
        if (!player_has_skill(game.player(), SkillId::Cat_Tinkering)) return;

        if (std::uniform_int_distribution<int>(0, 99)(rng) >= 40) return;

        int spare_count = 1 + std::uniform_int_distribution<int>(0, 1)(rng);
        Item spare = build_by_def_id(ITEM_SPARE_PARTS);
        spare.stack_count = spare_count;
        add_to_inventory_stacked(game.player().inventory, spare);

        bool got_circuitry = std::uniform_int_distribution<int>(0, 99)(rng) < 30;
        Item circ;
        if (got_circuitry) {
            circ = build_by_def_id(ITEM_CIRCUITRY);
            add_to_inventory_stacked(game.player().inventory, circ);
        }

        std::string msg;
        if (got_circuitry) {
            msg = "You salvage " + display_name(spare) + " and " +
                  display_name(circ) + " from the " + npc.name + ".";
        } else {
            msg = "You salvage " + display_name(spare) +
                  " from the " + npc.name + ".";
        }
        game.log(msg);
        return;
    }

    // Ungated universal path: 5% chance to drop Spare Parts to the ground.
    if (std::uniform_int_distribution<int>(0, 99)(rng) < 5) {
        Item spare = build_by_def_id(ITEM_SPARE_PARTS);
        auto [dx, dy] = find_loot_drop_tile(game.world().map(), game.world().ground_items(), npc.x, npc.y);
        game.world().ground_items().push_back({dx, dy, std::move(spare)});
    }
}

void award_npc_kill(Game& game, Npc& npc) {
    auto& rng = game.world().rng();

    game.player().kills++;

    // Faction reputation penalty
    if (!npc.faction.empty()) {
        for (auto& fs : game.player().reputation) {
            if (fs.faction_name == npc.faction) {
                fs.reputation = std::max(fs.reputation - 30, -600);
                game.log("Your reputation with " + npc.faction + " decreased.");
                break;
            }
        }
    }

    // Quest hook
    game.quests().on_npc_killed(npc.role);

    // XP grant + level-up check
    int xp = npc.xp_reward();
    if (xp > 0) {
        game.player().xp += xp;
        game.log("You gain " + std::to_string(xp) + " XP.");
        game.combat().check_level_up(game);
    }

    // Credits drop
    int credits = npc.level * 2 + (npc.elite ? 5 : 0);
    if (credits > 0) {
        game.player().money += credits;
        game.log("You salvage " + std::to_string(credits) + "$.");
    }

    // Loot drop (50% chance)
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
        if (auto loot = roll_loot(LootSource::NpcDrop, npc.level, rng)) {
            game.log("Dropped: " + display_name(*loot));
            auto [lx, ly] = find_loot_drop_tile(game.world().map(), game.world().ground_items(), npc.x, npc.y);
            game.world().ground_items().push_back({lx, ly, std::move(*loot)});
        }
    }

    // Salvage (mechanical NPC path)
    apply_salvage_on_kill(game, npc, rng);

    // Spec 1: place a corpse fixture carrying the NPC's Hackable so
    // Jack In (dead-implant) can be offered after death. Only for Electronic
    // hackable NPCs (Crystal); silently skip if the tile already holds
    // a fixture (rare collision) or if not in a dungeon/detail map.
    if (npc.cyber && has_tag(npc.cyber->tags, HackTag::Electronic)) {
        auto& map = game.world().map();
        if (map.get(npc.x, npc.y) != Tile::Fixture) {
            FixtureData corpse_fd = make_fixture(FixtureType::NpcCorpse);
            corpse_fd.cyber = *npc.cyber;
            map.add_fixture(npc.x, npc.y, std::move(corpse_fd));
        }
    }
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

enum class AttackKind { Melee, Ranged };

static int player_contextual_dv(const Player& player, const Game& game, AttackKind kind) {
    // Any active dv_zero effect (e.g. GridExposed while jacked) bypasses
    // skill bonuses too — body cannot dodge, period.
    for (const auto& e : player.effects) {
        if (e.dv_zero) return 0;
    }
    int bonus = 0;
    if (kind == AttackKind::Ranged && player_has_skill(player, SkillId::Swiftness)) {
        bonus += kSwiftnessDv;
    }
    if (kind == AttackKind::Melee && player_has_skill(player, SkillId::Sidestep)) {
        int px = player.x, py = player.y;
        for (const auto& npc : game.world().npcs()) {
            if (!npc.alive()) continue;
            if (!is_hostile_to_player(npc.faction, player)) continue;
            int dx = std::abs(npc.x - px), dy = std::abs(npc.y - py);
            if (std::max(dx, dy) == 1) { bonus += kSidestepDv; break; }
        }
    }
    return player.effective_dv() + bonus;
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

static void ranged_hit_player(Npc& npc, Game& game) {
    auto& rng = game.world().rng();
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log("You evade " + display_name(npc) + "'s shot!");
        return;
    }
    int attack_roll = natural + npc.level / 2;
    int player_dv = player_contextual_dv(game.player(), game, AttackKind::Ranged);
    if (natural != 20 && attack_roll < player_dv) {
        game.log("You evade " + display_name(npc) + "'s shot!");
        return;
    }

    const Dice& dmg = npc.ranged_damage_dice;
    DamageType dtype = npc.ranged_damage_type;

    auto* sh_ranged = game.player().shield_energy();
    if (sh_ranged && sh_ranged->current > 0) {
        auto pen = roll_penetration(rng, npc.level / 3, 0, dmg);
        if (pen.total_damage <= 0) {
            game.log(display_name(npc) + "'s shot is absorbed by your shield.");
            return;
        }
        int absorbed = shield_absorb(pen.total_damage, dtype, game.player().shield_affinity);
        sh_ranged->current -= absorbed;
        if (sh_ranged->current < 0) sh_ranged->current = 0;
        game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
        game.log(display_name(npc) + " shoots your shield for " +
                 std::to_string(absorbed) + " " + display_name(dtype) + " damage. [Shield " +
                 std::to_string(sh_ranged->current) + "/" +
                 std::to_string(sh_ranged->capacity) + "]");
        return;
    }

    int eff_av = game.player().effective_av(dtype);
    auto pen = roll_penetration(rng, npc.level / 3, eff_av, dmg);
    if (pen.total_damage <= 0) {
        game.log(display_name(npc) + " shoots at you but deals no damage.");
        return;
    }

    int damage = apply_resistance(pen.total_damage, dtype, game.player().resistances);
    damage = apply_damage_effects(game.player().effects, damage);
    if (damage <= 0) {
        game.log(display_name(npc) + " shoots at you but deals no damage.");
        return;
    }
    game.player().hp -= damage;
    if (game.player().hp < 0) game.player().hp = 0;
    soul_mirror::on_player_damaged(game);
    check_adrenal_pump(game);
    game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
    game.log(display_name(npc) + " shoots you for " +
             std::to_string(damage) + " " + display_name(dtype) + " damage!");
    if (game.player().hp <= 0) {
        game.set_death_message("Shot by " + display_name(npc));
    }
}

static void ranged_hit_npc(Npc& attacker, Npc& defender, Game& game) {
    auto& rng = game.world().rng();
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log(display_name(defender) + " evades " + display_name(attacker) + "'s shot!");
        return;
    }
    int attack_roll = natural + attacker.level / 2;
    if (natural != 20 && attack_roll < defender.dv) {
        game.log(display_name(defender) + " evades " + display_name(attacker) + "'s shot!");
        return;
    }

    const Dice& dmg = attacker.ranged_damage_dice;
    DamageType dtype = attacker.ranged_damage_type;

    int effective_av = defender.av + defender.type_affinity.for_type(dtype);
    auto pen = roll_penetration(rng, attacker.level / 3, effective_av, dmg);
    if (pen.total_damage <= 0) {
        game.log(display_name(attacker) + "'s shot has no effect on " + display_name(defender) + ".");
        return;
    }

    int damage = apply_damage_effects(defender.effects, pen.total_damage);
    if (damage <= 0) {
        game.log(display_name(attacker) + "'s shot has no effect on " + display_name(defender) + ".");
        return;
    }
    defender.hp -= damage;
    if (defender.hp < 0) defender.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, defender.x, defender.y);
    game.log(display_name(attacker) + " shoots " + display_name(defender) +
             " for " + std::to_string(damage) + " " + display_name(dtype) + " damage!");
    if (!defender.alive()) {
        game.log(display_name(defender) + " is destroyed by " + display_name(attacker) + "!");
    }
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

    // Apply vulnerability DoT before the NPC acts, then decay the stack.
    {
        int dot = npc.vuln.dot_per_turn();
        if (dot > 0) {
            dot = apply_damage_effects(npc.effects, dot);
            if (dot > 0) {
                npc.hp -= dot;
                if (npc.hp < 0) npc.hp = 0;
                game.log(display_name(npc) + " takes " + std::to_string(dot) +
                         " vulnerability damage.");
                if (!npc.alive()) {
                    game.log(display_name(npc) + " is destroyed!");
                    award_npc_kill(game, npc);
                    npc.vuln.tick();
                    return; // NPC is dead; skip action
                }
            }
        }
        npc.vuln.tick();
    }

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

    // Noise-event reaction: if any in-range, hostile-emitter noise event
    // exists, latch onto its location as a wander target. Only consulted
    // in the wander fallback below — active combat overrides the chase.
    {
        const auto& events = game.world().noise_events();
        for (const NoiseEvent& ev : events) {
            int dx = std::abs(npc.x - ev.x);
            int dy = std::abs(npc.y - ev.y);
            if (std::max(dx, dy) > ev.radius) continue;
            bool hostile = ev.emitter_is_player
                ? is_hostile_to_player(npc.faction, game.player())
                : is_hostile(npc.faction, ev.emitter_owner_faction);
            if (!hostile) continue;
            npc.move_target_x = ev.x;
            npc.move_target_y = ev.y;
            npc.move_target_ttl = ev.ttl_ticks;
            break; // first matching event wins
        }
    }

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

        // Ranged attack: in range, has ranged weapon, and clear LOS.
        // EMP-disabled NPCs cannot fire ranged/energy weapons.
        if (npc.ai == NpcAi::Turret
            && dist > 1
            && dist <= npc.attack_range
            && !npc.ranged_damage_dice.empty()
            && !has_effect(npc.effects, EffectId::EmpDisabled)
            && los_clear(game.world().map(), npc.x, npc.y,
                         game.player().x, game.player().y)) {
            ranged_hit_player(npc, game);
            return;
        }

        if (dist <= 1) {
            auto& rng = game.world().rng();

            // Attack roll: 1d20 + npc.level/2 vs player.effective_dv()
            int natural = roll_d20(rng);
            if (natural == 1) {
                game.log("You dodge " + display_name(npc) + "'s attack!");
                return;
            }
            int attack_roll = natural + npc.level / 2;
            int player_dv = player_contextual_dv(game.player(), game, AttackKind::Melee);
            if (natural != 20 && attack_roll < player_dv) {
                game.log("You dodge " + display_name(npc) + "'s attack!");
                return;
            }

            Dice dmg_dice = npc.damage_dice;
            if (dmg_dice.empty()) dmg_dice = Dice::make(1, 3);
            DamageType dtype = npc.damage_type;

            // Shield check
            auto* sh_melee = game.player().shield_energy();
            if (sh_melee && sh_melee->current > 0) {
                // Penetrate shield as AV=0
                auto pen = roll_penetration(rng, npc.level / 3, 0, dmg_dice);
                if (pen.total_damage <= 0) {
                    game.log(display_name(npc) + "'s attack is absorbed by your shield.");
                    return;
                }
                int absorbed = shield_absorb(pen.total_damage, dtype, game.player().shield_affinity);
                sh_melee->current -= absorbed;
                if (sh_melee->current < 0) sh_melee->current = 0;
                game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
                game.log(display_name(npc) + " hits your shield for " +
                         std::to_string(absorbed) + " " + display_name(dtype) + " damage. [Shield " +
                         std::to_string(sh_melee->current) + "/" +
                         std::to_string(sh_melee->capacity) + "]");
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
            soul_mirror::on_player_damaged(game);
            check_adrenal_pump(game);
            game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
            game.log(display_name(npc) + " strikes you for " +
                     std::to_string(damage) + " " + display_name(dtype) + " damage!");
            if (game.player().hp <= 0) {
                game.set_death_message("Slain by " + display_name(npc));
            }
            return;
        }

        // Turrets don't chase — they hold position when they can't shoot.
        if (npc.ai == NpcAi::Turret) return;

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

        // Ranged attack: in range, has ranged weapon, and clear LOS.
        // EMP-disabled NPCs cannot fire ranged/energy weapons.
        if (npc.ai == NpcAi::Turret
            && dist > 1
            && dist <= npc.attack_range
            && !npc.ranged_damage_dice.empty()
            && !has_effect(npc.effects, EffectId::EmpDisabled)
            && los_clear(game.world().map(), npc.x, npc.y,
                         target.npc->x, target.npc->y)) {
            ranged_hit_npc(npc, *target.npc, game);
            return;
        }

        if (dist <= 1) {
            attack_npc_vs_npc(npc, *target.npc, game);
            return;
        }

        // Turrets don't chase — they hold position when they can't shoot.
        if (npc.ai == NpcAi::Turret) return;

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

    // Turrets never wander — no hostile in range means idle at post.
    if (npc.ai == NpcAi::Turret) return;

    // Noise-event chase: if a recent decoy or other noise pinged us,
    // step toward the location instead of random wandering. Decrements
    // ttl every NPC turn; clears on arrival.
    if (npc.move_target_ttl > 0 &&
        npc.move_target_x >= 0 && npc.move_target_y >= 0) {
        --npc.move_target_ttl;
        if (npc.x == npc.move_target_x && npc.y == npc.move_target_y) {
            npc.move_target_ttl = 0;
        } else {
            int dx = sign(npc.move_target_x - npc.x);
            int dy = sign(npc.move_target_y - npc.y);
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
            // Blocked this turn; fall through to random wander.
        }
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

void CombatSystem::attack_npc(Npc& npc, Game& game, bool in_extra_hit) {
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
        int str_mod = (game.player().effective_strength() - 10) / 2;
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

    // 5a. melee_kinetic_bonus — flat kinetic bonus on every melee hit (kinetic weapons / unarmed)
    const auto& imods = game.player().implant_modifiers();
    if (imods.melee_kinetic_bonus > 0 && dtype == DamageType::Kinetic) {
        damage += imods.melee_kinetic_bonus;
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

    // 5b. melee_bleed_proc_pct — kinetic DoT proc on hit
    if (imods.melee_bleed_proc_pct > 0 && npc.alive()) {
        if (std::uniform_int_distribution<int>(0, 99)(rng) < imods.melee_bleed_proc_pct) {
            add_effect(npc.effects, make_bleed_ge(3, 1));
            game.log(colored(display_name(npc), Color::White) + colored(" bleeds!", Color::Red));
        }
    }

    // 5c. melee_emp_proc_pct — EmpDisabled proc on hit
    if (imods.melee_emp_proc_pct > 0 && npc.alive()) {
        if (std::uniform_int_distribution<int>(0, 99)(rng) < imods.melee_emp_proc_pct) {
            add_effect(npc.effects, make_emp_disabled_ge(1));
            game.log(colored(display_name(npc), Color::White) + colored(" is jolted offline!", Color::Cyan));
        }
    }

    if (!npc.alive()) {
        game.log(display_name(npc) + " is destroyed!");
        award_npc_kill(game, npc);
        return;
    }

    // 5d. melee_extra_hit_proc_pct — free 2nd melee strike (no chain recursion)
    if (!in_extra_hit && imods.melee_extra_hit_proc_pct > 0) {
        int dx = std::abs(npc.x - game.player().x);
        int dy = std::abs(npc.y - game.player().y);
        if (std::max(dx, dy) <= 1 &&
            std::uniform_int_distribution<int>(0, 99)(rng) < imods.melee_extra_hit_proc_pct) {
            game.log(colored("Coilgun discharges — second strike!", Color::Yellow));
            attack_npc(npc, game, /*in_extra_hit=*/true);
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
    // EMP-disabled players cannot fire energy/ranged weapons.
    if (has_effect(game.player().effects, EffectId::EmpDisabled)) {
        game.log("Your weapon is EMP-disabled.");
        return;
    }

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

    // Energy check — auto-recharge if below per-shot cost
    if (!weapon->energy || !weapon->consumer) {
        game.log("Weapon has no energy system.");
        return;
    }
    auto& estore = *weapon->energy;
    int per_shot = weapon->consumer->energy_per_use;
    if (estore.current < per_shot) {
        bool recharged = recharge_weapon(game, /*log_full=*/false, /*advance=*/false);
        if (!recharged || estore.current < per_shot) {
            game.log("Weapon empty. No charged cells available.");
            return;
        }
    }

    // Spend energy
    estore.current -= per_shot;

    auto& rng = game.world().rng();

    // Determine weapon dice
    Dice dmg_dice = weapon->damage_dice;
    if (dmg_dice.empty()) dmg_dice = Dice::make(1, 3);
    DamageType dtype = weapon->damage_type;
    WeaponClass wc = weapon->weapon_class;

    // From here on the attack is committed — mark the action regardless of hit/miss.
    game.player().last_action_was_attack = true;

    // Attack roll: 1d20 + (AGI-10)/2 + weapon_skill_bonus vs npc.dv
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log(display_name(*target_npc_) + " dodges your shot!");
        game.advance_world(ActionCost::shoot);
        return;
    }
    // Targeting Lattice implant: adds a flat AGI bonus specifically for pistol hit-rolls.
    bool weapon_is_pistol = (wc == WeaponClass::Pistol);
    const auto& imods = game.player().implant_modifiers();
    int eff_agi = game.player().attributes.agility;
    if (weapon_is_pistol) {
        eff_agi += imods.pistol_agility_bonus;
    }
    int agi_mod = (eff_agi - 10) / 2;
    // Pistol Targeter implant: pistol_hit_bonus_pct is an additive hit-% bonus.
    // In the d20-vs-DV system each 5% ≈ +1 to the roll; we convert here.
    int implant_hit_bonus = weapon_is_pistol ? imods.pistol_hit_bonus_pct / 5 : 0;
    int attack_roll = natural + agi_mod + weapon_skill_bonus(game.player(), wc) + implant_hit_bonus;
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
        int str_mod = (game.player().effective_strength() - 10) / 2;
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
        std::to_string(estore.current) + "/" +
        std::to_string(estore.capacity) + "]");

    // Task 6: ranged_rocket_proc_pct — Wrist Rocket splash on ranged hit
    {
        const auto& rimods = game.player().implant_modifiers();
        if (rimods.ranged_rocket_proc_pct > 0 &&
            std::uniform_int_distribution<int>(0, 99)(rng) < rimods.ranged_rocket_proc_pct) {
            game.log(colored("Wrist rocket fires!", Color::Yellow));
            int tx = target_npc_->x;
            int ty = target_npc_->y;
            // Apply 1d4 heat splash to target + 4 cardinal neighbors
            constexpr int splash_dirs[5][2] = { {0,0}, {0,-1}, {1,0}, {0,1}, {-1,0} };
            for (auto& d : splash_dirs) {
                int sx = tx + d[0];
                int sy = ty + d[1];
                for (auto& sn : game.world().npcs()) {
                    if (!sn.alive() || sn.x != sx || sn.y != sy) continue;
                    int rocket_dmg = std::uniform_int_distribution<int>(1, 4)(rng);
                    rocket_dmg = apply_damage_effects(sn.effects, rocket_dmg);
                    if (rocket_dmg > 0) {
                        sn.hp -= rocket_dmg;
                        if (sn.hp < 0) sn.hp = 0;
                        game.animations().spawn_effect(anim_damage_flash, sn.x, sn.y);
                        game.log("  " + display_name(sn) + " takes " +
                                 std::to_string(rocket_dmg) + " " +
                                 display_name(DamageType::Plasma) + " splash.");
                        if (!sn.alive()) {
                            game.log(display_name(sn) + " is destroyed!");
                            award_npc_kill(game, sn);
                        }
                    }
                }
            }
        }
    }

    if (!target_npc_->alive()) {
        game.log(display_name(*target_npc_) + " is destroyed!");
        award_npc_kill(game, *target_npc_);
        target_npc_ = nullptr;
    }

    game.advance_world(ActionCost::shoot);
}

// Fire any proc owned by `cell` after `drained` units of energy have been
// transferred out of it. Procs are gated by target kind for the overcharge
// kinds; player-buff kinds always fire when the threshold is reached.
// Returns nothing — side effects only.
void apply_cell_proc(Item& cell, int drained,
                     CombatSystem::RechargeTargetKind kind,
                     EnergyStore* target, Game& game) {
    if (!cell.proc || drained <= 0) return;
    auto& p = *cell.proc;
    if (p.kind == CellProcKind::None || p.threshold <= 0) return;

    // Gate by target type for overcharge kinds.
    bool applicable = true;
    if (p.kind == CellProcKind::ShieldOvercharge)
        applicable = (kind == CombatSystem::RechargeTargetKind::EquippedShield);
    else if (p.kind == CellProcKind::WeaponOvercharge)
        applicable = (kind == CombatSystem::RechargeTargetKind::EquippedWeapon);
    if (!applicable) return;

    p.accumulator += drained;
    while (p.accumulator >= p.threshold) {
        p.accumulator -= p.threshold;
        switch (p.kind) {
            case CellProcKind::ShieldOvercharge:
                if (target) {
                    target->current += p.magnitude;
                    game.log("Shield overcharged by +" + std::to_string(p.magnitude) +
                             "! [" + std::to_string(target->current) + "/" +
                             std::to_string(target->capacity) + "]");
                }
                break;
            case CellProcKind::WeaponOvercharge:
                if (target) {
                    target->current += p.magnitude;
                    game.log("Weapon overcharged by +" + std::to_string(p.magnitude) +
                             "! [" + std::to_string(target->current) + "/" +
                             std::to_string(target->capacity) + "]");
                }
                break;
            case CellProcKind::DefenseBoost:
                add_effect(game.player().effects,
                           make_defense_boost_ge(p.duration, p.magnitude));
                game.log("Defense surge: +" + std::to_string(p.magnitude) +
                         " DV for " + std::to_string(p.duration) + " turns.");
                break;
            case CellProcKind::AdrenalineRush:
                add_effect(game.player().effects, make_adrenaline_rush_ge(p.duration));
                game.log("Adrenaline rush! (" + std::to_string(p.duration) + " turns)");
                break;
            case CellProcKind::None:
                break;
        }
    }
}

int CombatSystem::recharge_target_(Game& game, EnergyStore& target,
                                   RechargeTargetKind kind) {
    auto& items = game.player().inventory.items;
    std::vector<int> idxs;
    for (int i = 0; i < (int)items.size(); ++i) {
        const auto& it = items[i];
        if (it.type == ItemType::Battery && it.energy && it.energy->current > 0)
            idxs.push_back(i);
    }
    std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
        return items[a].energy->current > items[b].energy->current;
    });

    int total = 0;
    for (int i : idxs) {
        if (is_full(target)) break;
        int eff = 0;
        for (const auto& enh : items[i].enhancements)
            if (enh.committed) eff += enh.energy_bonus.discharge_efficiency;
        int moved = transfer_energy(*items[i].energy, target, deficit(target), eff);
        if (moved > 0) apply_cell_proc(items[i], moved, kind, &target, game);
        total += moved;
    }
    return total;
}

bool CombatSystem::recharge_weapon(Game& game, bool log_full, bool advance) {
    auto& weapon = game.player().equipment.missile;
    if (!weapon || !weapon->energy) {
        if (log_full) game.log("No ranged weapon equipped.");
        return false;
    }
    auto& estore = *weapon->energy;
    if (is_full(estore)) {
        if (log_full) game.log(weapon->label() + " is fully charged.");
        return false;
    }
    int moved = recharge_target_(game, estore, RechargeTargetKind::EquippedWeapon);
    if (moved == 0) {
        if (log_full) game.log("No charged cells to recharge from.");
        return false;
    }
    game.log("Recharged " + display_name(*weapon) + ". (+" + std::to_string(moved) +
             " charge, " + std::to_string(estore.current) + "/" +
             std::to_string(estore.capacity) + ")");
    if (advance) game.advance_world(ActionCost::wait);
    return true;
}

bool CombatSystem::recharge_shield(Game& game, bool log_full, bool advance) {
    auto* sh = game.player().shield_energy();
    if (!sh) {
        if (log_full) game.log("No energy shield equipped.");
        return false;
    }
    if (is_full(*sh)) {
        if (log_full) game.log("Shield is at full charge.");
        return false;
    }
    int moved = recharge_target_(game, *sh, RechargeTargetKind::EquippedShield);
    if (moved == 0) {
        if (log_full) game.log("No charged cells to recharge shield.");
        return false;
    }
    const auto& shield_item = *game.player().equipment.shield;
    game.log("Recharged " + display_name(shield_item) + ". (+" + std::to_string(moved) +
             " charge, " + std::to_string(sh->current) + "/" + std::to_string(sh->capacity) + ")");
    if (advance) game.advance_world(ActionCost::wait);
    return true;
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


void grant_grid_xp(Game& game, int amount) {
    if (amount <= 0) return;
    game.player().xp += amount;
    game.log("+" + std::to_string(amount) + " XP");
    game.combat().check_level_up(game);
}

void CombatSystem::reset() {
    targeting_ = false;
    target_x_ = 0;
    target_y_ = 0;
    blink_phase_ = 0;
    target_npc_ = nullptr;
}

} // namespace astra
