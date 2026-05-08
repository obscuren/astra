#pragma once

#include "astra/skill_defs.h"
#include "astra/tinkering.h"   // MaterialReq

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

class Player;

// Outcome of a program-recipe operation (learn, compile, ...). Independent
// from TinkerResult — the program system stands on its own even though the
// shape happens to be identical today.
struct ProgramRecipeResult {
    bool success = false;        // did the action achieve its intent?
    bool consumed = false;       // should the caller remove the input item?
    std::string message;
};

// A craftable cyberdeck program. The player must have learned the recipe
// (learn_program_recipe) and pay the material cost; T3 recipes require the
// CodeCraft skill in addition to Cat_Hacking. Crafting UI lives on the
// Cyberdeck tab (TODO).
struct ProgramRecipe {
    uint16_t recipe_id     = 0;     // unique within program_recipes()
    uint32_t output_id     = 0;     // Item::id of the output program
    uint16_t output_def_id = 0;     // for build_by_def_id
    const char* output_name = "";
    const char* output_desc = "";
    std::vector<MaterialReq> material_costs;
    int output_count = 1;
    SkillId skill_gate = static_cast<SkillId>(0);   // 0 = no extra gate
};

// Per-player record of a learned program recipe.
struct LearnedProgramRecipe {
    uint16_t recipe_id = 0;
    std::string name;
    std::string description;
};

const std::vector<ProgramRecipe>& program_recipes();
const ProgramRecipe* find_program_recipe(uint16_t recipe_id);

// Append to player.learned_programs. Idempotent: returns success=false
// when already known.
ProgramRecipeResult learn_program_recipe(Player& player, uint16_t recipe_id,
                                         const char* name, const char* description);

// Validates Cat_Hacking + learned + skill_gate + materials, then consumes
// inputs and adds the output to the player's inventory.
ProgramRecipeResult compile_program_recipe(uint16_t recipe_id, Player& player);

} // namespace astra
