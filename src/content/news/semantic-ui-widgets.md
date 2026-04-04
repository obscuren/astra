---
title: "Semantic UI & Widget Panel"
date: "2026-04-01"
tag: "update"
summary: "Complete UI overhaul with semantic rendering pipeline, toggleable widget panel replacing the old tab system, and star chart redesign."
---

The entire UI has been rebuilt around a semantic rendering pipeline. Instead of direct terminal escape sequences scattered through game logic, all rendering now goes through a declarative UI component system.

## Widget Panel

The old tab-based side panel is gone. In its place: a toggleable widget system where multiple widgets can be active simultaneously. Toggle them with function keys:

- **F1** — Messages log
- **F2** — Wait/time controls  
- **F3** — Minimap

Widgets stack vertically and share the panel space. Messages always fills remaining room at the bottom.

## Star Chart Redesign

The galaxy map viewer has been rebuilt with a two-layer architecture: a broad galaxy spiral view and a detailed system view. Quest markers, visited systems, and navigation routes are clearly visible.

## What Changed Under the Hood

The semantic UI migration touched nearly every rendering path in the game. Components like progress bars, text labels, lists, and separators are now rendered through tagged descriptors that the renderer interprets — meaning the same game code can drive both the terminal and (future) SDL renderers without changes.
