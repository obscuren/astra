#include "astra/cyberdeck_mods.h"

#include "astra/item.h"
#include "astra/item_ids.h"
#include "astra/player.h"

namespace astra {

bool CyberdeckMods::wireless_jackin_installed(const Player& player) {
    for (const auto& it : player.inventory.items) {
        if (it.item_def_id == ITEM_AEROJACK ||
            it.item_def_id == ITEM_UNTETHER) {
            return true;
        }
    }
    return false;
}

} // namespace astra
