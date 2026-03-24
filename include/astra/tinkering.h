#pragma once

#include "astra/item.h"
#include "astra/skill_defs.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

struct Player; // forward declare

// Blueprint learned from analyzing an item
struct BlueprintSignature {
    uint32_t source_item_id = 0;
    std::string name;
    std::string description;
};

// Material effect when slotted as enhancement
struct MaterialEffect {
    uint32_t material_id;
    const char* name;
    StatModifiers bonus;
};

// Get the enhancement bonus for a crafting material
const MaterialEffect* get_material_effect(uint32_t material_id);

// Blueprint signatures available per item category
struct BlueprintEntry {
    ItemType category;
    const char* name;
    const char* description;
};
const std::vector<BlueprintEntry>& blueprint_catalog();

// --- Tinkering actions ---

struct TinkerResult {
    bool success = false;
    std::string message;
};

// Repair: restore durability using Nano-Fiber
int repair_cost(const Item& item);  // Nano-Fiber needed
TinkerResult repair_item(Item& item, Player& player);

// Enhance: slot a crafting material into an enhancement slot
TinkerResult enhance_item(Item& item, int slot_index, uint32_t material_id, Player& player);

// Analyze: learn a blueprint from an item (may destroy it)
TinkerResult analyze_item(Item& item, Player& player, std::mt19937& rng);

// Salvage: destroy item, receive crafting materials
TinkerResult salvage_item(const Item& item, Player& player, std::mt19937& rng);

// Check if player has a specific skill
bool player_has_skill(const Player& player, SkillId id);

// Initialize enhancement_slots based on rarity (call on item creation)
void init_enhancement_slots(Item& item);

// --- Synthesizer ---

struct SynthesisRecipe {
    const char* blueprint_1;
    const char* blueprint_2;
    const char* result_name;
    const char* result_desc;
    ItemType result_type;
    EquipSlot result_slot;
    char result_glyph;
    StatModifiers base_modifiers;
    int base_durability;
    int material_cost[4]; // [0]=Nano-Fiber, [1]=Power Core, [2]=Circuit Board, [3]=Alloy Ingot
};

const std::vector<SynthesisRecipe>& synthesis_recipes();
const SynthesisRecipe* find_recipe(const std::string& bp1, const std::string& bp2);
TinkerResult synthesize_item(const std::string& bp1, const std::string& bp2,
                              Player& player, std::mt19937& rng);

} // namespace astra
