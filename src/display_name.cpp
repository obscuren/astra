#include "astra/display_name.h"
#include "terminal_theme.h"

namespace astra {

std::string display_name(const Item& item) {
    auto vis = item_visual(item.item_def_id);
    std::string glyph = vis.utf8 ? std::string(vis.utf8) : std::string(1, vis.glyph);
    std::string out = colored(glyph + " " + item.name, rarity_color(item.rarity));
    if (!item.damage_dice.empty()) {
        out += colored(" - " + item.damage_dice.to_string(), Color::DarkGray);
    }
    return out;
}

} // namespace astra
