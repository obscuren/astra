---
title: "v0.6.0 — Lore, Terrain v2, POI Budget"
date: "2026-04-12"
tag: "release"
summary: "Procedural galactic history, complete terrain rewrite, civilization ruins, settlement generators, deterministic POI budgets, and the first SDL renderer preview."
---

**[View the full release on GitHub](https://github.com/obscuren/astra/releases/tag/v0.6.0)**

The v0.6.0 release transforms how Astra's worlds are built. Every planet now has a history — civilizations that rose and fell over billions of years, leaving scars on the terrain and ruins beneath the surface. The entire detail map generator has been rewritten from scratch, settlements and ruins are full procedural structures, and a new POI budget system makes planet content deterministic and scannable.

## Procedural World Lore

When you start a new game, Astra now simulates billions of years of galactic history before you take your first step. Between 4 and 8 precursor civilizations emerge across the timeline, each with unique traits that shape their behavior — expansionist empires, isolationist scholars, aggressive warmongers.

![Universe generation — watching civilizations rise and fall in real time](/astra/news/v060/2.png)

These civilizations interact: they form alliances, go to war, share knowledge, and eventually collapse under entropy. The simulation produces a rich timeline of events — first contact, golden ages, weapon tests, territorial wars, extinction events — all woven into multi-sentence narrative records.

A phoneme-based naming system generates civilization names, historical figures, and artifact names, giving each civilization a distinct linguistic feel.

![The lore viewer — browse billions of years of galactic history with a visual timeline](/astra/news/v060/1.png)

The full history is browsable in-game through a lore viewer with a species relations diagram and visual timeline.

## Terrain Shaping from Lore

History doesn't just exist in text — it shapes the terrain you walk on.

**Alien terrain** appears where civilizations terraformed planets. Overworld tiles shift to alien biomes with architecture-specific colors. Detail maps inherit the alien palette — crystalline ridges, organic flesh-walls, geometric grid floors, void fissures, or luminous plains depending on the civilization.

**Scar terrain** marks where ancient weapons were tested or battles raged. Scorched earth and glassed craters appear on the overworld, and entering these tiles now generates detail maps using dedicated scar biome profiles — jagged rubble for scorched zones, flat fused-glass plains with angular wall clusters for glassed areas.

![Overworld with scar terrain — scorched earth and alien patches visible across the surface](/astra/news/v060/3.png)

Lore markers appear on the star chart for significant systems, and dungeon entry text tells you which civilization built the ruins you're about to enter.

## Detail Map Generation v2

The old detail map generator produced claustrophobic, maze-like maps where every biome felt the same — just noise thresholds with different wall densities. It's been completely replaced.

The new system layers elevation, moisture, and structure into distinct terrain features. Floor is the default — walls are rare and intentional. Mountains have cliff bands and narrow passes. Forests are dense canopy with winding clearings. Swamps have raised platforms in standing water. Volcanic terrain has lava channels carved through rock.

19 biome profiles define distinct terrain across natural, alien, and scar worlds. Each biome feels genuinely different to explore — not just the same maze with a different wall color.

Maps are now much larger per overworld tile, with Zelda-style viewport scrolling. Neighboring tiles blend at edges, so biome transitions are seamless as you cross the overworld.

Tall trees in forests and crystal spires in ice biomes now block your line of sight while remaining walkable, creating tactical terrain variety. Open plains and deserts let you see far; jungles and fungal forests keep you guessing what's around the next corner.

## Settlement POI

Settlements are now full procedural structures. A placement scorer finds suitable terrain, then a planner lays out buildings, paths, and a defensive perimeter. Buildings get furnished interiors — table-and-bench pairs, wall-mounted shelves, crafting stations. NPCs spawn in appropriate roles based on the settlement's civilization style: frontier outposts get traders and guards, advanced settlements get scholars and engineers, ruined settlements get scavengers.

![A settlement on a forested world — buildings with furnished interiors, paths, and NPCs](/astra/news/v060/4.png)

![Settlement close-up — multiple buildings connected by paths, with crafting stations and shelves](/astra/news/v060/5.png)

## Ruins POI v2

Ruins are no longer simple room-and-corridor dungeons. They're sprawling wall networks — thick interconnected walls forming a megastructure footprint, with each precursor civilization leaving a distinct visual signature: angular geometric walls, organic tendrils, crystalline lattices.

Decay varies across each ruin — one wing might be pristine while the other crumbles into rubble. Enclosed rooms are detected and given thematic content based on the civilization that built them.

![Civilization ruins — a wall network megastructure with water features and scattered loot](/astra/news/v060/6.png)

## Overworld Generation

Every planet's surface is now generated through a layered simulation. The generator starts with a Voronoi region map that carves the surface into natural-looking continental zones. Temperature gradients run from pole to equator, driving biome selection — polar ice caps at the extremes, temperate forests and grasslands in the mid-latitudes, deserts and volcanic terrain near the equator.

Moisture is simulated independently, creating rivers that flow through valleys and lakes that pool in low-lying basins. Mountain ranges rise where tectonic boundaries would form, and their peaks pierce through polar ice when they're tall enough. Forests render in multiple shades of green, giving wooded areas a sense of depth rather than a flat uniform color.

Different planet types get dedicated generators — Earth-like worlds use the full temperate simulation, while Mars-like bodies get a cold rocky generator that produces barren, cratered terrain with thin frost patches.

![Earth's overworld — temperate forests, rivers, mountains, and polar ice generated from the layered simulation](/astra/news/v060/11.png)

## New POI Types

Three new POI generators join settlements and ruins:

**Outposts** — fenced forts with a main building, exterior tents, campfires, and biome-themed palisade walls.

**Crashed ships** — three classes (escape pod, freighter, corvette) with 4-way orientation, long scorched skid marks that plow through scatter, and a rare chance of a dungeon entrance beneath the wreckage.

**Cave entrances** — three variants (natural cave, abandoned mine, ancient excavation) embedded against cliff faces, with lore-weighted selection favoring excavation sites near civilization ruins.

## POI Budget System

Planet content is no longer random. Each planet now rolls a deterministic POI budget from its seed — exactly how many settlements, outposts, caves, ruins, and crashed ships it contains. This budget drives a unified placement pass that scores candidate sites against terrain requirements.

Some ruins are hidden — they look like normal terrain on the overworld until you stumble onto them, triggering a discovery event logged to your journal with a live map preview.

The star chart's planet info panel now shows a Scanner Report with POI counts, giving you intel before you land.

![Star chart Scanner Report — see what awaits on a planet before landing](/astra/news/v060/8.png)

## Archaeology Skills

Six new skills in the Archaeology category: Ruin Reader, Artifact ID, Excavation, Cultural Attunement, Precursor Linguist, and Beacon Sense. These will tie into the lore systems as effects are implemented.

## World Persistence

Previously visited maps now survive save and load. Before this release, leaving a detail map and saving meant losing all changes — dropped items, killed NPCs, discovered POIs. Now every map you've visited is preserved exactly as you left it.

## SDL Renderer Preview

The first functional SDL renderer is available on the `sdl` branch. It renders the game in a graphical window — same monospace cell grid as the terminal, with full color support, UTF-8 character rendering, and dynamic window resizing. The main menu renders identically to the terminal version. This is the foundation for eventual tile-based graphics.

![The SDL renderer — ASTRA running in a native window for the first time](/astra/news/v060/10.png)
