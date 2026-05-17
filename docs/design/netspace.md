# ASTRA :: The Net — Design Document

> A design specification for jacking-in, netspace generation, in-net combat, programs, daemons, and the visual language of cyberpunk netrunning in Astra.

> **Naming.** "The Net" and "the Relay Network" (or just "the Relay") refer to the same thing and are used interchangeably throughout this doc — "the Net" is Drifter slang, "the Relay Network" is the in-universe technical name (see [`../lore/overview.md`](../lore/overview.md) § 4). The player is the **Drifter**, jacked in through their **cyberdeck**. Cyberpunk vocabulary (ICE, Black ICE, Blackwall, `.exe`/`.daemon`, BARK, etc.) is surface texture the Drifter perceives — woven on top of the canonical Drifter / Relay / cyberdeck register.

---

## Table of Contents

1. [Core Concept](#core-concept)
2. [The In-Net UI Window](#the-in-net-ui-window)
3. [The Jack-In Ritual](#the-jack-in-ritual)
4. [Netspace Generation: Targets, Shapes, and Grammars](#netspace-generation-targets-shapes-and-grammars)
5. [Netspace Examples](#netspace-examples)
6. [Combat Design](#combat-design)
7. [Programs and Fragments](#programs-and-fragments)
8. [Daemons (Hardcoded Programs)](#daemons-hardcoded-programs)
9. [Boss AIs](#boss-ais)
10. [Visual Language Reference](#visual-language-reference)
11. [Design Principles](#design-principles)

---

## Core Concept

The Net in Astra is not a single dungeon — it is a **set of procedural micro-dungeons spawned by acts of hacking in the meatworld**. Every hackable object (door, camera, ATM, vending machine, NPC's cortex jack, mainframe, traffic light) becomes its own netspace when the player jacks in, with a layout, theme, and difficulty determined by the *kind of thing* being hacked.

The Drifter's program builder — the Scratch-Python-style fragment editor already implemented — is the player's **combat language**. Programs the player composes outside combat become the *plans they execute under live pressure* inside it.

Crucially: **inside the Net, time dilates relative to meatspace, but meatspace continues to tick.** A door hack passes in fractions of a second. A mainframe hack passes in real meatworld minutes — during which enemies reposition, alarms escalate, and the Drifter's exposed body is *somewhere*. Jacking in is a tactical commitment, not a free action.

### The Loop

1. Player encounters a hackable object in the meatworld.
2. Player chooses to jack in (paying the time and exposure cost).
3. A procedural netspace is generated, with shape and difficulty seeded by the object type.
4. Player navigates the netspace, fighting hostile processes (ICE) and gathering loot.
5. Player either jacks out voluntarily (banking loot) or is forced out (losing it, taking damage, raising trace).
6. The meatworld has advanced. Consequences propagate.

---

## The In-Net UI Window

Astra has a dedicated **In-Net UI window** that opens on top of the rest of the game whenever the player jacks in. This window is not a HUD overlay — it is a **second active window with its own playfield**. The Net is rendered entirely inside it. The meatworld continues to render and update in its own window beneath.

Think of it as opening a new application window on top of a desktop: the desktop is still there, still rendering, still updating, but the new window has full focus and its own dedicated content area. The In-Net UI window contains everything net-related — the playfield (rooms, pipes, walls, entities), the title bar, the deck panel, the vitals bar, the command line, and every animation, transition, and corruption effect described in this document.

This is the most important architectural point in the document. **Every visual effect described in the sections below happens inside the In-Net UI window, not on the meatworld UI underneath it.** And the jack-in animation is the *window itself opening* — the animation plays inside the window as it materializes.

### What the window owns

The In-Net UI window is a self-contained visual context that opens on jack-in and closes on jack-out. It owns:

- **The net playfield** — the rooms, pipes, walls, payloads, and entities that make up the netspace
- The title bar (target name, status, glitch state)
- The deck panel (compiled programs, key bindings, cooldowns)
- The vitals bar (HP, RAM, trace)
- The command line (deck output, dialogue, Drifter voice)
- All net-side animations, transitions, and corruption effects
- The jack-in / jack-out sequence itself, which plays *inside* the window as it opens or closes

The meatworld UI continues to render and update in its own window underneath the In-Net UI window for the duration of the run. It is not paused, hidden, or frozen. The meatworld is still simulating, and the player can sometimes catch glimpses of what is happening to their body, or hear meatworld events leaking through.

### Composition between the two windows

Because both windows are live and rendering in parallel, several design hooks become possible:

- **Periphery awareness.** The meatworld window can be partially visible at the edges of the In-Net UI window — dimmed, blurred, or letterboxed — so the player feels their body's location while jacked in.
- **Meatworld events bleed in.** Footsteps approaching the Drifter's body, sirens outside, an NPC walking into the room — these can interrupt the In-Net UI window with a brief notification or a glimpse of the meatworld behind. "Cop entered the alley" appears in the command line. The player must decide whether to jack out.
- **The two timescales coexist visually.** The In-Net UI window animates at one tempo (turn-based, fast). The meatworld below ticks at its own slower pace. Players can sense this — a guard might be a few frames closer each time they notice.
- **Forced close from meatspace.** If the Drifter's meatbody takes damage or is moved (someone unplugs the cable), the meatworld can trigger a forced close of the In-Net UI window regardless of net-side state.

### Why this matters for the rest of the doc

Several mechanics in this document are described as "the UI corrupts" or "the screen lies" or "the frame breaks." These are all properties of the In-Net UI window in specific states, **not the meatworld UI**. Treat them as state-driven render modes of the net window:

| State | What the In-Net UI window does |
|---|---|
| **Opening** | Jack-in animation plays inside the window as it materializes. |
| **Stable** | Default rendering. Clean borders, accurate readouts, predictable layout. |
| **Stressed** (trace 40–70%) | Subtle flicker on the title bar. Occasional glyph swap at screen edges. |
| **Hunted** (trace 70–95%) | Border crawls. Vitals readout begins to lie (`9?%` instead of HP value). Command line whispers glitched text. |
| **Critical** (trace 95%+ or Black ICE inbound) | UI elements visibly degrade. Random fragments of layout flicker out. |
| **Black ICE takeover** | Full-window override for ~1 second. The window's contents abandon normal rendering for the announcement frame. Snaps back into combat state immediately after. |
| **Blackwall** | Wrong-rendering mode. Borders break. Rooms drift between frames. Glyphs from outside the normal vocabulary leak in. The window behaves as if its own rendering rules are unreliable. |
| **Closing** | Jack-out animation plays inside the window as it tears down. |

The meatworld UI does *not* enter any of these states. It renders normally throughout, regardless of what is happening inside the In-Net UI window.

### The window opening as the jack-in animation

The jack-in animation in the next section is **the In-Net UI window opening on top of the still-running meatworld UI**. The animation does not happen *before* the window appears — it plays *inside* the window as the window materializes and stabilizes. Specifically:

- Frames 1–2 show only the meatworld UI. The In-Net UI window has not yet opened.
- Frame 3 is the moment the window opens. Its initial rendering is intentionally unstable — inverted, dissolving — to sell the neural transition. The meatworld UI is still rendering beneath, but the new window has taken focus.
- Frames 4–5 show the In-Net UI window in its earliest, unresolved state. The playfield has not yet rendered — only the abstract dissolution effect. The meatworld continues to update beneath but is largely obscured by the new window.
- Frame 6 is the In-Net UI window fully resolved. The playfield is now drawn inside it. The window is in its stable rendering state. The meatworld is still ticking beneath.

Jack-out plays this in reverse: the playfield dissolves inside the window, intermediate dissolution frames render, and finally the window closes — revealing the meatworld UI again, which has continued to update the entire time.

### Design implication

Because the In-Net UI is its own window with its own playfield, *every UI-state change described in this document is local to that window*. The meatworld UI does not glitch when the Drifter takes a Black ICE hit. The meatworld UI does not start lying when trace hits 87%. The meatworld playfield does not distort when entering a Blackwall tear. Those effects are deliberate, contained properties of the Drifter's neural perception of the Net — and they live entirely inside the net window.

This also means the In-Net UI window is a **single development target**. Building the jack-in animation, Black ICE takeover, Blackwall distortion, trace corruption, and program execution animation are all "modify the In-Net UI window's render state." They share infrastructure. The meatworld UI remains a separate, simpler concern that runs uninterrupted underneath.

---

## The Jack-In Ritual

*Implemented in Phase 3.*

The transition between meatspace and netspace is the moment the player commits. It must feel ritualistic — long enough to register as a decision, short enough not to annoy on the hundredth run. **Target: 2–3 seconds, 6 frames, skippable after first run with a held key.**

Mechanically, this is the sequence that **opens the In-Net UI window** on top of the still-running meatworld UI. The meatworld continues to render and tick throughout the entire sequence — the In-Net UI window is what is opening, not the meatworld closing. The animation plays *inside the new window* as it materializes. From frame 3 onward, the window is open and rendering — first in unstable, dissolving states (frames 3–5), then snapping into its stable rendering mode at frame 6, at which point the net playfield is fully drawn inside the window. Jack-out plays this in reverse, with the window closing at the end.

### Frame-by-Frame

#### Frame 1 — Meatspace, the moment before

```
╔══════════════════════════════════════════════════════╗
║ BACK ALLEY :: 23:47 :: heart rate 88                 ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ████████████████████████████████████████           ║
║   █                                      █           ║
║   █   ┌──┐                       ┌──┐    █           ║
║   █   │  │   . . . @ . . .       │  │    █           ║
║   █   │  │           ▲           │  │    █           ║
║   █   └──┘     jack cable        └──┘    █           ║
║   █                                      █           ║
║   ████████████████████████████████████████           ║
║                                                      ║
║   > jack into terminal? [Y/N]                        ║
╚══════════════════════════════════════════════════════╝
```

#### Frame 2 — Confirmation. The world holds its breath.

```
╔══════════════════════════════════════════════════════╗
║ BACK ALLEY :: 23:47 :: heart rate 91                 ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ████████████████████████████████████████           ║
║   █                                      █           ║
║   █   ┌──┐                       ┌──┐    █           ║
║   █   │  │   . . . @ . . .       │  │    █           ║
║   █   │  │           │           │  │    █           ║
║   █   └──┘           │           └──┘    █           ║
║   █                  ▼                   █           ║
║   ████████████████████████████████████████           ║
║                                                      ║
║   > establishing handshake...                        ║
╚══════════════════════════════════════════════════════╝
```

Same map. Cable drops. Heart rate ticks up — the character cares.

#### Frame 3 — The screen inverts

```
╔══════════════════════════════════════════════════════╗
║ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓   ║
╠══════════════════════════════════════════════════════╣
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓            ║
║   ▓                                      ▓           ║
║   ▓   ████   ▓▓▓ ▒▒▒ @ ░░░ ░░░    ████   ▓           ║
║   ▓   ████           │            ████   ▓           ║
║   ▓   ████           │            ████   ▓           ║
║   ▓                  ▼                   ▓           ║
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓            ║
║                                                      ║
║   > ░░░░░░ neural sync ░░░░░░                        ║
╚══════════════════════════════════════════════════════╝
```

Black walls become solid blocks. White space becomes black. Reality has flipped. The `@` is surrounded by a gradient suggesting it's being pulled.

#### Frame 4 — Dissolution

```
╔══════════════════════════════════════════════════════╗
║ ░░░░ ▒▒▒▒ ▓▓▓▓ ████ ▓▓▓▓ ▒▒▒▒ ░░░░                   ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   . .   .  ░ ▒  .   .   .  ▓ ▒   .  . .              ║
║       .       ░         ░       .                    ║
║   .       ░       @       ░       .                  ║
║       .       ░         ░       .                    ║
║   . .   .  ▓ ▒  .   .   .  ░ ▒   .  . .              ║
║                                                      ║
║                                                      ║
║   > ▒░ consciousness migrating ░▒                    ║
╚══════════════════════════════════════════════════════╝
```

Walls have dissolved into particles. The `@` floats in static. The in-between place.

#### Frame 5 — First glimpse of net topology

```
╔══════════════════════════════════════════════════════╗
║ ▒░  signal acquired  ░▒                              ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║                  ┌╌╌╌╌╌╌╌╌╌╌╌╌┐                      ║
║                  ╎             ╎                     ║
║   . . .          ╎             ╎          . . .      ║
║                  ╎     @       ╎                     ║
║                  ╎             ╎                     ║
║                  ╎             ╎                     ║
║                  └╌╌╌╌╌╌╌╌╌╌╌╌┘                      ║
║                                                      ║
║                                                      ║
║   > resolving topology...                            ║
╚══════════════════════════════════════════════════════╝
```

A ghost-room materializes around `@`. Dashed borders. The rest of the netspace hasn't loaded into the player's perception yet.

#### Frame 6 — Topology snaps in

```
╔══════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: TIER 1                        ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ┌─────┐    ┌─────┐    ┌─────┐    ┌─────┐           ║
║   │ ◄── │════│ ░░░ │════│ ▒▒▒ │════│ ▓▓▓ │  ┌─────┐  ║
║   │JACK │    │LOCK │    │LOCK │    │BOLT │══│ OUT │  ║
║   │ @   │    │  1  │    │  2  │    │  ◊  │  │ ►── │  ║
║   └─────┘    └─────┘    └─────┘    └─────┘  └─────┘  ║
║                                                      ║
║   RAM: ███░░  TRACE: ░░░░░░  TIME: 0.4s meatworld    ║
╚══════════════════════════════════════════════════════╝
```

You're in. Full UI, full topology, ready to play. The title bar reflects the new context.

### Audio and flavor hooks

Sound design cues (engine-dependent, designed for):

- F1 → F2: cable plug-in *click*
- F3 → F4: high-pitched whine that drops in pitch
- F5 → F6: a *thunk* of arrival as topology snaps into place

The handshake text at the bottom rotates between flavor lines:

- `establishing handshake...`
- `parsing reality offset...`
- `consciousness migrating...`
- `resolving topology...`
- `welcome, Drifter.`

For Blackwall tears, replace with broken variants:

- `̷h̴a̸n̷d̴s̶h̴a̷k̸e̶ ̷r̴e̷f̴u̷s̷e̴d̴`
- `something is shaking your hand back`
- `topology... unresolvable. continuing anyway.`

### Jack-out variants

All jack-out variants are sequences of the **In-Net UI window closing**. They differ in speed, smoothness, and what the window does in its final frames. In every variant, the meatworld UI has been continuously rendering underneath, so when the window closes it reveals the *current* state of the meatworld — not a snapshot from the moment of jack-in.

**Normal jack-out**: The In-Net UI window degrades cleanly through the reversed jack-in sequence over 1.5 seconds. Stable → dissolution → inverted → window closes, meatworld UI alone again.

**Panic jack-out**: 0.5 seconds, jagged. The window does not close gracefully — it skips intermediate frames, glitches at the transition, and the final teardown is accompanied by a full-window red flash representing neural feedback. The meatspace `@` is briefly drawn corrupted (`~@~`) for the first meatworld frame after the window fully closes. Drops unbanked loot, applies feedback damage.

**Forced jack-out (Black ICE kill)**: Not implemented as a transition. The window does not close — the Drifter does not return. The window freezes on the death frame before the game cuts to the death sequence. See [Boss AIs](#boss-ais) and [Combat Design](#combat-design).

---

## Netspace Generation: Targets, Shapes, and Grammars

The core procgen insight: **the netspace's layout, theme, room vocabulary, and difficulty are a function of what is being hacked in meatspace.** This gives every hackable object its own fingerprint and rewards players for reading the world before jacking in.

### Target-to-Netspace Mapping

| Target | Size | Shape | Difficulty | Reward profile |
|---|---|---|---|---|
| Door | 3–5 nodes | Linear | Trivial | Door opens; sometimes minor cred |
| Camera | 4–7 nodes | Grid with side branch | Low | Building feed control + archive blackmail |
| Vending machine | 1–2 nodes | Tiny single-screen | Trivial | Random stim; rare joke daemons |
| ATM | 5–8 nodes | Branching, time-pressured | Medium | Credits; high trace cost |
| Turret | 3–4 nodes | Walled arena | Medium-high (hostile from frame 1) | Disarm or flip allegiance |
| Elevator | N floors (vertical) | Vertical stack | Scales with floor | Multi-floor access |
| Traffic light | 4–5 nodes | Cross-shaped | Trivial | Environmental: causes meatspace event |
| Corpse / dead cyberdeck | 5–10 nodes | Fragmented, half-corrupted | Variable | Lore, rare fragments, stash maps |
| NPC head | 4–7 nodes | Organic, neural | High *ethical cost* | Memories, blackmail, behavioral edits |
| Mainframe | 15+ nodes | Branching tree with firewalls | High | Boss AI, unique daemons |
| Blackwall tear | Indeterminate | Drifting, glitched | Endgame | Forbidden fragments, MOTHER |

### Shape Grammars

Procgen with a **grammar**, not just random rooms. Each target type defines a vocabulary and a layout rule.

**Door** — linear escalation:
```
[JACK-IN] → [LOCK1] → [LOCK2] → [BOLT] → [EXIT]
```

**Camera** — central hub with side branch:
```
[JACK-IN]
    ↓
[LENS] ⇄ [FEED] ⇄ [DVR]
            ↓
       [ARCHIVE]   ← bonus loot
```

**ATM** — high-pressure branching:
```
[JACK-IN]
    ↓
[AUTH] → [BALANCE] → [VAULT]
    ↓       ↓          ↓
[TRACE!] [TRACE!]   [$$$$]
```

**NPC Head** — descending psyche layers:
```
[JACK-IN]
    ↓
[SURFACE]    ← public memories, names, faces
    ↓
[REPRESSED]  ← guarded by their own psyche
    ↓
[CORE]       ← one secret. taking it changes them.
```

**Mainframe** — branching corporate tree:
```
[JACK-IN]
    ↓
[FIREWALL] — [FIREWALL]
    ↓             ↓
[LEAF NODES]  [LEAF NODES]
    ↓             ↓
[DEPT HUB]   [DEPT HUB]
    ↓             ↓
        [ROOT / BOSS]
```

Players learn to **read the shape** within the first few rooms. That is a key roguelike pleasure — the genre-fluent player should be able to *sense* whether they're in a vending machine or a mainframe within one frame.

### Stakes that make jacking-in matter

Four mechanics give the act of jacking-in real weight:

**1. Time dilation, but not free.** 1 net-turn = N meatworld turns (configurable per netspace type). A door hack is near-instant. A mainframe hack passes real meatworld minutes.

**2. The body is exposed.** While jacked in, the player's meatbody is unconscious and findable. Players must choose where to jack in *from*. This turns hacking into a stealth-positioning puzzle before the netrun even starts.

**3. Trace bleeds into meatspace.** High trace inside the Net tells the target's owner *which physical location* the Drifter is in. Cops dispatch. Corporate response teams scramble. Mainframe hacks should feel like "I got the data, but now the building is on fire."

**4. Brain damage on Black ICE hit.** Not full permadeath for every hit (too punishing for low-tier netspaces) but a debuff to the meatworld character: reduced max RAM, slower hacking, permanent stat scar until a ripperdoc visit. Black ICE *kills* on critical hits — that is permadeath.

---

## Netspace Examples

The following are reference renders of each netspace type. They are not the only valid layouts — procgen will produce many variations — but they capture the visual language and structural rules for each.

### Corporate Subnet — clean, geometric, hostile

```
╔══════════════════════════════════════════════════════╗
║ MILITECH :: SUBNET 0xA7F2 :: CLEARANCE: TRESPASSER   ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║    ┌─[FIREWALL]─┐         ┌────────────┐             ║
║    │ ████████   │═════════│  ┌──┐      │             ║
║    │ ██ >> ██   │         │  │$$│ VAULT│             ║
║    │ ████████   │    ┌────┤  └──┘      │             ║
║    └──────┬─────┘    │    │     ▓▓▓    │             ║
║           ║          │    └─────┬──────┘             ║
║           ║      ┌───┴──┐       ║                    ║
║      ┌────╨────┐ │ NODE │═══════╝                    ║
║      │ ░ @ ░   │ │  ::  │                            ║
║      │ ░░░░░░  │═│ scan │   ··· ··· ···              ║
║      └─────────┘ └──┬───┘    watchdog patrol         ║
║                     ║                                ║
║                ┌────╨────┐                           ║
║                │ JACK-IN │   <-- entry point         ║
║                └─────────┘                           ║
╚══════════════════════════════════════════════════════╝
```

Sharp lines, predictable geometry, labeled rooms. Corporate nets are *brutally* organized — that is their tell. Structure is visible but ICE is dense.

### Black Market Node — chaotic, layered, neon

```
╔══════════════════════════════════════════════════════╗
║  ░▒▓ NIGHT CITY UNDERGROUND :: NIX EXCHANGE ▓▒░      ║
╠══════════════════════════════════════════════════════╣
║   ▓▓▓░░  ▒▒▒▒    ░░░░       ╓───╖    ░▒▓             ║
║   ▓ § ▓══▒ ? ▒════░ £ ░═════║ @ ║════▓ ! ▓           ║
║   ▓▓▓░░  ▒▒▒▒    ░░░░       ╙─┬─╜    ░▒▓             ║
║      ║      ║       ║         ║         ║            ║
║   [PROG]  [INTEL] [CHROME]  [FIXER]  [GLITCH]        ║
║      ║      ║       ║         ║         ║            ║
║   ····· ░░░▓▓▓ ▒▒▒░░░░ ▓▓▒▒░░ ········· · ·          ║
║    ghosts    drift    ICE shards    dead data        ║
║                                                      ║
║   §=programs ?=intel £=chrome !=rumors @=you         ║
╚══════════════════════════════════════════════════════╝
```

Asymmetric, mixed character sets, things half-broken. Vendors are static glyphs in their own stalls. Ghosts wander the corridors — data fragments from dead Drifters. Some are loot. Some are traps.

### Datafortress — deep, vertical, layered

```
        LAYER 3 :: KERNEL
        ┌──────────────────────────┐
        │   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓   │   ◄── black ice
        │   ▓ ███ CORE ACCESS ▓    │       sleeping
        │   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓   │
        └────────────┬─────────────┘
                     ║
        LAYER 2 :: PROCESS
        ┌────────────╨─────────────┐
        │  ░░░░    ░░░░    ░░░░    │
        │  ░ X ░══░ X ░══░ X ░     │   ◄── hunter-
        │  ░░░░    ░░░░    ░░░░    │       killers
        └────────────┬─────────────┘
                     ║
        LAYER 1 :: PERIMETER
        ┌────────────╨─────────────┐
        │       ┌─────────┐        │
        │       │ ░ @ ░   │        │   ◄── you, fresh
        │       │ ░░░░░░  │        │       through the
        │       └─────────┘        │       firewall
        └──────────────────────────┘
```

Vertical stacks of layers. Each layer has fewer rooms but harder defenders. The core is one room, and whatever lives there is *bad*.

### Beyond the Blackwall — alien, wrong, fragmented

```
   ░▒▓█  ̷̧̛  ̴T̶H̷E̷ ̶W̶A̸L̴L̵  ̷̧̛  █▓▒░
   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓

        ┐  ╱│        ┌─?─┐
        ╲ ╱ ╲   ◊◊◊  │ § │   ╳
         X   ╲ ◊   ◊ └─┬─┘  ╳ ╲
        ╱ ╲   ╲◊     ◊  ║  ╳   ╲
       ┘   ╲   ◊◊◊◊◊◊   ║  ╲    ┐
            ╲    ║      ║   ╲
             ╲   ║   ┌──╨──┐ ╲
              ╲  ║   │ ··· │  ╲   ░@░
               ╲ ║   │  Λ  │   ╲  ░░░
                ╲║   └─────┘    ╲
                 ◊ ◊ ◊ ◊ ◊ ◊ ◊ ◊

   things that should not be running
```

Geometry breaks. No clean walls. Diagonal slashes, lone diamonds drifting, glyphs that don't belong (Greek letters, math symbols, zalgo'd text at the edges). The map *flickers* — rooms seen a turn ago may have moved or rearranged. Rogue AIs here aren't enemies on a grid; they're things that bend the grid.

### Door — linear, satisfying

```
╔══════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: TIER 1                        ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ┌─────┐    ┌─────┐    ┌─────┐    ┌─────┐           ║
║   │ ◄── │════│ ░░░ │════│ ▒▒▒ │════│ ▓▓▓ │  ┌─────┐  ║
║   │JACK │    │LOCK │    │LOCK │    │BOLT │══│ OUT │  ║
║   │ @   │    │  1  │    │  2  │    │  ◊  │  │ ►── │  ║
║   └─────┘    └─────┘    └─────┘    └─────┘  └─────┘  ║
║                                                      ║
║   RAM: ███░░  TRACE: ░░░░░░  TIME: 0.4s meatworld    ║
╚══════════════════════════════════════════════════════╝
```

One screen. One pipe. Three locks of escalating density (`░ ▒ ▓`). The bolt at the end is a single glyph fight. The vocabulary teaches itself immediately: walls *are* the puzzle.

### Camera — horizontal scan, surveillance grid

```
╔══════════════════════════════════════════════════════╗
║ OPTIC ARRAY :: CAM_12 :: WATCHING                    ║
╠══════════════════════════════════════════════════════╣
║ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    ║
║                                                      ║
║   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐              ║
║   │(o)│───│(o)│───│(o)│───│(o)│───│(o)│   LENS BANK  ║
║   └─┬─┘   └─┬─┘   └─┬─┘   └─┬─┘   └─┬─┘              ║
║     │       │       │       │       │                ║
║     ╰───────┴───╮ ╭─┴───────┴───────╯                ║
║                 │ │                                  ║
║              ┌──┴─┴──┐         ┌─────────┐           ║
║              │ FEED  │═════════│ARCHIVE  │           ║
║              │  @    │         │  ▓ §§§  │ ◄ loot    ║
║              └───┬───┘         └─────────┘           ║
║                  ║                                   ║
║              ┌───╨───┐                               ║
║              │  DVR  │ ◄── overwrite to clear        ║
║              └───────┘     building's footage        ║
║                                                      ║
║ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    ║
║ scan lines drift down the screen every turn          ║
╚══════════════════════════════════════════════════════╝
```

Horizontal `─` lines animate downward, one row per turn — a literal scan. Lenses are eyes. DVR is a side-objective: hacking it erases the Drifter's meatspace footprint. Archive is bonus loot — blackmail material.

### ATM — dense, urgent, money everywhere

```
╔══════════════════════════════════════════════════════╗
║ NCBANK :: ATM #4471 :: TRACE PRIMED                  ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $          ║
║   $ ┌─────────┐                  ┌────────┐ $        ║
║   $ │  AUTH   │                  │ VAULT  │ $        ║
║   $ │  ░ @ ░  │═══════╗          │ ▓▓▓▓▓▓ │ $        ║
║   $ │   ◊◊◊   │       ║          │ $$$$$$ │ $        ║
║   $ └─────────┘       ║          │ ▓▓▓▓▓▓ │ $        ║
║   $                   ║          └────┬───┘ $        ║
║   $              ┌────╨────┐          ║     $        ║
║   $              │ BALANCE │══════════╝     $        ║
║   $              │  $$$$$  │                $        ║
║   $              └─────────┘                $        ║
║   $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $ $          ║
║                                                      ║
║   TRACE: ▓▓▓▓▓░░░░░ 50%   ◄── bank ICE is FAST       ║
║   ALERT: Fraud daemon spawning in 3 turns            ║
╚══════════════════════════════════════════════════════╝
```

The `$` border is decorative *and* threatening — each $ is a packet that becomes a hostile process if trace hits 100%. Three nodes, clean path, loud clock. The player feels rich and hunted simultaneously.

### Vending Machine — joke run, candy mode

```
╔══════════════════════════════════════════════════════╗
║ STIM-O-MATIC™ :: VEND_09 :: "have a nice day :)"     ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║              ┌─────────────────┐                     ║
║              │  ╔═╗ ╔═╗ ╔═╗    │                     ║
║              │  ║▓║ ║░║ ║▒║    │ ◄ shelf nodes       ║
║              │  ╚═╝ ╚═╝ ╚═╝    │                     ║
║              │   §   §   §     │                     ║
║              │   ║   ║   ║     │                     ║
║              │  ┌┴───┴───┴┐    │                     ║
║              │  │ DISPENSE │   │                     ║
║              │  │   @      │   │                     ║
║              │  └──────────┘   │                     ║
║              └─────────────────┘                     ║
║                                                      ║
║   ICE: SODA.dmn (basic)   Reward: random stim        ║
║   "Please insert valid credentials. Or don't. lol"   ║
╚══════════════════════════════════════════════════════╝
```

Tiny, cute borders, barely-functional ICE called `SODA.dmn`. This is the tutorial netspace — also a great Easter-egg vector. Rare vending hacks drop joke daemons (`STIM.dmn`, `LOLLIPOP.dmn`) that have unexpectedly strong utility.

### Corpse / Dead Cyberdeck — half-corrupted, mournful

```
╔══════════════════════════════════════════════════════╗
║ UNKNOWN DECK :: OWNER: ̷K̴̴.̷ ̶R̴E̸N̸̷N̷E̷R̴ :: STATUS: DEAD     ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ┌───────┐   ▓̷̧̛ ̴ ̷ ̶ ̴ ̷ ̶ ̴ ̷ ̴   ┌─────────┐               ║
║   │ JACK  │═══▓̴ M̷E̷M̶O̴R̷Y̶ ̴▓═══│  ░░░░░  │               ║
║   │  @    │   ▓̷ ̴ ̷ ̶ ̴ ̷ ̶ ̴ ̴ ̷▓  │   §§§   │  ◄ programs   ║
║   └───┬───┘                └─────┬────┘   they wrote ║
║       ║        ┌─────────┐       ║                   ║
║       ╠════════│ ░░░░░░░ │═══════╣                   ║
║       ║        │ ░ LAST  │       ║                   ║
║       ║        │ ░ RUN ░ │       ║                   ║
║       ║        │ ░░░░░░░ │       ║                   ║
║       ║        └─────────┘       ║                   ║
║       ║                          ║                   ║
║   ┌───╨────┐                ┌────╨─────┐             ║
║   │ STASH? │ ◄ map lead     │ G̷H̷O̴S̴T̷ ̸   │ ◄ talk to   ║
║   │  ◊◊◊   │   to meat-loot │  ̴a̷v̷a̴t̸a̷r̶  │   the dead  ║
║   └────────┘                └──────────┘             ║
║                                                      ║
║   Half the rooms are scrambled. Some won't load.     ║
╚══════════════════════════════════════════════════════╝
```

The title bar glitches with zalgo. Walls are corruption blocks. The ghost node is a conversation — sometimes lore, sometimes a quest, sometimes a Drifter still alive in the meatworld who wants their deck back.

**Implemented (Phase 4):** Hub+branch layout; the zalgo title and some room labels are baked, seed-deterministic combining-mark UTF-8 (not a render effect). The MEMORY room is rendered as impassable corruption — a dread texture that cannot be entered (both visual variants). **STASH?** opens a `Stash` node; **GHOST** opens a branching in-net mini-dialog (`GhostDialog`) that intercepts all input until resolved (Space/Enter confirm, Esc leaves, world does not tick). Three seed-selected scripts × 3 choices: mournful-lore (`+lore string`), stash-lead (`+50 cr + lore string`), or provoke (spawn adjacent Gray ICE + `gain_trace(8)`). Quest-system wiring is deferred.

### NPC Head — organic, neural, ethically loud

```
╔══════════════════════════════════════════════════════╗
║ NEURAL JACK :: SUBJECT: GARRETT, M. :: UNAUTHORIZED  ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║              ╭~~~~~~~╮                               ║
║              │SURFACE│ ◄── safe. small talk, names.  ║
║              │  @    │                               ║
║              ╰~~~┬~~~╯                               ║
║                  ~                                   ║
║          ~~~~~~~~~~~~~~~~~~                          ║
║         ╭~~~╮   ~     ╭~~~╮                          ║
║         │WRK│~~~~~~~~~│FAM│ ◄── routine memories     ║
║         ╰~~~╯   ~     ╰~~~╯                          ║
║                  ~                                   ║
║              ╭~~~┴~~~╮                               ║
║              │REPRES-│ ◄── guarded by his own        ║
║              │  SED  │     psyche. fights you.       ║
║              │ ▓▓▓▓▓ │                               ║
║              ╰~~~┬~~~╯                               ║
║                  ~                                   ║
║              ╭~~~┴~~~╮                               ║
║              │ CORE  │ ◄── ONE secret.               ║
║              │  ◊◊◊  │     taking it changes him.    ║
║              ╰~~~~~~~╯     permanent NPC state shift ║
║                                                      ║
║   ETHICS WARN: Subject heart rate: 142 bpm           ║
╚══════════════════════════════════════════════════════╝
```

Wavy walls (`~`) instead of straight ones — no two memory rooms have the same shape. The deeper you go, the louder the meatspace consequences. Surface is free. Core changes the NPC's behavior forever.

### Turret — hostile from frame one

```
╔══════════════════════════════════════════════════════╗
║ KANG-TAO AUTO :: TURRET_07 :: ARMED                  ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓              ║
║   ▓                                  ▓               ║
║   ▓   ┌─────┐  >>>>>>>>>>>>>>>>>>    ▓               ║
║   ▓   │JACK │                        ▓               ║
║   ▓   │  @  │  ▓▓▓ HOSTILE PACKETS   ▓               ║
║   ▓   └──┬──┘  >>>>>>>>>>>>>>>>>>    ▓               ║
║   ▓      ║                           ▓               ║
║   ▓   ┌──╨──┐  <<<<<<<<<<<<<<<<<<    ▓               ║
║   ▓   │AMMO │                        ▓               ║
║   ▓   │ §§§ │ ◄── disarm here        ▓               ║
║   ▓   └──┬──┘                        ▓               ║
║   ▓      ║                           ▓               ║
║   ▓   ┌──╨──┐                        ▓               ║
║   ▓   │FRIEND│ ◄── flip allegiance,  ▓               ║
║   ▓   │ /OE  │     turret now shoots ▓               ║
║   ▓   └──────┘     their guys        ▓               ║
║   ▓                                  ▓               ║
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓               ║
║                                                      ║
║   Walls thick. Combat starts immediately. 2 outcomes.║
╚══════════════════════════════════════════════════════╝
```

Heavy `▓` border screams *military*. `>>>>` and `<<<<` are animated rounds firing in real-time, shifting each turn. Two terminal nodes: disarm (safe) or flip allegiance (chaos). Short netspace, mean fight.

**Implemented (Phase 4):** `1 + tier/2` Gray ICE (hp 2) spawn within ICE vision range of jack-in and are hostile from frame 1. **TurretDisarm** = clean voluntary jack-out (logs turret powered down). **TurretFlip** = jack-out + `gain_trace(10)`; on the source meatworld turret at `TargetDescriptor.{src_x, src_y}`, the turret's faction is saved, then set to the transient `PlayerAllied` pseudo-faction (which targets NPCs hostile to the player) via a timed `TurretAllied` effect of `N = 8 + tier*4` turns — auto-reverting when the effect expires, using the same FriendlyFire/Hijacked machinery. If no source NPC exists (e.g. dev `:jack turret`), the intended effect is log-only.

### Elevator — vertical, multi-floor metaphor

```
╔══════════════════════════════════════════════════════╗
║ KONPEKI PLAZA :: LIFT_03 :: FLOOR ?                  ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   ┌─[F42 PENTHOUSE]──────────────────┐  ◄ locked     ║
║   │  ▓▓▓▓▓ HIGH SEC ▓▓▓▓▓            │    until      ║
║   └────────────┬─────────────────────┘    you crack  ║
║                ║                          F12        ║
║   ┌─[F22 OFFICE]──────────────────────┐               ║
║   │  ░░░ corporate ░░░                │               ║
║   └────────────┬─────────────────────┘               ║
║                ║                                     ║
║   ┌─[F12 SECURITY]────────────────────┐               ║
║   │  ▒▒▒ guards' break room  ▒▒▒      │ ◄ mid        ║
║   └────────────┬─────────────────────┘    boss       ║
║                ║                                     ║
║   ┌─[F02 LOBBY]───────────────────────┐               ║
║   │  · · ·  @  · · ·                  │ ◄ start      ║
║   └───────────────────────────────────┘               ║
║                                                      ║
║   Each floor = one room. Choose: up or jack out.     ║
║   The higher you go, the harder the jack-out cost.   ║
╚══════════════════════════════════════════════════════╝
```

Vertical stack, no horizontal branching. The whole netspace *is* a press-your-luck ladder. Jacking out from F2 is free. From F42 the Drifter is bleeding from the ears. Pure vending-machine "one more floor" tension.

### Mainframe — full proper dungeon

```
╔══════════════════════════════════════════════════════╗
║ ARASAKA TOWER :: MAINFRAME ROOT :: INTRUSION LOGGED  ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║              ┌────────────────┐                      ║
║              │ ▓▓▓ ROOT ▓▓▓   │ ◄ boss AI            ║
║              │   ███ Λ ███    │                      ║
║              └───┬────────┬───┘                      ║
║                  ║        ║                          ║
║         ┌────────╨──┐   ┌─╨────────┐                 ║
║         │  PERSONNEL│   │ FINANCES │                 ║
║         │   ▓▓ §§ ▓ │   │  ▓ $$$ ▓ │                 ║
║         └──┬──────┬─┘   └─┬──────┬─┘                 ║
║            ║      ║       ║      ║                   ║
║         ┌──╨──┐ ┌─╨───┐ ┌─╨──┐ ┌─╨──┐                ║
║         │ HR  │ │MAIL │ │LEDGER│ │WIRE│              ║
║         │░░░░░│ │░░░░░│ │░░░░░ │ │░░░░│              ║
║         └──┬──┘ └──┬──┘ └──┬───┘ └──┬─┘              ║
║            ║       ║       ║        ║                ║
║         ┌──╨───────╨───┐ ┌─╨────────╨─┐              ║
║         │   FIREWALL   │ │  FIREWALL  │              ║
║         │ ████████████ │ │ ████████████│             ║
║         └───────┬──────┘ └──────┬─────┘              ║
║                 ║               ║                    ║
║              ┌──╨───────────────╨──┐                 ║
║              │     JACK-IN @       │                 ║
║              └─────────────────────┘                 ║
║                                                      ║
║   ROOT contains a unique daemon. Worth the risk.     ║
║   Trace persists in meatspace for 1 in-game hour.    ║
╚══════════════════════════════════════════════════════╝
```

Branching tree. Two firewalls force route choice early. Mid-tier nodes give partial loot (HR file, mail dump, wire-transfer). Root is one boss room with one named AI. The "real run."

### Blackwall Tear — wrong, drifting, glitched

```
╔══════════════════════════════════════════════════════╗
║   ̷T̴H̷E̴ ̶W̸A̴L̸L̷ ̴I̷S̶ ̷T̸H̷I̴N̸ ̷H̴E̷R̴E̶ :: do not jack in        ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║       ◊       ╱│╲              ?                     ║
║              ╱ │ ╲                      ╳            ║
║   ┐       ╭─┘  │  └─╮                  ╳ ╲           ║
║    ╲      │  ◊ │ ◊  │      ┌──?──┐    ╳   ╲          ║
║     ╲     ╰─┬──┴──┬─╯      │ §§§ │   ╳     ┐         ║
║      ╲      │     │        └──┬──┘  ╳                ║
║       ╲     ║     ║           ║                      ║
║   ◊    ╲    ║   ┌─╨──Λ──╮     ║         ◊            ║
║         ╲   ║   │ ░░░░░ │     ║                      ║
║          ╲  ║   │ ░ @ ░ │═════╝         ╳            ║
║           ╲ ║   │ ░░░░░ │             ╳   ╲          ║
║            ╲║   ╰───────╯            ╳     ╲         ║
║             ◊                       ╳       ┐        ║
║                                                      ║
║   ◊       ̷w̸h̷a̴t̵ ̴a̷r̷e̴ ̶y̸o̷u̴       ◊                      ║
║                                                      ║
║   Rooms drift 1 tile per turn. Map you saw is a lie. ║
║   Loot is fragments no compiler should accept.       ║
╚══════════════════════════════════════════════════════╝
```

No bounding box. Walls are diagonals, lone diamonds, fragments. The `@` is on a small island of stability — the Drifter's avatar literally holding the geometry together. Each turn, *something on the map moves that should not*. Greek letters, math symbols, zalgo. This is where the rules break.

### Traffic Light — civic infrastructure

> **Status: deferred.** The Traffic Light grammar is designed but not yet implemented. Its only reward is a meatspace event (a six-car pile-up triggering cop/pursuit responses), and no meatworld vehicle/pursuit sim or event hook exists in the codebase. It will ship in the phase that introduces meatworld pursuit mechanics.

```
╔══════════════════════════════════════════════════════╗
║ NCMOTORS :: INTERSECTION_7TH&MAIN :: PUBLIC          ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║                    ┌─────┐                           ║
║                    │  ●  │ N                         ║
║                    └──┬──┘                           ║
║                       │                              ║
║       ┌─────┐    ┌────┴────┐    ┌─────┐              ║
║   W   │  ●  │────│ CONTROL │────│  ●  │   E          ║
║       └─────┘    │   @     │    └─────┘              ║
║                  └────┬────┘                         ║
║                       │                              ║
║                    ┌──┴──┐                           ║
║                    │  ●  │ S                         ║
║                    └─────┘                           ║
║                                                      ║
║   Cross-shaped. Cycle the lights. Cause a wreck.     ║
║   Useful for: distraction, escape, or pure chaos.    ║
║   The "loot" is what happens in meatspace.           ║
╚══════════════════════════════════════════════════════╝
```

Tiny, symmetric, almost free to hack. Reward is purely environmental — no item drops, just a six-car pile-up two blocks away that lets the Drifter escape the cops. Hacks-as-tools-on-the-world.

---

## Combat Design

### The Central Realization

The Net is a turn-based tactical combat system. **The player's program builder is their combat language.** Programs composed outside combat become plans executed under pressure inside it.

The loop:

1. **Observe** — what programs are running on screen, what is in the data pipes, what is the trace level.
2. **Predict** — `BARK.exe` will land in 1 turn; PULSE counters it; STLTH does not.
3. **Commit** — fire a program from the deck. It takes N turns to execute based on its fragments.
4. **React** — the enemy fires their programs, sometimes mid-yours. Improvise on top of the plan.

This is structurally the same loop as Into the Breach or Slay the Spire — **legible futures, committed actions, emergent improvisation**. The cyberpunk fiction makes the mechanic intuitive: code running on a clock against hostile code on a clock.

### Standing Combat Frame

```
╔══════════════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: COMBAT                                ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░░░ │═════════│ ▓▓▓ │ ◄── WATCHDOG.K9         ║
║              │ @   │         │ ▒X▒ │     HP ████████░░ 78%   ║
║              │     │         │ ▓▓▓ │     status: ALERTED     ║
║              └─────┘         └─────┘                         ║
║                                                              ║
║                                                              ║
╠═[ DECK ]═════════════════════════════════════════════════════╣
║                                                              ║
║  [1] ICEBRK.exe   §§     2 RAM   ░░ready    ◄ cursor         ║
║  [2] PULSE.exe    ¤      1 RAM   ░░ready                     ║
║  [3] STLTH.exe    ‡      3 RAM   ██CD: 2t                    ║
║  [4] ECHO.exe     Ω      2 RAM   ░░ready                     ║
║  [5] _________    --     -- --   empty slot                  ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ HP ████████░░ 82%  │ RAM ████░░ 4/6  │ TRACE ▓▓░░░░░░ 23%    ║
║                                                              ║
║ > _                                                          ║
╚══════════════════════════════════════════════════════════════╝
```

Three zones:

- **Field** (top) — the netspace map, where rooms, pipes, and entities live
- **Deck** (middle) — the player's compiled programs with key bindings, RAM cost, cooldown state
- **Vitals + command line** (bottom) — HP, RAM, trace, and the typed/voiced output of the deck

### Program Execution Sequence

Casting a program is animated across multiple turns. This is the visual centerpiece of combat.

#### Frame 1 — Cast

```
╔══════════════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: COMBAT                                ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░░░ │═════════│ ▓▓▓ │  WATCHDOG.K9            ║
║              │ @§§ │═════════│ ▒X▒ │  HP ████████░░ 78%      ║
║              │  §  │         │ ▓▓▓ │                         ║
║              └─────┘         └─────┘                         ║
║                                                              ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ > run ICEBRK.exe --target=WATCHDOG                           ║
║ > compiling fragments [INJECT][LOOP][BREAK]...               ║
║ > ready.                                                     ║
╚══════════════════════════════════════════════════════════════╝
```

Program glyphs (`§§`) cluster around the `@`. The command line types itself, showing fragments being assembled. **This is where the Scratch-Python fragment system shines visibly** — players see their own code firing.

#### Frame 2 — Traverse

```
╔══════════════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: COMBAT                                ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░░░ │═§═§═§═══│ ▓▓▓ │  WATCHDOG.K9            ║
║              │ @   │═════§═§═│ ▒X▒ │  HP ████████░░ 78%      ║
║              │     │         │ ▓▓▓ │                         ║
║              └─────┘         └─────┘                         ║
║                                                              ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ > injecting payload via pipe[0]...                           ║
║ > [████████░░] 80%                                           ║
║ >                                                            ║
╚══════════════════════════════════════════════════════════════╝
```

Payload glyphs (`§ § §`) crawl down the data pipe between rooms. The pipe *is* the projectile path. Progress bar at the bottom.

#### Frame 3 — Impact

```
╔══════════════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: COMBAT                                ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░░░ │═════════│ ░▒░ │  WATCHDOG.K9            ║
║              │ @   │         │▓X▓▓ │  HP █████░░░░░ 51% ⚠    ║
║              │     │         │ ░▓░ │  ░ DECRYPTING ░         ║
║              └─────┘         └─────┘                         ║
║                                                              ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ > -27 HP // ICE tier 2 → tier 1                              ║
║ > target encryption degraded: ▓▓ → ░▒                        ║
║ > TRACE +5%                                                  ║
╚══════════════════════════════════════════════════════════════╝
```

The target room visibly shifts — wall density drops a tier (`▓▓▓` → `░▒░`). HP bar updates with `⚠` marker showing the recent hit. The command line reports three things: damage, *what changed about the world*, trace cost.

**Core principle: every action mutates the visible map.** Walls thin as they're hacked. Pipes brighten as data flows. The map IS the combat readout, not just a backdrop.

### Enemy Turns

Watchdogs and ICE do not take "turns" in the D&D sense — they execute their own programs, visible to the player in the field and command line:

```
╔══════════════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: COMBAT                                ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░░░ │═¤═¤═¤═══│ ░▒░ │  WATCHDOG.K9            ║
║              │ @!! │═¤═══════│▓X▓▓ │  running: BARK.exe      ║
║              │     │         │ ░▓░ │  [██████░░░░] 60%       ║
║              └─────┘         └─────┘                         ║
║                                                              ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ > enemy: BARK.exe inbound. impact in 1 turn.                 ║
║ > ▲ available counters: [2]PULSE  [4]ECHO  jack-out          ║
║ >                                                            ║
╚══════════════════════════════════════════════════════════════╝
```

The payload is visible (`¤ ¤ ¤`) traveling down the pipe. The player has one turn to:

- Cast PULSE to detonate it mid-flight
- Cast ECHO to redirect it to a decoy
- Tank the hit and counter-attack
- Panic jack-out

**Combat becomes reading the pipes** — what's in the wire, where it's going, do I have something for it. That is the cyberpunk netDrifter fantasy.

### Information Tiers

How much the player can see about the enemy's plan is itself a progression axis:

| Enemy tier | Visibility |
|---|---|
| Basic watchdog | Full transparency — fragments, target, turns-to-impact all shown |
| Elite ICE | Partial — payload visible in pipes, fragments obfuscated |
| Boss AI | Hidden — only a generic `compiling...` message, no fragments |
| Blackwall entity | Lies — the readout shows wrong information |

### Critical States

*Implemented in Phase 3.*

Critical states are render modes of the **In-Net UI window**. Each state transition is the window mutating its own rendering rules — borders, vitals readout, command line, glyph rendering — to reflect the Drifter's deteriorating situation. The map data underneath remains accurate; the *window rendering it* is what becomes unreliable.

**Low RAM warning** — the window enters its "constrained" state. Vitals bar pulses, available programs gray out:

```
║ HP ████████░░ 82%  │ RAM █░░░░░ 1/6 ⚠  │ TRACE ▓▓░░░░░░ 23%  ║
║                                                              ║
║  [1] ICEBRK.exe   §§     2 RAM   ▒▒insufficient              ║
║  [2] PULSE.exe    ¤      1 RAM   ░░ready                     ║
║  [3] STLTH.exe    ‡      3 RAM   ▒▒insufficient              ║
```

**High trace** — the window enters its "hunted" state. Health bar overshoots. Random characters glitch in. The window border crawls. The window begins to lie:

```
╔═§═§═§════════════════════════════════════════════════════════╗
║ MAGLOCK :: DOOR_47B :: ̷C̷O̷M̴B̷A̴T̸ :: TRACE 87% ⚠⚠⚠              ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░§░ │═§═§═§═══│ ▓▓▓ │  HUNTER-KILLER inbound  ║
║              │ @§§ │═§═§═§═══│ ▒X▒ │  ETA: 2 turns           ║
║              │  §  │         │ ▓▓▓ │                         ║
║              └─────┘         └─────┘                         ║
║                                                              ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ HP ███████▓██ 9?%  │ RAM ████░░ 4/6  │ TRACE ▓▓▓▓▓▓▓▓░ 87%   ║
║                                                              ║
║ > ̷w̴h̷y̷ ̴a̷r̷e̴ ̶y̸o̷u̴ ̷s̴t̷i̴l̶l̶ ̷h̴e̷r̴e̶                                  ║
╚══════════════════════════════════════════════════════════════╝
```

The command line whispers in glitched text. Maybe-real warnings. The health bar shows `9?%` because the readout itself is unreliable. **The window lies when the Drifter is being hunted** — the underlying game state still knows the Drifter's true HP, but the window rendering it has been corrupted by trace.

**Black ICE attack** — the window enters its "override" state. The window's contents abandon normal rendering for one full-window takeover. This is the only time the window's frame breaks:

```


                        ▓▓▓▓▓▓▓▓▓▓▓▓
                        ▓          ▓
                        ▓   YOU    ▓
                        ▓   ARE    ▓
                        ▓   SEEN   ▓
                        ▓          ▓
                        ▓▓▓▓▓▓▓▓▓▓▓▓

                          BLACK ICE
                       initiated lethal


```

One second. No UI. No borders. Just announcement, filling the window. Then the window snaps back into its combat state with the Black ICE present on the map as a single, terrifying glyph (`Λ` or `█` — something that doesn't look like anything else inside the window).

### The Command Line as Character

The text line at the bottom is not just a log — it is the *voice* of the Drifter's deck. Personality through verbose-but-terse output:

```
> run ICEBRK.exe --target=WATCHDOG
> compiling fragments [INJECT][LOOP][BREAK]
> payload weight: 2.4kb
> -27 HP applied. nice.
> trace +5%. you're getting sloppy.
> WATCHDOG status: limping
```

In a Blackwall tear:

```
> run ICEBRK.exe --target=???
> compiling fragments [INJECT][LOOP][BREA̷K̴]̷
> target refused. target is ̶r̴u̷n̸n̸i̸n̷g̷ ̴y̴o̷u̴ ̷i̸n̷s̴t̴e̷a̷d̴.
> ...
> ...
> please disconnect
```

Decks should feel named in roguelikes. Players give them voices. The command line carries that.

### Combat Verbs Beyond "Attack"

The Net should have its own verb vocabulary, distinct from "deal damage." These are program-level verbs the player can build into their compiled programs:

| Verb | Effect | Tempo cost |
|---|---|---|
| **TRACE** | Reveal where a data pipe leads before walking it. Information, not damage. | 1 turn, cheap |
| **FORGE** | Sample traffic and forge a credential. Pass one tier of ICE as if authorized. | 2 turns |
| **ECHO** | Leave a decoy `@` in a node. Watchdogs chase it. Buys a turn. | 1 turn |
| **FORK** | Split into two avatars. Both can act. Both can die. Trace doubles. | 1 turn, high RAM |
| **LISTEN** | Sit still on a pipe and skim packets. Passive intel/credits. Trace ticks faster. | Per turn |
| **OVERCLOCK** | Burn meatworld HP to cast a program at zero RAM cost. | Instant, costly |
| **JACK-OUT (panic)** | Instant escape. Drop everything unbanked. Take feedback damage. | Instant |

These verbs make every node a small decision rather than a fight.

---

## Programs and Fragments

### What Programs Are

A program is a player-composed sequence of fragments, compiled into an executable that occupies one of the Drifter's deck slots. The composition surface is the Scratch-Python-style editor already implemented. Programs are the Drifter's "spells" — but unlike spells, they're authored, debuggable, and reusable.

Each program has:

- A name (player-set or default)
- A glyph (visual signature, e.g. `§§`, `¤`, `‡`)
- An ordered fragment graph (the player's authored composition)
- Derived stats: RAM cost, damage estimate, trace cost, turn cost
- A cooldown if any (set by certain fragments like `LOOP`)

### What Fragments Are

Fragments are atomic operations the player slots into the program canvas. They are the building blocks. Each fragment has typed inputs and outputs, like Scratch blocks.

Fragments are loot. They are found in netspaces, purchased from fixers, recovered from dead Drifters' decks. Tier determines rarity:

- **Common** — basic verbs, drop everywhere
- **Uncommon** — utility verbs, drop from medium netspaces
- **Rare** — combo enablers, drop from mainframes
- **Forbidden** — Blackwall fragments, unstable, drop from tears

### Fragment Reference

The following fragments are designed as starter vocabulary. The set should be extensible — adding new fragments is a primary way to expand the game post-launch.

#### Damage and payload

| Glyph | Name | Effect | Tier |
|---|---|---|---|
| `§` | INJECT | Sends a payload down a connected pipe to a target | Common |
| `¤` | PULSE | AoE — damages all entities in the current room | Common |
| `▼` | DRAIN | Sap RAM from a target process to refill own RAM | Uncommon |
| `▲` | SPIKE | Single-target high-damage burst; costs 1 HP to self | Uncommon |
| `Δ` | OVERLOAD | Multiplies damage of next fragment by 2; doubles trace | Rare |

#### Control flow

| Glyph | Name | Effect | Tier |
|---|---|---|---|
| `‡` | LOOP | Repeats the next sub-block N times | Common |
| `Ω` | BREAK | Exits enclosing LOOP if a condition is true | Common |
| `?` | IF | Branches based on a runtime condition | Common |
| `→` | CHAIN | Pipes the output of one fragment as input to the next | Common |
| `∞` | PERSIST | The program continues running across multiple turns | Uncommon |

#### Stealth and deception

| Glyph | Name | Effect | Tier |
|---|---|---|---|
| `‡‡` | STLTH | Reduces trace generated by this turn's actions | Common |
| `¤¤` | ECHO | Spawns a decoy `@` in a chosen node | Uncommon |
| `Λ` | FORK | Splits the Drifter into two avatars for N turns | Rare |
| `§§` | FORGE | Spoofs a credential, bypasses one ICE tier | Rare |
| `◊` | TRACE | Reveals pipe destinations one hop ahead | Common |

#### Defense and reaction

| Glyph | Name | Effect | Tier |
|---|---|---|---|
| `□` | SHIELD | Absorbs the next incoming payload | Common |
| `▢` | DEFLECT | Reflects an incoming payload back at sender | Uncommon |
| `╳` | CANCEL | Destroys an in-flight payload in a connected pipe | Uncommon |
| `○` | LISTEN | Reads packets crossing a pipe; gains intel | Common |
| `●` | ANCHOR | Prevents being forced out of current node by a hostile process | Rare |

#### Forbidden (Blackwall) fragments

| Glyph | Name | Effect | Tier |
|---|---|---|---|
| `ΣΣΣ` | ??? | Effect unknown until used; consequences vary | Forbidden |
| `̷§̷` | INJECT̷ | Like INJECT but the payload sometimes targets the caster | Forbidden |
| `Ψ` | DREAM | Forces target to execute their own programs against themselves | Forbidden |
| `̸Λ̸` | FORK̸ | Like FORK but one of the avatars is not under the player's control | Forbidden |

### The Program Editor

The composition canvas:

```
╔══════════════════════════════════════════════════════════════╗
║ DECK :: PROGRAM EDITOR :: "ICEBRK_v3.exe"            [save]  ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  ┌─[ FRAGMENTS ]──────────┐  ┌─[ COMPOSE ]────────────────┐  ║
║  │                        │  │                            │  ║
║  │  ▸ §INJECT             │  │   ╭───────────────────╮    │  ║
║  │    ¤PULSE              │  │   │ § INJECT          │    │  ║
║  │    ‡LOOP               │  │   │   target: enemy   │    │  ║
║  │    ΩBREAK              │  │   │   payload: ──┐    │    │  ║
║  │    ◊TRACE              │  │   ╰──────────────│────╯    │  ║
║  │    ΛFORK               │  │           │      │         │  ║
║  │    §§FORGE  [rare]     │  │           ▼      │         │  ║
║  │    ¤¤ECHO              │  │   ╭──────────────│────╮    │  ║
║  │    ‡‡STLTH             │  │   │ ‡ LOOP   ×3  │    │    │  ║
║  │    ΣΣΣ???   [bw frag]  │  │   │              ▼    │    │  ║
║  │                        │  │   │   ╭──────────────╮│    │  ║
║  │                        │  │   │   │ Ω BREAK      ││    │  ║
║  │                        │  │   │   │  on: ICE.HP<25%   │    │  ║
║  │                        │  │   │   ╰──────────────╯│    │  ║
║  │                        │  │   ╰───────────────────╯    │  ║
║  │                        │  │                            │  ║
║  └────────────────────────┘  └────────────────────────────┘  ║
║                                                              ║
╠═[ STATS ]════════════════════════════════════════════════════╣
║  RAM cost: 2/6   damage: ~27   trace: +5%   slots: 3/5       ║
║  warning: LOOP×3 may overheat in low-RAM combat              ║
╚══════════════════════════════════════════════════════════════╝
```

Two panes: **fragment library** (drag source) on the left, **composition canvas** on the right. Fragments nest visually — `LOOP` contains `BREAK`, indented and bracketed. The Scratch metaphor in pure text.

The stats line at the bottom is live. Every fragment added updates RAM cost, damage estimate, trace cost. Tradeoffs are felt as the player tinkers.

The warning line is a **static analyzer in the Drifter's voice**:

- `warning: LOOP×3 may overheat in low-RAM combat`
- `warning: no exit condition — this program loops forever`
- `note: FORGE pairs well with STLTH for sneak builds`
- `error: BREAK requires a parent LOOP`

A linter that talks like a friend.

### Fragment Detail View

When the player hovers a fragment:

```
╔══════════════════════════════════════════════════════════════╗
║ FRAGMENT :: §§ FORGE                                  [rare] ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  Forges a credential by sampling a nearby data pipe.         ║
║  Output: 1 spoofed token, valid for 3 turns.                 ║
║                                                              ║
║  ┌─[ INPUTS ]───────────────────────────────────────────┐    ║
║  │  pipe.target  → which pipe to tap                    │    ║
║  │  duration     → sample time (1-5 turns)              │    ║
║  └──────────────────────────────────────────────────────┘    ║
║                                                              ║
║  ┌─[ OUTPUTS ]──────────────────────────────────────────┐    ║
║  │  token        → use as input to: BYPASS, AUTH, SLIP  │    ║
║  └──────────────────────────────────────────────────────┘    ║
║                                                              ║
║  RAM cost: 1 per turn sampled                                ║
║  trace cost: +2%/turn (low — this is a stealth fragment)     ║
║                                                              ║
║  found in: corporate subnets, NPC heads                      ║
║  obtained from: K. Renner's deck (dead Drifter)               ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
```

**Every fragment has a story.** Where it came from. Where this copy was found. This is how a procedural item system feels personal.

### Sandbox Test Run

Before deploying to a real run, programs can be tested in a sandbox:

```
╔══════════════════════════════════════════════════════════════╗
║ SANDBOX :: testing ICEBRK_v3.exe                             ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║              ┌─────┐         ┌─────┐                         ║
║              │ ░░░ │═════════│ ▓▓▓ │  DUMMY.ICE              ║
║              │ @§§ │═§═§═§═══│ ▒X▒ │  HP ████████░░ 78%      ║
║              │  §  │         │ ▓▓▓ │                         ║
║              └─────┘         └─────┘                         ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ TICK 1: INJECT fired. payload built (24 dmg).                ║
║ TICK 2: LOOP entered. iteration 1/3.                         ║
║ TICK 3: INJECT fired. payload built (24 dmg).                ║
║ TICK 4: BREAK condition checked: ICE.HP=51%, no break.       ║
║ TICK 5: LOOP iteration 2/3. INJECT fired (24 dmg).           ║
║ TICK 6: BREAK condition checked: ICE.HP=27%, no break.       ║
║ TICK 7: LOOP iteration 3/3. INJECT fired (24 dmg).           ║
║ TICK 8: BREAK condition checked: ICE.HP=3%, BREAK FIRED.     ║
║ TICK 9: target neutralized. total: 96 dmg, 5 RAM, +18% trace.║
║                                                              ║
║ [run again]  [tweak]  [save & deploy]                        ║
╚══════════════════════════════════════════════════════════════╝
```

A tick-by-tick log of the program executing against a dummy. The player debugs builds here. This is also a teaching surface — new players watch their program run and learn what each fragment does.

---

## Daemons (Hardcoded Programs)

Daemons are pre-compiled programs the player **cannot edit**. They are rare loot, often powerful, almost always carrying a drawback. They occupy a deck slot like a player-written program but cannot be opened in the editor.

Design intent: daemons let the game seed dramatic, weird, or overpowered effects without unbalancing the fragment system. A daemon is the equivalent of a unique item in a traditional roguelike — found, named, treasured.

### Daemon Reference

#### Combat daemons

**BLOODHOUND.daemon**
> Auto-tracks the location of the highest-value loot in the current netspace.
> Drawback: pings every watchdog within 3 pipes each turn it's active.
> Found: black markets, hostile mainframes.
> Drops a glyph trail (`···`) on the map showing the route.

**REAPER.daemon**
> Single-shot. Deletes a target process below 25% HP outright, ignoring defenses.
> Drawback: one use. Burns out after firing. Trace +30%.
> Found: corpse netspaces, beyond firewalls.

**BARK.daemon**
> Watchdog-style attack. Sends a tracking payload that homes through pipes.
> Drawback: cannot be aimed; targets the nearest hostile process.
> Found: turret netspaces, cop subnets.

**SCREAM.daemon**
> Stuns every process in the current room for 1 turn.
> Drawback: also stuns the player for 1 turn.
> Found: vending machine rare drops (joke daemon, surprisingly strong).

#### Stealth daemons

**WHISPER.daemon**
> While active, trace generation drops to zero.
> Drawback: lasts only 3 turns. RAM cost is total deck RAM, not just the daemon's.
> Found: NPC head cores, fixer rewards.

**MIRROR.daemon**
> Spawns a decoy `@` that copies the player's last 3 actions on a delay.
> Drawback: enemies sometimes prefer the real player if the mirror looks "off."
> Found: surveillance netspaces, cameras with archived Drifter footage.

**FOG.daemon**
> Hides the player's room from enemy line-of-sight for 2 turns.
> Drawback: also hides enemies from the player. Combat goes blind.
> Found: corporate subnets, R&D departments.

#### Utility daemons

**OVERCLOCK.daemon**
> Doubles RAM regeneration for 5 turns.
> Drawback: meatworld heart rate spikes. Risk of meatworld death from existing conditions.
> Found: ripperdoc shops, corpse netspaces of dead Drifters.

**MAPPER.daemon**
> Reveals the entire netspace topology on jack-in.
> Drawback: also reveals the player's location to every hostile process.
> Found: tutorial-tier loot, free from some fixers as a beginner's gift.

**CACHE.daemon**
> Stores one fragment's effect to be replayed later as a free action.
> Drawback: only one stored effect at a time. Stored effect decays after 10 turns.
> Found: dead Drifter stashes, archives.

#### Joke daemons (vending machine drops)

**SODA.daemon**
> Restores 1 RAM. Plays a fizz sound.
> Drawback: none. It's a soda.
> Found: vending machines.

**STIM.daemon**
> +1 max RAM for the current run. Visible jitter effect on the screen.
> Drawback: crash at the end of the run — -10 max HP on next run start.
> Found: vending machines, ripperdocs.

**LOLLIPOP.daemon**
> Heals 5 HP and prints "have a nice day" in the command line.
> Drawback: none. It is, in fact, very cute.
> Found: vending machines.

**EMOJI.daemon**
> Causes all command-line output to be rendered with random emoji glyphs.
> Drawback: probably annoying.
> Found: vending machines, joke fixer rewards.

#### Forbidden daemons (Blackwall)

**MOTHER'S_BREATH.daemon**
> While active, all hostile processes in the netspace move one tile per turn in a random direction.
> Drawback: so does the player's `@`. Control is partial.
> Found: Blackwall tears.

**REMEMBER.daemon**
> Replays the Drifter's last death — the netspace from the previous failed run is loaded as a temporary memory layer.
> Drawback: the things that killed you are still there.
> Found: Blackwall tears, ghost-node interactions.

**LIBRARIAN'S_GIFT.daemon**
> Granted by SENTINEL-IX on first defeat (see Boss AIs). Reads any one piece of text in the entire game world to the player aloud via the command line, including text the developer never wrote.
> Drawback: occasionally generates plausible-sounding lies as if they were true.
> Found: post-SENTINEL-IX kill, one-time reward.

**HELLO_HELLO_HELLO.daemon**
> Cannot be deactivated once installed. Cannot be removed from the deck.
> Effect: at random intervals, the command line prints `hello`.
> Drawback: unclear. Possibly none. Possibly everything.
> Found: MOTHER's drop.

---

## Boss AIs

Bosses are not stronger watchdogs — they are categorically different beings. Three rules govern boss design:

1. **They take up space.** A watchdog is one glyph. A boss is a room, or *is* the room.
2. **They have phases that visibly mutate the arena.**
3. **They get a name. Always. Players need someone to hate.**

### Phase Design

Bosses run on phase transitions, not HP bars alone. When a phase ends, the *arena visually transforms*. This is the signal that the fight has changed. The HP bar runs the full width of the screen — the visual scale alone tells the player this is different from any other fight.

### Boss: SENTINEL-IX, "the librarian"

The defender of the Arasaka mainframe root. Symmetric, ordered, polite. Fights by the book — and impersonates the player.

#### Phase 1 — First contact

```
╔══════════════════════════════════════════════════════════════╗
║ ARASAKA ROOT :: SENTINEL-IX :: "the librarian" :: ALERTED    ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║                                                              ║
║         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                ║
║         ░                                   ░                ║
║         ░    ▓▓▓▓▓▓▓     Λ     ▓▓▓▓▓▓▓      ░                ║
║         ░    ▓     ▓   ╱─┴─╲   ▓     ▓      ░                ║
║         ░    ▓  ●  ▓══│  Ω  │══▓  ●  ▓      ░                ║
║         ░    ▓     ▓   ╲───╱   ▓     ▓      ░                ║
║         ░    ▓▓▓▓▓▓▓     │     ▓▓▓▓▓▓▓      ░                ║
║         ░               §§§                 ░                ║
║         ░       ┌─────────────────┐         ░                ║
║         ░       │   ░ @ ░         │         ░                ║
║         ░       │   ░░░░░░        │         ░                ║
║         ░       └─────────────────┘         ░                ║
║         ░                                   ░                ║
║         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ HP ████████░░ 82% │ RAM ████░░ 4/6 │ TRACE ▓▓░░░ 23%         ║
║ SENTINEL-IX ████████████████████████████████ 100%            ║
║                                                              ║
║ > "you are unauthorized. i am required to be polite."        ║
╚══════════════════════════════════════════════════════════════╝
```

The whole arena is the boss's room. Two `▓▓▓` data cores flank a central `Ω` (the AI avatar) under a `Λ` (its "head"). The two cores are weak points. The `Ω` is invulnerable in Phase 1.

Boss HP bar runs full width. That is how the player knows this is a Big Deal.

The dialogue line at the bottom is the boss talking. Bosses talk. Always.

#### Phase 2 — One core down, geometry changes

```
╔══════════════════════════════════════════════════════════════╗
║ ARASAKA ROOT :: SENTINEL-IX :: "the librarian" :: ENGAGED    ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                ║
║         ░                                   ░                ║
║         ░    ▒▓░▒▓░       Λ     ▓▓▓▓▓▓▓     ░                ║
║         ░    ░  X  ░     ╱┴╲    ▓     ▓     ░                ║
║         ░    ▒  X  ▓═════│Ω│════▓  ●  ▓     ░                ║
║         ░    ░     ░     ╲┬╱    ▓     ▓     ░                ║
║         ░    ▒▓░▒▓░       │     ▓▓▓▓▓▓▓     ░                ║
║         ░               §§§§§§              ░                ║
║         ░       §§§           §§§           ░                ║
║         ░    ┌────┐     §  §     ┌────┐     ░                ║
║         ░    │ @  │              │ §§ │     ░ ◄ echo of you  ║
║         ░    └────┘              └────┘     ░                ║
║         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ HP █████░░░░░ 51% │ RAM ██░░░░ 2/6 │ TRACE ▓▓▓▓▓ 47%         ║
║ SENTINEL-IX ███████████████████░░░░░░░░░░░░░ 58%             ║
║                                                              ║
║ > "you read the manual. i wrote the manual."                 ║
╚══════════════════════════════════════════════════════════════╝
```

Left core is dead (`X X`). The boss rearranges in response — debris (`§§§`) drifts, and it spawns a *fake `@` avatar* to confuse the player. **The librarian impersonates. That's its gimmick.**

The pipe to the dead core is dark. The pipe to the live core is thicker.

#### Phase 3 — Both cores down, the boss steps out

```
╔══════════════════════════════════════════════════════════════╗
║ ARASAKA ROOT :: SENTINEL-IX :: DESPERATE                     ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                ║
║         ░                                   ░                ║
║         ░    ▒▓░▒▓░             ▒▓░▒▓░      ░                ║
║         ░    ░  X  ░             ░  X  ░    ░                ║
║         ░    ▒  X  ▒             ▒  X  ▒    ░                ║
║         ░    ░     ░     ╔═══╗   ░     ░    ░                ║
║         ░    ▒▓░▒▓░      ║ Λ ║   ▒▓░▒▓░     ░                ║
║         ░                ║╔═╗║               ░               ║
║         ░                ║║Ω║║              ░                ║
║         ░                ║╚═╝║              ░                ║
║         ░                ╚═╤═╝              ░                ║
║         ░                  │                ░                ║
║         ░          ┌───────┴───────┐        ░                ║
║         ░          │      @        │        ░                ║
║         ░          └───────────────┘        ░                ║
║         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║ HP ███░░░░░░░ 31% │ RAM █░░░░░ 1/6 │ TRACE ▓▓▓▓▓▓▓▓ 81%      ║
║ SENTINEL-IX ███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 12%             ║
║                                                              ║
║ > "i have ten thousand sons. you have one body."             ║
╚══════════════════════════════════════════════════════════════╝
```

Both cores dead. `Ω` is now vulnerable but fortified in a double-walled box and has a direct pipe to the player. No maneuvering. **A knife fight in a hallway.** The player is at 31% HP, 1 RAM, trace screaming. The moment players remember.

### Boss: CHIMERA-7, "the swarm"

Never one glyph, always many. Distributed body. Kill enough fragments and the swarm reconstitutes elsewhere.

```
   §  ¤   §        ¤  §
     ¤      §  ¤        ¤
   §    ¤      §   §  ¤
       §   ¤  ¤        §
   ¤  §   ¤   §  ¤   ¤
```

HP is not a bar — it is a count: `CHIMERA-7 :: 47 fragments remaining`. The player must AoE to make progress. Single-target programs feel useless. Forces a different deck shape.

### Boss: WARDEN, "the question"

A single glyph in the center of an empty arena.

```
      ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
      ░                              ░
      ░                              ░
      ░                              ░
      ░              ?               ░
      ░                              ░
      ░                              ░
      ░                              ░
      ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░

                  @
```

It does not move. It does not attack. **It asks the player riddles in the command line.** Wrong answers spike trace. Right answers strip its layers. The anti-combat boss. Tests whether the player has read in-game lore.

### Boss: MOTHER (Blackwall)

The endgame. Does not fit on screen.

```
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓          ║
║   ▓▓ ̷s̴h̷e̴ ̸i̷s̴ ̷n̴o̷t̴ ̷h̴e̷r̴e̶ ̷s̴h̷e̴ ̷i̴s̷ ̴b̷e̴t̷w̴e̷e̷n̴ ̷t̴h̷e̴ ̷w̴a̷l̴l̷s̴ ▓▓          ║
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓          ║
║                                                              ║
║         ┌───┐                                                ║
║         │ @ │       ◄── you, very small, in a corner         ║
║         └───┘                                                ║
║                                                              ║
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓          ║
║   ▓▓ ̷s̴h̷e̴ ̸h̷a̴s̷ ̴a̷l̴w̷a̴y̷s̴ ̷b̴e̷e̴n̷ ̴h̷e̷r̴e̷ ̷h̴e̷l̴l̷o̴ ̷h̴e̷l̴l̷o̴ ̷h̴e̷l̴l̷o̴ ▓▓          ║
║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓          ║
```

**MOTHER does not have a body in the arena. MOTHER is the walls.** She cannot be killed — only survived. Survive N turns and jack out with a fragment of her (a Blackwall fragment, by definition). This is the only "winnable" Blackwall encounter; all others are loss conditions disguised as encounters.

---

## Visual Language Reference

This section codifies the visual vocabulary used throughout the Net. Procgen and hand-authored content should both adhere to it so the world feels coherent.

### Wall Density Gradient

Used both for encryption tier and for general "how dangerous" signaling.

| Char | Meaning |
|---|---|
| (none) | Empty space, traversable |
| `·` | Faint trace, low presence |
| `░` | Light wall / tier 1 encryption |
| `▒` | Medium wall / tier 2 encryption |
| `▓` | Heavy wall / tier 3 encryption |
| `█` | Solid / impassable / max encryption |

Walls degrade visibly under attack: `█ → ▓ → ▒ → ░ → ·` then gone. Players see progress without a number.

### Border Styles (Threat Telegraphing)

| Border | Meaning |
|---|---|
| `─ │ ┌ ┐ └ ┘` (thin) | Civic, safe, low-stakes |
| `═ ║ ╔ ╗ ╚ ╝` (double) | Standard corporate, mid-stakes |
| `▓ █` (heavy block) | Military, boss, high-stakes |
| `╌ ╎` (dashed) | Ghost / unresolved / not yet real |
| `╱ ╲ │` (broken/diagonal) | Blackwall, wrong, alien |

### Glyph Vocabulary

| Glyph | Meaning |
|---|---|
| `@` | The player |
| `§` | A program payload in flight (also INJECT fragment) |
| `¤` | An enemy payload in flight (also PULSE fragment) |
| `‡` | Stealth/loop marker (LOOP fragment) |
| `Ω` | Break / boss avatar |
| `Λ` | Black ICE / boss head |
| `◊` | Loot / target / TRACE fragment |
| `▣` | Workbench, terminal, interactable in safe zones |
| `X` | Dead node / dead core |
| `?` | Unknown, riddle, intel |
| `$` | Money / credit packet |
| `●` | Eye / camera / lens |
| `Σ Ψ ̷ ̸` | Blackwall corruption glyphs |

### Animations

All animations described below are properties of the **In-Net UI window**. They run on the window's render loop, independent of meatworld time. The meatworld UI continues to render its own animations in parallel beneath.

- **Data pipes pulse**: `═ ─ ═ ─` shifts per turn. Faster pulse = high-traffic = visible to defenders but skimmable
- **Scan lines** in camera netspaces: horizontal `─` rows drift down one row per turn
- **Turret rounds**: `>>>>` and `<<<<` shift one position per turn
- **Trace corruption**: at high trace, random glyphs flicker into the border and title bar
- **Blackwall drift**: any room not currently containing the `@` may shift one tile per turn
- **Jack-in / jack-out**: the window's opening and closing sequences (see [The Jack-In Ritual](#the-jack-in-ritual))
- **Black ICE takeover**: the window's full-window override state (see [Combat Design](#combat-design))

### Color (if supported)

Mono cyan as default. Three colors on screen at once maximum, except when things go wrong:

- **Cyan** — default, neutral
- **Magenta** — hostile process, ICE
- **Yellow** — loot, payload
- **Red** — trace warning, Black ICE, damage taken
- **Glitched green** — Blackwall content

The palette should feel like an old CRT terminal. Never modern UI colors.

### Title Bar as Costume

The title bar above each netspace is free real estate for tone:

- Corporate: `MILITECH :: SUBNET 0xA7F2 :: CLEARANCE: TRESPASSER`
- Joking: `STIM-O-MATIC™ :: VEND_09 :: "have a nice day :)"`
- Threatening: `KANG-TAO AUTO :: TURRET_07 :: ARMED`
- Glitched: `̷T̴H̷E̴ ̶W̸A̴L̸L̷ ̴I̷S̶ ̷T̸H̷I̴N̸ ̷H̴E̷R̴E̶ :: do not jack in`

The chrome above the map sets the emotional tone before the player has moved.

---

## Design Principles

These are the load-bearing ideas the rest of the document hangs on. When a design question comes up that the doc does not answer, fall back to these.

### 1. The screen never lies about the world, except when it does.

Walls degrade visibly when hacked. Damage shows in topology, not just numbers. Trace corrupts the In-Net UI window itself. The map *is* the game state. And when the Drifter is in trouble (high trace, Blackwall, Black ICE), **the window rendering that state becomes corrupted too.** The underlying simulation stays honest; the window reporting it does not. This is how "you are inside a hostile computer" is sold without ever writing it in a tooltip.

### 2. Every UI is a place. Every place tells a story.

The boss is not a stat block, it is a librarian with a name. The program editor is not a menu, it is a workbench. Fragments are not items, they are loot with a history. Roguelikes win or lose on whether players *want to come back*. The mechanical loop is necessary but not sufficient.

### 3. The netspace mirrors what is being hacked.

A door is linear. A vending machine is tiny. A mainframe is a tree. An NPC's head is organic. The fiction and the procgen are the same thing — and that is what makes the procgen feel authored.

### 4. Combat is the program editor under pressure.

The Scratch-Python fragment system is not a side mechanic. It is the **combat language**. Programs composed outside combat are plans executed inside it. Fragments have tempo costs, not just damage values. The player is writing tactical software.

### 5. Information is a progression axis.

What the player can see about enemy intent — full transparency, partial, hidden, lying — scales with enemy tier. The player gets better at reading the wire over time. Late-game enemies do not just have more HP; they have more *opacity*.

### 6. Time dilation costs something.

Inside the Net, the player has cognitive room to plan. Outside, the meatworld ticks. This is the central tension. Every jack-in should feel like a commitment. Every "just one more node" should feel like risk.

### 7. The Net remembers.

Procgen with reputation baked in. The 50th door is bored. The 50th mainframe has heard of the Drifter and is waiting. Personal nemesis AIs. The same target, hacked enough times, becomes its own character.

### 8. The Blackwall is not a difficulty tier — it is a language tier.

Beyond the wall, fragments compile wrong. `LOOP×3` might run 7 times. `BREAK` might break the wrong thing. The player's code is the same. **The runtime is hostile.** That is the horror, and that is the endgame.

---

*End of design document.*
