#pragma once

#include "astra/renderer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

enum class ItemType : uint8_t {
    Equipment,
    Trash,
    Credits,
    Food,
    Stim,
    Battery,
    Light,
    Special,
};

enum class EquipSlot : uint8_t {
    Head,
    Chest,
    Legs,
    Feet,
    Hands,
    MeleeWeapon,
    RangedWeapon,
    SpecialSlot,
};

static constexpr int equip_slot_count = 8;

enum class Rarity : uint8_t {
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary,
};

inline Color rarity_color(Rarity r) {
    switch (r) {
        case Rarity::Common:    return Color::White;
        case Rarity::Uncommon:  return Color::Green;
        case Rarity::Rare:      return Color::Blue;
        case Rarity::Epic:      return Color::Magenta;
        case Rarity::Legendary: return static_cast<Color>(208); // xterm orange
    }
    return Color::White;
}

inline const char* rarity_name(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "Common";
        case Rarity::Uncommon:  return "Uncommon";
        case Rarity::Rare:      return "Rare";
        case Rarity::Epic:      return "Epic";
        case Rarity::Legendary: return "Legendary";
    }
    return "Unknown";
}

struct StatModifiers {
    int attack = 0;
    int defense = 0;
    int max_hp = 0;
    int view_radius = 0;
    int quickness = 0;
};

struct RangedData {
    int charge_capacity = 0;
    int charge_per_shot = 1;
    int current_charge = 0;
};

struct Item {
    uint32_t id = 0;
    std::string name;
    std::string description;
    ItemType type = ItemType::Trash;
    std::optional<EquipSlot> slot;
    Rarity rarity = Rarity::Common;
    char glyph = '?';
    Color color = Color::White;
    int weight = 1;
    bool stackable = false;
    int stack_count = 1;
    int buy_value = 0;
    int sell_value = 0;
    StatModifiers modifiers;
    int level_requirement = 0;
    int durability = 0;
    int max_durability = 0;
    bool usable = false;
    std::optional<RangedData> ranged;
};

struct GroundItem {
    int x = 0;
    int y = 0;
    Item item;
};

struct Equipment {
    std::optional<Item> head;
    std::optional<Item> chest;
    std::optional<Item> legs;
    std::optional<Item> feet;
    std::optional<Item> hands;
    std::optional<Item> melee_weapon;
    std::optional<Item> ranged_weapon;
    std::optional<Item> special_slot;

    std::optional<Item>& slot_ref(EquipSlot slot);
    const std::optional<Item>& slot_ref(EquipSlot slot) const;
    StatModifiers total_modifiers() const;
};

struct Inventory {
    std::vector<Item> items;
    int max_carry_weight = 50;

    int total_weight() const;
    bool can_add(const Item& item) const;
};

struct LootEntry {
    uint32_t item_id = 0;
    float drop_chance = 0.0f;
    int min_qty = 1;
    int max_qty = 1;
};

struct LootTable {
    uint8_t npc_role = 0; // cast to/from NpcRole without circular include
    std::vector<LootEntry> entries;
};

} // namespace astra
