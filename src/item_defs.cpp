#include "astra/item_defs.h"

namespace astra {

// ---------------------------------------------------------------------------
// Ranged weapons
// ---------------------------------------------------------------------------

Item build_plasma_pistol() {
    Item it;
    it.id = 1001;
    it.name = "Plasma Pistol";
    it.description = "Standard-issue sidearm. Fires superheated plasma bolts.";
    it.type = ItemType::Equipment;
    it.slot = EquipSlot::RangedWeapon;
    it.rarity = Rarity::Common;
    it.glyph = ')';
    it.color = Color::Cyan;
    it.weight = 3;
    it.buy_value = 120;
    it.sell_value = 40;
    it.modifiers.attack = 3;
    it.max_durability = 80;
    it.durability = 80;
    it.ranged = RangedData{20, 1, 20, 6};
    return it;
}

Item build_ion_blaster() {
    Item it;
    it.id = 1002;
    it.name = "Ion Blaster";
    it.description = "Disrupts electronics and shields with ionized bursts.";
    it.type = ItemType::Equipment;
    it.slot = EquipSlot::RangedWeapon;
    it.rarity = Rarity::Uncommon;
    it.glyph = ')';
    it.color = Color::Green;
    it.weight = 4;
    it.buy_value = 250;
    it.sell_value = 85;
    it.modifiers.attack = 5;
    it.max_durability = 60;
    it.durability = 60;
    it.ranged = RangedData{15, 2, 15, 8};
    return it;
}

Item build_pulse_rifle() {
    Item it;
    it.id = 1003;
    it.name = "Pulse Rifle";
    it.description = "Military-grade rifle with rapid energy pulses.";
    it.type = ItemType::Equipment;
    it.slot = EquipSlot::RangedWeapon;
    it.rarity = Rarity::Rare;
    it.glyph = ')';
    it.color = Color::Blue;
    it.weight = 6;
    it.buy_value = 500;
    it.sell_value = 170;
    it.modifiers.attack = 8;
    it.modifiers.quickness = -5;
    it.max_durability = 100;
    it.durability = 100;
    it.ranged = RangedData{30, 2, 30, 12};
    return it;
}

Item build_arc_caster() {
    Item it;
    it.id = 1004;
    it.name = "Arc Caster";
    it.description = "Channels electricity in a devastating arc. Unstable.";
    it.type = ItemType::Equipment;
    it.slot = EquipSlot::RangedWeapon;
    it.rarity = Rarity::Epic;
    it.glyph = ')';
    it.color = Color::Magenta;
    it.weight = 5;
    it.buy_value = 900;
    it.sell_value = 300;
    it.modifiers.attack = 12;
    it.modifiers.quickness = -10;
    it.max_durability = 50;
    it.durability = 50;
    it.ranged = RangedData{12, 3, 12, 5};
    return it;
}

Item build_void_lance() {
    Item it;
    it.id = 1005;
    it.name = "Void Lance";
    it.description = "Fires a beam of compressed dark energy. Extremely rare.";
    it.type = ItemType::Equipment;
    it.slot = EquipSlot::RangedWeapon;
    it.rarity = Rarity::Legendary;
    it.glyph = ')';
    it.color = static_cast<Color>(208); // xterm orange
    it.weight = 7;
    it.buy_value = 2500;
    it.sell_value = 800;
    it.modifiers.attack = 18;
    it.modifiers.view_radius = 2;
    it.max_durability = 40;
    it.durability = 40;
    it.ranged = RangedData{10, 4, 10, 15};
    return it;
}

// ---------------------------------------------------------------------------
// Consumables
// ---------------------------------------------------------------------------

Item build_battery() {
    Item it;
    it.id = 2001;
    it.name = "Energy Cell";
    it.description = "Standard power cell. Recharges ranged weapons.";
    it.type = ItemType::Battery;
    it.rarity = Rarity::Common;
    it.glyph = '=';
    it.color = Color::Yellow;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 15;
    it.sell_value = 5;
    it.usable = true;
    return it;
}

Item build_ration_pack() {
    Item it;
    it.id = 2002;
    it.name = "Ration Pack";
    it.description = "Compact nutrient paste. Restores hunger.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Common;
    it.glyph = '%';
    it.color = Color::Green;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 10;
    it.sell_value = 3;
    it.usable = true;
    return it;
}

Item build_combat_stim() {
    Item it;
    it.id = 2003;
    it.name = "Combat Stim";
    it.description = "Adrenaline injection. Temporarily boosts attack.";
    it.type = ItemType::Stim;
    it.rarity = Rarity::Uncommon;
    it.glyph = '!';
    it.color = Color::Red;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 50;
    it.sell_value = 18;
    it.usable = true;
    return it;
}

// ---------------------------------------------------------------------------
// Random selection
// ---------------------------------------------------------------------------

Item random_ranged_weapon(std::mt19937& rng) {
    // Weighted toward common/uncommon
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);

    if (roll < 40) return build_plasma_pistol();   // 40%
    if (roll < 70) return build_ion_blaster();      // 30%
    if (roll < 88) return build_pulse_rifle();      // 18%
    if (roll < 97) return build_arc_caster();       //  9%
    return build_void_lance();                      //  3%
}

// ---------------------------------------------------------------------------
// Merchant stock generators
// ---------------------------------------------------------------------------

static Item make_stack(Item item, int count) {
    item.stack_count = count;
    return item;
}

std::vector<Item> generate_merchant_stock(std::mt19937& rng) {
    std::vector<Item> stock;
    stock.push_back(make_stack(build_battery(), 3));
    stock.push_back(make_stack(build_ration_pack(), 5));
    stock.push_back(make_stack(build_combat_stim(), 2));
    stock.push_back(random_ranged_weapon(rng));
    return stock;
}

std::vector<Item> generate_arms_dealer_stock(std::mt19937& rng) {
    std::vector<Item> stock;
    std::uniform_int_distribution<int> dist(2, 3);
    int weapon_count = dist(rng);
    for (int i = 0; i < weapon_count; ++i) {
        stock.push_back(random_ranged_weapon(rng));
    }
    stock.push_back(make_stack(build_battery(), 5));
    return stock;
}

std::vector<Item> generate_food_merchant_stock(std::mt19937& rng) {
    (void)rng;
    std::vector<Item> stock;
    stock.push_back(make_stack(build_ration_pack(), 10));
    stock.push_back(make_stack(build_combat_stim(), 3));
    return stock;
}

} // namespace astra
