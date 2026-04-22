#pragma once

#include "astra/item.h"     // StatModifiers
#include "astra/renderer.h" // Color

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class EffectId : uint32_t {
    Invulnerable = 1,
    Burn         = 2,
    Poison       = 3,
    Regen        = 4,
    DodgeBoost   = 5,
    AttackBoost  = 6,
    DefenseBoost = 7,
    Slow         = 8,
    Haste        = 9,
    Haggle       = 10,
    ThickSkin    = 11,
    // Ability cooldowns (100+)
    CooldownJab         = 100,
    CooldownCleave      = 101,
    CooldownQuickdraw   = 102,
    CooldownIntimidate  = 103,
    CooldownSuppressing = 104,
    CooldownCampMaking  = 105,
    Flee                = 200,

    // Environmental buffs (300+)
    Cozy                = 300,
};

struct Effect {
    EffectId id;
    std::string name;
    Color color = Color::White;
    int duration = -1;          // total ticks, -1 = infinite
    int remaining = -1;         // ticks left, -1 = infinite
    int applied_tick = 0;
    bool show_in_bar = true;

    // Per-tick damage (positive = damage, negative = heal)
    int tick_damage = 0;

    // Stat modifiers (additive while active)
    StatModifiers modifiers;

    // Extra modifiers not in StatModifiers
    int dodge_mod = 0;
    int move_speed_mod = 0;

    // Damage modification — applied when receiving damage
    // multiplier: 100 = normal, 0 = immune, 50 = half, 200 = double
    int damage_multiplier = 100;
    int damage_flat_mod = 0;    // added after multiplier (positive = more damage taken)

    // Trade price modifiers (percentage: -10 = 10% cheaper buy, +10 = 10% better sell)
    int buy_price_pct = 0;
    int sell_price_pct = 0;
};

using EffectList = std::vector<Effect>;

// Query
bool has_effect(const EffectList& effects, EffectId id);
const Effect* find_effect(const EffectList& effects, EffectId id);

// Mutation
void add_effect(EffectList& effects, Effect e);
void remove_effect(EffectList& effects, EffectId id);
void tick_effects(EffectList& effects, int& hp, int max_hp);
void expire_effects(EffectList& effects);

// Aggregation
StatModifiers effect_modifiers(const EffectList& effects);
int effect_dodge_mod(const EffectList& effects);
int effect_buy_price_pct(const EffectList& effects);
int effect_sell_price_pct(const EffectList& effects);

// Damage pipeline — pass raw damage through all active effects.
// Returns modified damage (clamped to >= 0).
int apply_damage_effects(const EffectList& effects, int raw_damage);

// Factories
Effect make_invulnerable(int duration = -1);
Effect make_burn(int duration, int damage_per_tick);
Effect make_poison(int duration, int damage_per_tick);
Effect make_regen(int duration, int heal_per_tick);
Effect make_dodge_boost(int duration, int amount);
Effect make_attack_boost(int duration, int amount);
Effect make_defense_boost(int duration, int amount);
Effect make_haggle();
Effect make_thick_skin();
Effect make_flee(int duration);
Effect make_cozy();

} // namespace astra
