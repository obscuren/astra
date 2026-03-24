#pragma once

#include "astra/item.h"

#include <random>

namespace astra {

struct ItemAffix {
    const char* name;
    bool is_prefix;
    StatModifiers modifiers;
    int durability_bonus = 0;
    float value_mult = 1.0f;
};

// Apply level scaling to an item's stats (modifies in place)
void scale_item_to_level(Item& item, int level);

// Apply an affix to an item (modifies name and stats in place)
void apply_affix(Item& item, const ItemAffix& affix);

// Roll a random rarity
Rarity roll_rarity(std::mt19937& rng);

// Generate a fully random item: picks base, applies level scaling, rolls
// rarity, and adds affixes based on rarity.
Item generate_random_weapon(std::mt19937& rng, int level);
Item generate_random_armor(std::mt19937& rng, int level);
Item generate_loot_drop(std::mt19937& rng, int level);

} // namespace astra
