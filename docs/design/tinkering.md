# Tinkering

Astra's crafting system: repair, enhance, refine, synthesize, and craft consumables at any workbench.

**Skills:**
- `Basic Repair` — gates repair / enhance / refine
- `Cat_Tinkering` — gates analyze / synthesize / craft schematic
- `Disassemble` — gates salvage
- `Synthesize` — sub-skill, gates the synthesizer step

**Workbench actions** (Tinkering tab in the Character screen):
- `[r]` Repair — restore durability with Nano-Fiber
- `[a]` Analyze — learn a blueprint from an equipped item (item may be destroyed)
- `[s]` Salvage — destroy an item, receive tier-weighted materials
- `[f]` Assemble — commit staged enhancements to the workbench item
- `[x]` Clear — undo a staged enhancement
- `[y]` Synth — combine two known blueprints + materials → new equipment
- `[R]` Refine — convert junk + low-tier reagents into Tier 2 materials
- `[C]` Craft — craft a consumable from a learned schematic

## Materials

**24 materials across 3 tiers.** Materials drop from monsters, chests, and merchants. Junk-typed reagents (Scrap, Broken Circuit, etc.) are still sellable as junk but also count as crafting reagents.

### Tier 1 — Common

| Glyph | Name | ID | Sell | Notes |
|---|---|---|---|---|
| `~` Dark Gray | Scrap Metal | 30 | 1 | Junk-typed |
| `~` Dark Gray | Broken Circuit | 31 | 2 | Junk-typed |
| `~` Dark Gray | Empty Casing | 32 | 1 | Junk-typed |
| `,` Yellow | Copper Wire | 7010 | 2 | Stack name "Strand of Copper Wire" |
| `,` White | Polymer Strip | 7011 | 2 | Flexible plastic |
| `,` Cyan | Glass Shard | 7012 | 1 | Optic / vial reagent |
| `,` Bright Yellow | Adhesive Resin | 7013 | 2 | Bonding agent |
| `,` Blue | Coolant Vial | 7014 | 3 | Thermal reagent |

### Tier 2 — Uncommon

| Glyph | Name | ID | Sell | Notes |
|---|---|---|---|---|
| `+` Cyan | Nano-Fiber | 7001 | 8 | Repair reagent |
| `+` Yellow | Power Core | 7002 | 12 | Energy recipes |
| `+` Green | Circuit Board | 7003 | 10 | Tech recipes |
| `+` White | Alloy Ingot | 7004 | 10 | Heavy/armor recipes |
| `~` Yellow | Spare Parts | 47 | 6 | Junk-typed; mechanical |
| `~` Cyan | Circuitry | 48 | 8 | Junk-typed; in most recipes |
| `+` Bright White | Nano Lattice | 7020 | 14 | Advanced structural |
| `+` Cyan | Polished Lens | 7021 | 12 | Optics |
| `+` Bright Yellow | Micro-Servo | 7022 | 14 | Mechanical |
| `+` Red | Plasma Cartridge | 7023 | 16 | Energy weapon |

### Tier 3 — Rare

| Glyph | Name | ID | Sell | Notes |
|---|---|---|---|---|
| `*` Bright Magenta | Quantum Resonance Crystal | 7030 | 50 | AI Module input |
| `*` Bright White | Strange Strobing Crystal | 7031 | 60 | Future legendary |
| `*` Bright Yellow | Prime Catalyst | 7032 | 55 | Future exotic |
| `*` Cyan | Prime Filament | 7033 | 55 | Future exotic |
| `*` Magenta | Voidshard | 7034 | 70 | Future warp |
| `*` Blue | Phase Coil | 7035 | 65 | Future phase |

## Refinement (junk → T2)

Skill: Basic Repair. Hotkey `[R]` opens the recipe picker.

| Recipe | Inputs → Output |
|---|---|
| Smelt Alloy Ingot | 3× Scrap Metal → 1× Alloy Ingot |
| Recover Circuit Board | 2× Broken Circuit + 1× Copper Wire → 1× Circuit Board |
| Spin Nano-Fiber | 4× Empty Casing + 1× Adhesive Resin → 1× Nano-Fiber |
| Assemble Power Core | 2× Spare Parts + 1× Copper Wire → 1× Power Core |
| Build Circuitry | 1× Broken Circuit + 1× Spare Parts + 1× Copper Wire → 1× Circuitry |
| Polish Lens | 2× Glass Shard + 1× Polymer Strip → 1× Polished Lens |
| Tune Micro-Servo | 2× Spare Parts + 1× Coolant Vial → 1× Micro-Servo |
| Weave Nano Lattice | 3× Nano-Fiber + 1× Polymer Strip → 1× Nano Lattice |
| Pressurize Plasma Cartridge | 2× Power Core + 1× Coolant Vial → 1× Plasma Cartridge |

## Blueprints (paired)

Equipment recipes use **two** blueprints learned by `analyze`-ing existing equipment. The 12 blueprints by category:

- **Ranged:** Plasma Emitter, Grip Assembly, Power Conduit
- **Melee:** Blade Housing, Hilt Assembly, Edge Material
- **Armor:** Plating Alloy, Padding Weave, Joint Mechanism
- **Accessory:** Optic Module, Thruster Core, Storage Frame

## Synthesis recipes (15)

Skill: Cat_Tinkering + Synthesize. Hotkey `[y]` triggers from the synthesizer once both blueprints are loaded. Most recipes take Scrap and Circuitry; Reinforced Pack and Armored Blade are intentionally Circuitry-free (mechanical theme).

| Recipe | Material Costs |
|---|---|
| Plasma Edge | 2 Scrap + 1 Circuitry + 1 Power Core + 1 Alloy Ingot + 1 Plasma Cartridge |
| Thruster Plate | 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo |
| Targeting Array | 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens + 2 Copper Wire |
| Dual-Edge | 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Nano-Fiber |
| Reinforced Pack | 2 Scrap + 2 Nano-Fiber + 1 Polymer Strip + 1 Adhesive Resin |
| Overcharged Engine | 2 Scrap + 1 Circuitry + 2 Power Core + 1 Coolant Vial + 1 Plasma Cartridge |
| Articulated Armor | 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo + 1 Nano Lattice |
| Guided Blaster | 2 Scrap + 1 Circuitry + 1 Power Core + 1 Polished Lens + 1 Plasma Cartridge |
| Combat Gauntlet | 2 Scrap + 1 Circuitry + 1 Alloy Ingot + 1 Nano-Fiber + 1 Micro-Servo |
| Armored Blade | 4 Scrap + 2 Alloy Ingot + 1 Nano-Fiber |
| Reinforced Casing (mat) | 2 Scrap + 1 Alloy Ingot + 1 Polymer Strip |
| Receptor Plate (mat) | 1 Scrap + 1 Circuitry + 1 Copper Wire + 1 Polished Lens |
| Brass Conduit (mat) | 1 Scrap + 1 Copper Wire + 1 Power Core |
| AI Module (mat) | 2 Scrap + 1 Circuitry + 1 Circuit Board + 1 Nano Lattice + 1 Quantum Resonance Crystal |
| Light Sensor (mat) | 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens |

## Schematics & Consumables

Schematics are **single-use pickups**: read one and the recipe is permanently added to your codex. The crafted consumable exists in inventory but its **gameplay use** (throw / inject / detonate) is not yet implemented and lands in the next spec.

Skill: Cat_Tinkering. Hotkey `[C]` opens the picker.

### Stims

| Item | Recipe Cost |
|---|---|
| Healing Stim | 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Glass Shard |
| Adrenaline Stim | 1 Scrap + 1 Empty Casing + 1 Power Core + 1 Coolant Vial |
| Endure Stim | 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Adhesive Resin |
| Focus Stim | 1 Scrap + 1 Empty Casing + 1 Polished Lens + 1 Coolant Vial |
| Berserker Stim | 2 Scrap + 1 Empty Casing + 1 Power Core + 1 Plasma Cartridge |
| Medkit | 2 Scrap + 1 Polymer Strip + 2 Nano-Fiber + 1 Adhesive Resin |

### Grenades

| Item | Recipe Cost |
|---|---|
| Frag Grenade | 2 Scrap + 1 Empty Casing + 1 Power Core + 1 Adhesive Resin |
| EMP Grenade | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Circuit Board + 1 Copper Wire |
| Incendiary Grenade | 2 Scrap + 1 Empty Casing + 1 Coolant Vial + 1 Plasma Cartridge |
| Smoke Grenade | 1 Scrap + 1 Empty Casing + 1 Polymer Strip + 1 Adhesive Resin |
| Flashbang | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Glass Shard + 1 Power Core |

### Mines

| Item | Recipe Cost |
|---|---|
| Proximity Mine | 2 Scrap + 1 Empty Casing + 1 Circuitry + 1 Power Core + 1 Spare Parts |
| EMP Mine | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Circuit Board + 1 Spare Parts |
| Incendiary Mine | 2 Scrap + 1 Empty Casing + 1 Circuitry + 1 Plasma Cartridge + 1 Spare Parts |
| Decoy Mine | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Copper Wire + 1 Spare Parts |
| Caltrops | 3 Scrap + 1 Adhesive Resin |

### Turrets

`ItemType::Turret`. Deployable autonomous defenders. Sentry Drone is the mobile variant; the others are stationary.

| Item | Recipe Cost |
|---|---|
| Auto-Turret | 3 Scrap + 1 Empty Casing + 1 Spare Parts + 1 Power Core + 1 Alloy Ingot |
| Flame Turret | 3 Scrap + 1 Circuitry + 1 Spare Parts + 1 Coolant Vial + 1 Plasma Cartridge |
| Arc Turret | 2 Scrap + 1 Circuitry + 2 Copper Wire + 1 Power Core + 1 Polished Lens |
| Sentry Drone | 3 Scrap + 1 Circuitry + 1 Micro-Servo + 1 Nano Lattice + 1 Power Core + 1 Plasma Cartridge |

## Salvage

Skill: Disassemble. Hotkey `[s]` (item must be on the workbench). Salvage destroys the item and returns tier-weighted materials.

| Item rarity | Yield | Tier weights |
|---|---|---|
| Common | 1-2 | 100% T1 |
| Uncommon | 2-3 | 70% T1 / 30% T2 |
| Rare | 2-3 | 40% T1 / 60% T2 |
| Epic | 3 | 20% T1 / 70% T2 / 10% T3 |
| Legendary | 3-4 | 60% T2 / 40% T3 |

Quest items cannot be salvaged.
