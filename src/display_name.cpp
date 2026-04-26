#include "astra/display_name.h"
#include "terminal_theme.h"

namespace astra {

namespace {

// Render a single mod-slot bracket. Empty slot => "[ ]" in dim. Filled slot
// => "[" + colored mod glyph + "]".
std::string render_slot(const EnhancementSlot& slot) {
    if (!slot.filled) {
        return colored("[ ]", Color::DarkGray);
    }
    auto vis = item_visual(static_cast<uint16_t>(slot.material_id));
    std::string glyph = vis.utf8 ? std::string(vis.utf8) : std::string(1, vis.glyph);
    return colored("[", Color::DarkGray)
         + colored(glyph, vis.fg)
         + colored("]", Color::DarkGray);
}

} // namespace

std::string display_name(const Item& item) {
    auto vis = item_visual(item.item_def_id);
    std::string glyph = vis.utf8 ? std::string(vis.utf8) : std::string(1, vis.glyph);
    std::string out = colored(glyph, vis.fg) + " "
                    + colored(item.name, rarity_color(item.rarity));

    // Mod slots (one bracket per declared slot)
    if (item.enhancement_slots > 0) {
        out += colored(" - ", Color::DarkGray);
        for (int i = 0; i < item.enhancement_slots; ++i) {
            if (i < static_cast<int>(item.enhancements.size())) {
                out += render_slot(item.enhancements[i]);
            } else {
                out += colored("[ ]", Color::DarkGray);
            }
        }
    }

    // Damage dice (weapons)
    if (!item.damage_dice.empty()) {
        out += colored(" - " + item.damage_dice.to_string(), Color::DarkGray);
    }

    // Energy current/capacity (cells, weapons, shields, powered accessories)
    if (item.energy) {
        Color charge_color = (item.energy->current > 0) ? Color::White : Color::Red;
        out += colored(" - ", Color::DarkGray)
             + colored(std::to_string(item.energy->current), charge_color)
             + colored("/", Color::DarkGray)
             + colored(std::to_string(item.energy->capacity), charge_color);
    }

    // Stack count for stackable items
    if (item.stackable && item.stack_count > 1) {
        out += colored(" x" + std::to_string(item.stack_count), Color::DarkGray);
    }

    return out;
}

} // namespace astra
