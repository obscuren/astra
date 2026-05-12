#include "astra/grenade.h"

#include "astra/display_name.h"
#include "astra/ground_effect.h"
#include "astra/effect.h"
#include "astra/game.h"
#include "astra/item.h"
#include "astra/item_ids.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace astra {

namespace {

// Per-kind defaults indexed by GrenadeKind. status of 0 means none.
constexpr GrenadeDef kGrenadeDefs[] = {
    /* Frag       */ { 12, 1, static_cast<int>(EffectId::Invulnerable), 0, 0 },
    /* Emp        */ { 4,  1, static_cast<int>(EffectId::EmpDisabled),  5, 0 },
    /* Cryo       */ { 6,  1, static_cast<int>(EffectId::Slow),         5, 0 },
    /* Incendiary */ { 8,  1, static_cast<int>(EffectId::Burn),         4, 2 },
    /* Smoke      */ { 0,  2, 0,                                        0, 0 },
    /* Flashbang  */ { 0,  1, static_cast<int>(EffectId::EmpDisabled),  4, 0 },
};

int chebyshev(int ax, int ay, int bx, int by) {
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

const char* short_status(int status_int) {
    switch (static_cast<EffectId>(status_int)) {
        case EffectId::Burn:        return "Burn";
        case EffectId::EmpDisabled: return "EMP";
        case EffectId::Slow:        return "Slow";
        default: return nullptr;
    }
}

void apply_to(Game& game, Player* p, Npc* n, const GrenadeDef& def) {
    EffectId sid = static_cast<EffectId>(def.status);
    bool is_emp_event = (sid == EffectId::EmpDisabled);

    // EMP Buffer — absorbs first EMP/electric event per level for the player.
    if (p && is_emp_event) {
        auto im = p->implant_modifiers();
        if (im.has_emp_buffer && !p->emp_buffer_used_this_level) {
            p->emp_buffer_used_this_level = true;
            game.log(colored("EMP Buffer", Color::Cyan) + " absorbs the surge!");
            return;
        }
    }

    int dmg = def.damage;
    if (p) p->hp = std::max(0, p->hp - dmg);
    else if (n) n->hp = std::max(0, n->hp - dmg);

    if (sid != EffectId::Invulnerable) {
        Effect e;
        if (sid == EffectId::Burn) {
            e = make_burn_ge(def.status_duration, def.status_tick_damage);
        } else if (sid == EffectId::EmpDisabled) {
            e = make_emp_disabled_ge(def.status_duration);
        } else if (sid == EffectId::Slow) {
            e.id = EffectId::Slow;
            e.name = "Slowed";
            e.color = Color::Cyan;
            e.duration = def.status_duration;
            e.remaining = def.status_duration;
            e.show_in_bar = true;
            e.move_speed_mod = -25;
        } else {
            e = effect_for_id(sid);
            if (def.status_duration > 0) {
                e.duration = def.status_duration;
                e.remaining = def.status_duration;
            }
        }
        if (p) add_effect(p->effects, e);
        else if (n) add_effect(n->effects, e);
    }
}

} // namespace

const GrenadeDef& grenade_def_for(GrenadeKind k) {
    return kGrenadeDefs[static_cast<int>(k)];
}

int grenade_throw_range(GrenadeKind k) {
    // Smoke arcs farther; everything else flies the same distance.
    return (k == GrenadeKind::Smoke) ? 6 : 5;
}

int grenade_throw_burst_width(GrenadeKind k) {
    // Telegraph reticule width matches the actual blast radius so the
    // player previews exactly what gets hit.
    return grenade_def_for(k).burst_radius;
}

GrenadeKind grenade_kind_for_item_id(uint16_t id) {
    switch (id) {
        case ITEM_FRAG_GRENADE:        return GrenadeKind::Frag;
        case ITEM_EMP_GRENADE:         return GrenadeKind::Emp;
        case ITEM_CRYO_GRENADE:        return GrenadeKind::Cryo;
        case ITEM_INCENDIARY_GRENADE:  return GrenadeKind::Incendiary;
        case ITEM_SMOKE_GRENADE:       return GrenadeKind::Smoke;
        case ITEM_FLASHBANG:           return GrenadeKind::Flashbang;
    }
    return GrenadeKind::Frag;
}

const char* grenade_kind_name(GrenadeKind k) {
    switch (k) {
        case GrenadeKind::Frag:       return "frag grenade";
        case GrenadeKind::Emp:        return "EMP grenade";
        case GrenadeKind::Cryo:       return "cryo grenade";
        case GrenadeKind::Incendiary: return "incendiary grenade";
        case GrenadeKind::Smoke:      return "smoke grenade";
        case GrenadeKind::Flashbang:  return "flashbang";
    }
    return "grenade";
}

char grenade_glyph(GrenadeKind) { return '*'; }

int grenade_color(GrenadeKind k) {
    switch (k) {
        case GrenadeKind::Frag:       return static_cast<int>(Color::White);
        case GrenadeKind::Emp:        return static_cast<int>(Color::Blue);
        case GrenadeKind::Cryo:       return static_cast<int>(Color::Cyan);
        case GrenadeKind::Incendiary: return static_cast<int>(Color::BrightYellow);
        case GrenadeKind::Smoke:      return static_cast<int>(Color::DarkGray);
        case GrenadeKind::Flashbang:  return static_cast<int>(Color::BrightWhite);
    }
    return static_cast<int>(Color::White);
}

void detonate_grenade(Game& game, GrenadeKind kind, int x, int y,
                      bool placer_is_player) {
    if (kind == GrenadeKind::Smoke) {
        game.log("The smoke grenade pops — a thick cloud billows out.");
        stamp_ground_effect(game, GroundEffectKind::Smoke, x, y);
        // Refresh FOV now so the player immediately loses sight through
        // the freshly-laid cloud — otherwise visibility stays stale until
        // the next tick recompute fires.
        game.recompute_fov();
        return;
    }
    const GrenadeDef& def = grenade_def_for(kind);

    // Headline log.
    game.log("The " + display_name(kind) + " detonates!");

    auto status_part = [&]() -> std::string {
        if (const char* s = short_status(def.status)) {
            return std::string(" + ") + s + " (" + std::to_string(def.status_duration) + "t)";
        }
        return {};
    };

    // Player splash — placer is immune.
    if (!placer_is_player &&
        chebyshev(game.player().x, game.player().y, x, y) <= def.burst_radius) {
        apply_to(game, &game.player(), nullptr, def);
        if (def.damage > 0) {
            game.log("  You take " + std::to_string(def.damage) + " damage" + status_part() + ".");
        } else if (!status_part().empty()) {
            game.log("  You are caught" + status_part() + ".");
        }
    }
    if (placer_is_player && def.damage > 0 &&
        chebyshev(game.player().x, game.player().y, x, y) <= def.burst_radius) {
        // Owner immune — note that they're inside the blast but unscathed.
    }

    // NPC splash — every alive NPC in radius takes it (regardless of faction;
    // grenades are area-of-effect physics, not friend/foe-aware).
    auto& npcs = game.world().npcs();
    for (Npc& n : npcs) {
        if (!n.alive()) continue;
        if (chebyshev(n.x, n.y, x, y) > def.burst_radius) continue;
        apply_to(game, nullptr, &n, def);
        if (def.damage > 0) {
            game.log("  " + display_name(n) + " takes "
                     + std::to_string(def.damage) + " damage" + status_part() + ".");
        } else if (!status_part().empty()) {
            game.log("  " + display_name(n) + " is caught" + status_part() + ".");
        }
    }
}

} // namespace astra
