#include "astra/item_defs.h"
#include "astra/dice.h"
#include "astra/item_ids.h"
#include "astra/effect.h"  // EffectId for DishOutput::granted
#include "astra/energy.h"
#include "astra/tinkering.h"

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
    it.ranged = RangedData{6};
    it.energy = EnergyStore{20, 20};
    it.consumer = EnergyConsumer{1};
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
    it.ranged = RangedData{8};
    it.energy = EnergyStore{15, 15};
    it.consumer = EnergyConsumer{2};
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
    it.ranged = RangedData{12};
    it.energy = EnergyStore{30, 30};
    it.consumer = EnergyConsumer{2};
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
    it.ranged = RangedData{5};
    it.energy = EnergyStore{12, 12};
    it.consumer = EnergyConsumer{3};
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
    it.ranged = RangedData{15};
    it.energy = EnergyStore{10, 10};
    it.consumer = EnergyConsumer{4};
    return it;
}

// ---------------------------------------------------------------------------
// Consumables
// ---------------------------------------------------------------------------

static Item build_cell(uint16_t def_id, uint32_t id, const char* name,
                       Rarity rarity, int capacity, int weight,
                       int buy, int sell, int slot_override = -1) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = "Persistent power cell. Holds energy for weapons, shields, and gadgets.";
    it.type = ItemType::Battery;
    it.rarity = rarity;
    it.weight = weight;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = buy;
    it.sell_value = sell;
    it.usable = true;
    it.energy = EnergyStore{capacity, capacity};
    init_enhancement_slots(it);
    if (slot_override >= 0) it.enhancement_slots = slot_override;
    return it;
}

Item build_small_energy_cell()      { return build_cell(ITEM_SMALL_ENERGY_CELL,      2001, "Small Energy Cell",      Rarity::Common,   60,   1, 15,  5); }
Item build_standard_energy_cell()   { return build_cell(ITEM_STANDARD_ENERGY_CELL,   2010, "Standard Energy Cell",   Rarity::Common,   150,  1, 35,  12); }
Item build_large_energy_cell()      { return build_cell(ITEM_LARGE_ENERGY_CELL,      2011, "Large Energy Cell",      Rarity::Uncommon, 400,  2, 90,  30); }
Item build_industrial_energy_cell() { return build_cell(ITEM_INDUSTRIAL_ENERGY_CELL, 2012, "Industrial Energy Cell", Rarity::Rare,     800,  3, 220, 70, /*slot_override=*/2); }
Item build_antimatter_cell()        { return build_cell(ITEM_ANTIMATTER_CELL,        2013, "Antimatter Cell",        Rarity::Epic,     2000, 3, 650, 200); }

// Legendary specialty cells: standard build_cell + a CellProc bonus.
Item build_bulwark_cell() {
    Item it = build_cell(ITEM_BULWARK_CELL, 2020, "Bulwark Cell",
                         Rarity::Legendary, 1500, 3, 900, 280);
    it.description = "Reinforced cell. Channels overflow into the shield matrix.";
    it.proc = CellProc{ CellProcKind::ShieldOvercharge, /*magnitude=*/25, /*duration=*/0,
                        /*threshold=*/250, /*accumulator=*/0 };
    return it;
}
Item build_volatile_cell() {
    Item it = build_cell(ITEM_VOLATILE_CELL, 2021, "Volatile Cell",
                         Rarity::Legendary, 1500, 3, 900, 280);
    it.description = "Unstable cell. Surges past safe limits when discharged into a weapon.";
    it.proc = CellProc{ CellProcKind::WeaponOvercharge, /*magnitude=*/15, /*duration=*/0,
                        /*threshold=*/150, /*accumulator=*/0 };
    return it;
}
Item build_adrenal_cell() {
    Item it = build_cell(ITEM_ADRENAL_CELL, 2022, "Adrenal Cell",
                         Rarity::Legendary, 1500, 3, 900, 280);
    it.description = "Bio-coupled cell. Triggers an adrenal surge when drained.";
    it.proc = CellProc{ CellProcKind::AdrenalineRush, /*magnitude=*/0, /*duration=*/5,
                        /*threshold=*/300, /*accumulator=*/0 };
    return it;
}

// Legacy alias so existing callers compile. Returns a Standard cell.
Item build_battery() { return build_standard_energy_cell(); }

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
    it.dish = DishOutput{ .hunger_shift = -1, .hp_restore = 5, .granted = {} };
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
// Ingredients
// ---------------------------------------------------------------------------

Item build_raw_meat() {
    Item it;
    it.item_def_id = ITEM_RAW_MEAT;
    it.id = 2100;
    it.name = "Raw Meat";
    it.description = "A cut of uncooked meat. Needs cooking to be edible.";
    it.type = ItemType::Ingredient;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 4;
    it.sell_value = 1;
    return it;
}

Item build_carrot() {
    Item it;
    it.item_def_id = ITEM_CARROT;
    it.id = 2101;
    it.name = "Carrot";
    it.description = "A crunchy root vegetable.";
    it.type = ItemType::Ingredient;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 2;
    it.sell_value = 1;
    return it;
}

Item build_flour() {
    Item it;
    it.item_def_id = ITEM_FLOUR;
    it.id = 2102;
    it.name = "Flour";
    it.description = "A sack of milled grain.";
    it.type = ItemType::Ingredient;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 2;
    it.sell_value = 1;
    return it;
}

Item build_herbs() {
    Item it;
    it.item_def_id = ITEM_HERBS;
    it.id = 2103;
    it.name = "Herbs";
    it.description = "A bundle of aromatic herbs.";
    it.type = ItemType::Ingredient;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 3;
    it.sell_value = 1;
    return it;
}

Item build_synth_protein() {
    Item it;
    it.item_def_id = ITEM_SYNTH_PROTEIN;
    it.id = 2104;
    it.name = "Synth-Protein";
    it.description = "A brick of station-processed protein.";
    it.type = ItemType::Ingredient;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 5;
    it.sell_value = 2;
    return it;
}

// ---------------------------------------------------------------------------
// Cooked dishes
// ---------------------------------------------------------------------------

Item build_cooked_meat() {
    Item it;
    it.item_def_id = ITEM_COOKED_MEAT;
    it.id = 2200;
    it.name = "Cooked Meat";
    it.description = "Meat cooked over a fire. Simple and satisfying.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 8;
    it.sell_value = 2;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -1, .hp_restore = 3, .granted = {} };
    return it;
}

Item build_bowl_of_broth() {
    Item it;
    it.item_def_id = ITEM_BOWL_OF_BROTH;
    it.id = 2201;
    it.name = "Bowl of Broth";
    it.description = "A simple vegetable broth. Warms the bones.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 10;
    it.sell_value = 3;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -1, .hp_restore = 2,
                          .granted = { EffectId::WarmMeal } };
    return it;
}

Item build_flatbread() {
    Item it;
    it.item_def_id = ITEM_FLATBREAD;
    it.id = 2202;
    it.name = "Flatbread";
    it.description = "Flatbread cooked on a hot stone.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 6;
    it.sell_value = 2;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -2, .hp_restore = 1, .granted = {} };
    return it;
}

Item build_hearty_stew() {
    Item it;
    it.item_def_id = ITEM_HEARTY_STEW;
    it.id = 2203;
    it.name = "Hearty Stew";
    it.description = "A thick, meaty stew. Restores stamina and fortifies you.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 20;
    it.sell_value = 5;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -2, .hp_restore = 5,
                          .granted = { EffectId::WellFed } };
    return it;
}

Item build_protein_bake() {
    Item it;
    it.item_def_id = ITEM_PROTEIN_BAKE;
    it.id = 2204;
    it.name = "Protein Bake";
    it.description = "Synth-protein baked into a filling loaf.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 18;
    it.sell_value = 5;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -2, .hp_restore = 4, .granted = {} };
    return it;
}

Item build_heros_feast() {
    Item it;
    it.item_def_id = ITEM_HEROS_FEAST;
    it.id = 2205;
    it.name = "Hero's Feast";
    it.description = "A legendary dish said to steel a warrior for any trial.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Rare;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 80;
    it.sell_value = 20;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -3, .hp_restore = 10,
                          .granted = { EffectId::Hearty } };
    return it;
}

Item build_burnt_slop() {
    Item it;
    it.item_def_id = ITEM_BURNT_SLOP;
    it.id = 2206;
    it.name = "Burnt Slop";
    it.description = "A charred, inedible-looking mass. Better than nothing.";
    it.type = ItemType::Food;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 0;
    it.sell_value = 0;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -1, .hp_restore = 0, .granted = {} };
    return it;
}

// ---------------------------------------------------------------------------
// Cookbooks
// ---------------------------------------------------------------------------

Item build_cookbook_hearty_stew() {
    Item it;
    it.item_def_id = ITEM_COOKBOOK_HEARTY_STEW;
    it.id = 2300;
    it.name = "Cookbook: Hearty Stew";
    it.description = "A well-thumbed recipe card for a hearty stew.";
    it.type = ItemType::Cookbook;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = 40;
    it.sell_value = 10;
    it.usable = true;
    it.teaches_recipe_id = 10;  // matches Recipe::id in Task 7
    return it;
}

Item build_cookbook_protein_bake() {
    Item it;
    it.item_def_id = ITEM_COOKBOOK_PROTEIN_BAKE;
    it.id = 2301;
    it.name = "Cookbook: Protein Bake";
    it.description = "A station-issue ration-hall recipe for a protein bake.";
    it.type = ItemType::Cookbook;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = 35;
    it.sell_value = 9;
    it.usable = true;
    it.teaches_recipe_id = 11;
    return it;
}

Item build_cookbook_heros_feast() {
    Item it;
    it.item_def_id = ITEM_COOKBOOK_HEROS_FEAST;
    it.id = 2302;
    it.name = "Cookbook: Hero's Feast";
    it.description = "An ancient recipe whispered to grant a warrior's courage.";
    it.type = ItemType::Cookbook;
    it.rarity = Rarity::Rare;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = 80;
    it.sell_value = 20;
    it.usable = true;
    it.teaches_recipe_id = 12;
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
    it.energy = EnergyStore{10, 10};
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
    it.energy = EnergyStore{15, 15};
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
    it.energy = EnergyStore{15, 15};
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
    it.energy = EnergyStore{20, 20};
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
    it.energy = EnergyStore{30, 30};
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
    it.energy = EnergyStore{40, 40};
    it.type_affinity = {2, 2, 2, 2, 2};
    return it;
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
// Salvage
// ---------------------------------------------------------------------------

Item build_spare_parts() {
    Item it;
    it.item_def_id = ITEM_SPARE_PARTS;
    it.id = 6010; it.name = "Spare Parts"; it.type = ItemType::CraftingMaterial;
    it.description = "Usable parts pulled from wreckage. Good for repairs.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 4;
    return it;
}

Item build_circuitry() {
    Item it;
    it.item_def_id = ITEM_CIRCUITRY;
    it.id = 6011; it.name = "Circuitry"; it.type = ItemType::CraftingMaterial;
    it.description = "Salvaged integrated circuits. Essential for advanced repair.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 8;
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
// Solar panel crafting materials
// ---------------------------------------------------------------------------

static Item build_solar_panel_(uint16_t def_id, uint32_t id, const char* name, Rarity rarity, int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = "Photovoltaic mod. Slots into any energy item; recharges it while outdoors.";
    it.type = ItemType::CraftingMaterial;
    it.rarity = rarity;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = buy;
    it.sell_value = sell;
    return it;
}

Item build_solar_panel_common()   { return build_solar_panel_(ITEM_SOLAR_PANEL_COMMON,   2050, "Solar Panel",           Rarity::Common,   60,  20); }
Item build_solar_panel_uncommon() { return build_solar_panel_(ITEM_SOLAR_PANEL_UNCOMMON, 2051, "Polished Solar Panel",  Rarity::Uncommon, 180, 60); }
Item build_solar_panel_rare()     { return build_solar_panel_(ITEM_SOLAR_PANEL_RARE,     2052, "Prismatic Solar Panel", Rarity::Rare,     500, 170); }

static Item build_energy_mod_(uint16_t def_id, uint32_t id, const char* name,
                              const char* desc, Rarity rarity, int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = desc;
    it.type = ItemType::CraftingMaterial;
    it.rarity = rarity;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = buy;
    it.sell_value = sell;
    return it;
}

Item build_capacitor_coil()    { return build_energy_mod_(ITEM_CAPACITOR_COIL,    2053, "Capacitor Coil",    "Energy mod. Slotted into a cell to add +30 capacity.",                       Rarity::Uncommon, 140, 50); }
Item build_charge_catalyst()   { return build_energy_mod_(ITEM_CHARGE_CATALYST,   2054, "Charge Catalyst",   "Energy mod. Slotted into a cell to boost incoming charge rate by +25%.",      Rarity::Uncommon, 160, 55); }
Item build_polished_conduit()  { return build_energy_mod_(ITEM_POLISHED_CONDUIT,  2055, "Polished Conduit",  "Energy mod. Slotted into a cell so every 5 units transferred yields +1 free.", Rarity::Rare,    220, 75); }

// Minor mods — small magnitudes meant to stack across multiple slots for custom cell builds.
Item build_reinforced_casing() { return build_energy_mod_(ITEM_REINFORCED_CASING, 2056, "Reinforced Casing", "Minor energy mod. Adds +10 capacity to the host cell.",                       Rarity::Common,   25,  8); }
Item build_receptor_plate()    { return build_energy_mod_(ITEM_RECEPTOR_PLATE,    2057, "Receptor Plate",    "Minor energy mod. +10% to incoming charge rate (Solar Panels, stations).",   Rarity::Common,   30, 10); }
Item build_brass_conduit()     { return build_energy_mod_(ITEM_BRASS_CONDUIT,     2058, "Brass Conduit",     "Minor energy mod. +1 free unit per 10 transferred.",                          Rarity::Common,   35, 12); }
Item build_power_junction()    { return build_energy_mod_(ITEM_POWER_JUNCTION,    2059, "Power Junction",    "Hybrid energy mod. +15 capacity and +10% charge rate.",                       Rarity::Uncommon, 100, 30); }
Item build_tuned_catalyst()    { return build_energy_mod_(ITEM_TUNED_CATALYST,    2060, "Tuned Catalyst",    "Hybrid energy mod. +15% charge rate and +1 free per 8 transferred.",          Rarity::Rare,    200, 70); }

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

Item build_by_def_id(uint16_t def_id) {
    switch (def_id) {
        // Ranged weapons
        case ITEM_PLASMA_PISTOL:           return build_plasma_pistol();
        case ITEM_ION_BLASTER:             return build_ion_blaster();
        case ITEM_PULSE_RIFLE:             return build_pulse_rifle();
        case ITEM_ARC_CASTER:              return build_arc_caster();
        case ITEM_VOID_LANCE:              return build_void_lance();

        // Melee weapons
        case ITEM_COMBAT_KNIFE:            return build_combat_knife();
        case ITEM_VIBRO_BLADE:             return build_vibro_blade();
        case ITEM_PLASMA_SABER:            return build_plasma_saber();
        case ITEM_STUN_BATON:              return build_stun_baton();
        case ITEM_ANCIENT_MONO_EDGE:       return build_ancient_mono_edge();

        // Armor
        case ITEM_PADDED_VEST:             return build_padded_vest();
        case ITEM_COMPOSITE_ARMOR:         return build_composite_armor();
        case ITEM_EXO_SUIT:                return build_exo_suit();
        case ITEM_FLIGHT_HELMET:           return build_flight_helmet();
        case ITEM_TACTICAL_HELMET:         return build_tactical_helmet();
        case ITEM_COMBAT_BOOTS:            return build_combat_boots();
        case ITEM_MAG_LOCK_BOOTS:          return build_mag_lock_boots();
        case ITEM_ARM_GUARD:               return build_arm_guard();

        // Shields
        case ITEM_BASIC_DEFLECTOR:         return build_basic_deflector();
        case ITEM_PLASMA_SCREEN:           return build_plasma_screen();
        case ITEM_ION_BARRIER:             return build_ion_barrier();
        case ITEM_COMPOSITE_BARRIER:       return build_composite_barrier();
        case ITEM_HARDLIGHT_AEGIS:         return build_hardlight_aegis();
        case ITEM_VOID_MANTLE:             return build_void_mantle();

        // Accessories
        case ITEM_RECON_VISOR:             return build_recon_visor();
        case ITEM_NIGHT_GOGGLES:           return build_night_goggles();
        case ITEM_JETPACK:                 return build_jetpack();
        case ITEM_CARGO_PACK:              return build_cargo_pack();

        // Grenades
        case ITEM_FRAG_GRENADE:            return build_frag_grenade();
        case ITEM_EMP_GRENADE:             return build_emp_grenade();
        case ITEM_CRYO_GRENADE:            return build_cryo_grenade();

        // Consumables
        case ITEM_BATTERY:                 return build_battery();
        case ITEM_RATION_PACK:             return build_ration_pack();
        case ITEM_COMBAT_STIM:             return build_combat_stim();

        // Energy cells
        case ITEM_SMALL_ENERGY_CELL:       return build_small_energy_cell();
        case ITEM_STANDARD_ENERGY_CELL:    return build_standard_energy_cell();
        case ITEM_LARGE_ENERGY_CELL:       return build_large_energy_cell();
        case ITEM_INDUSTRIAL_ENERGY_CELL:  return build_industrial_energy_cell();
        case ITEM_ANTIMATTER_CELL:         return build_antimatter_cell();
        case ITEM_BULWARK_CELL:            return build_bulwark_cell();
        case ITEM_VOLATILE_CELL:           return build_volatile_cell();
        case ITEM_ADRENAL_CELL:            return build_adrenal_cell();

        // Energy mods
        case ITEM_SOLAR_PANEL_COMMON:      return build_solar_panel_common();
        case ITEM_SOLAR_PANEL_UNCOMMON:    return build_solar_panel_uncommon();
        case ITEM_SOLAR_PANEL_RARE:        return build_solar_panel_rare();
        case ITEM_CAPACITOR_COIL:          return build_capacitor_coil();
        case ITEM_CHARGE_CATALYST:         return build_charge_catalyst();
        case ITEM_POLISHED_CONDUIT:        return build_polished_conduit();
        case ITEM_REINFORCED_CASING:       return build_reinforced_casing();
        case ITEM_RECEPTOR_PLATE:          return build_receptor_plate();
        case ITEM_BRASS_CONDUIT:           return build_brass_conduit();
        case ITEM_POWER_JUNCTION:          return build_power_junction();
        case ITEM_TUNED_CATALYST:          return build_tuned_catalyst();

        // Junk
        case ITEM_SCRAP_METAL:             return build_scrap_metal();
        case ITEM_BROKEN_CIRCUIT:          return build_broken_circuit();
        case ITEM_EMPTY_CASING:            return build_empty_casing();

        // Salvage
        case ITEM_SPARE_PARTS:             return build_spare_parts();
        case ITEM_CIRCUITRY:               return build_circuitry();

        // Crafting materials
        case ITEM_NANO_FIBER:              return build_nano_fiber();
        case ITEM_POWER_CORE:              return build_power_core();
        case ITEM_CIRCUIT_BOARD:           return build_circuit_board();
        case ITEM_ALLOY_INGOT:             return build_alloy_ingot();

        // Ship components
        case ITEM_ENGINE_COIL_MK1:         return build_engine_coil_mk1();
        case ITEM_HULL_PLATE:              return build_hull_plate();
        case ITEM_SHIELD_GENERATOR:        return build_shield_generator();
        case ITEM_NAVI_COMPUTER_MK2:       return build_navi_computer_mk2();

        // Cooking ingredients
        case ITEM_RAW_MEAT:                return build_raw_meat();
        case ITEM_CARROT:                  return build_carrot();
        case ITEM_FLOUR:                   return build_flour();
        case ITEM_HERBS:                   return build_herbs();
        case ITEM_SYNTH_PROTEIN:           return build_synth_protein();

        // Cooked dishes
        case ITEM_COOKED_MEAT:             return build_cooked_meat();
        case ITEM_BOWL_OF_BROTH:           return build_bowl_of_broth();
        case ITEM_FLATBREAD:               return build_flatbread();
        case ITEM_HEARTY_STEW:             return build_hearty_stew();
        case ITEM_PROTEIN_BAKE:            return build_protein_bake();
        case ITEM_HEROS_FEAST:             return build_heros_feast();
        case ITEM_BURNT_SLOP:              return build_burnt_slop();

        // Cookbooks
        case ITEM_COOKBOOK_HEARTY_STEW:    return build_cookbook_hearty_stew();
        case ITEM_COOKBOOK_PROTEIN_BAKE:   return build_cookbook_protein_bake();
        case ITEM_COOKBOOK_HEROS_FEAST:    return build_cookbook_heros_feast();
    }
    return Item{};
}

} // namespace astra
