#include "astra/tinkering.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/item_ids.h"
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

    // Stage enhancement (pending — not applied until commit)
    auto& slot = item.enhancements[slot_index];
    slot.filled = true;
    slot.material_id = material_id;
    slot.material_name = effect->name;
    slot.bonus = effect->bonus;

    return {true, "Slotted " + std::string(effect->name) + ". [f] Assemble to apply."};
}

TinkerResult commit_enhancements(Item& item) {
    int applied = 0;
    for (auto& slot : item.enhancements) {
        if (slot.filled && !slot.committed) {
            // Apply bonus permanently
            item.modifiers.attack += slot.bonus.attack;
            item.modifiers.defense += slot.bonus.defense;
            item.modifiers.max_hp += slot.bonus.max_hp;
            item.modifiers.view_radius += slot.bonus.view_radius;
            item.modifiers.quickness += slot.bonus.quickness;
            slot.committed = true;
            applied++;
        }
    }
    if (applied == 0)
        return {false, "Nothing to assemble."};
    return {true, "Assembled! " + std::to_string(applied) + " enhancement(s) applied to " + item.name + "."};
}

TinkerResult clear_enhancement_slot(Item& item, int slot_index, Player& player) {
    if (slot_index < 0 || slot_index >= static_cast<int>(item.enhancements.size()))
        return {false, "Invalid slot."};

    auto& slot = item.enhancements[slot_index];
    if (!slot.filled)
        return {false, "Slot is empty."};
    if (slot.committed)
        return {false, "Cannot remove committed enhancements."};

    // Return material to inventory
    const MaterialEffect* effect = get_material_effect(slot.material_id);
    if (effect) {
        bool merged = false;
        for (auto& inv_item : player.inventory.items) {
            if (inv_item.id == slot.material_id) {
                inv_item.stack_count++;
                merged = true;
                break;
            }
        }
        if (!merged) {
            // Rebuild the material item
            Item mat;
            mat.id = slot.material_id;
            mat.name = slot.material_name;
            mat.type = ItemType::CraftingMaterial;
            mat.stackable = true;
            mat.stack_count = 1;
            mat.glyph = '+';
            mat.weight = 1;
            player.inventory.items.push_back(std::move(mat));
        }
    }

    std::string name = slot.material_name;
    slot = {}; // reset slot
    return {true, "Removed " + name + " from slot."};
}

bool has_pending_enhancements(const Item& item) {
    for (const auto& slot : item.enhancements)
        if (slot.filled && !slot.committed) return true;
    return false;
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

// ---------------------------------------------------------------------------
// Synthesizer
// ---------------------------------------------------------------------------

// Material IDs: 7001=Nano-Fiber, 7002=Power Core, 7003=Circuit Board, 7004=Alloy Ingot
const std::vector<SynthesisRecipe>& synthesis_recipes() {
    static const std::vector<SynthesisRecipe> recipes = {
        {"Plasma Emitter", "Blade Housing", "Plasma Edge",
         "A blade wreathed in plasma energy. Burns on contact.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {8, 0, 0, 0, 0}, 60, {0, 2, 0, 1}},

        {"Plating Alloy", "Thruster Core", "Thruster Plate",
         "Armored plating with integrated micro-thrusters for agile combat.",
         ItemType::Armor, EquipSlot::Body, ']',
         {0, 4, 0, 0, 3}, 80, {0, 1, 0, 2}},

        {"Optic Module", "Power Conduit", "Targeting Array",
         "Advanced optics fused with a power feed. Enhances aim and awareness.",
         ItemType::Accessory, EquipSlot::Face, '&',
         {2, 0, 0, 3, 0}, 0, {0, 1, 2, 0}},

        {"Edge Material", "Grip Assembly", "Dual-Edge",
         "A twin-bladed weapon with perfect balance. Strikes twice as fast.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {6, 0, 0, 0, 2}, 50, {1, 0, 0, 1}},

        {"Padding Weave", "Storage Frame", "Reinforced Pack",
         "A heavily padded cargo pack. Protects both you and your gear.",
         ItemType::Accessory, EquipSlot::Back, '\\',
         {0, 2, 3, 0, 0}, 0, {2, 0, 0, 1}},

        {"Power Conduit", "Thruster Core", "Overcharged Engine",
         "A hyperspace engine component running at dangerous output levels.",
         ItemType::ShipComponent, EquipSlot::Back, '#',
         {0, 0, 0, 0, 5}, 0, {0, 3, 0, 0}},

        {"Plating Alloy", "Joint Mechanism", "Articulated Armor",
         "Segmented armor that moves with you. Full protection, zero penalty.",
         ItemType::Armor, EquipSlot::Body, ']',
         {0, 5, 0, 0, 1}, 100, {0, 0, 1, 2}},

        {"Plasma Emitter", "Optic Module", "Guided Blaster",
         "A plasma weapon with auto-tracking optics. Rarely misses.",
         ItemType::RangedWeapon, EquipSlot::Missile, ')',
         {6, 0, 0, 1, 0}, 50, {0, 2, 1, 0}},

        {"Blade Housing", "Joint Mechanism", "Combat Gauntlet",
         "An armored fist with embedded blades. Strike and defend as one.",
         ItemType::Armor, EquipSlot::LeftHand, '}',
         {3, 2, 0, 0, 0}, 70, {1, 0, 0, 1}},

        {"Edge Material", "Plating Alloy", "Armored Blade",
         "A thick, heavy blade reinforced with armor plating. Hits like a wall.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {5, 3, 0, 0, 0}, 90, {0, 0, 0, 2}},
    };
    return recipes;
}

const SynthesisRecipe* find_recipe(const std::string& bp1, const std::string& bp2) {
    for (const auto& r : synthesis_recipes()) {
        if ((bp1 == r.blueprint_1 && bp2 == r.blueprint_2) ||
            (bp1 == r.blueprint_2 && bp2 == r.blueprint_1))
            return &r;
    }
    return nullptr;
}

static const uint32_t s_material_ids[4] = {7001, 7002, 7003, 7004};
static const char* s_material_names[4] = {"Nano-Fiber", "Power Core", "Circuit Board", "Alloy Ingot"};

TinkerResult synthesize_item(const std::string& bp1, const std::string& bp2,
                              Player& player, std::mt19937& rng) {
    if (!player_has_skill(player, SkillId::Cat_Tinkering))
        return {false, "Requires Tinkering skill unlocked."};

    const auto* recipe = find_recipe(bp1, bp2);
    if (!recipe)
        return {false, "No known recipe for this combination."};

    // Check material costs
    for (int m = 0; m < 4; ++m) {
        if (recipe->material_cost[m] <= 0) continue;
        int have = 0;
        for (const auto& it : player.inventory.items) {
            if (it.id == s_material_ids[m]) have = it.stack_count;
        }
        if (have < recipe->material_cost[m])
            return {false, "Need " + std::to_string(recipe->material_cost[m]) + " " +
                    s_material_names[m] + " (have " + std::to_string(have) + ")."};
    }

    // Consume materials
    for (int m = 0; m < 4; ++m) {
        int needed = recipe->material_cost[m];
        if (needed <= 0) continue;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end(); ) {
            if (it->id == s_material_ids[m]) {
                if (it->stack_count > needed) {
                    it->stack_count -= needed;
                    needed = 0;
                } else {
                    needed -= it->stack_count;
                    it = player.inventory.items.erase(it);
                    continue;
                }
            }
            ++it;
            if (needed <= 0) break;
        }
    }

    // Create result item
    Item item;
    item.id = 9000 + static_cast<uint32_t>(&*recipe - &synthesis_recipes()[0]);
    item.item_def_id = ITEM_SYNTH_PLASMA_EDGE + static_cast<uint16_t>(&*recipe - &synthesis_recipes()[0]);
    item.name = recipe->result_name;
    item.description = recipe->result_desc;
    item.type = recipe->result_type;
    if (recipe->result_slot != EquipSlot::Back || recipe->result_type != ItemType::ShipComponent)
        item.slot = recipe->result_slot;
    else
        item.slot = std::nullopt; // ship components have no equip slot
    item.glyph = recipe->result_glyph;
    item.modifiers = recipe->base_modifiers;
    item.max_durability = recipe->base_durability;
    item.durability = recipe->base_durability;
    item.weight = 3;

    // Scale by player level
    scale_item_to_level(item, player.level);

    // Roll rarity influenced by Luck
    int luck_bonus = std::max(0, (player.attributes.luck - 10)) * 2;
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng) + luck_bonus;
    if (roll >= 99) item.rarity = Rarity::Legendary;
    else if (roll >= 95) item.rarity = Rarity::Epic;
    else if (roll >= 80) item.rarity = Rarity::Rare;
    else if (roll >= 50) item.rarity = Rarity::Uncommon;
    else item.rarity = Rarity::Common;

    item.color = rarity_color(item.rarity);
    init_enhancement_slots(item);

    // Set buy/sell based on rarity
    int rarity_mult = 1 + static_cast<int>(item.rarity);
    item.buy_value = 100 * rarity_mult;
    item.sell_value = item.buy_value / 3;

    std::string result_name = item.name;
    player.inventory.items.push_back(std::move(item));

    return {true, "Synthesized: " + result_name + "!"};
}

} // namespace astra
