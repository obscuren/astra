---
title: "Minimap & Wayfinding Skills"
date: "2026-04-04"
tag: "feature"
summary: "Half-block pixel minimap with 3x downsampling, player-centered scrolling, and two new Wayfinding skills: Scout's Eye and Cartographer."
---

The side panel now features a toggleable minimap widget (F3) that renders the current map using half-block pixel characters for double vertical resolution.

## How It Works

Each terminal cell packs two map rows using upper-half-block characters with foreground and background colors. Combined with 3x3 downsampling, a 28x10 widget covers roughly 84x60 map tiles — enough to see the broader layout at a glance.

The viewport is player-centered and scrolls as you move. At map edges it clamps rather than showing blank space.

## All Map Types

The minimap works across every map type:

- **Dungeons** — walkable space rendered as filled blocks, fog of war hides unexplored areas
- **Stations** — schematic mode shows faint wall outlines for unexplored areas
- **Detail maps** — terrain-colored fills with structure outlines
- **Overworld** — biome-colored terrain with POI markers

## New Wayfinding Skills

Two new skills in the Wayfinding category unlock minimap features:

- **Scout's Eye** (75 SP, INT 13) — reveals all NPCs on the minimap as colored markers
- **Cartographer** (100 SP, INT 14) — shows items and points of interest on the minimap

Without these skills, the minimap shows terrain, your position, and exits only — keeping exploration rewarding as you invest in navigation.
