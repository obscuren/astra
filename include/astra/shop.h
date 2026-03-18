#pragma once

#include "astra/item.h"
#include "astra/player.h"

#include <functional>
#include <string>
#include <vector>

namespace astra {

// A single menu entry at the food terminal (or any future shop).
struct FoodMenuItem {
    std::string label;  // display string, e.g. "Protein Bar"
    int cost;           // credits
    int heal;           // HP restored, -1 = full heal
    bool to_inventory;  // true = add item to inventory instead of instant-use
    std::function<Item()> build_item; // if to_inventory, builds the item
};

// Returns the canonical food terminal menu.
std::vector<FoodMenuItem> food_terminal_menu();

// Result of a purchase attempt.
struct PurchaseResult {
    bool success = false;
    std::string message;
};

// Attempt to buy a food menu item. Deducts money, applies heal or adds item.
PurchaseResult buy_food_item(Player& player, const FoodMenuItem& entry);

} // namespace astra
