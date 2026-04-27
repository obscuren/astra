# Items

Catalog of every hand-crafted item in Astra. Each row lists the fields a player or designer is most likely to care about. The **Dev** column is the loot-table identifier — pass it to `give item <name>` in the dev console.

Other references:
- Mechanics, formulas, generation: [`docs/mechanics.md`](mechanics.md)
- Tinkering: materials catalog, refinement, synthesis, schematics: [`docs/tinkering.md`](tinkering.md)
- Loot table source-of-truth: [`src/loot_table.cpp`](../src/loot_table.cpp)
- Item builders: [`src/item_defs.cpp`](../src/item_defs.cpp)
- Item ID constants: [`include/astra/item_ids.h`](../include/astra/item_ids.h)

> **Note on rarity range:** items spawn at any rarity within `[min..max]` from the loot table. Stats shown are the **base** values; `scale_item_to_rarity` multiplies them (×1.00/×1.10/×1.25/×1.45/×1.75 for Common/Uncommon/Rare/Epic/Legendary), then `scale_item_to_level` multiplies again (×1 + 0.15·(level-1)). Affixes add small additive bonuses on top.

---

## Ranged Weapons

Slot: `Missile`. Use one charge per shot from the equipped cell.

| Name | ID | Dev | Rarity range | Class | Damage | Type | Cost/shot | Base $ |
|------|----|----|----|----|----|----|----|----|
| Plasma Pistol | 1 | `plasma_pistol` | Common..Rare | Pistol | 1d6 | Plasma | 1 | 120 |
| Ion Blaster | 2 | `ion_blaster` | Common..Rare | Pistol | 1d8+1 | Electrical | 2 | 250 |
| Pulse Rifle | 3 | `pulse_rifle` | Uncommon..Epic | Rifle | 2d6 | Kinetic | 2 | 500 |
| Arc Caster | 4 | `arc_caster` | Rare..Epic | Rifle | 2d8+1 | Electrical | 3 | 900 |
| Void Lance | 5 | `void_lance` | Epic..Legendary | Rifle | 3d8+2 | Plasma | 4 | 2500 |

## Melee Weapons

Slot: `RightHand`. Wear durability per hit.

| Name | ID | Dev | Rarity range | Class | Damage | Type | Durability | Base $ | Notes |
|------|----|----|----|----|----|----|----|----|----|
| Combat Knife | 9 | `combat_knife` | Common..Rare | ShortBlade | 1d4 | Kinetic | 60 | 60 |  |
| Stun Baton | 12 | `stun_baton` | Common..Rare | LongBlade | 1d4+1 | Electrical | 70 | 80 | +5 quickness |
| Vibro Blade | 10 | `vibro_blade` | Uncommon..Epic | ShortBlade | 1d6+1 | Kinetic | 50 | 180 |  |
| Plasma Saber | 11 | `plasma_saber` | Rare..Epic | LongBlade | 2d4+2 | Plasma | 40 | 400 |  |
| Ancient Mono-Edge | 13 | `ancient_mono_edge` | Epic..Legendary | LongBlade | 2d6+2 | Kinetic | 200 | 1200 |  |

## Armor

`av` is armor value (penetration soak), `dv` adjusts dodge. `aff` is `{Kinetic, Plasma, Electrical, Cryo, Acid}` per-type bonus.

| Name | ID | Dev | Slot | Rarity range | AV | DV | Aff | Notes | Dur | $ |
|------|----|----|----|----|----|----|----|----|----|----|
| Padded Vest | 14 | `padded_vest` | Body | Common..Rare | +2 | — | {1,0,0,0,-1} |  | 50 | 80 |
| Composite Armor | 15 | `composite_armor` | Body | Uncommon..Epic | +4 | -1 | {2,-1,0,0,-2} |  | 80 | 250 |
| Exo-Suit | 16 | `exo_suit` | Body | Epic..Legendary | +6 | -2 | {1,1,-2,1,0} | +3 max HP | 120 | 600 |
| Flight Helmet | 17 | `flight_helmet` | Head | Common..Rare | +1 | — | — |  | 40 | 50 |
| Tactical Helmet | 18 | `tactical_helmet` | Head | Uncommon..Epic | +2 | — | {1,0,0,-1,0} | view +1 | 60 | 150 |
| Combat Boots | 19 | `combat_boots` | Feet | Common..Rare | +1 | — | — |  | 50 | 60 |
| Mag-Lock Boots | 20 | `mag_lock_boots` | Feet | Rare..Epic | +1 | — | — | quickness +3 | 60 | 120 |
| Arm Guard | 21 | `arm_guard` | LeftArm | Common..Rare | +1 | — | — |  | 40 | 40 |

## Energy Shields

Slot: `Shield`. Consume charge from an equipped cell when absorbing damage. Affinity multiplies absorption per damage type.

| Name | ID | Dev | Rarity range | Cap | Aff `{K,P,E,C,A}` | $ |
|------|----|----|----|----|----|----|
| Basic Deflector | 41 | `basic_deflector` | Common..Rare | 10 | {0,0,0,0,0} | 100 |
| Plasma Screen | 42 | `plasma_screen` | Uncommon..Epic | 15 | {0,+3,0,0,-1} | 250 |
| Ion Barrier | 43 | `ion_barrier` | Uncommon..Epic | 15 | {0,-1,+3,0,0} | 250 |
| Composite Barrier | 44 | `composite_barrier` | Rare..Epic | 20 | {1,1,1,1,1} | 500 |
| Hardlight Aegis | 45 | `hardlight_aegis` | Rare..Legendary | 30 | {3,1,-1,0,0} | 900 |
| Void Mantle | 46 | `void_mantle` | Epic..Legendary | 40 | {2,2,2,2,2} | 2500 |

## Accessories

Slot: `Face`, `Back`. Passive bonuses while equipped.

| Name | ID | Dev | Slot | Rarity range | Effect | $ |
|------|----|----|----|----|----|----|
| Nightvision Goggles | 24 | `nightvision_goggles` | Face | Common..Rare | view +1 (passive); view +5 when powered+active in dark; EnergyStore 60/60, drain 1/10 ticks; 1 enhancement slot | 80 |
| Recon Visor | 23 | `recon_visor` | Face | Uncommon..Epic | view +2 | 200 |
| Jetpack | 25 | `jetpack` | Back | Rare..Epic | quickness +5 (durability 40) | 500 |
| Cargo Pack | 26 | `cargo_pack` | Back | Common..Rare | (extra carry capacity) | 60 |

## Grenades

Slot: `Thrown`, stackable. Frag/EMP/Cryo are the original three; Incendiary/Smoke/Flashbang come from the schematic-craftable set and are inert until the consumable-use spec lands.

| Name | ID | Dev | Rarity range | Damage | Type | $ |
|------|----|----|----|----|----|----|
| Frag Grenade | 27 | `frag_grenade` | Common..Uncommon | 2d6 | Kinetic | 30 |
| EMP Grenade | 28 | `emp_grenade` | Uncommon..Rare | 1d8 | Electrical | 50 |
| Cryo Grenade | 29 | `cryo_grenade` | Uncommon..Rare | 2d8 | Cryo | 80 |
| Incendiary Grenade | 205 | `incendiary_grenade` | Uncommon..Rare | (inert) | — | 60 |
| Smoke Grenade | 206 | `smoke_grenade` | Common..Common | (inert) | — | 30 |
| Flashbang | 207 | `flashbang` | Uncommon..Uncommon | (inert) | — | 50 |

## Mines

`ItemType::Mine`. Inert until the consumable-use spec lands.

| Name | ID | Dev | Rarity range | $ |
|------|----|----|----|----|
| Proximity Mine | 208 | `proximity_mine` | Uncommon..Rare | 70 |
| EMP Mine | 209 | `emp_mine` | Uncommon..Rare | 80 |
| Incendiary Mine | 210 | `incendiary_mine` | Uncommon..Rare | 80 |
| Decoy Mine | 211 | `decoy_mine` | Common..Common | 40 |
| Caltrops | 212 | `caltrops` | Common..Common | 20 |

## Turrets

`ItemType::Turret`. Deployable autonomous defenders. Sentry Drone is the mobile variant. Inert until the consumable-use spec lands.

| Name | ID | Dev | Rarity range | $ |
|------|----|----|----|----|
| Auto-Turret | 213 | `auto_turret` | Uncommon..Rare | 110 |
| Flame Turret | 214 | `flame_turret` | Rare..Rare | 160 |
| Arc Turret | 215 | `arc_turret` | Rare..Rare | 180 |
| Sentry Drone | 216 | `sentry_drone` | Epic..Epic | 260 |

## Stims

`ItemType::Stim`. Adrenaline Stim was previously named "Combat Stim" — id and dev name unchanged. Newer stims are inert until the consumable-use spec lands.

| Name | ID | Dev | Rarity range | Effect | $ |
|------|----|----|----|----|----|
| Adrenaline Stim | 8 | `combat_stim` | Common..Uncommon | +5 attack-boost (short duration) | 50 |
| Healing Stim | 200 | `healing_stim` | Common..Uncommon | (inert) | 40 |
| Endure Stim | 201 | `endure_stim` | Uncommon..Uncommon | (inert) | 60 |
| Focus Stim | 202 | `focus_stim` | Uncommon..Uncommon | (inert) | 60 |
| Berserker Stim | 203 | `berserker_stim` | Rare..Rare | (inert) | 100 |
| Medkit | 204 | `medkit` | Uncommon..Uncommon | (inert) | 90 |

## Other Consumables

| Name | ID | Dev | Type | Rarity range | Effect |
|------|----|----|----|----|----|
| Ration Pack | 7 | `ration_pack` | Food | Common..Common | -1 hunger step |

## Energy Cells

Slot host for tinker mods. Each tier adds capacity and enhancement-slot count.

| Name | ID | Dev | Rarity range | Cap | Slots | $ |
|------|----|----|----|----|----|----|
| Small Energy Cell | 70 | `cell_small` | Common..Common | 60 | 1 | 15 |
| Standard Energy Cell | 71 | `cell_standard` | Common..Uncommon | 150 | 1 | 35 |
| Large Energy Cell | 72 | `cell_large` | Uncommon..Rare | 400 | 2 | 90 |
| Industrial Energy Cell | 73 | `cell_industrial` | Rare..Epic | 800 | 2 | 220 |
| Antimatter Cell | 74 | `cell_antimatter` | Epic..Legendary | 2000 | 3 | 650 |

### Legendary Specialty Cells (procs)

Cells that fire a one-off effect once per `threshold` units actually drained.

| Name | ID | Dev | Cap | Slots | Proc | $ |
|------|----|----|----|----|----|----|
| Bulwark Cell | 86 | `cell_bulwark` | 1500 | 3 | Shield Overcharge: +25 shield HP per 250 drain | 900 |
| Volatile Cell | 87 | `cell_volatile` | 1500 | 3 | Weapon Overcharge: +15 weapon energy per 150 drain | 900 |
| Adrenal Cell | 88 | `cell_adrenal` | 1500 | 3 | Adrenaline Rush: 5-turn buff per 300 drain | 900 |

## Energy Mods (slotted into cells)

`Category::EnergyMod` materials. Slot one into a cell's enhancement slot, then commit at the workbench.

| Name | ID | Dev | Rarity | Effect | $ |
|------|----|----|----|----|----|
| Reinforced Casing | 89 | `reinforced_casing` | Common | +10 capacity | 25 |
| Receptor Plate | 90 | `receptor_plate` | Common | +10% incoming charge rate | 30 |
| Brass Conduit | 91 | `brass_conduit` | Common | discharge_efficiency = 10 (+1 free per 10 drained) | 35 |
| Capacitor Coil | 83 | `capacitor_coil` | Uncommon | +30 capacity | 140 |
| Charge Catalyst | 84 | `charge_catalyst` | Uncommon | +25% incoming charge rate | 160 |
| Power Junction | 92 | `power_junction` | Uncommon | +15 capacity, +10% charge rate | 100 |
| Polished Conduit | 85 | `polished_conduit` | Rare | discharge_efficiency = 5 (+1 free per 5 drained) | 220 |
| Tuned Catalyst | 93 | `tuned_catalyst` | Rare | +15% charge rate, discharge_efficiency = 8 | 200 |

### Solar Panels

Outdoor-only passive trickle charge.

| Name | ID | Dev | Rarity | Energy / tick | Tick interval | $ |
|------|----|----|----|----|----|----|
| Solar Panel | 80 | `solar_panel` | Common | 5 | 2 | 60 |
| Polished Solar Panel | 81 | `solar_panel_uncommon` | Uncommon | 8 | 2 | 180 |
| Prismatic Solar Panel | 82 | `solar_panel_rare` | Rare | 12 | 2 | 500 |

## Accessory Modules

`Category::AccessoryMod`. Slot into an accessory's enhancement slot, then commit at the workbench to promote its behavior from manual to automatic.

| Name | ID | Dev | Rarity | Effect | Sources | $ |
|------|----|----|----|----|----|----|
| Light Sensor | 95 | `light_sensor` | Uncommon | Photodiode array. Auto-toggles light-dependent items (e.g. Nightvision Goggles). | Chest, General merchant, Arms dealer | 120 |
| AI Module | 94 | `ai_module` | Rare | Adaptive control circuit. Auto-triggers any manual-toggle benefit. | Chest, Arms dealer (rep≥50), Black market | 280 |

**Tinkering recipes:** `Optic Module + Joint Mechanism → AI Module`; `Optic Module + Padding Weave → Light Sensor`

## Ship Components

Installed in the ship's component slots from the inventory. Each slot accepts a single component.

| Name | ID | Dev | Ship slot | Effect | $ |
|------|----|----|----|----|----|
| Engine Coil Mk1 | 37 | (hardcoded — not droppable) | Engine | Enables hyperspace travel | 300 |
| Hull Plate Mk1 | 38 | `hull_plate` | Hull | +25 hull HP | 50 |
| Shield Generator | 39 | `shield_generator` | Shield | +15 shield HP | 500 |
| Navi Computer Mk2 | 40 | `navi_computer_mk2` | NaviComputer | +1 warp range | 400 |

## Junk

`Category::Junk`. Sells for parts. Promoted entries (Scrap, Broken Circuit, Empty Casing, Spare Parts, Circuitry) double as crafting reagents — the loot table sees them as Junk; tinkering recipes match by id.

| Name | ID | Dev | Notes |
|------|----|----|----|
| Scrap Metal | 30 | `scrap_metal` | also: crafting reagent (T1) |
| Broken Circuit | 31 | `broken_circuit` | also: crafting reagent (T1) |
| Empty Casing | 32 | `empty_casing` | also: crafting reagent (T1) |
| Spare Parts | 47 | `spare_parts` | also: crafting reagent (T2) |
| Circuitry | 48 | `circuitry` | also: crafting reagent (T2) |

## Crafting Materials

`Category::CraftingMaterial`. Consumed by tinkering recipes. **24 materials across 3 tiers** — see [`tinkering.md`](tinkering.md) for recipe usage.

### Tier 1 — Common

| Name | ID | Dev | Sell | Notes |
|------|----|----|----|----|
| Scrap Metal | 30 | `scrap_metal` | 1 | Junk-typed |
| Broken Circuit | 31 | `broken_circuit` | 2 | Junk-typed |
| Empty Casing | 32 | `empty_casing` | 1 | Junk-typed |
| Copper Wire | 7010 | `copper_wire` | 2 | Stack name "Strand of Copper Wire" |
| Polymer Strip | 7011 | `polymer_strip` | 2 | Flexible plastic |
| Glass Shard | 7012 | `glass_shard` | 1 | Optic / vial reagent |
| Adhesive Resin | 7013 | `adhesive_resin` | 2 | Bonding agent |
| Coolant Vial | 7014 | `coolant_vial` | 3 | Thermal reagent |

### Tier 2 — Uncommon

| Name | ID | Dev | Sell | Notes |
|------|----|----|----|----|
| Nano-Fiber | 7001 | `nano_fiber` | 8 | Repair reagent |
| Power Core | 7002 | `power_core` | 12 | Energy recipes |
| Circuit Board | 7003 | `circuit_board` | 10 | Tech recipes |
| Alloy Ingot | 7004 | `alloy_ingot` | 10 | Heavy/armor recipes |
| Spare Parts | 47 | `spare_parts` | 6 | Junk-typed; mechanical |
| Circuitry | 48 | `circuitry` | 8 | Junk-typed; in most recipes |
| Nano Lattice | 7020 | `nano_lattice` | 14 | Advanced structural |
| Polished Lens | 7021 | `polished_lens` | 12 | Optics |
| Micro-Servo | 7022 | `micro_servo` | 14 | Mechanical |
| Plasma Cartridge | 7023 | `plasma_cartridge` | 16 | Energy weapon |

### Tier 3 — Rare

| Name | ID | Dev | Sell | Notes |
|------|----|----|----|----|
| Quantum Resonance Crystal | 7030 | `quantum_resonance_crystal` | 50 | AI Module input |
| Strange Strobing Crystal | 7031 | `strange_strobing_crystal` | 60 | Future legendary |
| Prime Catalyst | 7032 | `prime_catalyst` | 55 | Future exotic |
| Prime Filament | 7033 | `prime_filament` | 55 | Future exotic |
| Voidshard | 7034 | `voidshard` | 70 | Future warp |
| Phase Coil | 7035 | `phase_coil` | 65 | Future phase |

## Cooking — Ingredients

`Category::Ingredient`. Combine in a campfire-adjacent slot to cook a dish. (Placed by the cooking system, not the loot table — no dev console name.)

| Name | ID |
|------|----|
| Raw Meat | 49 |
| Carrot | 50 |
| Flour | 51 |
| Herbs | 52 |
| Synth Protein | 53 |

## Cooking — Dishes

`ItemType::Food`. Each carries a `DishOutput` (hunger shift, HP restore, optional granted effects). Produced by the cooking system, not the loot table.

| Name | ID | Made from |
|------|----|----|
| Cooked Meat | 54 | 1 Raw Meat |
| Bowl of Broth | 55 | 1 Raw Meat + 1 Herbs |
| Flatbread | 56 | 1 Flour |
| Hearty Stew | 57 | 1 Raw Meat + 1 Carrot + 1 Herbs |
| Protein Bake | 58 | 1 Synth Protein + 1 Flour + 1 Herbs |
| Hero's Feast | 59 | 1 Raw Meat + 1 Synth Protein + 1 Carrot + 1 Flour + 1 Herbs |
| Burnt Slop | 60 | (failure output from mismatched recipe) |

## Cookbooks

`ItemType::Cookbook`. Read to learn the dish recipe permanently.

| Name | ID | Dev | Teaches |
|------|----|----|----|
| Cookbook: Hearty Stew | 61 | `cookbook_hearty_stew` | Hearty Stew recipe |
| Cookbook: Protein Bake | 62 | `cookbook_protein_bake` | Protein Bake recipe |
| Cookbook: Hero's Feast | 63 | `cookbook_heros_feast` | Hero's Feast recipe |

## Schematics

`ItemType::Schematic`. Single-use pickup; reading teaches a tinkering recipe permanently. See [`tinkering.md`](tinkering.md) for the matching crafted-item recipes.

| Name | ID | Dev | Rarity | Teaches |
|------|----|----|----|----|
| Schematic: Healing Stim | 220 | `schem_healing_stim` | Common | Healing Stim |
| Schematic: Adrenaline Stim | 221 | `schem_adrenaline_stim` | Uncommon | Adrenaline Stim |
| Schematic: Endure Stim | 222 | `schem_endure_stim` | Uncommon | Endure Stim |
| Schematic: Focus Stim | 223 | `schem_focus_stim` | Uncommon | Focus Stim |
| Schematic: Berserker Stim | 224 | `schem_berserker_stim` | Rare | Berserker Stim |
| Schematic: Medkit | 225 | `schem_medkit` | Uncommon | Medkit |
| Schematic: Frag Grenade | 226 | `schem_frag_grenade` | Common | Frag Grenade |
| Schematic: EMP Grenade | 227 | `schem_emp_grenade` | Uncommon | EMP Grenade |
| Schematic: Incendiary Grenade | 228 | `schem_incendiary_grenade` | Uncommon | Incendiary Grenade |
| Schematic: Smoke Grenade | 229 | `schem_smoke_grenade` | Common | Smoke Grenade |
| Schematic: Flashbang | 230 | `schem_flashbang` | Uncommon | Flashbang |
| Schematic: Proximity Mine | 231 | `schem_proximity_mine` | Uncommon | Proximity Mine |
| Schematic: EMP Mine | 232 | `schem_emp_mine` | Rare | EMP Mine |
| Schematic: Incendiary Mine | 233 | `schem_incendiary_mine` | Rare | Incendiary Mine |
| Schematic: Decoy Mine | 234 | `schem_decoy_mine` | Common | Decoy Mine |
| Schematic: Caltrops | 235 | `schem_caltrops` | Common | Caltrops |
| Schematic: Auto-Turret | 236 | `schem_auto_turret` | Uncommon | Auto-Turret |
| Schematic: Flame Turret | 237 | `schem_flame_turret` | Rare | Flame Turret |
| Schematic: Arc Turret | 238 | `schem_arc_turret` | Rare | Arc Turret |
| Schematic: Sentry Drone | 239 | `schem_sentry_drone` | Epic | Sentry Drone |

## Synthesized Items (tinker output, IDs 1000+)

Created by `synthesize_item` from two known blueprints + materials. Not drop-eligible — never appears in the loot table, no dev console name.

| Name | ID | From blueprints |
|------|----|----|
| Plasma Edge | 1000 | Plasma Emitter + Blade Housing |
| Thruster Plate | 1001 | Plating Alloy + Thruster Core |
| Targeting Array | 1002 | Optic Module + Power Conduit |
| Dual-Edge | 1003 | Edge Material + Grip Assembly |
| Reinforced Pack | 1004 | Padding Weave + Storage Frame |
| Overcharged Engine | 1005 | Power Conduit + Thruster Core |
| Articulated Armor | 1006 | Plating Alloy + Joint Mechanism |
| Guided Blaster | 1007 | Plasma Emitter + Optic Module |
| Combat Gauntlet | 1008 | Blade Housing + Joint Mechanism |
| Armored Blade | 1009 | Edge Material + Plating Alloy |

---

## Hardcoded one-offs

These items exist but never drop and aren't in the loot table.

- **Engine Coil Mk1** (id 37) — placed deterministically in Maintenance Tunnel dungeons (`game_world.cpp`).
- **Quest items** — placed by quest definitions (`game_world.cpp`); names and IDs vary per quest.
- **Burnt Slop** (id 60) — produced only by a failed cooking recipe.
