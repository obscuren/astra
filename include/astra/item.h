#pragma once

#include "astra/aura_grant.h"
#include "astra/dice.h"
#include "astra/renderer.h"
#include "astra/ui_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

// Forward-declared to avoid a cycle with effect.h (which already
// includes item.h for StatModifiers). DishOutput stores EffectIds
// but never uses their full definition here.
enum class EffectId : uint32_t;

enum class ItemType : uint8_t {
    Equipment = 0,  // legacy — kept for save compat
    Trash,
    Credits,
    Food,
    Stim,
    Battery,
    Light,
    Special,
    // V2 item types
    MeleeWeapon,
    RangedWeapon,
    Armor,
    Shield,
    Accessory,
    Grenade,
    Junk,
    CraftingMaterial,
    ShipComponent,
    QuestItem,
    Ingredient,   // cooking raw material
    Cookbook,     // teaches a recipe when read
};

const char* item_type_name(ItemType t);

enum class WeaponClass : uint8_t {
    None,
    ShortBlade,
    LongBlade,
    Pistol,
    Rifle,
};

enum class EquipSlot : uint8_t {
    Face,
    Head,
    Body,
    LeftArm,
    RightArm,
    LeftHand,
    RightHand,
    Back,
    Feet,
    Thrown,
    Missile,
    Shield,
};

static constexpr int equip_slot_count = 12;

const char* equip_slot_name(EquipSlot slot);

enum class ShipSlot : uint8_t {
    Engine,
    Hull,
    NaviComputer,
    Shield,
    Utility1,
    Utility2,
};

static constexpr int ship_slot_count = 6;

const char* ship_slot_name(ShipSlot slot);

struct ShipModifiers {
    int hull_hp = 0;
    int shield_hp = 0;
    int warp_range = 0;
    int cargo_capacity = 0;
};

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

inline UITag rarity_tag(Rarity r) {
    switch (r) {
        case Rarity::Common:    return UITag::RarityCommon;
        case Rarity::Uncommon:  return UITag::RarityUncommon;
        case Rarity::Rare:      return UITag::RarityRare;
        case Rarity::Epic:      return UITag::RarityEpic;
        case Rarity::Legendary: return UITag::RarityLegendary;
    }
    return UITag::RarityCommon;
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
    int av = 0;
    int dv = 0;
    int max_hp = 0;
    int view_radius = 0;
    int quickness = 0;
};

struct EnhancementSlot {
    bool filled = false;
    bool committed = false;   // true after assemble, false while staged
    uint32_t material_id = 0;
    std::string material_name;
    StatModifiers bonus;
};

// Effect of consuming a Food item (cooked dish, ration pack, looted meal).
// Set on the Item definition so eating is symmetric across sources.
struct DishOutput {
    int hunger_shift = 0;            // negative moves toward Satiated
    int hp_restore = 0;              // instant heal on consume, clamped
    std::vector<EffectId> granted;   // GEs applied via add_effect
};

struct RangedData {
    int charge_capacity = 0;
    int charge_per_shot = 1;
    int current_charge = 0;
    int max_range = 8;
};

struct Item {
    uint32_t id = 0;
    uint16_t item_def_id = 0;    // definition registry ID — renderer resolves visual from this
    std::string name;
    std::string description;
    ItemType type = ItemType::Trash;
    WeaponClass weapon_class = WeaponClass::None;
    std::optional<EquipSlot> slot;
    Rarity rarity = Rarity::Common;
    int weight = 1;
    bool stackable = false;
    int stack_count = 1;
    int buy_value = 0;
    int sell_value = 0;
    StatModifiers modifiers;
    int item_level = 1;
    int level_requirement = 0;
    int durability = 0;
    int max_durability = 0;
    bool usable = false;
    std::optional<RangedData> ranged;

    // Combat dice (weapons)
    Dice damage_dice;
    DamageType damage_type = DamageType::Kinetic;

    // Armor/shield type affinities
    TypeAffinity type_affinity;

    // Shield fields (only meaningful when slot == EquipSlot::Shield)
    int shield_capacity = 0;
    int shield_hp = 0;

    int enhancement_slots = 0;
    std::vector<EnhancementSlot> enhancements;

    // Auras this item contributes while equipped.
    std::vector<AuraGrant> granted_auras;

    // Ship component fields (only meaningful when type == ShipComponent)
    std::optional<ShipSlot> ship_slot;
    ShipModifiers ship_modifiers;

    // Food consumption output. Only populated on ItemType::Food defs.
    std::optional<DishOutput> dish;

    // Cookbook payload. Non-zero only when type == ItemType::Cookbook.
    uint16_t teaches_recipe_id = 0;

    // Plain-text label: "name - 1d6" for weapons, just "name" otherwise
    std::string label() const {
        if (!damage_dice.empty())
            return name + " - " + damage_dice.to_string();
        return name;
    }
};

struct GroundItem {
    int x = 0;
    int y = 0;
    Item item;
};

struct Equipment {
    std::optional<Item> face;
    std::optional<Item> head;
    std::optional<Item> body;
    std::optional<Item> left_arm;
    std::optional<Item> right_arm;
    std::optional<Item> left_hand;
    std::optional<Item> right_hand;
    std::optional<Item> back;
    std::optional<Item> feet;
    std::optional<Item> thrown;
    std::optional<Item> missile;
    std::optional<Item> shield;

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
