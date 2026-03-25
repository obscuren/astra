# ASTRA

```
        .              *                              .             |
   *         .                   .                              .  -o-
    █████╗ ███████╗████████╗██████╗  █████╗        *               |
   ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔══██╗            .
   ███████║███████╗   ██║   ██████╔╝███████║   .
 * ██╔══██║╚════██║   ██║   ██╔══██╗██╔══██║        *
   ██║  ██║███████║   ██║   ██║  ██║██║  ██║  .
   ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝
         .          *       .          .
   .            .                 *
```

[![Build](https://github.com/obscuren/astra/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/obscuren/astra/actions/workflows/cmake-multi-platform.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/obscuren/astra?include_prereleases&label=release)](https://github.com/obscuren/astra/releases)
[![Website](https://img.shields.io/badge/web-jeff.lookingforteam.com%2Fastra-blue)](https://jeff.lookingforteam.com/astra)

A sci-fi roguelike set in the far future. Travel across star systems, explore asteroid overworlds, crawl procedurally generated dungeons, trade with merchants, upgrade your starship, and journey toward Sagittarius A* — the supermassive black hole at the center of the galaxy.

## How to Play

### Character Creation

When starting a new game you are guided through a multi-step character creation wizard:

1. **Character Type** — choose Presets (guided flow), or Random (fully randomized)
2. **Race** — pick from six races: Human, Veldrani, Kreth, Sylphari, Xytomorph, Stellari — each with unique attribute modifiers and resistances
3. **Class** — choose your playstyle: Voidwalker (melee tank), Gunslinger (ranged agility), Technomancer (tinkering/intel), Operative (stealth/social), or Marauder (survivalist)
4. **Attributes** — distribute 10 points across Strength, Agility, Toughness, Intelligence, Willpower, and Luck
5. **Name** — name your character or generate a random one
6. **Location** — choose your starting location
7. **Summary** — review your build and begin the journey

All journeys begin aboard **The Heavens Above**, a space station orbiting Jupiter. Trade with merchants, talk to NPCs, equip gear, and board your starship to set out across the galaxy.

### Controls

| Key | Action |
|-----|--------|
| Arrow keys / `hjkl` | Move |
| `o` | Interact with adjacent NPC or object |
| `e` | Equip / use item |
| `d` | Drop item |
| `g` | Pick up item |
| `c` | Character screen |
| `i` | Inventory tab |
| `t` | Target enemy |
| `s` | Shoot (ranged weapon) |
| `r` | Reload |
| `.` | Wait one turn |
| `>` / `<` | Enter / exit stairs, board ship |
| `Tab` / `Shift+Tab` | Cycle side panel tabs |
| `Ctrl+H` | Toggle side panel |
| `Esc` | Close window / pause menu |

### Exploration

Move through the world one step at a time — everything advances when you do. Land on asteroids to explore their overworld surface, discover settlements, outposts, and ruins on detail maps, find cave entrances, and descend into multi-level dungeons filled with loot and enemies.

### Star Chart & Travel

Open the star chart to view the Milky Way at multiple zoom levels — galaxy, region, local cluster, and individual star systems. Systems are procedurally generated with planets, moons, asteroid belts, and space stations left behind by an ancient civilization. Plot hyperspace jumps between systems using your ship's navi computer.

### Starship

Your starship is your home and your way between systems. Walk around its interior, manage systems, and upgrade components like the hyperspace engine and navi computer for longer-range or faster jumps.

### Trading

Interact with merchant NPCs and select "Show me your wares" to open a fullscreen trade window. Browse the merchant's stock on the left and your inventory on the right, buy and sell items with `Space`, and switch sides with `Tab`.

### Combat & Gear

Fight hostile creatures in dungeons with melee and ranged weapons. Equip armor, helmets, shields, and accessories. Items come in multiple rarities — Common, Uncommon, Rare, Epic, Legendary — with random affixes that add stat bonuses. Level up to earn attribute points and skill points for character progression.

### Skills

Spend skill points to unlock eight skill categories: Acrobatics, Short Blade, Long Blade, Pistol, Rifle, Tinkering, Endurance, and Persuasion. Each category contains passive bonuses and active abilities gated by attribute requirements.

### Tinkering

The tinkering workbench (accessible from the character screen) lets you work with equipment and materials:

- **Repair** — restore durability on damaged gear using scrap
- **Disassemble** — break down items into crafting materials (Nano-Fiber, Power Core, Circuit Board, Alloy Ingot)
- **Analyze** — study items to learn their blueprints, recorded in your journal
- **Enhance** — slot crafting materials into equipment enhancement slots for permanent stat boosts
- **Synthesize** — combine two learned blueprints with materials to craft entirely new items

### Character Screen

Press `c` to open the character screen with tabs for Skills, Attributes, Equipment, Tinkering, Journal, Quests, Reputation, and Ship. Allocate attribute points, manage equipment, review learned blueprints, and track faction standings.

### Journal

Blueprint discoveries, encounters, and events are logged in a journal. Each entry records what was learned, where, and when — useful for tracking which blueprints you've unlocked for synthesis.

## Features

- Multi-step character creation with race, class, and attribute selection
- Five playable classes with unique skills and stat profiles
- Six alien races with attribute modifiers and elemental resistances
- Eight skill categories with passive and active abilities
- Tinkering system: repair, disassemble, analyze, enhance, and synthesize items
- Blueprint learning and item synthesis from crafting materials
- Item enhancement slots with material-based stat boosts
- Item affix system with rarity tiers (Common through Legendary)
- Journal system tracking blueprints, discoveries, and events
- Faction reputation tracking
- Character screen with 8 tabs (Skills, Attributes, Equipment, Tinkering, Journal, Quests, Reputation, Ship)
- Procedurally generated galaxy with hundreds of star systems
- Noise-based overworld terrain with biomes, rivers, lakes, and mountains
- Detail maps with settlements, outposts, ruins, and landing pads
- Multi-level dungeon crawling with rooms, corridors, and locked doors
- Day/night cycle with dawn, day, dusk, and night phases affecting visibility
- NPCs with branching dialog trees, shops, and quests
- Fullscreen trade window for buying and selling with merchants
- Full inventory and equipment system
- Ranged and melee combat with targeting
- Four elemental resistances: acid, electrical, cold, heat
- Hunger system affecting health regeneration
- Starship interiors and navigation
- Star chart viewer with galaxy, region, local, and system zoom levels
- Hyperspace travel between star systems
- Space stations with docking and trading
- Save and load with hall of fame for fallen characters
- Turn-based world simulation

## Build

```bash
cmake -B build && cmake --build build
```

Run: `./build/astra`

## Platforms

Linux, macOS, Windows

## License

[MIT](LICENSE)
