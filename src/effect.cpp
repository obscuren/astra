#include "astra/effect.h"

#include <algorithm>

namespace astra {

bool has_effect(const EffectList& effects, EffectId id) {
    for (const auto& e : effects) {
        if (e.id == id) return true;
    }
    return false;
}

const Effect* find_effect(const EffectList& effects, EffectId id) {
    for (const auto& e : effects) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

void add_effect(EffectList& effects, Effect e) {
    // Replace existing effect with same ID (refresh duration)
    for (auto& existing : effects) {
        if (existing.id == e.id) {
            existing = std::move(e);
            return;
        }
    }
    effects.push_back(std::move(e));
}

void remove_effect(EffectList& effects, EffectId id) {
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
                       [id](const Effect& e) { return e.id == id; }),
        effects.end());
}

void tick_effects(EffectList& effects, int& hp, int max_hp) {
    for (auto& e : effects) {
        // Apply per-tick damage/heal
        if (e.tick_damage != 0) {
            hp -= e.tick_damage;
            if (hp > max_hp) hp = max_hp;
            if (hp < 0) hp = 0;
        }
        // Count down duration
        if (e.remaining > 0) {
            --e.remaining;
        }
    }
}

void expire_effects(EffectList& effects) {
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
                       [](const Effect& e) { return e.remaining == 0; }),
        effects.end());
}

StatModifiers effect_modifiers(const EffectList& effects) {
    StatModifiers total;
    for (const auto& e : effects) {
        total.av += e.modifiers.av;
        total.dv += e.modifiers.dv;
        total.max_hp += e.modifiers.max_hp;
        total.view_radius += e.modifiers.view_radius;
        total.quickness += e.modifiers.quickness;
    }
    return total;
}

int effect_dodge_mod(const EffectList& effects) {
    int total = 0;
    for (const auto& e : effects) total += e.dodge_mod;
    return total;
}

int effect_buy_price_pct(const EffectList& effects) {
    int total = 0;
    for (const auto& e : effects) total += e.buy_price_pct;
    return total;
}

int effect_sell_price_pct(const EffectList& effects) {
    int total = 0;
    for (const auto& e : effects) total += e.sell_price_pct;
    return total;
}

int apply_damage_effects(const EffectList& effects, int raw_damage) {
    int damage = raw_damage;
    for (const auto& e : effects) {
        damage = damage * e.damage_multiplier / 100;
        damage += e.damage_flat_mod;
    }
    return damage < 0 ? 0 : damage;
}

// ── Factories ───────────────────────────────────────────────────────

Effect make_invulnerable_ge(int duration) {
    Effect e;
    e.id = EffectId::Invulnerable;
    e.name = "Invulnerable";
    e.color = Color::Cyan;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    e.damage_multiplier = 0; // immune to all damage
    return e;
}

Effect make_burn_ge(int duration, int damage_per_tick) {
    Effect e;
    e.id = EffectId::Burn;
    e.name = "Burning";
    e.color = Color::Red;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    e.tick_damage = damage_per_tick;
    return e;
}

Effect make_poison_ge(int duration, int damage_per_tick) {
    Effect e;
    e.id = EffectId::Poison;
    e.name = "Poisoned";
    e.color = Color::Green;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    e.tick_damage = damage_per_tick;
    return e;
}

Effect make_regen_ge(int duration, int heal_per_tick) {
    Effect e;
    e.id = EffectId::Regen;
    e.name = "Regen";
    e.color = Color::Green;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    e.tick_damage = -heal_per_tick; // negative = heal
    return e;
}

Effect make_dodge_boost_ge(int duration, int amount) {
    Effect e;
    e.id = EffectId::DodgeBoost;
    e.name = "Evasion";
    e.color = Color::Yellow;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = false;
    e.dodge_mod = amount;
    return e;
}

Effect make_attack_boost_ge(int duration, int amount) {
    Effect e;
    e.id = EffectId::AttackBoost;
    e.name = "Empowered";
    e.color = Color::Yellow;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = false;
    e.dodge_mod = amount;
    return e;
}

Effect make_defense_boost_ge(int duration, int amount) {
    Effect e;
    e.id = EffectId::DefenseBoost;
    e.name = "Fortified";
    e.color = Color::Yellow;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = false;
    e.modifiers.av = amount;
    return e;
}

Effect make_haggle_ge() {
    Effect e;
    e.id = EffectId::Haggle;
    e.name = "Haggle";
    e.color = Color::Yellow;
    e.duration = -1;
    e.remaining = -1;
    e.show_in_bar = false;
    e.buy_price_pct = -10;
    e.sell_price_pct = 10;
    return e;
}

Effect make_thick_skin_ge() {
    Effect e;
    e.id = EffectId::ThickSkin;
    e.name = "Thick Skin";
    e.color = Color::Green;
    e.duration = -1;
    e.remaining = -1;
    e.show_in_bar = false;
    e.modifiers.av = 1;
    return e;
}

Effect make_flee_ge(int duration) {
    Effect e;
    e.id = EffectId::Flee;
    e.name = "Fleeing";
    e.color = Color::Yellow;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = false; // NPC effect, not shown on player bar
    return e;
}

Effect make_cozy_ge() {
    Effect e;
    e.id = EffectId::Cozy;
    e.name = "Cozy";
    // Orange — matches the campfire palette. 208 is xterm orange.
    e.color = static_cast<Color>(208);
    // Refreshed each tick by the proximity scanner; when the player steps
    // out of range it naturally expires on the next tick.
    e.duration = 1;
    e.remaining = 1;
    e.show_in_bar = true;
    return e;
}

} // namespace astra
