#include "astra/program_recipes.h"

#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/player.h"
#include "astra/skill_defs.h"

#include <string>

namespace astra {

namespace {
bool item_matches_material(const Item& it, uint32_t material_id) {
    return it.id == material_id || it.item_def_id == material_id;
}
} // namespace

const std::vector<ProgramRecipe>& program_recipes() {
    static const std::vector<ProgramRecipe> recipes = {
        // T1 — base programs.
        { 1, 9100, ITEM_PROG_ICEBREAKER_LITE,
              "icebreaker_lite.exe", "ATK program. Light cracker for white ICE.",
              { {7100, 2}, {7011, 1}, {7003, 1} }, 1 },
        { 2, 9104, ITEM_PROG_DECRYPT,
              "decrypt.exe", "UTL program. Reads one encrypted file.",
              { {7100, 1}, {7012, 1}, {7003, 1} }, 1 },
        { 3, 9105, ITEM_PROG_REBOOT_OPTICS,
              "reboot_optics.qh", "QH program. Blinds a camera or turret for 4 turns.",
              { {7100, 1}, {7010, 1}, {31, 1} }, 1 },
        // T3 — gated behind CodeCraft skill.
        { 4, 9108, ITEM_PROG_PULSE_HAMMER,
              "pulse_hammer.exe", "ATK T3 program. AoE 1d6 dmg to all ICE adjacent to target tile.",
              { {7102, 2}, {7003, 1} }, 1,
              SkillId::CodeCraft },
        { 5, 9109, ITEM_PROG_DAEMON_HIJACK,
              "daemon_hijack.exe", "UTL T3 program. Take control of one ICE for 3 turns.",
              { {7102, 3}, {7003, 1} }, 1,
              SkillId::CodeCraft },
    };
    return recipes;
}

const ProgramRecipe* find_program_recipe(uint16_t recipe_id) {
    for (const auto& r : program_recipes())
        if (r.recipe_id == recipe_id) return &r;
    return nullptr;
}

TinkerResult learn_program_recipe(Player& player, uint16_t recipe_id,
                                  const char* name, const char* description) {
    for (const auto& lp : player.learned_programs) {
        if (lp.recipe_id == recipe_id)
            return {false, false, std::string("You already know ") + name + "."};
    }
    player.learned_programs.push_back({ recipe_id, name, description ? description : "" });
    return {true, true, std::string("Learned program recipe: ") + name + "."};
}

TinkerResult craft_program_recipe(uint16_t recipe_id, Player& player) {
    if (!player_has_skill(player, SkillId::Cat_Hacking))
        return {false, false, "Requires Hacking skill unlocked."};

    bool known = false;
    for (const auto& lp : player.learned_programs)
        if (lp.recipe_id == recipe_id) { known = true; break; }
    if (!known)
        return {false, false, "Program recipe not learned."};

    const ProgramRecipe* recipe = find_program_recipe(recipe_id);
    if (!recipe)
        return {false, false, "Unknown program recipe."};

    if (static_cast<uint32_t>(recipe->skill_gate) != 0 &&
        !player_has_skill(player, recipe->skill_gate))
        return {false, false, "Missing required skill to compile this program."};

    for (const auto& req : recipe->material_costs) {
        int have = 0;
        for (const auto& it : player.inventory.items)
            if (item_matches_material(it, req.material_id)) have += it.stack_count;
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    for (const auto& req : recipe->material_costs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin();
             it != player.inventory.items.end() && needed > 0; ) {
            if (item_matches_material(*it, req.material_id)) {
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
        }
    }

    for (int i = 0; i < recipe->output_count; ++i) {
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == recipe->output_id) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            Item out = build_by_def_id(recipe->output_def_id);
            player.inventory.items.push_back(std::move(out));
        }
    }

    return {true, false, std::string("Compiled: ") + recipe->output_name + "."};
}

} // namespace astra
