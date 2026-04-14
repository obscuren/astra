#include "astra/display_name.h"
#include "terminal_theme.h"

namespace astra {

std::string display_name(const Item& item) {
    auto vis = item_visual(item.item_def_id);
    std::string glyph = vis.utf8 ? std::string(vis.utf8) : std::string(1, vis.glyph);
    return colored(glyph + " " + item.label(), rarity_color(item.rarity));
}

} // namespace astra
