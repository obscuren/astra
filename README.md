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

A sci-fi roguelike set in the far future. Travel across star systems, explore asteroid overworlds, crawl procedurally generated dungeons, upgrade your starship, and journey toward Sagittarius A* — the supermassive black hole at the center of the galaxy.

## How to Play

All journeys begin aboard **The Heavens Above**, a space station orbiting Jupiter. From there you can trade with shopkeepers, equip gear, and board your starship to set out across the galaxy.

### Controls

| Key | Action |
|-----|--------|
| Arrow keys / `hjkl` | Move |
| `i` | Inventory |
| `e` | Equip / use item |
| `d` | Drop item |
| `g` | Pick up item |
| `t` | Target enemy |
| `s` | Shoot (ranged weapon) |
| `r` | Reload |
| `m` | Star chart |
| `>` / `<` | Enter / exit stairs, board ship |
| `p` | Pause menu |
| `Esc` | Close window / cancel |

### Exploration

Move through the world one step at a time — everything advances when you do. Land on asteroids to explore their overworld surface, find cave entrances, and descend into multi-level dungeons filled with loot and enemies.

### Star Chart & Travel

Open the star chart to view the Milky Way at multiple zoom levels — galaxy, region, local cluster, and individual star systems. Systems are procedurally generated with planets, moons, asteroid belts, and space stations left behind by an ancient civilization. Plot hyperspace jumps between systems using your ship's navi computer.

### Starship

Your starship is your home and your way between systems. Walk around its interior, manage systems, and upgrade components like the hyperspace engine and navi computer for longer-range or faster jumps.

### Combat & Gear

Fight hostile creatures in dungeons with melee and ranged weapons. Equip armor, helmets, shields, and accessories. Level up to improve your stats — health, attack, defense, quickness, and move speed.

## Features

- Procedurally generated galaxy with hundreds of star systems
- Noise-based overworld terrain with biomes, rivers, lakes, and mountains
- Multi-level dungeon crawling with rooms, corridors, and locked doors
- NPCs with shops and dialog
- Full inventory and equipment system
- Ranged and melee combat with targeting
- Starship interiors and navigation
- Star chart viewer with galaxy, region, local, and system zoom levels
- Hyperspace travel between star systems
- Space stations with docking and trading
- Save and load system
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
