#include "astra/recipe.h"

#include <algorithm>
#include "astra/item_ids.h"

namespace astra {

const char* recipe_category_name(RecipeCategory c) {
    switch (c) {
        case RecipeCategory::Basic:    return "Basic";
        case RecipeCategory::Hearty:   return "Hearty";
        case RecipeCategory::Gourmet:  return "Gourmet";
        case RecipeCategory::Survival: return "Survival";
    }
    return "Unknown";
}

const std::vector<Recipe>& recipe_catalog() {
    static const std::vector<Recipe> catalog = {
        {
            1,
            "Cooked Meat",
            "Meat cooked over a fire. Simple and satisfying.",
            RecipeCategory::Basic,
            { { ITEM_RAW_MEAT, 1 } },
            ITEM_COOKED_MEAT,
            1,
        },
        {
            2,
            "Warm Broth",
            "A simple vegetable broth. Warms the bones.",
            RecipeCategory::Basic,
            { { ITEM_CARROT, 1 }, { ITEM_HERBS, 1 } },
            ITEM_BOWL_OF_BROTH,
            1,
        },
        {
            3,
            "Campfire Bread",
            "Flatbread cooked on a hot stone.",
            RecipeCategory::Basic,
            { { ITEM_FLOUR, 2 } },
            ITEM_FLATBREAD,
            1,
        },
        {
            10,
            "Hearty Stew",
            "Meat, roots, and herbs simmered into a thick stew.",
            RecipeCategory::Hearty,
            { { ITEM_RAW_MEAT, 2 }, { ITEM_CARROT, 1 }, { ITEM_HERBS, 1 } },
            ITEM_HEARTY_STEW,
            1,
        },
        {
            11,
            "Protein Bake",
            "Synth-protein baked into a filling loaf.",
            RecipeCategory::Hearty,
            { { ITEM_SYNTH_PROTEIN, 2 }, { ITEM_FLOUR, 1 } },
            ITEM_PROTEIN_BAKE,
            1,
        },
        {
            12,
            "Hero's Feast",
            "A legendary dish said to steel a warrior for any trial.",
            RecipeCategory::Gourmet,
            { { ITEM_RAW_MEAT, 1 }, { ITEM_HERBS, 1 }, { ITEM_SYNTH_PROTEIN, 1 } },
            ITEM_HEROS_FEAST,
            1,
        },
    };
    return catalog;
}

const Recipe* find_recipe(uint16_t id) {
    for (const auto& r : recipe_catalog()) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

const Recipe* match_recipe(const std::array<PotSlot, 3>& slots) {
    // Canonicalise the slot bag: drop empties, sort by item_def_id.
    // UI prevents duplicate item_def_ids across slots, so we do not
    // merge duplicates here.
    std::vector<RecipeIngredient> bag;
    bag.reserve(3);
    for (const auto& s : slots) {
        if (s.item_def_id == 0 || s.qty <= 0) continue;
        bag.push_back({s.item_def_id, s.qty});
    }
    if (bag.empty()) return nullptr;
    std::sort(bag.begin(), bag.end(),
              [](const RecipeIngredient& a, const RecipeIngredient& b) {
                  return a.item_def_id < b.item_def_id;
              });

    for (const auto& r : recipe_catalog()) {
        if (r.ingredients.size() != bag.size()) continue;

        std::vector<RecipeIngredient> ri(r.ingredients.begin(),
                                         r.ingredients.end());
        std::sort(ri.begin(), ri.end(),
                  [](const RecipeIngredient& a, const RecipeIngredient& b) {
                      return a.item_def_id < b.item_def_id;
                  });

        bool match = true;
        for (size_t i = 0; i < bag.size(); ++i) {
            if (ri[i].item_def_id != bag[i].item_def_id ||
                ri[i].qty != bag[i].qty) {
                match = false;
                break;
            }
        }
        if (match) return &r;
    }
    return nullptr;
}

} // namespace astra
