#pragma once

#include <cstdint>
#include <string_view>

namespace astra {

// Bitset enum: an item's source_mask is OR of every LootSource it appears in.
// uint64_t gives us 64 distinct source slots.
enum class LootSource : uint64_t {
    None              = 0,
    NpcDrop           = 1ULL << 0,
    Chest             = 1ULL << 1,  // TODO: chest loot integration deferred (loot table populated, caller pending)
    MerchantGeneral   = 1ULL << 2,
    MerchantArms      = 1ULL << 3,
    MerchantFood      = 1ULL << 4,
    ScavMerchant      = 1ULL << 5,
    BlackMarket       = 1ULL << 6,
    MaintenanceTunnel = 1ULL << 7,  // TODO: maintenance tunnel loot deferred (loot table populated, caller pending)
};

constexpr uint64_t loot_source_bit(LootSource s) {
    return static_cast<uint64_t>(s);
}

constexpr uint64_t operator|(LootSource a, LootSource b) {
    return loot_source_bit(a) | loot_source_bit(b);
}

constexpr uint64_t operator|(uint64_t a, LootSource b) {
    return a | loot_source_bit(b);
}

// Single-valued per entry. Used for filtering and future affix-theme alignment.
enum class Theme : uint8_t {
    None,
    Military,
    Ancient,
    Alien,
    Scrap,
    Civilian,
    Tech,
};

// Coarser than ItemType: MeleeWeapon and RangedWeapon both → Category::Weapon.
// Used by the per-source category roll and by manifest filters.
enum class Category : uint8_t {
    Weapon,
    Armor,
    Shield,
    Accessory,
    Consumable,
    Battery,
    Junk,
    CraftingMaterial,
    ShipComponent,
    Ingredient,
    Cookbook,
    EnergyMod,
    AccessoryMod,
    QuestItem,
    Cyberdeck,
    Program,
    CodeFragment,
};

constexpr std::string_view category_name(Category c) {
    switch (c) {
        case Category::Weapon:           return "weapon";
        case Category::Armor:            return "armor";
        case Category::Shield:           return "shield";
        case Category::Accessory:        return "accessory";
        case Category::Consumable:       return "consumable";
        case Category::Battery:          return "battery";
        case Category::Junk:             return "junk";
        case Category::CraftingMaterial: return "crafting";
        case Category::ShipComponent:    return "ship";
        case Category::Ingredient:       return "ingredient";
        case Category::Cookbook:         return "cookbook";
        case Category::EnergyMod:        return "energy mod";
        case Category::AccessoryMod:     return "accessory mod";
        case Category::QuestItem:        return "quest";
        case Category::Cyberdeck:        return "cyberdeck";
        case Category::Program:          return "program";
        case Category::CodeFragment:     return "code fragment";
    }
    return "?";
}

} // namespace astra
