#include "astra/item_defs.h"
#include "astra/dice.h"
#include "astra/item_ids.h"

namespace astra {

// ---------------------------------------------------------------------------
// Ranged weapons
// ---------------------------------------------------------------------------

Item build_plasma_pistol() {
    Item it;
    it.item_def_id = ITEM_PLASMA_PISTOL;
    it.id = 1001;
    it.name = "Plasma Pistol";
    it.description = "Standard-issue sidearm. Fires superheated plasma bolts.";
    it.type = ItemType::RangedWeapon;
    it.weapon_class = WeaponClass::Pistol;
    it.slot = EquipSlot::Missile;
    it.rarity = Rarity::Common;
    it.weight = 3;
    it.buy_value = 120;
    it.sell_value = 40;
    it.damage_dice = Dice::make(1, 6);
    it.damage_type = DamageType::Plasma;
    it.max_durability = 80;
    it.durability = 80;
    it.ranged = RangedData{20, 1, 20, 6};
    return it;
}

Item build_ion_blaster() {
    Item it;
    it.item_def_id = ITEM_ION_BLASTER;
    it.id = 1002;
    it.name = "Ion Blaster";
    it.description = "Disrupts electronics and shields with ionized electrical bursts.";
    it.type = ItemType::RangedWeapon;
    it.weapon_class = WeaponClass::Pistol;
    it.slot = EquipSlot::Missile;
    it.rarity = Rarity::Uncommon;
    it.weight = 4;
    it.buy_value = 250;
    it.sell_value = 85;
    it.damage_dice = Dice::make(1, 8, 1);
    it.damage_type = DamageType::Electrical;
    it.max_durability = 60;
    it.durability = 60;
    it.ranged = RangedData{15, 2, 15, 8};
    return it;
}

Item build_pulse_rifle() {
    Item it;
    it.item_def_id = ITEM_PULSE_RIFLE;
    it.id = 1003;
    it.name = "Pulse Rifle";
    it.description = "Military-grade kinetic rifle with rapid energy pulses.";
    it.type = ItemType::RangedWeapon;
    it.weapon_class = WeaponClass::Rifle;
    it.slot = EquipSlot::Missile;
    it.rarity = Rarity::Rare;
    it.weight = 6;
    it.buy_value = 500;
    it.sell_value = 170;
    it.damage_dice = Dice::make(2, 6);
    it.damage_type = DamageType::Kinetic;
    it.modifiers.quickness = -5;
    it.max_durability = 100;
    it.durability = 100;
    it.ranged = RangedData{30, 2, 30, 12};
    return it;
}

Item build_arc_caster() {
    Item it;
    it.item_def_id = ITEM_ARC_CASTER;
    it.id = 1004;
    it.name = "Arc Caster";
    it.description = "Channels electricity in a devastating arc. Unstable.";
    it.type = ItemType::RangedWeapon;
    it.weapon_class = WeaponClass::Rifle;
    it.slot = EquipSlot::Missile;
    it.rarity = Rarity::Epic;
    it.weight = 5;
    it.buy_value = 900;
    it.sell_value = 300;
    it.damage_dice = Dice::make(2, 8, 1);
    it.damage_type = DamageType::Electrical;
    it.modifiers.quickness = -10;
    it.max_durability = 50;
    it.durability = 50;
    it.ranged = RangedData{12, 3, 12, 5};
    return it;
}

Item build_void_lance() {
    Item it;
    it.item_def_id = ITEM_VOID_LANCE;
    it.id = 1005;
    it.name = "Void Lance";
    it.description = "Fires a beam of compressed plasma dark energy. Extremely rare.";
    it.type = ItemType::RangedWeapon;
    it.weapon_class = WeaponClass::Rifle;
    it.slot = EquipSlot::Missile;
    it.rarity = Rarity::Legendary;
    it.weight = 7;
    it.buy_value = 2500;
    it.sell_value = 800;
    it.damage_dice = Dice::make(3, 8, 2);
    it.damage_type = DamageType::Plasma;
    it.modifiers.view_radius = 2;
    it.max_durability = 40;
    it.durability = 40;
    it.ranged = RangedData{10, 4, 10, 15};
    return it;
}

// ---------------------------------------------------------------------------
// Consumables
// ---------------------------------------------------------------------------

Item build_battery() {
    Item it;
    it.item_def_id = ITEM_BATTERY;
    it.id = 2001;
    it.name = "Energy Cell";
    it.description = "Standard power cell. Recharges ranged weapons.";
    it.type = ItemType::Battery;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 15;
    it.sell_value = 5;
    it.usable = true;
    return it;
}

Item build_ration_pack() {
    Item it;
    it.item_def_id = ITEM_RATION_PACK;
    it.id = 2002;
    it.name = "Ration Pack";
    it.description = "Compact nutrient paste. Restores hunger.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 10;
    it.sell_value = 3;
    it.usable = true;
    return it;
}

Item build_combat_stim() {
    Item it;
    it.item_def_id = ITEM_COMBAT_STIM;
    it.id = 2003;
    it.name = "Combat Stim";
    it.description = "Adrenaline injection. Temporarily boosts attack.";
    it.type = ItemType::Stim;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 50;
    it.sell_value = 18;
    it.usable = true;
    return it;
}

// ---------------------------------------------------------------------------
// Melee weapons
// ---------------------------------------------------------------------------

Item build_combat_knife() {
    Item it;
    it.item_def_id = ITEM_COMBAT_KNIFE;
    it.id = 1101; it.name = "Combat Knife"; it.type = ItemType::MeleeWeapon; it.weapon_class = WeaponClass::ShortBlade;
    it.description = "A short, serrated kinetic blade. Fast and deadly at close range.";
    it.slot = EquipSlot::RightHand; it.rarity = Rarity::Common;
    it.weight = 2;
    it.buy_value = 60; it.sell_value = 20;
    it.damage_dice = Dice::make(1, 4);
    it.damage_type = DamageType::Kinetic;
    it.max_durability = 60; it.durability = 60;
    return it;
}

Item build_vibro_blade() {
    Item it;
    it.item_def_id = ITEM_VIBRO_BLADE;
    it.id = 1102; it.name = "Vibro Blade"; it.type = ItemType::MeleeWeapon; it.weapon_class = WeaponClass::ShortBlade;
    it.description = "A high-frequency vibrating kinetic blade that cuts through armor.";
    it.slot = EquipSlot::RightHand; it.rarity = Rarity::Uncommon;
    it.weight = 3;
    it.buy_value = 180; it.sell_value = 60;
    it.damage_dice = Dice::make(1, 6, 1);
    it.damage_type = DamageType::Kinetic;
    it.max_durability = 50; it.durability = 50;
    return it;
}

Item build_plasma_saber() {
    Item it;
    it.item_def_id = ITEM_PLASMA_SABER;
    it.id = 1103; it.name = "Plasma Saber"; it.type = ItemType::MeleeWeapon; it.weapon_class = WeaponClass::LongBlade;
    it.description = "A long blade wreathed in superheated plasma. Devastating.";
    it.slot = EquipSlot::RightHand; it.rarity = Rarity::Rare;
    it.weight = 4;
    it.buy_value = 400; it.sell_value = 135;
    it.damage_dice = Dice::make(2, 4, 2);
    it.damage_type = DamageType::Plasma;
    it.max_durability = 40; it.durability = 40;
    return it;
}

Item build_stun_baton() {
    Item it;
    it.item_def_id = ITEM_STUN_BATON;
    it.id = 1104; it.name = "Stun Baton"; it.type = ItemType::MeleeWeapon; it.weapon_class = WeaponClass::LongBlade;
    it.description = "An electrified baton. Slow but stuns on hit with electrical damage.";
    it.slot = EquipSlot::RightHand; it.rarity = Rarity::Common;
    it.weight = 3;
    it.buy_value = 80; it.sell_value = 25;
    it.damage_dice = Dice::make(1, 4, 1);
    it.damage_type = DamageType::Electrical;
    it.modifiers.quickness = 5; it.max_durability = 70; it.durability = 70;
    return it;
}

Item build_ancient_mono_edge() {
    Item it;
    it.item_def_id = ITEM_ANCIENT_MONO_EDGE;
    it.id = 1105; it.name = "Ancient Mono-Edge"; it.type = ItemType::MeleeWeapon; it.weapon_class = WeaponClass::LongBlade;
    it.description = "A relic kinetic blade from a lost civilization. Its molecular edge never dulls.";
    it.slot = EquipSlot::RightHand; it.rarity = Rarity::Epic;
    it.weight = 2;
    it.buy_value = 1200; it.sell_value = 400;
    it.damage_dice = Dice::make(2, 6, 2);
    it.damage_type = DamageType::Kinetic;
    it.max_durability = 200; it.durability = 200;
    return it;
}

// ---------------------------------------------------------------------------
// Armor
// ---------------------------------------------------------------------------

Item build_padded_vest() {
    Item it;
    it.item_def_id = ITEM_PADDED_VEST;
    it.id = 3001; it.name = "Padded Vest"; it.type = ItemType::Armor;
    it.description = "Basic torso protection. Better than nothing.";
    it.slot = EquipSlot::Body; it.rarity = Rarity::Common;
    it.weight = 4;
    it.buy_value = 80; it.sell_value = 25; it.modifiers.av = 2;
    it.type_affinity = {1, 0, 0, 0, -1};
    it.max_durability = 50; it.durability = 50;
    return it;
}

Item build_composite_armor() {
    Item it;
    it.item_def_id = ITEM_COMPOSITE_ARMOR;
    it.id = 3002; it.name = "Composite Armor"; it.type = ItemType::Armor;
    it.description = "Layered ceramic-polymer plates. Standard military issue.";
    it.slot = EquipSlot::Body; it.rarity = Rarity::Uncommon;
    it.weight = 8;
    it.buy_value = 250; it.sell_value = 85; it.modifiers.av = 4; it.modifiers.dv = -1;
    it.type_affinity = {2, -1, 0, 0, -2};
    it.max_durability = 80; it.durability = 80;
    return it;
}

Item build_exo_suit() {
    Item it;
    it.item_def_id = ITEM_EXO_SUIT;
    it.id = 3003; it.name = "Exo-Suit"; it.type = ItemType::Armor;
    it.description = "Powered exoskeleton with integrated armor plating.";
    it.slot = EquipSlot::Body; it.rarity = Rarity::Rare;
    it.weight = 12;
    it.buy_value = 600; it.sell_value = 200; it.modifiers.av = 6; it.modifiers.dv = -2;
    it.type_affinity = {1, 1, -2, 1, 0};
    it.modifiers.max_hp = 3; it.max_durability = 120; it.durability = 120;
    return it;
}

Item build_flight_helmet() {
    Item it;
    it.item_def_id = ITEM_FLIGHT_HELMET;
    it.id = 3004; it.name = "Flight Helmet"; it.type = ItemType::Armor;
    it.description = "Lightweight helmet with a tinted visor.";
    it.slot = EquipSlot::Head; it.rarity = Rarity::Common;
    it.weight = 2;
    it.buy_value = 50; it.sell_value = 15; it.modifiers.av = 1;
    it.max_durability = 40; it.durability = 40;
    return it;
}

Item build_tactical_helmet() {
    Item it;
    it.item_def_id = ITEM_TACTICAL_HELMET;
    it.id = 3005; it.name = "Tactical Helmet"; it.type = ItemType::Armor;
    it.description = "Ballistic-rated helmet with HUD overlay.";
    it.slot = EquipSlot::Head; it.rarity = Rarity::Uncommon;
    it.weight = 3;
    it.buy_value = 150; it.sell_value = 50; it.modifiers.av = 2;
    it.type_affinity = {1, 0, 0, -1, 0};
    it.modifiers.view_radius = 1; it.max_durability = 60; it.durability = 60;
    return it;
}

Item build_combat_boots() {
    Item it;
    it.item_def_id = ITEM_COMBAT_BOOTS;
    it.id = 3006; it.name = "Combat Boots"; it.type = ItemType::Armor;
    it.description = "Sturdy boots with reinforced soles.";
    it.slot = EquipSlot::Feet; it.rarity = Rarity::Common;
    it.weight = 3;
    it.buy_value = 60; it.sell_value = 20; it.modifiers.av = 1;
    it.max_durability = 50; it.durability = 50;
    return it;
}

Item build_mag_lock_boots() {
    Item it;
    it.item_def_id = ITEM_MAG_LOCK_BOOTS;
    it.id = 3007; it.name = "Mag-Lock Boots"; it.type = ItemType::Armor;
    it.description = "Magnetic boots for zero-G traversal. Surprisingly agile.";
    it.slot = EquipSlot::Feet; it.rarity = Rarity::Uncommon;
    it.weight = 4;
    it.buy_value = 120; it.sell_value = 40; it.modifiers.av = 1;
    it.modifiers.quickness = 3; it.max_durability = 60; it.durability = 60;
    return it;
}

Item build_arm_guard() {
    Item it;
    it.item_def_id = ITEM_ARM_GUARD;
    it.id = 3008; it.name = "Arm Guard"; it.type = ItemType::Armor;
    it.description = "Lightweight forearm protector.";
    it.slot = EquipSlot::LeftArm; it.rarity = Rarity::Common;
    it.weight = 2;
    it.buy_value = 40; it.sell_value = 12; it.modifiers.av = 1;
    it.max_durability = 40; it.durability = 40;
    return it;
}

// ---------------------------------------------------------------------------
// Energy shields
// ---------------------------------------------------------------------------

Item build_basic_deflector() {
    Item it;
    it.item_def_id = ITEM_BASIC_DEFLECTOR;
    it.id = 3100; it.name = "Basic Deflector"; it.type = ItemType::Shield;
    it.description = "Entry-level energy shield. Absorbs a small amount of damage.";
    it.slot = EquipSlot::Shield; it.rarity = Rarity::Common;
    it.weight = 2;
    it.buy_value = 100; it.sell_value = 35;
    it.shield_capacity = 10; it.shield_hp = 10;
    it.type_affinity = {0, 0, 0, 0, 0};
    return it;
}

Item build_plasma_screen() {
    Item it;
    it.item_def_id = ITEM_PLASMA_SCREEN;
    it.id = 3101; it.name = "Plasma Screen"; it.type = ItemType::Shield;
    it.description = "Tuned to deflect plasma-based attacks.";
    it.slot = EquipSlot::Shield; it.rarity = Rarity::Uncommon;
    it.weight = 3;
    it.buy_value = 250; it.sell_value = 85;
    it.shield_capacity = 15; it.shield_hp = 15;
    it.type_affinity = {0, 3, 0, 0, -1};
    return it;
}

Item build_ion_barrier() {
    Item it;
    it.item_def_id = ITEM_ION_BARRIER;
    it.id = 3102; it.name = "Ion Barrier"; it.type = ItemType::Shield;
    it.description = "Disperses electrical and ion-based damage.";
    it.slot = EquipSlot::Shield; it.rarity = Rarity::Uncommon;
    it.weight = 3;
    it.buy_value = 250; it.sell_value = 85;
    it.shield_capacity = 15; it.shield_hp = 15;
    it.type_affinity = {0, -1, 3, 0, 0};
    return it;
}

Item build_composite_barrier() {
    Item it;
    it.item_def_id = ITEM_COMPOSITE_BARRIER;
    it.id = 3103; it.name = "Composite Barrier"; it.type = ItemType::Shield;
    it.description = "Balanced shield with moderate resistance across damage types.";
    it.slot = EquipSlot::Shield; it.rarity = Rarity::Rare;
    it.weight = 4;
    it.buy_value = 500; it.sell_value = 170;
    it.shield_capacity = 20; it.shield_hp = 20;
    it.type_affinity = {1, 1, 1, 1, 1};
    return it;
}

Item build_hardlight_aegis() {
    Item it;
    it.item_def_id = ITEM_HARDLIGHT_AEGIS;
    it.id = 3104; it.name = "Hardlight Aegis"; it.type = ItemType::Shield;
    it.description = "Projects a hardened light barrier. Excellent kinetic defense.";
    it.slot = EquipSlot::Shield; it.rarity = Rarity::Epic;
    it.weight = 3;
    it.buy_value = 900; it.sell_value = 300;
    it.shield_capacity = 30; it.shield_hp = 30;
    it.type_affinity = {3, 1, -1, 0, 0};
    return it;
}

Item build_void_mantle() {
    Item it;
    it.item_def_id = ITEM_VOID_MANTLE;
    it.id = 3105; it.name = "Void Mantle"; it.type = ItemType::Shield;
    it.description = "Ancient technology that bends space around the wearer.";
    it.slot = EquipSlot::Shield; it.rarity = Rarity::Legendary;
    it.weight = 2;
    it.buy_value = 2500; it.sell_value = 800;
    it.shield_capacity = 40; it.shield_hp = 40;
    it.type_affinity = {2, 2, 2, 2, 2};
    return it;
}

Item random_shield(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);
    if (roll < 35) return build_basic_deflector();     // 35%
    if (roll < 55) return build_plasma_screen();        // 20%
    if (roll < 75) return build_ion_barrier();           // 20%
    if (roll < 90) return build_composite_barrier();     // 15%
    if (roll < 97) return build_hardlight_aegis();       //  7%
    return build_void_mantle();                          //  3%
}

// ---------------------------------------------------------------------------
// Accessories
// ---------------------------------------------------------------------------

Item build_recon_visor() {
    Item it;
    it.item_def_id = ITEM_RECON_VISOR;
    it.id = 4001; it.name = "Recon Visor"; it.type = ItemType::Accessory;
    it.description = "Enhanced optics with thermal overlay. Extends vision range.";
    it.slot = EquipSlot::Face; it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.buy_value = 200; it.sell_value = 65; it.modifiers.view_radius = 2;
    return it;
}

Item build_night_goggles() {
    Item it;
    it.item_def_id = ITEM_NIGHT_GOGGLES;
    it.id = 4002; it.name = "Night Goggles"; it.type = ItemType::Accessory;
    it.description = "Amplifies ambient light. Useful in dark environments.";
    it.slot = EquipSlot::Face; it.rarity = Rarity::Common;
    it.weight = 1;
    it.buy_value = 80; it.sell_value = 25; it.modifiers.view_radius = 1;
    return it;
}

Item build_jetpack() {
    Item it;
    it.item_def_id = ITEM_JETPACK;
    it.id = 4003; it.name = "Jetpack"; it.type = ItemType::Accessory;
    it.description = "Compact thruster pack. Greatly increases movement speed.";
    it.slot = EquipSlot::Back; it.rarity = Rarity::Rare;
    it.weight = 5;
    it.buy_value = 500; it.sell_value = 170; it.modifiers.quickness = 5;
    it.max_durability = 40; it.durability = 40;
    return it;
}

Item build_cargo_pack() {
    Item it;
    it.item_def_id = ITEM_CARGO_PACK;
    it.id = 4004; it.name = "Cargo Pack"; it.type = ItemType::Accessory;
    it.description = "A reinforced backpack. Increases carrying capacity.";
    it.slot = EquipSlot::Back; it.rarity = Rarity::Common;
    it.weight = 2;
    it.buy_value = 60; it.sell_value = 20;
    return it;
}

// ---------------------------------------------------------------------------
// Grenades
// ---------------------------------------------------------------------------

Item build_frag_grenade() {
    Item it;
    it.item_def_id = ITEM_FRAG_GRENADE;
    it.id = 5001; it.name = "Frag Grenade"; it.type = ItemType::Grenade;
    it.description = "Explosive fragmentation grenade. Lethal in a small radius.";
    it.slot = EquipSlot::Thrown; it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true; it.buy_value = 30; it.sell_value = 10;
    it.damage_dice = Dice::make(2, 6);
    it.damage_type = DamageType::Kinetic;
    return it;
}

Item build_emp_grenade() {
    Item it;
    it.item_def_id = ITEM_EMP_GRENADE;
    it.id = 5002; it.name = "EMP Grenade"; it.type = ItemType::Grenade;
    it.description = "Electromagnetic pulse. Disables electronics and shields.";
    it.slot = EquipSlot::Thrown; it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = true; it.buy_value = 50; it.sell_value = 18;
    it.damage_dice = Dice::make(1, 8);
    it.damage_type = DamageType::Electrical;
    return it;
}

Item build_cryo_grenade() {
    Item it;
    it.item_def_id = ITEM_CRYO_GRENADE;
    it.id = 5003; it.name = "Cryo Grenade"; it.type = ItemType::Grenade;
    it.description = "Flash-freezes the target area. Slows and damages.";
    it.slot = EquipSlot::Thrown; it.rarity = Rarity::Rare;
    it.weight = 1;
    it.stackable = true; it.buy_value = 80; it.sell_value = 28;
    it.damage_dice = Dice::make(2, 8);
    it.damage_type = DamageType::Cryo;
    return it;
}

// ---------------------------------------------------------------------------
// Junk
// ---------------------------------------------------------------------------

Item build_scrap_metal() {
    Item it;
    it.item_def_id = ITEM_SCRAP_METAL;
    it.id = 6001; it.name = "Scrap Metal"; it.type = ItemType::Junk;
    it.description = "Twisted metal salvage. Worth a few credits.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 2;
    return it;
}

Item build_broken_circuit() {
    Item it;
    it.item_def_id = ITEM_BROKEN_CIRCUIT;
    it.id = 6002; it.name = "Broken Circuit"; it.type = ItemType::Junk;
    it.description = "A fried circuit board. Might be useful for tinkering.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 3;
    return it;
}

Item build_empty_casing() {
    Item it;
    it.item_def_id = ITEM_EMPTY_CASING;
    it.id = 6003; it.name = "Empty Casing"; it.type = ItemType::Junk;
    it.description = "Spent ammunition casing. Recycle for scrap.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 1;
    return it;
}

// ---------------------------------------------------------------------------
// Crafting materials
// ---------------------------------------------------------------------------

Item build_nano_fiber() {
    Item it;
    it.item_def_id = ITEM_NANO_FIBER;
    it.id = 7001; it.name = "Nano-Fiber"; it.type = ItemType::CraftingMaterial;
    it.description = "Ultra-strong synthetic fiber. Used in advanced repairs.";
    it.weight = 1;
    it.stackable = true; it.buy_value = 20; it.sell_value = 8;
    return it;
}

Item build_power_core() {
    Item it;
    it.item_def_id = ITEM_POWER_CORE;
    it.id = 7002; it.name = "Power Core"; it.type = ItemType::CraftingMaterial;
    it.description = "A compact energy source. Powers advanced equipment.";
    it.weight = 2;
    it.stackable = true; it.buy_value = 40; it.sell_value = 15;
    return it;
}

Item build_circuit_board() {
    Item it;
    it.item_def_id = ITEM_CIRCUIT_BOARD;
    it.id = 7003; it.name = "Circuit Board"; it.type = ItemType::CraftingMaterial;
    it.description = "Intact circuit board. Essential for electronics work.";
    it.weight = 1;
    it.stackable = true; it.buy_value = 25; it.sell_value = 10;
    return it;
}

Item build_alloy_ingot() {
    Item it;
    it.item_def_id = ITEM_ALLOY_INGOT;
    it.id = 7004; it.name = "Alloy Ingot"; it.type = ItemType::CraftingMaterial;
    it.description = "Refined metal alloy. Used in armor and weapon smithing.";
    it.weight = 3;
    it.stackable = true; it.buy_value = 30; it.sell_value = 12;
    return it;
}

// ---------------------------------------------------------------------------
// Ship components
// ---------------------------------------------------------------------------

Item build_engine_coil_mk1() {
    Item it;
    it.item_def_id = ITEM_ENGINE_COIL_MK1;
    it.id = 8000; it.name = "Engine Coil Mk1"; it.type = ItemType::ShipComponent;
    it.description = "Standard hyperspace engine coil. Enables interstellar travel.";
    it.weight = 12;
    it.buy_value = 300; it.sell_value = 100;
    it.ship_slot = ShipSlot::Engine;
    return it;
}

Item build_hull_plate() {
    Item it;
    it.item_def_id = ITEM_HULL_PLATE;
    it.id = 8001; it.name = "Hull Plate Mk1"; it.type = ItemType::ShipComponent;
    it.description = "Standard hull plating. Reinforces ship integrity.";
    it.weight = 10;
    it.buy_value = 50; it.sell_value = 15;
    it.ship_slot = ShipSlot::Hull;
    it.ship_modifiers.hull_hp = 25;
    return it;
}

Item build_shield_generator() {
    Item it;
    it.item_def_id = ITEM_SHIELD_GENERATOR;
    it.id = 8002; it.name = "Shield Generator"; it.type = ItemType::ShipComponent;
    it.description = "Energy shield emitter. Absorbs incoming fire.";
    it.weight = 8;
    it.buy_value = 500; it.sell_value = 170;
    it.ship_slot = ShipSlot::Shield;
    it.ship_modifiers.shield_hp = 15;
    return it;
}

Item build_navi_computer_mk2() {
    Item it;
    it.item_def_id = ITEM_NAVI_COMPUTER_MK2;
    it.id = 8003; it.name = "Navi Computer Mk2"; it.type = ItemType::ShipComponent;
    it.description = "Upgraded navigation computer. Plots longer hyperspace routes.";
    it.weight = 5;
    it.buy_value = 400; it.sell_value = 135;
    it.ship_slot = ShipSlot::NaviComputer;
    it.ship_modifiers.warp_range = 1;
    return it;
}

// ---------------------------------------------------------------------------
// Random selection
// ---------------------------------------------------------------------------

Item random_ranged_weapon(std::mt19937& rng) {
    // Weighted toward common/uncommon
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);

    if (roll < 40) return build_plasma_pistol();   // 40%
    if (roll < 70) return build_ion_blaster();      // 30%
    if (roll < 88) return build_pulse_rifle();      // 18%
    if (roll < 97) return build_arc_caster();       //  9%
    return build_void_lance();                      //  3%
}

Item random_melee_weapon(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);
    if (roll < 35) return build_combat_knife();     // 35%
    if (roll < 55) return build_stun_baton();        // 20%
    if (roll < 75) return build_vibro_blade();       // 20%
    if (roll < 92) return build_plasma_saber();      // 17%
    return build_ancient_mono_edge();                //  8%
}

Item random_armor(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);
    if (roll < 22) return build_padded_vest();       // 22%
    if (roll < 38) return build_flight_helmet();     // 16%
    if (roll < 54) return build_combat_boots();      // 16%
    if (roll < 67) return build_arm_guard();         // 13%
    if (roll < 80) return build_composite_armor();   // 13%
    if (roll < 90) return build_tactical_helmet();   // 10%
    if (roll < 97) return build_mag_lock_boots();    //  7%
    return build_exo_suit();                         //  3%
}

Item random_junk(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng)) {
        case 0: return build_scrap_metal();
        case 1: return build_broken_circuit();
        default: return build_empty_casing();
    }
}

// ---------------------------------------------------------------------------
// Merchant stock generators
// ---------------------------------------------------------------------------

static Item make_stack(Item item, int count) {
    item.stack_count = count;
    return item;
}

std::vector<Item> generate_merchant_stock(std::mt19937& rng, int faction_rep) {
    std::vector<Item> stock;
    stock.push_back(make_stack(build_battery(), 3));
    stock.push_back(make_stack(build_ration_pack(), 5));
    stock.push_back(make_stack(build_combat_stim(), 2));
    stock.push_back(random_ranged_weapon(rng));
    stock.push_back(random_armor(rng));
    stock.push_back(make_stack(build_frag_grenade(), 3));
    stock.push_back(build_night_goggles());
    stock.push_back(random_shield(rng));
    // Ship components
    stock.push_back(build_hull_plate());
    stock.push_back(build_shield_generator());
    if (faction_rep >= 10) { // Liked+
        stock.push_back(random_ranged_weapon(rng));
        stock.push_back(make_stack(build_combat_stim(), 3));
    }
    if (faction_rep >= 50) { // Trusted
        stock.push_back(random_armor(rng));
    }
    return stock;
}

std::vector<Item> generate_arms_dealer_stock(std::mt19937& rng, int faction_rep) {
    std::vector<Item> stock;
    std::uniform_int_distribution<int> dist(2, 3);
    int weapon_count = dist(rng);
    for (int i = 0; i < weapon_count; ++i) {
        stock.push_back(random_ranged_weapon(rng));
    }
    stock.push_back(random_melee_weapon(rng));
    stock.push_back(make_stack(build_battery(), 5));
    stock.push_back(random_armor(rng));
    stock.push_back(random_shield(rng));
    stock.push_back(make_stack(build_emp_grenade(), 2));
    if (faction_rep >= 10) { // Liked+
        stock.push_back(random_ranged_weapon(rng));
        stock.push_back(make_stack(build_emp_grenade(), 2));
    }
    if (faction_rep >= 50) { // Trusted
        stock.push_back(random_melee_weapon(rng));
        stock.push_back(random_armor(rng));
    }
    return stock;
}

std::vector<Item> generate_food_merchant_stock(std::mt19937& rng, int faction_rep) {
    (void)rng;
    std::vector<Item> stock;
    stock.push_back(make_stack(build_ration_pack(), 10));
    stock.push_back(make_stack(build_combat_stim(), 3));
    if (faction_rep >= 10) { // Liked+
        stock.push_back(make_stack(build_ration_pack(), 5));
        stock.push_back(make_stack(build_combat_stim(), 2));
    }
    return stock;
}

} // namespace astra
