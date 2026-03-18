#include "astra/item.h"

namespace astra {

std::optional<Item>& Equipment::slot_ref(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Head:         return head;
        case EquipSlot::Chest:        return chest;
        case EquipSlot::Legs:         return legs;
        case EquipSlot::Feet:         return feet;
        case EquipSlot::Hands:        return hands;
        case EquipSlot::MeleeWeapon:  return melee_weapon;
        case EquipSlot::RangedWeapon: return ranged_weapon;
        case EquipSlot::SpecialSlot:  return special_slot;
    }
    return head; // unreachable
}

const std::optional<Item>& Equipment::slot_ref(EquipSlot slot) const {
    switch (slot) {
        case EquipSlot::Head:         return head;
        case EquipSlot::Chest:        return chest;
        case EquipSlot::Legs:         return legs;
        case EquipSlot::Feet:         return feet;
        case EquipSlot::Hands:        return hands;
        case EquipSlot::MeleeWeapon:  return melee_weapon;
        case EquipSlot::RangedWeapon: return ranged_weapon;
        case EquipSlot::SpecialSlot:  return special_slot;
    }
    return head; // unreachable
}

StatModifiers Equipment::total_modifiers() const {
    StatModifiers total;
    const std::optional<Item>* slots[] = {
        &head, &chest, &legs, &feet,
        &hands, &melee_weapon, &ranged_weapon, &special_slot
    };
    for (const auto* s : slots) {
        if (*s) {
            total.attack += (*s)->modifiers.attack;
            total.defense += (*s)->modifiers.defense;
            total.max_hp += (*s)->modifiers.max_hp;
            total.view_radius += (*s)->modifiers.view_radius;
            total.quickness += (*s)->modifiers.quickness;
        }
    }
    return total;
}

int Inventory::total_weight() const {
    int w = 0;
    for (const auto& item : items) {
        w += item.weight * item.stack_count;
    }
    return w;
}

bool Inventory::can_add(const Item& item) const {
    return total_weight() + item.weight * item.stack_count <= max_carry_weight;
}

} // namespace astra
