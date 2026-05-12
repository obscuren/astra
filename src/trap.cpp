#include "astra/trap.h"

#include "astra/action.h"
#include "astra/display_name.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/item.h"
#include "astra/item_ids.h"
#include "astra/noise_event.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace astra {

const char* trap_kind_name(TrapKind k) {
    switch (k) {
        case TrapKind::ProximityMine:  return "proximity mine";
        case TrapKind::EmpMine:        return "EMP mine";
        case TrapKind::IncendiaryMine: return "incendiary mine";
        case TrapKind::DecoyMine:      return "decoy mine";
        case TrapKind::Caltrops:       return "caltrops";
        case TrapKind::DungeonGeneric: return "trap";
    }
    return "trap";
}

char trap_glyph(TrapKind k) {
    switch (k) {
        case TrapKind::Caltrops: return '*';
        default:                 return '^';
    }
}

int trap_color(TrapKind k) {
    // Returns a Color enum index (xterm-256 palette). Color enum values
    // live in include/astra/renderer.h.
    switch (k) {
        case TrapKind::ProximityMine:  return static_cast<int>(Color::Cyan);
        case TrapKind::EmpMine:        return static_cast<int>(Color::Blue);
        case TrapKind::IncendiaryMine: return static_cast<int>(Color::BrightYellow);
        case TrapKind::DecoyMine:      return static_cast<int>(Color::Yellow);
        case TrapKind::Caltrops:       return static_cast<int>(Color::White);
        case TrapKind::DungeonGeneric: return static_cast<int>(Color::Red);
    }
    return static_cast<int>(Color::White);
}

TrapKind trap_kind_for_item_id(uint16_t id) {
    switch (id) {
        case ITEM_PROXIMITY_MINE:  return TrapKind::ProximityMine;
        case ITEM_EMP_MINE:        return TrapKind::EmpMine;
        case ITEM_INCENDIARY_MINE: return TrapKind::IncendiaryMine;
        case ITEM_DECOY_MINE:      return TrapKind::DecoyMine;
        case ITEM_CALTROPS:        return TrapKind::Caltrops;
    }
    return TrapKind::ProximityMine;
}

namespace {

// Per-kind defaults. Indexed by TrapKind enum value. Status of 0 means
// "no status" (EffectId::Invulnerable is the chosen sentinel).
constexpr TrapDef kTrapDefs[] = {
    /* ProximityMine  */ { 12, 1, static_cast<int>(EffectId::Invulnerable), 0, 0 },
    /* EmpMine        */ { 4,  1, static_cast<int>(EffectId::EmpDisabled),  5, 0 },
    /* IncendiaryMine */ { 8,  1, static_cast<int>(EffectId::Burn),         4, 2 },
    /* DecoyMine      */ { 0,  0, static_cast<int>(EffectId::Invulnerable), 0, 0 },
    /* Caltrops       */ { 3,  0, static_cast<int>(EffectId::Slow),         3, 0 },
    /* DungeonGeneric */ { 6,  0, static_cast<int>(EffectId::Invulnerable), 0, 0 },
};

const TrapDef& def_for(TrapKind k) {
    return kTrapDefs[static_cast<int>(k)];
}

int chebyshev(int ax, int ay, int bx, int by) {
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

const char* relative_dir(int from_x, int from_y, int to_x, int to_y) {
    int dx = to_x - from_x, dy = to_y - from_y;
    if (dx == 0 && dy < 0)  return "to the north";
    if (dx == 0 && dy > 0)  return "to the south";
    if (dx > 0 && dy == 0)  return "to the east";
    if (dx < 0 && dy == 0)  return "to the west";
    if (dx > 0 && dy < 0)   return "to the northeast";
    if (dx < 0 && dy < 0)   return "to the northwest";
    if (dx > 0 && dy > 0)   return "to the southeast";
    if (dx < 0 && dy > 0)   return "to the southwest";
    return "nearby";
}

constexpr int kCaltropsScatterCount = 4;

bool tile_passable(const Game& game, int x, int y) {
    const auto& map = game.world().map();
    if (x < 0 || y < 0 || x >= map.width() || y >= map.height()) return false;
    return map.passable(x, y);
}

void scatter_caltrops(Game& game, int cx, int cy) {
    auto& traps = game.world().traps();
    auto& rng = game.world().rng();

    struct P { int x; int y; };
    std::vector<P> candidates;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            if (x == game.player().x && y == game.player().y) continue;
            if (!tile_passable(game, x, y)) continue;
            candidates.push_back({x, y});
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);
    int n = std::min<int>(kCaltropsScatterCount, static_cast<int>(candidates.size()));
    for (int i = 0; i < n; ++i) {
        Trap t;
        t.kind = TrapKind::Caltrops;
        t.x = candidates[i].x;
        t.y = candidates[i].y;
        t.hidden = false;
        t.reveal_radius = 0;
        t.detection_dc = 0;
        t.activations_remaining = 3;
        t.placer_is_player = true;
        t.owner_faction = "";
        t.placer_npc_id = -1;
        t.placed_tick = game.world().world_tick();
        traps.push_back(std::move(t));
    }
}

void place_single_trap(Game& game, TrapKind kind, int x, int y) {
    Trap t;
    t.kind = kind;
    t.x = x;
    t.y = y;
    switch (kind) {
        case TrapKind::ProximityMine:  t.detection_dc = 12; t.hidden = true;  break;
        case TrapKind::EmpMine:        t.detection_dc = 13; t.hidden = true;  break;
        case TrapKind::IncendiaryMine: t.detection_dc = 11; t.hidden = true;  break;
        case TrapKind::DecoyMine:      t.detection_dc = 0;  t.hidden = false; break;
        default:                                                              break;
    }
    t.placer_is_player = true;
    t.owner_faction = "";
    t.placer_npc_id = -1;
    t.activations_remaining = (kind == TrapKind::Caltrops) ? 3 : 1;
    t.placed_tick = game.world().world_tick();
    game.world().traps().push_back(std::move(t));
}

// NPC lookup by id == index in world().npcs(). Returns nullptr if out of
// range or NPC is dead. Use carefully — pointer is invalidated by any
// mutation to the npcs vector during the same frame.
Npc* npc_by_id(Game& game, int id) {
    if (id < 0) return nullptr;
    auto& list = game.world().npcs();
    if (id >= static_cast<int>(list.size())) return nullptr;
    return &list[id];
}

bool should_trigger(const Trap& t, Game& game,
                    bool stepper_is_player, int stepper_npc_id) {
    if (t.trigger_mode == TrapTrigger::AnyEntity)  return true;
    if (t.trigger_mode == TrapTrigger::PlayerOnly) return stepper_is_player;

    // NonFriendlyToOwner — live faction eval.
    if (t.placer_is_player) {
        if (stepper_is_player) return false;
        Npc* npc = npc_by_id(game, stepper_npc_id);
        if (!npc) return false;
        return is_hostile_to_player(npc->faction, game.player());
    }
    // NPC-placed.
    if (stepper_is_player) {
        return is_hostile_to_player(t.owner_faction, game.player());
    }
    if (stepper_npc_id == t.placer_npc_id) return false;
    Npc* stepper = npc_by_id(game, stepper_npc_id);
    if (!stepper) return false;
    return is_hostile(stepper->faction, t.owner_faction);
}

// Returns the damage actually applied (0 if the entity was placer-immune).
int apply_damage_and_status(Game& game, Player* player, Npc* npc,
                            const Trap& t, const TrapDef& def,
                            int stepper_npc_id) {
    // Splash immunity for the placer.
    if (player && t.placer_is_player) return 0;
    if (npc && !t.placer_is_player && stepper_npc_id == t.placer_npc_id) return 0;

    EffectId status_id = static_cast<EffectId>(def.status);
    bool is_emp_event = (status_id == EffectId::EmpDisabled);

    // EMP Buffer — absorbs first EMP/electric event per level for the player.
    if (player && is_emp_event) {
        auto im = player->implant_modifiers();
        if (im.has_emp_buffer && !player->emp_buffer_used_this_level) {
            player->emp_buffer_used_this_level = true;
            game.log(colored("EMP Buffer", Color::Cyan) + " absorbs the surge!");
            return 0;
        }
    }

    int dmg = def.damage;
    if (player) {
        player->hp = std::max(0, player->hp - dmg);
    } else if (npc) {
        npc->hp = std::max(0, npc->hp - dmg);
    }

    if (status_id != EffectId::Invulnerable) {
        Effect e;
        if (status_id == EffectId::Burn) {
            e = make_burn_ge(def.status_duration, def.status_tick_damage);
        } else if (status_id == EffectId::EmpDisabled) {
            e = make_emp_disabled_ge(def.status_duration);
        } else if (status_id == EffectId::Slow) {
            // No factory for Slow; build a minimal effect.
            e.id = EffectId::Slow;
            e.name = "Slowed";
            e.color = Color::Cyan;
            e.duration = def.status_duration;
            e.remaining = def.status_duration;
            e.show_in_bar = true;
            e.move_speed_mod = -25;
        } else {
            e = effect_for_id(status_id);
            if (def.status_duration > 0) {
                e.duration = def.status_duration;
                e.remaining = def.status_duration;
            }
        }
        if (player) add_effect(player->effects, e);
        else if (npc) add_effect(npc->effects, e);
    }
    return dmg;
}

// Status descriptor for log lines (e.g. "Burn 4t", "Slow 3t", "EMP 5t").
const char* short_status(int status_int) {
    switch (static_cast<EffectId>(status_int)) {
        case EffectId::Burn:        return "Burn";
        case EffectId::EmpDisabled: return "EMP";
        case EffectId::Slow:        return "Slow";
        default: return nullptr;
    }
}

void resolve_trap(Game& game, const Trap& t, int x, int y,
                  bool stepper_is_player, int stepper_npc_id) {
    const TrapDef& def = def_for(t.kind);

    // Decoy mine — log + emit a noise event for hostile NPCs to chase.
    if (t.kind == TrapKind::DecoyMine) {
        game.log("The " + display_name(t.kind) + " beeps loudly!");
        NoiseEvent ev;
        ev.x = t.x;
        ev.y = t.y;
        ev.radius = 5;
        ev.ttl_ticks = 5;
        ev.emitter_is_player = t.placer_is_player;
        ev.emitter_owner_faction = t.owner_faction;
        emit_noise_event(game, std::move(ev));
        return;
    }

    // Headline log — who triggered what. The trap kind is colored.
    if (stepper_is_player) {
        game.log("You set off the " + display_name(t.kind) + "!");
    } else if (Npc* n = npc_by_id(game, stepper_npc_id)) {
        game.log(display_name(*n) + " sets off the " + display_name(t.kind) + "!");
    } else {
        game.log("Something sets off the " + display_name(t.kind) + "!");
    }

    // Damage the stepping entity.
    int dmg_to_stepper = 0;
    if (stepper_is_player) {
        dmg_to_stepper = apply_damage_and_status(game, &game.player(), nullptr, t, def, -1);
    } else {
        Npc* n = npc_by_id(game, stepper_npc_id);
        if (n) dmg_to_stepper = apply_damage_and_status(game, nullptr, n, t, def, stepper_npc_id);
    }

    if (dmg_to_stepper > 0) {
        std::string status_part;
        if (const char* s = short_status(def.status)) {
            status_part = std::string(" + ") + s
                        + " (" + std::to_string(def.status_duration) + "t)";
        }
        if (stepper_is_player) {
            game.log("  You take " + std::to_string(dmg_to_stepper) + " damage" + status_part + ".");
        } else if (Npc* n = npc_by_id(game, stepper_npc_id)) {
            game.log("  " + display_name(*n) + " takes "
                     + std::to_string(dmg_to_stepper) + " damage" + status_part + ".");
        }
    }

    // Splash to other entities in burst radius (skip stepper).
    if (def.burst_radius > 0) {
        // Player splash (if stepper isn't the player).
        if (!stepper_is_player &&
            chebyshev(game.player().x, game.player().y, x, y) <= def.burst_radius) {
            int sd = apply_damage_and_status(game, &game.player(), nullptr, t, def, -1);
            if (sd > 0) {
                std::string status_part;
                if (const char* s = short_status(def.status)) {
                    status_part = std::string(" + ") + s
                                + " (" + std::to_string(def.status_duration) + "t)";
                }
                game.log("  You take " + std::to_string(sd)
                         + " splash damage" + status_part + ".");
            }
        }
        // NPC splash.
        auto& npcs = game.world().npcs();
        for (int i = 0; i < static_cast<int>(npcs.size()); ++i) {
            if (i == stepper_npc_id) continue;
            Npc& n = npcs[i];
            if (!n.alive()) continue;
            if (chebyshev(n.x, n.y, x, y) <= def.burst_radius) {
                int sd = apply_damage_and_status(game, nullptr, &n, t, def, i);
                if (sd > 0) {
                    std::string status_part;
                    if (const char* s = short_status(def.status)) {
                        status_part = std::string(" + ") + s
                                    + " (" + std::to_string(def.status_duration) + "t)";
                    }
                    game.log("  " + display_name(n) + " takes "
                             + std::to_string(sd) + " splash damage"
                             + status_part + ".");
                }
            }
        }
    }
}

} // namespace

void place_player_trap(Game& game, TrapKind kind, int dest_x, int dest_y) {
    if (kind == TrapKind::Caltrops) {
        scatter_caltrops(game, dest_x, dest_y);
    } else {
        place_single_trap(game, kind, dest_x, dest_y);
    }
    game.log("You deploy the " + display_name(kind) + ".");
    game.advance_world(ActionCost::wait);
}

void place_dungeon_trap(WorldManager& wm, int x, int y, TrapKind kind,
                        TrapTrigger trigger, bool hidden, int detection_dc) {
    Trap t;
    t.kind = kind;
    t.x = x;
    t.y = y;
    t.hidden = hidden;
    t.reveal_radius = 2;
    t.detection_dc = detection_dc;
    t.trigger_mode = trigger;
    t.placer_is_player = false;
    t.placer_npc_id = -1;
    t.owner_faction = "";
    t.activations_remaining = 1;
    t.placed_tick = 0;
    wm.traps().push_back(std::move(t));
}

// --- Public accessors for the kTrapDefs table + throw geometry ---

const TrapDef& trap_def_for(TrapKind k) {
    return def_for(k);
}

int trap_throw_range(TrapKind k) {
    return (k == TrapKind::Caltrops) ? 4 : 3;
}

int trap_throw_burst_width(TrapKind k) {
    return (k == TrapKind::Caltrops) ? 1 : 0;
}

void on_entity_enters_tile(Game& game, int x, int y, bool is_player, int npc_id) {
    auto& traps = game.world().traps();
    for (auto it = traps.begin(); it != traps.end(); /* manual */) {
        if (it->x != x || it->y != y) { ++it; continue; }
        if (!should_trigger(*it, game, is_player, npc_id)) { ++it; continue; }

        // Snapshot the trap before potential erase — resolve_trap may live
        // through the iterator's tile, but resolve_trap doesn't mutate
        // the traps vector itself.
        Trap snapshot = *it;
        resolve_trap(game, snapshot, x, y, is_player, npc_id);

        if (--it->activations_remaining <= 0) {
            it = traps.erase(it);
        } else {
            ++it;
        }
    }
}

void update_trap_detection(Game& game) {
    auto& traps = game.world().traps();
    auto& rng = game.world().rng();
    const Player& p = game.player();

    for (Trap& t : traps) {
        if (!t.hidden || t.placer_is_player) continue;

        bool now_in = chebyshev(p.x, p.y, t.x, t.y) <= t.reveal_radius;
        if (!t.was_in_player_radius && now_in) {
            std::uniform_int_distribution<int> d20(1, 20);
            int roll = d20(rng);
            if (roll + p.trap_detection >= t.detection_dc) {
                t.hidden = false;
                game.log("You spot a " + display_name(t.kind) + " " +
                         relative_dir(p.x, p.y, t.x, t.y) + "!");
            }
        }
        t.was_in_player_radius = now_in;
    }
}

} // namespace astra
