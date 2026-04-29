#pragma once

#include "astra/aura_grant.h"
#include "astra/cyberdeck.h"
#include "astra/program.h"
#include "astra/dice.h"
#include "astra/energy.h"
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
    Mine,         // placeable trigger-on-step consumable
    Schematic,    // teaches a tinkering recipe when read
    Turret,       // deployable autonomous defender (stationary or mobile)
    Cyberdeck,    // hacking deck — held in EquipSlot::Cyberdeck
    Program,      // .exe / .qh loadable into a cyberdeck slot
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
    Cyberdeck,
};

static constexpr int equip_slot_count = 13;

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

enum class ModuleKind : uint8_t {
    None,
    AiModule,      // generic auto-trigger for any manual benefit
    LightSensor,   // light-conditional auto-toggle
};

struct EnhancementSlot {
    bool filled = false;
    bool committed = false;   // true after assemble, false while staged
    uint32_t material_id = 0;
    std::string material_name;
    StatModifiers stat_bonus;
    EnergyModifiers energy_bonus;
    std::optional<SolarPanelData> solar_panel;
    ModuleKind module_kind = ModuleKind::None;
};

// Effect of consuming a Food item (cooked dish, ration pack, looted meal).
// Set on the Item definition so eating is symmetric across sources.
struct DishOutput {
    int hunger_shift = 0;            // negative moves toward Satiated
    int hp_restore = 0;              // instant heal on consume, clamped
    std::vector<EffectId> granted;   // GEs applied via add_effect
};

struct RangedData {
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
    std::optional<EnergyStore> energy;
    std::optional<EnergyConsumer> consumer;
    std::optional<CellProc> proc;          // cells fire this on drain

    // Combat dice (weapons)
    Dice damage_dice;
    DamageType damage_type = DamageType::Kinetic;

    // Armor/shield type affinities
    TypeAffinity type_affinity;

    int enhancement_slots = 0;
    std::vector<EnhancementSlot> enhancements;

    bool toggleable = false;
    bool active = false;
    int drain_accumulator = 0;

    // Auras this item contributes while equipped.
    std::vector<AuraGrant> granted_auras;

    // Ship component fields (only meaningful when type == ShipComponent)
    std::optional<ShipSlot> ship_slot;
    ShipModifiers ship_modifiers;

    // Food consumption output. Only populated on ItemType::Food defs.
    std::optional<DishOutput> dish;

    // Cookbook payload. Non-zero only when type == ItemType::Cookbook.
    uint16_t teaches_recipe_id = 0;

    // Schematic payload. Non-zero only when type == ItemType::Schematic.
    uint16_t teaches_schematic_id = 0;

    // Cyberdeck payload — non-empty only when type == ItemType::Cyberdeck.
    // Holds RAM/heat/slot state and currently loaded programs.
    std::optional<CyberdeckData> deck;

    // Program payload — non-empty only when type == ItemType::Program.
    std::optional<ProgramData> program;

    // Plain-text label: "name - 1d6" for weapons, "name - cur/cap charge" for cells, plain name otherwise
    std::string label() const {
        if (!damage_dice.empty())
            return name + " - " + damage_dice.to_string();
        if (type == ItemType::Battery && energy)
            return name + " - " + std::to_string(energy->current) + "/" +
                   std::to_string(energy->capacity) + " charge";
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
    std::optional<Item> cyberdeck;

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

bool item_has_active_module(const Item& item);
std::string active_module_name(const Item& item);

// Push an item into the inventory, merging with an existing stack of the
// same item_def_id when both sides are stackable. Otherwise appends a new
// entry. Use this for any code path that grants the player items.
void add_to_inventory_stacked(Inventory& inv, Item item);

} // namespace astra
