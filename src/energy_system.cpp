#include "astra/energy_system.h"

#include "astra/energy.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/world_manager.h"

#include <algorithm>

namespace astra {

namespace {

void tick_item(Item& item, int ticks) {
    if (!item.energy) return;
    if (is_full(*item.energy)) return;

    int rate_bonus_pct = 0;
    for (const auto& enh : item.enhancements)
        if (enh.committed) rate_bonus_pct += enh.energy_bonus.charge_rate_bonus;

    for (auto& enh : item.enhancements) {
        if (!enh.committed || !enh.solar_panel) continue;
        auto& sp = *enh.solar_panel;
        if (!sp.active) continue;

        sp.accumulator += ticks;
        while (sp.accumulator >= sp.tick_interval) {
            sp.accumulator -= sp.tick_interval;
            int deposit = sp.energy_per_tick + (sp.energy_per_tick * rate_bonus_pct) / 100;
            item.energy->current = std::min(item.energy->capacity,
                                            item.energy->current + deposit);
            if (is_full(*item.energy)) {
                sp.accumulator = 0;
                break;
            }
        }
    }
}

} // anon

void EnergySystem::tick(Player& player, const WorldManager& world, int ticks) {
    if (ticks <= 0) return;
    if (!world.is_outdoor()) return;

    for (auto& it : player.inventory.items) tick_item(it, ticks);

    auto sweep = [&](std::optional<Item>& slot) { if (slot) tick_item(*slot, ticks); };
    sweep(player.equipment.face);
    sweep(player.equipment.head);
    sweep(player.equipment.body);
    sweep(player.equipment.left_arm);
    sweep(player.equipment.right_arm);
    sweep(player.equipment.left_hand);
    sweep(player.equipment.right_hand);
    sweep(player.equipment.back);
    sweep(player.equipment.feet);
    sweep(player.equipment.thrown);
    sweep(player.equipment.missile);
    sweep(player.equipment.shield);
}

} // namespace astra
