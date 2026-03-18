#include "astra/shop.h"
#include "astra/item_defs.h"

#include <algorithm>

namespace astra {

std::vector<FoodMenuItem> food_terminal_menu() {
    std::vector<FoodMenuItem> menu;
    menu.push_back({"Protein Bar",      2, 10, false, {}});
    menu.push_back({"Synth-Brew Meal",  5, 25, false, {}});
    menu.push_back({"Spiced Void-Eel",  8, -1, false, {}});
    menu.push_back({"Ration Pack",      3,  0, true,  [] {
        Item r = build_ration_pack();
        r.buy_value = 3;
        r.sell_value = 1;
        return r;
    }});
    return menu;
}

PurchaseResult buy_food_item(Player& player, const FoodMenuItem& entry) {
    if (player.money < entry.cost) {
        return {false, "You can't afford that. (Need " +
                       std::to_string(entry.cost) + "$)"};
    }

    player.money -= entry.cost;

    if (entry.to_inventory) {
        Item item = entry.build_item();
        player.inventory.items.push_back(std::move(item));
        return {true, "You buy a " + entry.label + ". (Stored in inventory)"};
    }

    // Instant consume — heal
    if (entry.heal < 0) {
        player.hp = player.max_hp;
        return {true, "The " + entry.label + " is exquisite. Fully healed."};
    }

    int actual = std::min(entry.heal, player.max_hp - player.hp);
    player.hp += actual;
    return {true, "You eat the " + entry.label + ". (+" +
                  std::to_string(actual) + " HP)"};
}

} // namespace astra
