#include "astra/item_gen.h"
#include "astra/item_defs.h"

#include <cmath>

namespace astra {

// ---------------------------------------------------------------------------
// Affix pools
// ---------------------------------------------------------------------------

static const ItemAffix s_prefixes[] = {
    {"Corroded",     true, {0, -1, 0, 0, 0}, -5,  0.7f},
    {"Rusted",       true, {-2, 0, 0, 0, 0}, -3,  0.6f},
    {"Reinforced",   true, {0, 2, 0, 0, 0},   5,  1.3f},
    {"Overcharged",  true, {3, 0, 0, 0, 0},  -5,  1.2f},
    {"Pristine",     true, {0, 0, 0, 0, 0},  10,  1.1f},
    {"Salvaged",     true, {-1, -1, 0, 0, 0}, 0,  0.5f},
    {"Military",     true, {2, 1, 0, 0, 0},   0,  1.5f},
    {"Prototype",    true, {4, -2, 0, 0, 0},  0,  1.4f},
    {"Ancient",      true, {3, 0, 0, 2, 0},   0,  2.0f},
    {"Nano-Enhanced",true, {2, 2, 0, 0, 0},   0,  1.8f},
};

static const ItemAffix s_suffixes[] = {
    {"of Precision",    false, {2, 0, 0, 0, 0},   0, 1.3f},
    {"of the Void",     false, {0, 0, 0, 2, 0},   0, 1.2f},
    {"of Endurance",    false, {0, 0, 2, 0, 0},   0, 1.2f},
    {"of Speed",        false, {0, 0, 0, 0, 3},   0, 1.3f},
    {"of the Ancients", false, {3, 0, 0, 1, 0},   0, 2.0f},
    {"of Salvage",      false, {-1, 0, 0, 0, 0},  0, 0.6f},
    {"of Protection",   false, {0, 3, 0, 0, 0},   0, 1.4f},
    {"of the Swarm",    false, {1, 0, 0, 0, 1},   0, 1.1f},
    {"of Decay",        false, {0, 0, 0, 0, 0}, -10, 0.5f},
    {"of the Stars",    false, {2, 0, 1, 0, 0},   0, 1.5f},
};

static constexpr int prefix_count = sizeof(s_prefixes) / sizeof(s_prefixes[0]);
static constexpr int suffix_count = sizeof(s_suffixes) / sizeof(s_suffixes[0]);

// ---------------------------------------------------------------------------
// Level scaling
// ---------------------------------------------------------------------------

void scale_item_to_level(Item& item, int level) {
    if (level <= 1) return;
    float mult = 1.0f + (level - 1) * 0.15f;

    auto scale = [mult](int v) -> int {
        return static_cast<int>(std::round(v * mult));
    };

    item.item_level = level;
    item.modifiers.attack = scale(item.modifiers.attack);
    item.modifiers.defense = scale(item.modifiers.defense);
    item.modifiers.max_hp = scale(item.modifiers.max_hp);
    item.buy_value = scale(item.buy_value);
    item.sell_value = scale(item.sell_value);
    item.max_durability = scale(item.max_durability);
    item.durability = item.max_durability;

    if (item.ranged) {
        item.ranged->charge_capacity = scale(item.ranged->charge_capacity);
        item.ranged->current_charge = item.ranged->charge_capacity;
    }
}

// ---------------------------------------------------------------------------
// Affix application
// ---------------------------------------------------------------------------

void apply_affix(Item& item, const ItemAffix& affix) {
    if (affix.is_prefix) {
        item.name = std::string(affix.name) + " " + item.name;
    } else {
        item.name = item.name + " " + affix.name;
    }

    item.modifiers.attack += affix.modifiers.attack;
    item.modifiers.defense += affix.modifiers.defense;
    item.modifiers.max_hp += affix.modifiers.max_hp;
    item.modifiers.view_radius += affix.modifiers.view_radius;
    item.modifiers.quickness += affix.modifiers.quickness;

    item.max_durability += affix.durability_bonus;
    if (item.max_durability < 1 && item.max_durability != 0)
        item.max_durability = 1;
    item.durability = item.max_durability;

    item.buy_value = static_cast<int>(item.buy_value * affix.value_mult);
    item.sell_value = static_cast<int>(item.sell_value * affix.value_mult);
}

// ---------------------------------------------------------------------------
// Rarity rolling
// ---------------------------------------------------------------------------

Rarity roll_rarity(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);
    if (roll < 50) return Rarity::Common;       // 50%
    if (roll < 80) return Rarity::Uncommon;      // 30%
    if (roll < 95) return Rarity::Rare;          // 15%
    if (roll < 99) return Rarity::Epic;          //  4%
    return Rarity::Legendary;                    //  1%
}

// ---------------------------------------------------------------------------
// Random generation
// ---------------------------------------------------------------------------

static void apply_rarity_affixes(Item& item, Rarity rarity, std::mt19937& rng) {
    item.rarity = rarity;
    item.color = rarity_color(rarity);

    std::uniform_int_distribution<int> prefix_dist(0, prefix_count - 1);
    std::uniform_int_distribution<int> suffix_dist(0, suffix_count - 1);

    switch (rarity) {
        case Rarity::Common:
            // No affixes
            break;
        case Rarity::Uncommon: {
            // 1 prefix OR 1 suffix
            if (std::uniform_int_distribution<int>(0, 1)(rng) == 0)
                apply_affix(item, s_prefixes[prefix_dist(rng)]);
            else
                apply_affix(item, s_suffixes[suffix_dist(rng)]);
            break;
        }
        case Rarity::Rare:
            // 1 prefix AND 1 suffix
            apply_affix(item, s_prefixes[prefix_dist(rng)]);
            apply_affix(item, s_suffixes[suffix_dist(rng)]);
            break;
        case Rarity::Epic:
            // Stronger affixes (pick from top half of pool)
            apply_affix(item, s_prefixes[std::uniform_int_distribution<int>(prefix_count / 2, prefix_count - 1)(rng)]);
            apply_affix(item, s_suffixes[suffix_dist(rng)]);
            break;
        case Rarity::Legendary:
            // Always "Ancient" + "of the Ancients" or similar top-tier
            apply_affix(item, s_prefixes[8]); // Ancient
            apply_affix(item, s_suffixes[4]); // of the Ancients
            break;
    }
}

Item generate_random_weapon(std::mt19937& rng, int level) {
    Item item;
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0)
        item = random_ranged_weapon(rng);
    else
        item = random_melee_weapon(rng);

    scale_item_to_level(item, level);
    apply_rarity_affixes(item, roll_rarity(rng), rng);
    return item;
}

Item generate_random_armor(std::mt19937& rng, int level) {
    Item item = random_armor(rng);
    scale_item_to_level(item, level);
    apply_rarity_affixes(item, roll_rarity(rng), rng);
    return item;
}

Item generate_loot_drop(std::mt19937& rng, int level) {
    // Weighted: 30% weapon, 25% armor, 20% consumable, 15% junk, 10% crafting
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);

    if (roll < 30) {
        return generate_random_weapon(rng, level);
    } else if (roll < 55) {
        return generate_random_armor(rng, level);
    } else if (roll < 75) {
        // Random consumable
        switch (std::uniform_int_distribution<int>(0, 2)(rng)) {
            case 0: return build_battery();
            case 1: return build_ration_pack();
            default: return build_combat_stim();
        }
    } else if (roll < 90) {
        Item junk = random_junk(rng);
        junk.stack_count = std::uniform_int_distribution<int>(1, 3)(rng);
        return junk;
    } else {
        // Crafting material
        switch (std::uniform_int_distribution<int>(0, 3)(rng)) {
            case 0: return build_nano_fiber();
            case 1: return build_power_core();
            case 2: return build_circuit_board();
            default: return build_alloy_ingot();
        }
    }
}

} // namespace astra
