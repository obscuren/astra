#pragma once

#include <cstdint>
#include <vector>

namespace astra {

// A typed quantity of crafting material. Lives in its own header so
// recipe systems (Tinkering, ProgramRecipe, ...) can share the type
// without dragging in the full Tinkering surface.
struct MaterialReq {
    uint32_t material_id = 0;   // == Item::id used for inventory stack matching
    int count = 0;
};

enum class MaterialTier : uint8_t {
    Common = 1,
    Uncommon = 2,
    Rare = 3,
};

struct MaterialDef {
    uint32_t material_id = 0;       // Item::id
    const char* name = "";
    MaterialTier tier = MaterialTier::Common;
    char glyph = '+';
    uint8_t color = 0;              // Color enum value
    int sell_value = 0;
    bool is_junk_typed = false;     // true for Scrap, Broken Circuit, etc.
};

const std::vector<MaterialDef>& material_catalog();
const MaterialDef* find_material(uint32_t material_id);

} // namespace astra
