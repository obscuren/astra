# Per-Device Diegetic Shells (Plan 7) — Design Spec

**Date:** 2026-05-01
**Status:** Draft, awaiting user review
**Branch (target):** TBD (Plan 7 work — not started)
**Parent specs:**
- `2026-04-29-hacking-design.md` (root hacking spec)
- `2026-04-30-hacking-deep-grid-design.md` (Plan 4 — deep-Grid)
- `2026-05-01-grid-expansion-design.md` (Plan 5 — LAN sector + tags; current branch)

Plan 7 work; sequenced after Plan 5 (LAN/tag system) and Plan 6 (HUD redesign). This spec is one of multiple Plan 7 features (Your.Anchor full mechanics, darknet revival, etc.) — it does not own the whole plan.

---

## 1. Concept

Every electronic device runs a fake mini-OS the player can drop a CLI shell into. The shell is the **diegetic surface** of Plan 5's tag-driven hacking model: a device's `HackTagMask` directly determines which commands are available; its `FixtureType` provides the OS banner and prompt; its faction provides the flavor of files, log entries, fake usernames, and MOTD copy.

The shell is reachable from two doorways, both gated by `Cat_Hacking`:

- **Real-world.** Walk up to an electronic fixture; an extra "Hack" interaction opens the PDA hacking tab and autoruns `ssh root@<device-ip>`. The player's body becomes physically wired into the device's data port — frozen, vulnerable, Esc to yank the cable.
- **In-Grid.** While inside a LAN/subnet sector (Plan 5), the shell is reachable as a UI overlay from anywhere — no walking back to a port required. Body is already phased out, so vulnerability lives on the Grid clock (Trace, Heat) rather than meatspace.

Two playstyles emerge from one set of mechanics: the **Netrunner** walks the spatial LAN sector and engages ICE; the **Sysadmin** pivots through device shells and never sees an ICE. Same gear, same skills, same LAN — totally different rhythm.

### Pillars

- **The shell is a UI surface, not a parallel universe.** It rides Plan 5's tag system. Adding a tag adds commands; adding a fixture inherits commands automatically. No central switch statement.
- **Two privilege tiers, one shell.** `ssh` always succeeds at `guest`. `hashcat` (long-channel) escalates to `root`. Locked-vs-unlocked is visible diegetically — no out-of-shell crack minigame.
- **Reads are free, privileged actions are committed.** Reading `cat /var/log` is instant, paused world. Privileged actions (`firmware --wipe`, `blind`, `friendly_fire`, etc.) are long-channel: world ticks, body is wired-in (real-world), Trace ticks (Grid). Inline progress bar shows in the shell itself.
- **Shell pivots are Grid-only.** From inside the Grid, `ssh root@<other-ip>` jumps to any device on the breached LAN, costs Heat, lowers Trace. From real-world, you're physically wired — pivots are blocked.
- **Authoring scales with content, not code.** Three orthogonal authoring layers (tag-derived commands / per-FixtureType banner / per-faction flavor) keep content debt linear. New fixture = pick tags + write banner string. New faction = write flavor pack.
- **Diegetic legibility.** `--help` exposes duration and cost *for this player* (post-INT/skill scaling). The system documents itself.

### Non-goals

- New tags beyond the Plan 5 set (`Electronic`, `Locked`, `PowerNode`, `DataStore`, `HasOptics`, `Weaponized`, `Mobile`, `AlienTech`, `JackInPort`).
- New programs / quickhacks. Shell commands are *not* programs and don't ship from inventory.
- Shell-driven combat. ICE is a Grid concept; the shell never spawns or fights ICE.
- Real-world pivots. Body is wired into one device.
- Pre-shell crack minigames. The shell is always the only UI.
- Replacing the spatial LAN/subnet sectors. Shell is *additive*; spatial walking remains.
- Replacing quickhacks. QHs stay the fast-twitch combat layer; shell is the deep / slow / privileged layer.
- File-system simulation beyond what `ls` / `cat` need. No editors, no shells-within-shells, no piping, no scripting.

---

## 2. Architecture

### New subsystems

| Unit | Header | Responsibility |
|---|---|---|
| `HackCommand` | `hack_command.h` | Command-as-object: name, synopsis, description, required tag, base costs, execute fn. Registered in a global table. |
| Per-command modules | `src/hack_commands/cmd_*.cpp` | One file per command. Each defines the `HackCommand` instance and its execute function. |
| `DeviceShell` | `device_shell.h/cpp` | Active shell session state: target Hackable, privilege tier, history, current channel state, prompt assembly, MOTD render. |
| `HackChannel` | `device_shell.h` (member) | Long-channel state machine: command, args, base/scaled duration, Heat/Detection cost, progress, interrupt handlers, partial-state hooks. |
| `HackFlavor` | `hack_flavor.h/cpp` | Faction flavor packs (banner templates, MOTD lines, user-name tables, file-content templates, prompt suffixes). In-code tables. |
| `DeviceFsView` | `device_fs.h/cpp` | Procedurally generates the device's "filesystem" (path → content map) from tags + flavor pack. Used by `ls`, `cat`, `grep`, `find`. |

### Existing systems extended

- **`Hackable`** (`hackable.h`, exists post-Plan 5) — gains `firmware_state: enum { Stock, Wiped, Glitched }`, `cracked_digits: uint8_t` (for `hashcat` partial-state), `escalated: bool` (true once `root`), `dumped_bytes: uint32_t` (for `data --dump` partial-state). Plan 5's `tags` and `ip` already provide what the shell needs to address and gate.
- **`PdaHackingTab`** (`pda_hacking_tab.cpp`) — gains the `ssh` command. Resolves IP → Hackable → opens `DeviceShell`. The PDA's `pda>` shell stays for `nmap`/`programs`/`jack`/etc; the device's shell is a separate sub-mode the PDA "tunnels into."
- **`HackingSystem`** (`hacking_system.h/cpp`) — drives the long-channel ticker, applies command effects, persists mutations. Owns the active `DeviceShell` (at most one).
- **Interactables (`interactables_widget`)** — gains a "Hack" entry on any `Hackable` with `Electronic` tag if the player has `Cat_Hacking`. Triggering it routes through `HackingSystem::open_real_world_shell(Hackable*)`.
- **`Cat_Hacking` skills** — three new skill nodes: `PivotMaster` (pivot magnitudes), `ColdHands` (fewer Detection ticks per privileged command), `RootKit` (faster `hashcat`). Stubbed out elsewhere; this plan unlocks the slots.
- **Save (`galaxy_*.dat`)** — schema bumps for the new `Hackable` fields. Per project rule: reject old saves, no migration.

### File-size discipline

`device_shell.cpp` ≤ ~400 lines. `hack_flavor.cpp` likely ~600 lines (5 packs × ~120 lines of templates) — split per-faction (`hack_flavor_corp.cpp`, etc.) if it crosses 800. Each `cmd_*.cpp` ≤ ~150 lines; if a command needs more, the command is too big — split or trim. `device_fs.cpp` ≤ ~250 lines.

---

## 3. The two doorways

### 3a. Real-world doorway

**Trigger.** Player adjacent to any `Hackable` with `Electronic` tag and has `Cat_Hacking` unlocked. `interactables_widget` shows `Hack` as an extra option (or sole option if nothing else applies, e.g., a power conduit).

**Sequence.**

1. Player selects `Hack`.
2. Body becomes wired-in. State flag `player.is_jacked_into = hackable_id`. Movement, attack, item-use disabled. Render: `@` glyph gains a subtle cyan pulse / `[⟳]` superscript.
3. PDA opens to the Hacking tab. The `pda>` terminal autotypes `ssh root@<device-ip>` with a small typing animation (~0.3s) for vibe.
4. Connection ritual runs (1s, non-skippable):
   ```
   Connecting to 10.0.4.17.... [SUCCESS]
   Negotiating cipher... [SUCCESS]
   Authentication accepted. Welcome Guest.
   ```
5. Banner renders.
6. Prompt appears: `<DEVICE-NAME>:guest$ _`.

**Exit.** `exit`, closing the PDA, or pressing Esc at the prompt yanks the cable. Body unfreezes, shell session ends. Esc *during* a long-channel only aborts the channel; you're still in the shell.

**Constraints.**
- Body frozen for the entire shell duration — including idle time at the prompt. Tactical decision: open shell only when safe.
- Damage to the body interrupts the active channel (per Edge 1) but does **not** auto-close the shell. Shell remains open; the player can continue or exit. (Open question for plan: should an alarm trigger an auto-yank?)
- Real-world shell cannot pivot. The `pivot` command refuses with `pivot: unavailable from a wired session`.

### 3b. In-Grid doorway

**Trigger.** Player is inside a LAN or subnet sector (Plan 5). At any time, opens the PDA (or a dedicated key — to be settled in implementation plan) → terminal sub-mode → types `ssh root@<ip>`.

**Differences from real-world.**
- Body is already phased out (per Plan 4 hacking spec). No new physical-vulnerability state; the player is already "in" the Grid.
- Long-channel ticks the **Grid clock** (Trace, Heat), not the world clock.
- `pivot` is available. From a Grid shell session you can `ssh` into any device on the breached LAN.
- No connection-ritual gate to physical proximity — proximity in the Grid is logical (LAN-graph membership), not spatial. The IP must resolve to a device on the currently-breached LAN.

The shell *itself* — banner, commands, flavor, channel mechanics, interrupt rules — is identical in both doorways. Only the surrounding context differs.

---

## 4. Auth model — guest vs root

### Two-tier privilege

`ssh` always succeeds. The shell opens at one of two privileges, derived from `Hackable` runtime state:

| Tier | When | Visible as | Allowed |
|---|---|---|---|
| `guest` | `Hackable` is `Locked` and not yet escalated | `<DEVICE>:guest$` and `Welcome Guest.` | Reads (`ls`, `cat`, `whoami`, `help`, `clear`, `history`), and the `Locked`-tag commands (`hashcat`, `unlock`) |
| `root` | `Hackable` is unlocked OR has been escalated this session | `<DEVICE>:root#` and `Welcome <root-name>.` | All tag-derived commands + reads + universals |

A device with no `Locked` tag opens directly at `root`.

### Escalation

`hashcat --fast` is a long-channel command (typical: 8–14 turns, 4–8 Heat, +10–20 Detection scaled by INT/skill). On success, sets `Hackable.escalated = true`, prompt switches to `root#`, full command set unlocks. On failure (skill check), Heat is spent, `cracked_digits` increments by 1–2 (partial-state for next attempt), prompt stays `guest$`.

`hashcat --fast` reveals digits one at a time:
```
TURRET-OS:guest$ hashcat --fast
[*] Cracking authentication... [▓▓▓░░░░░░░] 30%
[+] Recovered: ****1**5***
[*] Cracking authentication... [▓▓▓▓▓▓░░░░] 60%
[+] Recovered: ****1**5*K8
TURRET-OS:guest$ _    # interrupted
TURRET-OS:guest$ hashcat --fast    # later: resumes from cracked_digits=5
[+] Recovered: ****1**5*K8
[*] Cracking authentication... [▓▓▓▓▓▓░░░░] 60%
```

### Persistence

`escalated`, `cracked_digits`, `firmware_state`, and `dumped_bytes` persist in the save (rides Plan 5's tile-mutation persistence). A device cracked in one session is still cracked in the next. Wiped firmware is permanently wiped until a tinkering repair (future plan).

---

## 5. Connection ritual

Hard-coded sequence, 1 second, non-skippable. Output streams character-by-character at ~60 chars/sec:

```
Connecting to <DOTTED-IP>.... [SUCCESS]
Negotiating cipher... [SUCCESS]
Authentication accepted. Welcome <TIER-NAME>.

<BANNER-FROM-FIXTURE>

<MOTD-FROM-FACTION-PACK>

<DEVICE-NAME>:<TIER>$ _
```

`TIER-NAME` is `Guest` (guest tier) or one of the faction's authored root names (e.g., `Welcome root.` for Corp; `Welcome ROOT_USER (mikko)` for Cartel; `Welcome ▲` for Precursor).

`MOTD-FROM-FACTION-PACK` is one randomly-selected line from the faction's MOTD list. Examples below in §10.

### Cursor rendering

The shell prompt's cursor is an **inverted-color block**, mirroring the existing `dev_console` cursor:

- Single cell.
- Background = the prompt's foreground color (typically `Color::White`).
- Foreground = the prompt's background color (typically `Color::Black`).
- Drawn *on top of* the character at the cursor position — so a cursor over `firmwa█e` shows the `r` as black-on-white in that one cell. End-of-line cursor shows a solid block over a space.
- Implementation reference: `dev_console::render` at `src/dev_console.cpp:1730–1732` — same `ctx.put(col, row, ch, Color::Black, Color::White)` pattern with the under-cursor character supplied.
- Behavior: arrow keys move the cursor through the input buffer (left/right), Home/End jumps to ends, Backspace/Delete edit, characters insert at cursor — full mid-line editing matching dev console.
- During an active long-channel the prompt is busy (showing the progress bar); the input cursor is suppressed and the bar's last `▓` cell acts as the visual focus instead.

In the spec's sample blocks, the trailing `_` represents "cursor here, line empty" — read it as the inverted block at end-of-line.

---

## 6. Command system

### `HackCommand` class

```cpp
struct HackCommand {
    const char*     name;            // "hashcat", "blind", "ls"
    const char*     synopsis;        // "hashcat [--fast] [--wordlist=FILE]"
    const char*     description;     // shown by `help` and `<cmd> --help`
    HackTag         required_tag;    // command appears iff device has this tag; HackTag::None for universals
    bool            requires_root;   // command appears at guest if false; root-only if true
    int             base_turns;      // long-channel duration; 0 = instant
    int             base_heat;       // 0 for reads
    int             base_detection;  // 0 for reads
    bool            allow_partial;   // true if command has authored partial-state behavior
    Effect (*execute)(const ParsedArgs&, Hackable&, DeviceShell&, World&);
};
```

Commands self-register at static-init time into a global registry indexed by name. The registry exposes:

```cpp
const HackCommand* find_command(std::string_view name);
std::vector<const HackCommand*> commands_for(HackTagMask tags, bool is_root);
```

Adding a new command = drop a `cmd_<name>.cpp` file. Adding a new device-side capability bound to a new tag = add the tag to the Plan 5 `HackTag` enum, write the `cmd_*` files for that tag.

### Universal commands

Tag-less, always available:

| Command | Behavior |
|---|---|
| `help` | Lists all commands available on this device (current tier). One line per command: `name — short description`. |
| `<cmd> --help` | Synopsis + description + scaled cost (`Cost for you: 6 turns, 3 Heat, +12 Detection`). |
| `whoami` | Prints current user (`guest@TURRET-OS-2.7` or `root@TURRET-OS-2.7`). |
| `clear` | Clears the visible scroll. |
| `history` | Prints command history (this session). |
| `exit` | Yanks cable (real-world) or closes shell (in-Grid). |
| `pivot <ip>` | Grid-only. Lateral move to another device on the LAN. (See §8.) |

### Tag → command starting map

| Tag | Commands |
|---|---|
| `Locked` | `hashcat`, `unlock` |
| `HasOptics` | `blind`, `feed`, `restream`, `purge` |
| `Weaponized` | `disarm`, `lockout`, `friendly_fire`, `targetlist` |
| `PowerNode` | `surge`, `kill`, `reroute`, `dim` |
| `DataStore` | `ls`, `cat`, `grep`, `dump`, `wipe`, `find` |
| `Mobile` | `halt`, `redirect`, `gps` |
| `AlienTech` | `decode`, `mirror`, `query` |

Commands are **additive** — a Camera with `Electronic | HasOptics` exposes `blind`, `feed`, `restream`, `purge` plus universals plus reads. A Turret with `Electronic | HasOptics | Weaponized` adds `disarm`, `lockout`, `friendly_fire`, `targetlist` on top.

### `--help` format

```
TURRET-OS:root# firmware --help
firmware - permanently modify device firmware

USAGE:
  firmware --wipe        Permanently brick the device
  firmware --reflash     Reload stock firmware
  firmware --status      Show current firmware state

COST (for you, post-skill):
  --wipe:    12 turns | 8 Heat | +25 Detection
  --reflash: 18 turns | 12 Heat | +20 Detection
  --status:  instant  | 0 Heat  | +0 Detection

NOTES:
  Privileged. Channel can be interrupted by damage.
  Wipe persists across saves; reflash requires firmware in inventory (future).
```

The cost numbers are computed live from `base_*` × INT/skill modifiers. The `--help` text is per-command authored.

---

## 7. Long-channels

### Mechanics

A **long-channel** is any command where `base_turns > 0`. When executed:

1. Cost is paid up front: Heat += scaled, Detection += scaled (real-world only).
2. `HackChannel` becomes active. The shell renders an inline progress bar that updates every world tick:
   ```
   TURRET-OS:root# firmware --wipe
   Wipe in progress [▓▓▓▓▓             ] 25%
   ```
3. Each world tick (or Grid tick, in-Grid) advances progress by `1 / scaled_turns`.
4. World/Grid simulation **continues** while the channel runs. AI moves, alarms tick, ICE moves, body remains frozen (real-world) or phased out (Grid).
5. On completion: `execute()` runs, applies the effect, prints the success line, prompt returns. Channel cleared.

### INT and skill scaling

- `scaled_turns = base_turns × (1 - 0.05 × INT_mod) × skill_factor`. Floored at 1.
- `scaled_heat = round(base_heat × (1 - 0.04 × INT_mod) × skill_factor)`. Floored at 0.
- `skill_factor`: 1.0 baseline; specific skills (`RootKit`, `ColdHands`) reduce factor for specific command subsets.
- `base_detection` scales by `ColdHands` only.

`INT_mod` is the player's INT modifier (current attribute system: -2 to +5). Final numbers are exposed via `--help`.

### Interrupt rules

A channel aborts on any of:

- Player takes any damage (real-world) or HP damage from Black ICE (Grid).
- Player presses Esc.
- Real-world only: player would otherwise be forced to move (knockback, teleport effect).

While a channel is active, the prompt accepts only **read commands** and Esc. Attempting to issue another privileged command prints `<DEVICE>:root# <cmd>: a command is already in progress (Esc to abort)` — it does **not** interrupt the active channel.

On abort:

- **Heat-spent-is-spent.** No refund.
- Detection already added stays added.
- Effect is not applied — *unless* the command is `allow_partial = true`.
- For partial-state commands (`hashcat`, `data --dump`), the command's `on_partial(progress, Hackable&)` hook fires — typically writing a small persistent token (`cracked_digits++`, `dumped_bytes += amount`) so the next attempt resumes.

Authored partial-state commands in v1: `hashcat`, `dump`. Everything else aborts atomically.

### Body during channel (real-world)

- **Frozen.** No movement, no attacks, no item use. Esc and read commands only.
- Wired-in state is shown by an `@` glyph cyan pulse / `[⟳]` superscript.
- Enemies can target the player normally. AI sees the player as a normal target (no special "is hacking" awareness for v1).
- Open question for plan: do nearby AIs gain a *suspicion* tick when a hack is in progress? Probably no for v1 — keep AI-side unchanged.

### Body during channel (Grid)

- Player is phased out per Plan 4. Trace and Heat tick on the Grid clock.
- ICE present in the same LAN sector continues moving and attacking the player avatar at the device's location while the channel runs. (Open: does the avatar "freeze" in the LAN sector while channeling? Recommend: yes, mirror the real-world frozen behavior. ICE can still hit the avatar.)

---

## 8. Pivots (Grid-only)

### Mechanics

`pivot <ip>` (or `ssh root@<ip>` — alias) inside a Grid shell session:

1. Validates target: must be on the breached LAN, must be `Electronic`. If not, `pivot: no route to <ip>`.
2. Skill check: `1d100 ≤ 50 + 5 × INT_mod + 10 × CatHacking_rank + 15 × PivotMaster_rank`.
3. **Long-channel**: 3 base turns, 4 Heat, no Detection (Grid-only). Skill scaling applies.
4. On success:
   - Trace -= 8 (`PivotMaster` ranks add -3 each).
   - Current shell session closes; new shell session opens against target IP.
   - Connection ritual replays for the new device.
   - History is preserved in the underlying terminal scroll — readable via the PDA's outer terminal scrollback, but the new session has its own banner and prompt.
5. On failure:
   - Trace += 15, Detection += 0 (Grid-only).
   - 25% chance: alarm on the LAN sector (spawns a Gray ICE on the route).
   - Prompt remains on the original device.
   - Message: `pivot: connection refused — honeypot detected, trace boosted`.

### Real-world block

`pivot` is registered but unconditionally rejects when `DeviceShell.via == RealWorld`:
```
TURRET-OS:root# pivot 10.0.4.18
pivot: unavailable from a wired session.
       (Yank the cable and walk to the next device, or jack into the LAN.)
```

### Why pivots matter

This is the crux of the playstyle split:

- **Netrunner** — walks the LAN spatial sector. Engages ICE. Manages Trace through gateway choice and breach timing. Never relies on `pivot`.
- **Sysadmin** — never leaves the shell. Pivots through the LAN's logical graph, dumping Trace as they go, picking off devices one by one.

Both can clear the same LAN. Different gear emphasis (the Sysadmin wants Heat capacity, the Netrunner wants Trace decay), different skill-tree paths, different vibes.

---

## 9. Authoring layers

### Layer 1 — Commands (per-tag)

Decoupled from fixture kind and faction. A new tag in Plan 5's enum requires writing the corresponding `cmd_*.cpp` files. A new fixture inheriting an existing tag costs nothing.

### Layer 2 — Banner / OS-name / version (per-FixtureType)

Static table:

```cpp
struct FixtureOsId {
    const char* os_name;      // "TURRET-OS"
    const char* version;      // "v2.7.4"
    const char* prompt_user;  // "root"
    const char* prompt_host;  // "TURRET-17"  (with %d substitution for ID suffix)
    const char* prompt_glyph; // "$" guest, "#" root  (per-faction override possible)
};
const FixtureOsId& os_id_for(FixtureType f);
```

Per-FixtureType ASCII art / banner-shape lives in the same table — short string with `%FACTION%`, `%VERSION%`, `%FIXTURE_NAME%` substitution slots.

### Layer 3 — Faction flavor packs

Per-faction tables, in code:

```cpp
struct HackFlavorPack {
    const char* faction_name;      // "Kaguya Heavy Industries" / "Cartel" / etc.
    const char* root_user_name;    // "root" / "ROOT_USER (mikko)" / "▲"
    std::span<const char*> motd_lines;     // pool, one selected at random
    std::span<const char*> log_lines;      // templates with %TIME%, %USER%, %IP%
    std::span<const char*> user_names;     // for fake home dirs and fs entries
    std::span<const char*> file_contents;  // templates for `cat` results
    std::span<const char*> banner_chrome;  // box-drawing decorations / ASCII art
};
const HackFlavorPack& flavor_for(Faction f);
```

**Stored in code**, not external files. `hack_flavor_corp.cpp` etc. are pure data tables.

### Authoring budget

| Asset | Per-faction count (target) |
|---|---|
| MOTD lines | 8 |
| Log line templates | 6 |
| User-name pool | 12 |
| File-content templates | 10 |
| Banner chrome variants | 3 |

Five factions × ~40 lines of strings per asset class = ~1000 string literals. Substantial but linear in code, no parser, no asset pipeline.

---

## 10. v1 faction packs

### Corp (Kaguya / Helion / generic Corporate)

- **Voice.** Legalistic, sterile, version-numbered, threats of prosecution.
- **Sample MOTD.** `Unauthorized access prosecuted under Sec. 12 of the Combined Charter.` / `This system is monitored. By accessing it you agree to the EULA.` / `KHI Asset 0x4477 — do not modify.`
- **Sample log line.** `2476-04-12.08:14:33 INFO auth.success user=admin from=10.0.4.1`
- **Banner sample (Turret):**
  ```
  ╔══════════════════════════════════════════════╗
  ║  KAGUYA HEAVY INDUSTRIES — TUR-OS v3.1.4     ║
  ║  Property of Kaguya Heavy Industries.        ║
  ║  Unauthorized access prosecuted under        ║
  ║  Sec. 12 of the Combined Charter.            ║
  ╚══════════════════════════════════════════════╝
  Last login: 2476-04-12.08:14 by root from 10.0.4.1
  ```

### Cartel

- **Voice.** Profane, personal, busted-but-functional, sticky notes from "mikko."
- **Sample MOTD.** `if u read this and ur not mikko ur fired` / `DO NOT UPDATE — last guy bricked it` / `wifi password: cartel123 dont tell denis`
- **Sample log line.** `04/12 8:14a — auth ok mikko frm vending closet`
- **Banner sample (Vending):**
  ```
    VENDOTRON 9 v1.4 (modified — do not update)
    THIS UNIT BELONGS TO MIKKO. DO NOT FUCK WITH IT.
    if u read this and ur not mikko ur fired
     -- mikko
  ```

### Precursor

- **Voice.** Cryptic, glyphic, wrong-feeling timestamps, fragmentary.
- **Sample MOTD.** `we left this here for them` / `the door is the door is the door` / `▲ touch the mirror`
- **Sample log line.** `CYCLE 11704883 ⟁ ▲▼▲ ⟁ ???`
- **Banner sample (Console):**
  ```
  ⟁ ARCH.PRE / NODE-▲▼▲ / CYCLE 11704883
     we left this here for them
     the door is the door is the door
     ▲ touch the mirror
  ```

### Conclave

- **Voice.** Liturgical, preachy, "the door is closed to the unworthy."
- **Sample MOTD.** `What is closed shall remain closed to the unworthy.` / `Keep faith. Be patient.` / `The Lit Path watches.`
- **Sample log line.** `Day 412 of the Lit Path — 8th hour — admission of the worthy: roenne`
- **Banner sample (Door):**
  ```
    ✚ The Conclave of the Lit Path ✚
    ✚ Door 7-3 / Sanctum Gate          ✚
    ✚ "What is closed shall remain     ✚
    ✚  closed to the unworthy."        ✚
    ✚ Keep faith. Be patient.          ✚
  ```

### Civilian / Outpost

- **Voice.** Mundane, friendly, careless ("password is on the fridge").
- **Sample MOTD.** `hi! please dont mess with the settings` / `wifi password is on the fridge` / `if you find a bug pls let greta know`
- **Sample log line.** `apr 12 8:14am — greta logged in (front porch cam)`
- **Banner sample (Camera):**
  ```
  CAM-04 v0.9 (Outpost Co-op stock unit)
  Hi! This is Greta's camera. If you're a friend
  the wifi password is on the fridge — please don't
  mess with the settings, last time it took us a week.
  ```

---

## 11. Filesystem (`ls` / `cat` / `grep` / `find`)

### Procedural generation

When the shell opens, `DeviceFsView` builds an in-memory map of `path → content` for the device:

- `/etc/version` — always present. One-line OS string.
- `/etc/motd` — always present. Selected MOTD line.
- `/var/log/auth.log` — always present. 5–10 generated log lines using faction templates.
- `/var/log/<system>.log` — one per relevant tag (`optics.log` for `HasOptics`, `power.log` for `PowerNode`, etc.).
- `/home/<user>/` — one user from the faction pool. Contains 1–3 small files (`notes.txt`, `todo.txt`) using `file_contents` templates.
- `/firmware/<tag>.fw` — one entry per tag. Permission denied at guest, readable as a hex blob at root.
- `/data/` — present only for `DataStore`-tagged devices. Contains the "loot": lore fragments (Plan 4), credit balances (vending), encrypted archives (Precursor consoles).

Generation is seeded from `(network_id, hackable_id)` so re-opening the shell always shows the same files — but new devices and new playthroughs get fresh content.

### Permission model

- `guest` — `/etc/version`, `/etc/motd`, top-level `/var/log/auth.log` only. Anything else: `permission denied`.
- `root` — everything except files explicitly marked `kernel-only` (rare, plot-gated).

### Reads stay free

`ls`, `cat`, `grep`, `find` are all instant. The whole filesystem is a flavor + lore vehicle. Reading a Cartel terminal's `/home/mikko/notes.txt` and finding a clue about a stash location is the *point*.

### `dump` and `wipe`

`dump <path>` is the privileged sibling — leeches the file contents to player inventory as a `data fragment` item (long-channel, partial-state via `dumped_bytes`). `wipe <path>` permanently removes the file from the device (long-channel, atomic).

---

## 12. UX walkthroughs

### Walkthrough A — real-world door bypass

```
[player approaches a locked corp door, has Cat_Hacking + cyberdeck]

> press E (interactables widget)
  → menu: [Open (locked)] [Hack]
> select [Hack]
  → body becomes wired-in
  → PDA opens, hacking tab active

pda> ssh root@10.0.4.22

Connecting to 10.0.4.22.... [SUCCESS]
Negotiating cipher... [SUCCESS]
Authentication accepted. Welcome Guest.

╔═══════════════════════════════════════════╗
║ KAGUYA HEAVY — DOOR-OS v2.0               ║
║ Restricted Access. Authorization required.║
╚═══════════════════════════════════════════╝

KAGUYA-DOOR-22:guest$ help
  hashcat  - attempt password recovery
  unlock   - request open (requires root)
  ls / cat / whoami / help / clear / history / exit

KAGUYA-DOOR-22:guest$ hashcat --fast
[*] Cracking authentication... [▓▓▓▓▓▓▓▓▓▓] 100%
[+] Recovered: 4Z71J9K8X2L
[+] Authentication elevated.

KAGUYA-DOOR-22:root# unlock
[+] Door cycle requested.
[+] Lock disengaged.

KAGUYA-DOOR-22:root# exit
[player unwires; door is now unlocked, walks through]
```

### Walkthrough B — in-Grid pivot chain

```
[player has jacked into a station LAN, walked the spatial sector,
 reached a Camera node's gateway tile, breached it]

CAM-07:guest$ hashcat --fast
[+] Recovered.
CAM-07:root# ls /home/mikko/
notes.txt  todo.txt
CAM-07:root# cat /home/mikko/notes.txt
"the turret in 4-A is on default creds. dont tell denis."

CAM-07:root# pivot 10.0.4.17
[*] Pivoting... [▓▓▓░░░░░░░] 30%   trace -3, heat +2

Connecting to 10.0.4.17.... [SUCCESS]
...
KAGUYA-TUR-17:guest$ hashcat --fast      # default creds — instant
[+] Recovered immediately (default credentials).
KAGUYA-TUR-17:root# friendly_fire --target=cartel
[*] Reconfiguring target priority... [▓▓▓▓▓▓▓▓▓▓] 100%
[+] Turret will engage Cartel-tagged actors as hostile.

KAGUYA-TUR-17:root# pivot 10.0.4.18
...
[player chains through 4 devices without ever walking back to a gateway]
```

---

## 13. Persistence

All shell-driven mutations persist via Plan 5's tile-mutation persistence:

- `Hackable.escalated` — sticks across sessions. Once cracked, always cracked (until firmware reflash).
- `Hackable.cracked_digits` — partial-crack progress, sticks across save/load.
- `Hackable.firmware_state` — Stock/Wiped/Glitched. Wiped turret is permanently dead until tinkered.
- `Hackable.dumped_bytes` — partial dump progress.
- Filesystem mutations (`wipe`) — encoded as a small bitmask of "wiped-paths" on the Hackable. Persists.

No new save file. Schema bump on `galaxy_*.dat` (per project rule, reject old saves).

---

## 14. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Command count balloons during authoring | High | Hard cap at "one cmd file per concept." If a tag wants more than 5 commands, redesign the tag. |
| Faction flavor packs feel samey in play | Medium | Specific banner samples are designed to vary on register, not just word choice. Playtest after pack 2; if the texture isn't differentiated, rework before shipping pack 3+. |
| Long-channel pacing wrong on first play | High | All `base_turns` and `base_heat` are constants in `cmd_*.cpp`. Tunable post-merge. |
| Real-world frozen body feels frustrating | Medium | The flow is opt-in (skill-gated) and short-loop (most channels < 15 turns). If still bad, add a "hold-to-channel" ergonomic shortcut so common actions feel responsive. |
| `--help` cost numbers drift from runtime values | Medium | Single source: scaling lives in one helper; both `--help` and channel-runtime call it. |
| Pivot makes spatial walking feel useless | Medium | Pivot costs Heat per hop; a deep pivot chain runs out of Heat before a deep walk runs out of patience. Trace-dump benefit is small per pivot, so reckless pivoting still trips alarms. Tune in playtest. |
| Filesystem becomes lore-firehose / dilutes signal | Low | Per-device file count capped; lore fragments live behind `DataStore` only; rest is texture. |
| In-code flavor packs balloon `hack_flavor.cpp` | Low | Split per-faction file the moment any single pack crosses ~150 lines. |

---

## 15. Open questions (for the implementation plan)

These are tactical, not design — they belong in the plan:

- Which key opens the in-Grid shell sub-mode? (Reuse PDA toggle? Dedicated key?)
- Visual rendering of the inline progress bar — terminal cell width, refresh rate, character set.
- Does an alarm on the LAN auto-yank a real-world wired-in player, or only abort the active channel?
- AI awareness of "hacking in progress" — v1 keeps AI dumb to it; revisit if playtest shows guards walk past frozen players too obliviously.
- Order of cuts: real-world doorway first vs. in-Grid doorway first? (Recommend real-world first — simpler context, faster iteration.)
- Skills `PivotMaster`, `ColdHands`, `RootKit` — placement in the `Cat_Hacking` tree.
- `cmd_pivot.cpp` lives in `src/hack_commands/` like the others, even though it's special-cased Grid-only.

---

## 16. Plan 5 amendments — `nmap` / `jack` / breach semantics

Plan 5's hacking-terminal CLI (`pda_hacking_tab`) and `GridNmapWidget` ship with two affordances that **conflict with this spec's auth model** and must be revised when this spec lands. They're called out here so Plan 7's implementation plan can carry the change as part of its scope.

### The conflict

Plan 5 (current) gates Grid entry on per-device lock state in two places:

1. **`jack <ip>` CLI** — returns `jack: locked — try breach.exe` when the destination's gateway edge is locked. The player must run `breach.exe` first, then re-issue `jack`.
2. **`nmap -m` widget — `b` key** — runs `breach.exe` on the cursor's current edge from outside the sector. Per-device "button-mash breach" with no other interaction.

Both treat **per-device locks** as a Grid-traversal gate that must be cleared before entry. This contradicts the device-shells design, where:

- `ssh root@<ip>` always succeeds and lands the player at `guest` tier on locked devices.
- `hashcat` (long-channel, inside the shell) is the only path to escalation.
- There is no remote per-device unlock. Lock state is resolved diegetically, not as an external gate.

If both systems coexist, the player has two parallel unlock paths (`b`-press in nmap; `hashcat` in shell) that diverge in cost, speed, risk, and skill expression — exactly the kind of redundancy the original hacking spec rules out.

### Required changes

1. **Drop the `jack <ip>` lock error.** `jack <ip>` succeeds for any reachable IP on the current LAN (and Atlas warp tiles in deep-Grid). Locked devices still resolve normally — the player arrives in the device's subnet sector at guest privilege and can crack from there. The error string `jack: <ip>: host unreachable` remains for unknown IPs; `jack: requires Cat_Hacking skill.` remains. The `locked — try breach.exe` line is removed.

2. **Drop `nmap -m`'s `b` key for per-device gateway edges.** Per-device `╳` gateway edges in the LAN graph are no longer breachable from the netmap. The Enter key (jump-to-sector) works on any node regardless of lock state — it routes through the new always-succeeding `jack` path.

3. **Retain `breach.exe` for spatial firewalls only.** The breach mechanic survives but is **scoped to region/zone firewalls**: tier-2/3 walls in the LAN spatial sector that gate a *region* (not a device), and the deep-Grid Atlas↔Frontier and inter-Frontier firewalls. These are walked-up-to-and-pressed in the spatial sector itself; they are not netmap-side actions.

4. **`nmap -m` `b` key — removed or repurposed.** Cleanest: remove. If retained, only fires on region-firewall edges (not per-device gateway edges) and still runs `breach.exe` with full program cost — but the simpler call is to remove the netmap-side breach affordance entirely and let the spatial sector own it. **Recommended: remove.**

5. **`nmap -l` listing — keep tier column, mark it informational.** The `tier: 2 (locked)` field stays in `ping` and `nmap -l` output so the player knows what they're walking into. Make explicit (in `man nmap` and `--help`): "Tier indicates the privilege you'll receive on connect; locked devices land you at guest. Locked is not a barrier to `jack`."

### What this preserves

- LAN sector firewalls between regions still exist as spatial obstacles. The Netrunner playstyle still has Grid-walking gates to clear.
- `breach.exe` program is still useful — just for region-scope obstacles instead of per-device.
- Plan 5's existing `apply_breach_grid` machinery stays; its callers shrink.

### What this breaks (and that's fine)

- Players who currently use `nmap -m` + `b` + Enter as a fast unlock path lose that path. The replacement — `jack <ip>` → `hashcat --fast` — is *richer* (skill check, partial state, INT/skill scaling, faction-flavored shell) and explicitly the design.
- One-step quickhacks against locked devices remain unaffected; QHs target by tag mask, not lock state.

### Where this lives in the implementation plan

This amendment lands in the same plan as the device-shells implementation, because:

- Removing `jack`'s lock error without `hashcat` available leaves locked devices unreachable.
- Removing `nmap -m`'s `b` without the shell-tier model leaves players with no unlock path at all.

The two changes are atomic: device-shells ships and the Plan 5 amendments land in the same sequence of commits.

---

## 17. Cross-references

- Plan 4 spec — `2026-04-30-hacking-deep-grid-design.md` — body-phased-out behavior, Trace/Heat semantics.
- Plan 5 spec — `2026-05-01-grid-expansion-design.md` — tag taxonomy, LAN sector, `Hackable.ip`, persistence machinery, current `nmap`/`jack`/breach semantics that §16 amends.
- Root hacking spec — `2026-04-29-hacking-design.md` — `Cat_Hacking` skill tree, PDA structure, the v1-exclusion line that this spec answers.
- Plan 5 handoff — `docs/plans/2026-05-01-grid-loop-handoff.md` — current branch context.

---

## 18. Status

Draft. Awaiting user review. Implementation plan to follow once approved.
