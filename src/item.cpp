#include "astra/item.h"

namespace astra {

const char* item_type_name(ItemType t) {
    switch (t) {
        case ItemType::Equipment:        return "Equipment";
        case ItemType::Trash:            return "Trash";
        case ItemType::Credits:          return "Credits";
        case ItemType::Food:             return "Food";
        case ItemType::Stim:             return "Stims";
        case ItemType::Battery:          return "Batteries";
        case ItemType::Light:            return "Light Sources";
        case ItemType::Special:          return "Special";
        case ItemType::MeleeWeapon:      return "Melee Weapons";
        case ItemType::RangedWeapon:     return "Ranged Weapons";
        case ItemType::Armor:            return "Armor";
        case ItemType::Shield:           return "Shields";
        case ItemType::Accessory:        return "Accessories";
        case ItemType::Grenade:          return "Grenades";
        case ItemType::Junk:             return "Junk";
        case ItemType::CraftingMaterial: return "Crafting Materials";
        case ItemType::ShipComponent:    return "Ship Components";
        case ItemType::QuestItem:        return "Quest Items";
    }
    return "Unknown";
}

const char* equip_slot_name(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Face:      return "Face";
        case EquipSlot::Head:      return "Head";
        case EquipSlot::Body:      return "Body";
        case EquipSlot::LeftArm:   return "L.Arm";
        case EquipSlot::RightArm:  return "R.Arm";
        case EquipSlot::LeftHand:  return "L.Hand";
        case EquipSlot::RightHand: return "R.Hand";
        case EquipSlot::Back:      return "Back";
        case EquipSlot::Feet:      return "Feet";
        case EquipSlot::Thrown:    return "Thrown";
        case EquipSlot::Missile:   return "Missile";
        case EquipSlot::Shield:    return "Shield";
    }
    return "?";
}

std::optional<Item>& Equipment::slot_ref(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Face:      return face;
        case EquipSlot::Head:      return head;
        case EquipSlot::Body:      return body;
        case EquipSlot::LeftArm:   return left_arm;
        case EquipSlot::RightArm:  return right_arm;
        case EquipSlot::LeftHand:  return left_hand;
        case EquipSlot::RightHand: return right_hand;
        case EquipSlot::Back:      return back;
        case EquipSlot::Feet:      return feet;
        case EquipSlot::Thrown:    return thrown;
        case EquipSlot::Missile:   return missile;
        case EquipSlot::Shield:    return shield;
    }
    return head; // unreachable
}

const std::optional<Item>& Equipment::slot_ref(EquipSlot slot) const {
    switch (slot) {
        case EquipSlot::Face:      return face;
        case EquipSlot::Head:      return head;
        case EquipSlot::Body:      return body;
        case EquipSlot::LeftArm:   return left_arm;
        case EquipSlot::RightArm:  return right_arm;
        case EquipSlot::LeftHand:  return left_hand;
        case EquipSlot::RightHand: return right_hand;
        case EquipSlot::Back:      return back;
        case EquipSlot::Feet:      return feet;
        case EquipSlot::Thrown:    return thrown;
        case EquipSlot::Missile:   return missile;
        case EquipSlot::Shield:    return shield;
    }
    return head; // unreachable
}

StatModifiers Equipment::total_modifiers() const {
    StatModifiers total;
    const std::optional<Item>* slots[] = {
        &face, &head, &body, &left_arm, &right_arm,
        &left_hand, &right_hand, &back, &feet,
        &thrown, &missile, &shield,
    };
    for (const auto* s : slots) {
        if (*s) {
            total.av += (*s)->modifiers.av;
            total.dv += (*s)->modifiers.dv;
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
