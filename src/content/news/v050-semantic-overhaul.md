---
title: "v0.5.0 — The Semantic Overhaul"
date: "2026-04-04"
tag: "release"
summary: "Massive UI rebuild with semantic rendering, toggleable widget panel, half-block minimap, new Wayfinding skills, skill tree redesign, and star chart overhaul."
---

The v0.5.0 release is the largest update since launch — a ground-up rebuild of how Astra renders its interface, plus new gameplay systems layered on top.

## Semantic Rendering Pipeline

Every UI element in the game now flows through a declarative semantic system. Instead of raw terminal escape codes scattered through game logic, components like progress bars, text labels, lists, and separators are described as tagged structures that the renderer interprets. This means the same game code will drive both terminal and future graphical renderers without changes.

The migration touched nearly every rendering path: the main HUD, side panel, character screen, dialog system, dev console, help overlay, and star chart viewer.

## Widget Panel

The old tab-based side panel is replaced by a toggleable widget system. Multiple widgets can be active simultaneously, stacked vertically:

- **F1** — Messages log
- **F2** — Wait / time controls
- **F3** — Minimap

Toggle any combination. Messages always fills the remaining space at the bottom.

## Minimap

A new half-block pixel minimap renders the current map using upper-half-block characters with foreground and background colors, packing two map rows per terminal cell. Combined with 3x3 downsampling, each widget cell covers a 3x6 block of map tiles — enough to see the broader dungeon layout at a glance.

Features:
- Player-centered viewport with edge clamping
- Works across all map types: dungeons, stations, detail maps, overworld
- Fog of war for dungeons, schematic outlines for stations
- Portals and exits always visible
- NPC and item display gated by new Wayfinding skills

## New Wayfinding Skills

Two new skills in the Wayfinding category unlock minimap features:

- **Scout's Eye** (75 SP, INT 13) — reveals all NPCs on the minimap
- **Cartographer** (100 SP, INT 14) — shows items and points of interest

## Skill Tree Redesign

The character screen skills tab has been rebuilt with background-color category bars, clean skill entries, and 2D grid navigation for both the equipment paper doll and attribute allocation.

## Star Chart Overhaul

The galaxy map viewer now uses a two-layer architecture: a broad spiral galaxy view and a detailed system view with orbital body information. Quest markers, visited systems, and navigation routes are clearly rendered through the new semantic UI components.

## Main Menu Polish

The main menu received a visual refresh: gradient block-letter logo, version string, flavor text, and a sparse starfield with a clear void around the title.

## Quality of Life

- Terminal redraws correctly on window resize
- `player_has_skill` moved from tinkering to skill_defs for cleaner architecture
- Old UI classes removed — complete migration to semantic rendering
