#include "astra/tinkering.h"
#include "astra/item_defs.h"
#include "astra/player.h"

#include <algorithm>
#include <cmath>

namespace astra {

// ---------------------------------------------------------------------------
// Material effects
// ---------------------------------------------------------------------------

static const MaterialEffect s_material_effects[] = {
    {7002, "Power Core",    {2, 0, 0, 0, 0}},  // +2 ATK
    {7003, "Circuit Board", {0, 0, 0, 1, 0}},  // +1 view
    {7004, "Alloy Ingot",   {0, 2, 0, 0, 0}},  // +2 DEF
};

const MaterialEffect* get_material_effect(uint32_t material_id) {
    for (const auto& me : s_material_effects) {
        if (me.material_id == material_id) return &me;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Blueprint catalog
// ---------------------------------------------------------------------------

const std::vector<BlueprintEntry>& blueprint_catalog() {
    static const std::vector<BlueprintEntry> catalog = {
        // Ranged weapons
        {ItemType::RangedWeapon, "Plasma Emitter",
         "A superheated plasma projection system."},
        {ItemType::RangedWeapon, "Grip Assembly",
         "Ergonomic weapon grip with recoil dampening."},
        {ItemType::RangedWeapon, "Power Conduit",
         "Energy routing system for ranged weapons."},
        // Melee weapons
        {ItemType::MeleeWeapon, "Blade Housing",
         "Structural frame for edged weapons."},
        {ItemType::MeleeWeapon, "Hilt Assembly",
         "Balanced weapon handle with shock absorption."},
        {ItemType::MeleeWeapon, "Edge Material",
         "Molecular-honed cutting surface."},
        // Armor
        {ItemType::Armor, "Plating Alloy",
         "Composite metal alloy for defensive plating."},
        {ItemType::Armor, "Padding Weave",
         "Impact-absorbing fiber weave."},
        {ItemType::Armor, "Joint Mechanism",
         "Flexible joint system for armored mobility."},
        // Accessories
        {ItemType::Accessory, "Optic Module",
         "Enhanced optical sensor array."},
        {ItemType::Accessory, "Thruster Core",
         "Miniaturized propulsion system."},
        {ItemType::Accessory, "Storage Frame",
         "Structural frame for cargo containment."},
    };
    return catalog;
}

// ---------------------------------------------------------------------------
// Skill check helper
// ---------------------------------------------------------------------------

bool player_has_skill(const Player& player, SkillId id) {
    for (auto sid : player.learned_skills)
        if (sid == id) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Init enhancement slots
// ---------------------------------------------------------------------------

void init_enhancement_slots(Item& item) {
    switch (item.rarity) {
        case Rarity::Common:    item.enhancement_slots = 1; break;
        case Rarity::Uncommon:  item.enhancement_slots = 2; break;
        case Rarity::Rare:
        case Rarity::Epic:
        case Rarity::Legendary: item.enhancement_slots = 3; break;
    }
}

// ---------------------------------------------------------------------------
// Repair
// ---------------------------------------------------------------------------

int repair_cost(const Item& item) {
    if (item.max_durability <= 0) return 0;
    int missing = item.max_durability - item.durability;
    if (missing <= 0) return 0;
    return std::max(1, static_cast<int>(std::ceil(missing / 10.0)));
}

TinkerResult repair_item(Item& item, Player& player) {
    if (!player_has_skill(player, SkillId::BasicRepair))
        return {false, "Requires Basic Repair skill."};

    if (item.max_durability <= 0)
        return {false, "This item cannot be repaired."};

    if (item.durability >= item.max_durability)
        return {false, "Already at full durability."};

    int cost = repair_cost(item);

    // Find Nano-Fiber in inventory
    int fiber_idx = -1;
    for (int i = 0; i < static_cast<int>(player.inventory.items.size()); ++i) {
        if (player.inventory.items[i].id == 7001) {
            fiber_idx = i;
            break;
        }
    }
    if (fiber_idx < 0 || player.inventory.items[fiber_idx].stack_count < cost)
        return {false, "Need " + std::to_string(cost) + " Nano-Fiber. (have " +
                (fiber_idx >= 0 ? std::to_string(player.inventory.items[fiber_idx].stack_count) : "0") + ")"};

    // Consume Nano-Fiber
    player.inventory.items[fiber_idx].stack_count -= cost;
    if (player.inventory.items[fiber_idx].stack_count <= 0)
        player.inventory.items.erase(player.inventory.items.begin() + fiber_idx);

    item.durability = item.max_durability;
    return {true, "Repaired! Used " + std::to_string(cost) + " Nano-Fiber."};
}

// ---------------------------------------------------------------------------
// Enhancement
// ---------------------------------------------------------------------------

TinkerResult enhance_item(Item& item, int slot_index, uint32_t material_id, Player& player) {
    if (!player_has_skill(player, SkillId::BasicRepair))
        return {false, "Requires Basic Repair skill."};

    if (slot_index < 0 || slot_index >= item.enhancement_slots)
        return {false, "Slot is locked."};

    // Ensure enhancements vector is large enough
    while (static_cast<int>(item.enhancements.size()) <= slot_index)
        item.enhancements.push_back({});

    if (item.enhancements[slot_index].filled)
        return {false, "Slot already filled."};

    const MaterialEffect* effect = get_material_effect(material_id);
    if (!effect)
        return {false, "This material cannot be used for enhancement."};

    // Find and consume material from inventory
    int mat_idx = -1;
    for (int i = 0; i < static_cast<int>(player.inventory.items.size()); ++i) {
        if (player.inventory.items[i].id == material_id) {
            mat_idx = i;
            break;
        }
    }
    if (mat_idx < 0)
        return {false, "You don't have this material."};

    // Consume 1
    if (player.inventory.items[mat_idx].stack_count > 1) {
        player.inventory.items[mat_idx].stack_count--;
    } else {
        player.inventory.items.erase(player.inventory.items.begin() + mat_idx);
    }

    // Apply enhancement
    auto& slot = item.enhancements[slot_index];
    slot.filled = true;
    slot.material_id = material_id;
    slot.material_name = effect->name;
    slot.bonus = effect->bonus;

    // Apply bonus to item modifiers
    item.modifiers.attack += effect->bonus.attack;
    item.modifiers.defense += effect->bonus.defense;
    item.modifiers.max_hp += effect->bonus.max_hp;
    item.modifiers.view_radius += effect->bonus.view_radius;
    item.modifiers.quickness += effect->bonus.quickness;

    return {true, "Enhanced with " + std::string(effect->name) + "!"};
}

// ---------------------------------------------------------------------------
// Analyze
// ---------------------------------------------------------------------------

TinkerResult analyze_item(Item& item, Player& player, std::mt19937& rng) {
    if (!player_has_skill(player, SkillId::Cat_Tinkering))
        return {false, "Requires Tinkering skill unlocked."};

    if (!item.slot.has_value())
        return {false, "Can only analyze equipment."};

    // Find blueprints for this item's type
    std::vector<const BlueprintEntry*> candidates;
    for (const auto& bp : blueprint_catalog()) {
        if (bp.category == item.type) candidates.push_back(&bp);
    }
    // Also check legacy Equipment type
    if (candidates.empty() && item.type == ItemType::Equipment) {
        for (const auto& bp : blueprint_catalog()) {
            if (bp.category == ItemType::RangedWeapon) candidates.push_back(&bp);
        }
    }
    if (candidates.empty())
        return {false, "Nothing to learn from this item."};

    // Pick a random blueprint not already known
    std::vector<const BlueprintEntry*> unknown;
    for (auto* bp : candidates) {
        bool known = false;
        for (const auto& learned : player.learned_blueprints) {
            if (learned.name == bp->name) { known = true; break; }
        }
        if (!known) unknown.push_back(bp);
    }
    if (unknown.empty())
        return {false, "You already know all blueprints from this type."};

    auto* chosen = unknown[std::uniform_int_distribution<size_t>(0, unknown.size() - 1)(rng)];

    // Survival chance: 50% base + 3% per INT above 10
    int survive_chance = 50 + std::max(0, (player.attributes.intelligence - 10)) * 3;
    bool survived = std::uniform_int_distribution<int>(0, 99)(rng) < survive_chance;

    // Learn the blueprint
    player.learned_blueprints.push_back({item.id, chosen->name, chosen->description});

    std::string msg = "Learned blueprint: " + std::string(chosen->name) + "!";
    if (!survived) {
        msg += " The item was destroyed in the process.";
        // Caller should remove the item from workbench/inventory
    }

    return {survived, msg}; // success=true means item survived, false means destroyed
}

// ---------------------------------------------------------------------------
// Salvage
// ---------------------------------------------------------------------------

TinkerResult salvage_item(const Item& item, Player& player, std::mt19937& rng) {
    if (!player_has_skill(player, SkillId::Disassemble))
        return {false, "Requires Disassemble skill."};

    if (item.type == ItemType::QuestItem)
        return {false, "Cannot salvage quest items."};

    // Yield: 1-3 random materials. More for rarer items.
    int base_yield = 1;
    switch (item.rarity) {
        case Rarity::Common:    base_yield = 1; break;
        case Rarity::Uncommon:  base_yield = 2; break;
        case Rarity::Rare:      base_yield = 2; break;
        case Rarity::Epic:      base_yield = 3; break;
        case Rarity::Legendary: base_yield = 3; break;
    }
    int yield = base_yield + std::uniform_int_distribution<int>(0, 1)(rng);

    // Generate random crafting materials
    Item(*builders[])() = {build_nano_fiber, build_power_core, build_circuit_board, build_alloy_ingot};
    int builder_count = 4;

    for (int i = 0; i < yield; ++i) {
        Item mat = builders[std::uniform_int_distribution<int>(0, builder_count - 1)(rng)]();
        // Merge into existing stack if possible
        bool merged = false;
        for (auto& inv_item : player.inventory.items) {
            if (inv_item.id == mat.id) {
                inv_item.stack_count++;
                merged = true;
                break;
            }
        }
        if (!merged) {
            player.inventory.items.push_back(std::move(mat));
        }
    }

    return {true, "Salvaged " + item.name + ". Received " + std::to_string(yield) + " materials."};
}

} // namespace astra
