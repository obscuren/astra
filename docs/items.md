# Items

Catalog of every hand-crafted item in Astra. Each row lists the fields a player or designer is most likely to care about.

Other references:
- Mechanics, formulas, generation: [`docs/mechanics.md`](mechanics.md)
- Loot table source-of-truth: [`src/loot_table.cpp`](../src/loot_table.cpp)
- Item builders: [`src/item_defs.cpp`](../src/item_defs.cpp)
- Item ID constants: [`include/astra/item_ids.h`](../include/astra/item_ids.h)

> **Note on rarity range:** items spawn at any rarity within `[min..max]` from the loot table. Stats shown are the **base** values; `scale_item_to_rarity` multiplies them (×1.00/×1.10/×1.25/×1.45/×1.75 for Common/Uncommon/Rare/Epic/Legendary), then `scale_item_to_level` multiplies again (×1 + 0.15·(level-1)). Affixes add small additive bonuses on top.

---

## Ranged Weapons

Slot: `Missile`. Use one charge per shot from the equipped cell.

| Glyph | Name | ID | Rarity range | Class | Damage | Type | Cost/shot | Base $ |
|------|------|----|----|----|----|----|----|----|
| `)` Cyan | Plasma Pistol | 1 | Common..Rare | Pistol | 1d6 | Plasma | 1 | 120 |
| `)` Green | Ion Blaster | 2 | Common..Rare | Pistol | 1d8+1 | Electrical | 2 | 250 |
| `)` Blue | Pulse Rifle | 3 | Uncommon..Epic | Rifle | 2d6 | Kinetic | 2 | 500 |
| `)` Magenta | Arc Caster | 4 | Rare..Epic | Rifle | 2d8+1 | Electrical | 3 | 900 |
| `)` Orange | Void Lance | 5 | Epic..Legendary | Rifle | 3d8+2 | Plasma | 4 | 2500 |

## Melee Weapons

Slot: `RightHand`. Wear durability per hit.

| Glyph | Name | ID | Rarity range | Class | Damage | Type | Durability | Base $ | Notes |
|------|------|----|----|----|----|----|----|----|----|
| `/` White | Combat Knife | 9 | Common..Rare | ShortBlade | 1d4 | Kinetic | 60 | 60 |  |
| `/` Yellow | Stun Baton | 12 | Common..Rare | LongBlade | 1d4+1 | Electrical | 70 | 80 | +5 quickness |
| `/` Green | Vibro Blade | 10 | Uncommon..Epic | ShortBlade | 1d6+1 | Kinetic | 50 | 180 |  |
| `/` Blue | Plasma Saber | 11 | Rare..Epic | LongBlade | 2d4+2 | Plasma | 40 | 400 |  |
| `/` Magenta | Ancient Mono-Edge | 13 | Epic..Legendary | LongBlade | 2d6+2 | Kinetic | 200 | 1200 |  |

## Armor

`av` is armor value (penetration soak), `dv` adjusts dodge. `aff` is `{Kinetic, Plasma, Electrical, Cryo, Acid}` per-type bonus.

| Glyph | Name | ID | Slot | Rarity range | AV | DV | Aff | Notes | Dur | $ |
|------|------|----|----|----|----|----|----|----|----|----|
| `[` White | Padded Vest | 14 | Body | Common..Rare | +2 | — | {1,0,0,0,-1} |  | 50 | 80 |
| `[` Green | Composite Armor | 15 | Body | Uncommon..Epic | +4 | -1 | {2,-1,0,0,-2} |  | 80 | 250 |
| `[` Blue | Exo-Suit | 16 | Body | Epic..Legendary | +6 | -2 | {1,1,-2,1,0} | +3 max HP | 120 | 600 |
| `^` White | Flight Helmet | 17 | Head | Common..Rare | +1 | — | — |  | 40 | 50 |
| `^` Green | Tactical Helmet | 18 | Head | Uncommon..Epic | +2 | — | {1,0,0,-1,0} | view +1 | 60 | 150 |
| `_` White | Combat Boots | 19 | Feet | Common..Rare | +1 | — | — |  | 50 | 60 |
| `_` Green | Mag-Lock Boots | 20 | Feet | Rare..Epic | +1 | — | — | quickness +3 | 60 | 120 |
| `}` White | Arm Guard | 21 | LeftArm | Common..Rare | +1 | — | — |  | 40 | 40 |

## Energy Shields

Slot: `Shield`. Consume charge from an equipped cell when absorbing damage. Affinity multiplies absorption per damage type.

| Glyph | Name | ID | Rarity range | Cap | Aff `{K,P,E,C,A}` | $ |
|------|------|----|----|----|----|----|
| `0` White | Basic Deflector | 41 | Common..Rare | 10 | {0,0,0,0,0} | 100 |
| `0` Red | Plasma Screen | 42 | Uncommon..Epic | 15 | {0,+3,0,0,-1} | 250 |
| `0` Cyan | Ion Barrier | 43 | Uncommon..Epic | 15 | {0,-1,+3,0,0} | 250 |
| `0` Green | Composite Barrier | 44 | Rare..Epic | 20 | {1,1,1,1,1} | 500 |
| `0` Yellow | Hardlight Aegis | 45 | Rare..Legendary | 30 | {3,1,-1,0,0} | 900 |
| `0` Magenta | Void Mantle | 46 | Epic..Legendary | 40 | {2,2,2,2,2} | 2500 |

## Accessories

Slot: `Face`, `Back`. Passive bonuses while equipped.

| Glyph | Name | ID | Slot | Rarity range | Effect | $ |
|------|------|----|----|----|----|----|
| `&` White | Nightvision Goggles | 24 | Face | Common..Rare | view +1 (passive); view +5 when powered+active in dark; EnergyStore 60/60, drain 1/10 ticks; 1 enhancement slot | 80 |
| `&` Green | Recon Visor | 23 | Face | Uncommon..Epic | view +2 | 200 |
| `\` Blue | Jetpack | 25 | Back | Rare..Epic | quickness +5 (durability 40) | 500 |
| `\` White | Cargo Pack | 26 | Back | Common..Rare | (extra carry capacity) | 60 |

## Grenades

Slot: `Thrown`, stackable.

| Glyph | Name | ID | Rarity range | Damage | Type | $ |
|------|------|----|----|----|----|----|
| `*` Red | Frag Grenade | 27 | Common..Uncommon | 2d6 | Kinetic | 30 |
| `*` Cyan | EMP Grenade | 28 | Uncommon..Rare | 1d8 | Electrical | 50 |
| `*` Blue | Cryo Grenade | 29 | Uncommon..Rare | 2d8 | Cryo | 80 |

## Consumables

| Glyph | Name | ID | Type | Rarity range | Effect |
|------|------|----|----|----|----|
| `%` Green | Ration Pack | 7 | Food | Common..Common | -1 hunger step |
| `!` Red | Combat Stim | 8 | Stim | Common..Uncommon | +5 attack-boost (short duration) |

## Energy Cells

Slot host for tinker mods. Each tier adds capacity and enhancement-slot count.

| Glyph | Name | ID | Rarity range | Cap | Slots | $ |
|------|------|----|----|----|----|----|
| `=` White | Small Energy Cell | 70 | Common..Common | 60 | 1 | 15 |
| `=` Green | Standard Energy Cell | 71 | Common..Uncommon | 150 | 1 | 35 |
| `=` Cyan | Large Energy Cell | 72 | Uncommon..Rare | 400 | 2 | 90 |
| `=` Blue | Industrial Energy Cell | 73 | Rare..Epic | 800 | 2 | 220 |
| `=` Magenta | Antimatter Cell | 74 | Epic..Legendary | 2000 | 3 | 650 |

### Legendary Specialty Cells (procs)

Cells that fire a one-off effect once per `threshold` units actually drained.

| Glyph | Name | ID | Cap | Slots | Proc | $ |
|------|------|----|----|----|----|----|
| `=` Orange | Bulwark Cell | 86 | 1500 | 3 | Shield Overcharge: +25 shield HP per 250 drain | 900 |
| `=` Bright Red | Volatile Cell | 87 | 1500 | 3 | Weapon Overcharge: +15 weapon energy per 150 drain | 900 |
| `=` Bright Magenta | Adrenal Cell | 88 | 1500 | 3 | Adrenaline Rush: 5-turn buff per 300 drain | 900 |

## Energy Mods (slotted into cells)

`Category::EnergyMod` materials. Slot one into a cell's enhancement slot, then commit at the workbench.

| Glyph | Name | ID | Rarity | Effect | $ |
|------|------|----|----|----|----|
| `*` White | Reinforced Casing | 89 | Common | +10 capacity | 25 |
| `*` Dark Gray | Receptor Plate | 90 | Common | +10% incoming charge rate | 30 |
| `*` Yellow | Brass Conduit | 91 | Common | discharge_efficiency = 10 (+1 free per 10 drained) | 35 |
| `*` Cyan | Capacitor Coil | 83 | Uncommon | +30 capacity | 140 |
| `*` Green | Charge Catalyst | 84 | Uncommon | +25% incoming charge rate | 160 |
| `*` Bright Cyan | Power Junction | 92 | Uncommon | +15 capacity, +10% charge rate | 100 |
| `*` Blue | Polished Conduit | 85 | Rare | discharge_efficiency = 5 (+1 free per 5 drained) | 220 |
| `*` Bright Green | Tuned Catalyst | 93 | Rare | +15% charge rate, discharge_efficiency = 8 | 200 |

### Solar Panels

Outdoor-only passive trickle charge.

| Glyph | Name | ID | Rarity | Energy / tick | Tick interval | $ |
|------|------|----|----|----|----|----|
| `*` Yellow | Solar Panel | 80 | Common | 5 | 2 | 60 |
| `*` Bright Yellow | Polished Solar Panel | 81 | Uncommon | 8 | 2 | 180 |
| `*` Orange | Prismatic Solar Panel | 82 | Rare | 12 | 2 | 500 |

## Accessory Modules

`Category::AccessoryMod`. Slot into an accessory's enhancement slot, then commit at the workbench to promote its behavior from manual to automatic.

| Glyph | Name | ID | Rarity | Effect | Sources | $ |
|------|------|----|----|----|----|-----|
| `*` Bright Yellow | Light Sensor | 95 | Uncommon | Photodiode array. Auto-toggles light-dependent items (e.g. Nightvision Goggles). | Chest, General merchant, Arms dealer | 120 |
| `*` Bright Cyan | AI Module | 94 | Rare | Adaptive control circuit. Auto-triggers any manual-toggle benefit. | Chest, Arms dealer (rep≥50), Black market | 280 |

**Tinkering recipes:** `Optic Module + Joint Mechanism → AI Module`; `Optic Module + Padding Weave → Light Sensor`

## Ship Components

Installed in the ship's component slots from the inventory. Each slot accepts a single component.

| Glyph | Name | ID | Ship slot | Effect | $ |
|------|------|----|----|----|----|
| `#` Yellow | Engine Coil Mk1 | 37 | Engine | Enables hyperspace travel | 300 |
| `#` White | Hull Plate Mk1 | 38 | Hull | +25 hull HP | 50 |
| `#` Cyan | Shield Generator | 39 | Shield | +15 shield HP | 500 |
| `#` Green | Navi Computer Mk2 | 40 | NaviComputer | +1 warp range | 400 |

## Junk (NPC drops, salvage)

`Category::Junk`. Sells for parts; no equip / use.

| Glyph | Name | ID |
|------|------|----|
| `~` Dark Gray | Scrap Metal | 30 |
| `~` Dark Gray | Broken Circuit | 31 |
| `~` Dark Gray | Empty Casing | 32 |
| `~` Yellow | Spare Parts | 47 |
| `~` Cyan | Circuitry | 48 |

## Crafting Materials

`Category::CraftingMaterial`. Consumed by tinkering recipes.

| Glyph | Name | ID | Used in |
|------|------|----|----|
| `+` Cyan | Nano-Fiber | 33 | All recipes (default reagent) |
| `+` Yellow | Power Core | 34 | High-energy recipes |
| `+` Green | Circuit Board | 35 | Tech / mod recipes |
| `+` White | Alloy Ingot | 36 | Heavy/armor recipes |

## Cooking — Ingredients

`Category::Ingredient`. Combine in a campfire-adjacent slot to cook a dish.

| Glyph | Name | ID |
|------|------|----|
| `%` Red | Raw Meat | 49 |
| `%` Orange | Carrot | 50 |
| `%` White | Flour | 51 |
| `%` Green | Herbs | 52 |
| `%` Cyan | Synth Protein | 53 |

## Cooking — Dishes

`ItemType::Food`. Each carries a `DishOutput` (hunger shift, HP restore, optional granted effects).

| Glyph | Name | ID | Made from |
|------|------|----|----|
| `%` Bright Red | Cooked Meat | 54 | 1 Raw Meat |
| `%` Yellow | Bowl of Broth | 55 | 1 Raw Meat + 1 Herbs |
| `%` Bright White | Flatbread | 56 | 1 Flour |
| `%` Bright Yellow | Hearty Stew | 57 | 1 Raw Meat + 1 Carrot + 1 Herbs |
| `%` Bright Cyan | Protein Bake | 58 | 1 Synth Protein + 1 Flour + 1 Herbs |
| `%` Bright Magenta | Hero's Feast | 59 | 1 Raw Meat + 1 Synth Protein + 1 Carrot + 1 Flour + 1 Herbs |
| `%` Dark Gray | Burnt Slop | 60 | (failure output from mismatched recipe) |

## Cookbooks

`ItemType::Cookbook`. Eaten/used to learn the dish recipe permanently.

| Glyph | Name | ID | Teaches |
|------|------|----|----|
| `?` Bright Yellow | Cookbook: Hearty Stew | 61 | Hearty Stew recipe |
| `?` Bright Cyan | Cookbook: Protein Bake | 62 | Protein Bake recipe |
| `?` Bright Magenta | Cookbook: Hero's Feast | 63 | Hero's Feast recipe |

## Synthesized Items (tinker output, IDs 1000+)

Created by `synthesize_item` from two known blueprints + materials. Not drop-eligible.

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

## Synthesized items (IDs 1000+)

Same as the table above — these come exclusively from the tinkering synthesizer, not from any drop or shop.
