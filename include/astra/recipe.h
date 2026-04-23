#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class RecipeCategory : uint8_t {
    Basic,      // starting repertoire
    Hearty,     // mid-tier, from common cookbooks
    Gourmet,    // rare cookbooks / future NPC teachers
    Survival,   // field cooking, future tier
};

const char* recipe_category_name(RecipeCategory c);

struct RecipeIngredient {
    uint16_t item_def_id = 0;
    int qty = 0;
};

struct Recipe {
    uint16_t id = 0;
    std::string name;
    std::string description;
    RecipeCategory category = RecipeCategory::Basic;
    std::vector<RecipeIngredient> ingredients;  // size 1..3
    uint16_t result_item_def_id = 0;
    int result_qty = 1;
};

// One "pot slot" in the cooking UI. The three-slot array is the bag
// we try to match against a recipe. A zero item_def_id means empty.
struct PotSlot {
    uint16_t item_def_id = 0;
    int qty = 0;
};

const std::vector<Recipe>& recipe_catalog();
const Recipe* find_recipe(uint16_t id);

// Order-independent, quantity-exact match. Returns nullptr if no
// recipe matches the slotted bag (caller produces Burnt Slop).
const Recipe* match_recipe(const std::array<PotSlot, 3>& slots);

} // namespace astra
