# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Astra is a roguelike dungeon crawler written in C++20 with pluggable renderer backends.

## Build

```bash
# Terminal only (default)
cmake -B build && cmake --build build

# With SDL3 support
cmake -B build -DSDL=ON && cmake --build build
```

Run: `./build/astra` (add `--sdl` for graphical mode, `--term` is default).

## Architecture

Game logic is completely decoupled from platform code via an abstract `Renderer` interface (`include/astra/renderer.h`). Two implementations exist:

- **TerminalRenderer** — POSIX raw terminal with escape sequences and character buffer
- **SdlRenderer** — SDL3 + SDL3_ttf graphical window with monospace font grid

`Game` owns a `unique_ptr<Renderer>` and drives a 60fps loop: `poll_input → handle_input → update → render`. Game state is managed via `GameState` enum (MainMenu, Playing).

Key virtual keycodes (`KEY_UP`, `KEY_DOWN`, etc.) are defined in `renderer.h` so both backends return the same values for arrow keys.

## Conventions

- Namespace: `astra`
- Member variables: `snake_case_` (trailing underscore)
- Classes: PascalCase
- Headers: `#pragma once`, located in `include/astra/`
- Compile definition for SDL: `ASTRA_HAS_SDL`
- Default compile: Terminal
- Update `docs/roadmap.md` and checkof boxes as wel implement features.
- All game formulas go in to `docs/formulas.md` and must be updated.

## Rules

- **Platform isolation**: game logic (`game.h/cpp`, `renderer.h`, `options.h`) must contain zero platform-specific includes, ifdefs, or system calls. All platform differences live in renderer implementations.
- SDL work is deferred — focus on the terminal renderer unless explicitly asked otherwise.

## Custom Agents

- `cross-platform` — audits for platform-specific code leaking into shared logic
- `terminal-review` — reviews terminal renderer for correctness, performance, and interface compliance
- `feature-suggestion` - review new gameplay features to determine the level of funness and complexity to implement

## CI

GitHub Actions runs CMake builds on Ubuntu (gcc, clang) and Windows (MSVC) on pushes to main and PRs. Website deploys from `gh-pages` branch via Astro.

## Gameplay

Astra is a random dungeon very much like rogue, nethack, admom and moria. However as most of these games
are set in a fantasy world, this one is set in the far future (hence sci fi). The goal of the game is to travel to
the center of the galaxy and enter the Sagittarius A* (Sgr A*) blackhole, in which the player starts again in a newly
generated world, an intact startship but infinite knowledge of the universe.

The world progresses by player ticks, meaning the world is on pause between key presses.

All stories begin on the Space Station above Jupiter called 'The Heavens Above'. This is similar to other games "towns"
in which the player can move around, interact with shop keepers (some not speaking the same language) where they can
buy and sell stuff. Space is also littered with astroid belts. Astroids can be landed on and mostly contain a overworld
and an underworld (dungeon) which can be crawled and is full of rewards (items, new gear, monsters, etc.).

The player's starship is also docked here. The player's startship is how the player must travel from system to system.
All systems are randomly generated and use the real map of the universe. System are generated based on a seed. All systems
have either 1 start or 2 and are called a binary star system. About 80% of all systems have a space station, left by an 
ancient civilization (to be designed). Travel works by traversing blackholes and require a Hyperspace Engine and a
Navi Computer. 

Star ships can be upgraded, for example a navi computer can be upgraded for further plotting or quicker calculation of
routes.

New star ships can be purchased and is a to be discussed feature.

Players can upgrade their own gear and equip them, improve their stats and use weapons for fighting.

### The Hero

You play the hero of this story. The hero of the story (for now) is displayed as '@' when crawling dungeons or when moving
through space.

### Space ship

When the player enter's their ship, we'll see the star ship and navigation is passed to the star ship. All star ships
have interior and can be navigated (underworld). All ships have their own unique layout.

### Stats

Both the star ship and player have their own stats. Stats are yet to be determined but one thing is sure, it has health (both).

## UI

The player's screen, when playing, should display the gaming UI. On the right side (about 20%) we'll have a pane that displays information 
regarding the player or startship (like health, condition, etc) and right we'll the gaming world; either the overworld or
underworld.

The UI will also have several different "windows" that may display information or windows that cover the entire screen.
The inventory for example displays the players items in their "bag" and what they have equipped.

## World persistence

The entire world state need to be able to be persisted. 

## Plans

After each plan is made also write it to the docs/plans/ folder so we can track the plans

## C++ structure

Keep files consistent and consice. Make sure that logic is not all dumped in one monolithic file. Keep neat classes, use proper inheritance where it make sense. 

* All input should be handled by the InputManager
* Containerize code
    - InputManager handles input
    - HelpScreen handles help related things
    - Renderer is for rendering
    - etc.
* The Game class should be a coordinator rather than a CONTAIN everything
