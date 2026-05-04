# Per-Device Diegetic Shells (Plan 7) — Design Spec

**Date:** 2026-05-01 (drafted) / 2026-05-03 (revised post-brainstorm)
**Status:** Approved
**Branch (target):** `feature/device-shells`
**Parent specs:**
- `2026-04-29-hacking-design.md` (root hacking spec)
- `2026-04-30-hacking-deep-grid-design.md` (Plan 4 — deep-Grid)
- `2026-05-01-grid-expansion-design.md` (Plan 5 — LAN sector + tags)
- `2026-05-02-grid-hud-design.md` (Plan 6 — Tron HUD)

This is the Plan 7 spec, sequenced after Plan 5 (LAN/tag system) and Plan 6 (HUD redesign). Other Plan 7-adjacent ideas (Your.Anchor full mechanics, alien-tech hacking dialect, darknet revival) are scoped to later plans — see §16.

---

## 1. Concept

Every electronic device runs a fake mini-OS the player can drop a CLI shell into. The shell is the **diegetic surface** of Plan 5's tag-driven hacking model: a device's `HackTagMask` directly determines which commands are available; its `FixtureType` provides the OS banner and prompt; its faction provides the flavor of files, log entries, fake usernames, and MOTD copy.

The shell is reachable from two doorways, both gated by `Cat_Hacking`:

- **Real-world.** Walk up to an electronic fixture; the interactables widget shows a `(hack) Shell Access` option. Selecting it wires the player's body into the device's data port and autoruns `ssh ...@<device-ip>` in the cyberdeck shell. Body is frozen, vulnerable, Esc to yank the cable. Wired-in access reaches **only the one device** the cable is plugged into.
- **In-Grid.** The player has jacked into a LAN spatial sector (via a `(hack) Jack In` interactable on a `JackInPort` fixture). Avatar walks the LAN. To open a device shell, the avatar must be **adjacent to that device's gateway tile** in the spatial sector; from there the cyberdeck shell can ssh it.

The shells share one machinery — the spatial walking layer (Plan 5/6) is how you reach devices, and the shell layer is what you do at them. There is no "Sysadmin path" that bypasses spatial movement.

### Pillars

- **The shell is a UI surface, not a parallel universe.** It rides Plan 5's tag system. Adding a tag adds commands; adding a fixture inherits commands automatically. No central switch statement.
- **Two privilege tiers, one shell.** `ssh root@<ip>` succeeds at root only when the device is unlocked or already escalated; otherwise it rejects with a "permission denied" message and the player tries `ssh guest@<ip>`. `hashcat` (long-channel, inside the shell) is the only path to escalate guest → root. Locked-vs-unlocked is visible diegetically — no out-of-shell crack minigame.
- **Reads are free, privileged actions are committed.** Reading `cat /var/log` is instant, paused world. Privileged actions (`firmware --wipe`, `blind`, `friendly_fire`, etc.) are long-channel: world ticks, body is wired-in (real-world), Trace ticks (Grid). Inline progress bar shows in the shell itself.
- **Walking is mandatory.** SSH only succeeds against a device the player can reach — adjacent to its gateway tile (in-Grid) or wired into it via Shell Access (real-world). There is no remote ssh, no pivot, no chain. Each device interaction is an atomic walk-and-shell.
- **Authoring scales with content, not code.** Three orthogonal authoring layers (tag-derived commands / per-FixtureType banner / per-faction flavor) keep content debt linear. New fixture = pick tags + write banner string. New faction = write flavor pack.
- **Diegetic legibility.** `--help` exposes duration and cost *for this player* (post-INT/skill scaling). The system documents itself.

### Non-goals

- New tags beyond the Plan 5 set (`Electronic`, `Locked`, `PowerNode`, `DataStore`, `HasOptics`, `Weaponized`, `Mobile`, `AlienTech`, `JackInPort`).
- New programs / quickhacks. Shell commands are *not* programs and don't ship from inventory.
- Shell-driven combat. ICE is a Grid concept; the shell never spawns or fights ICE.
- Pivots / device-to-device ssh. Each device shell session is atomic; ssh originates only from the cyberdeck.
- Pre-shell crack minigames. The shell is always the only UI.
- Replacing the spatial LAN/subnet sectors. Shell is *additive*; spatial walking is the only traversal.
- Replacing quickhacks. QHs stay the fast-twitch combat layer; shell is the deep / slow / privileged layer.
- File-system simulation beyond what `ls` / `cat` need. No editors, no shells-within-shells, no piping, no scripting.
- Alien-tech hacking dialect (Precursor, Conclave/Stellari). Deferred to Plan 11 — see §16.

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
| `CyberdeckMods` | `cyberdeck_mods.h/cpp` | Tracks installed mods on the player's cyberdeck. v1 ships only the gate for `WirelessJackIn` (Aerojack / Untether). |

### Existing systems extended

- **`Hackable`** (`hackable.h`, exists post-Plan 5) — gains `firmware_state: enum { Stock, Wiped, Glitched }`, `cracked_digits: uint8_t` (for `hashcat` partial-state), `escalated: bool` (true once `root`), `dumped_bytes: uint32_t` (for `data --dump` partial-state). Plan 5's `tags` and `ip` already provide what the shell needs to address and gate.
- **`PdaHackingTab`** (`pda_hacking_tab.cpp`) — gains the `ssh` command. Resolves IP → Hackable → opens `DeviceShell`. The PDA's `pda>` shell stays for `nmap`/`programs`/`jack`/etc; the device's shell is a sub-mode the PDA tunnels into.
- **`HackingSystem`** (`hacking_system.h/cpp`) — drives the long-channel ticker, applies command effects, persists mutations. Owns the active `DeviceShell` (at most one).
- **Interactables (`interactables_widget`)** — gains two new entries on any `Hackable` with `Electronic` tag if the player has `Cat_Hacking`:
  - `(hack) Shell Access` — opens the device shell directly (wired-in real-world).
  - `(hack) Jack In` — only on `JackInPort`-tagged fixtures. Avatar enters the LAN spatial sector. As before.
- **`Cat_Hacking` skills** — two new skill nodes for v1: `ColdHands` (fewer Detection ticks per privileged command), `RootKit` (faster `hashcat`). The originally-spec'd `PivotMaster` is dropped (no pivot in v1).
- **Save (`galaxy_*.dat`)** — schema bumps for the new `Hackable` fields + `CyberdeckMods` slot. Per project rule: reject old saves, no migration.

### File-size discipline

`device_shell.cpp` ≤ ~400 lines. `hack_flavor.cpp` likely ~400 lines (3 packs × ~120 lines of templates) — split per-faction (`hack_flavor_corp.cpp`, etc.) if it crosses 800. Each `cmd_*.cpp` ≤ ~150 lines; if a command needs more, split or trim. `device_fs.cpp` ≤ ~250 lines.

---

## 3. The two doorways

### 3a. Real-world doorway (Shell Access)

**Trigger.** Player adjacent to any `Hackable` with `Electronic` tag and has `Cat_Hacking` unlocked. `interactables_widget` shows `(hack) Shell Access` as one of the hack options.

**Sequence.**

1. Player selects `Shell Access`.
2. Body becomes wired-in. State flag `player.is_jacked_into = hackable_id`. Movement, attack, item-use disabled. Render: `@` glyph gains a subtle cyan pulse / `[⟳]` superscript.
3. PDA opens to the Hacking tab. The `pda>` terminal autotypes `ssh <user>@<device-ip>` (smart-typed user — see §4) with a small typing animation (~0.3s) for vibe.
4. Connection ritual runs (1s, non-skippable). See §5.
5. Banner renders.
6. Prompt appears: `<DEVICE-NAME>:<tier>$ _`.

**Exit.** `exit`, closing the PDA, or pressing Esc at the prompt yanks the cable. Body unfreezes, shell session ends. Esc *during* a long-channel only aborts the channel; you're still in the shell.

**Reach.** The wire connects to ONE device. The cyberdeck cannot ssh other devices on the LAN through the wire. To reach more, the player must exit, walk away, and either Shell Access another device or Jack In via a `JackInPort` fixture.

**Constraints.**
- Body frozen for the entire shell duration — including idle time at the prompt. Tactical decision: open shell only when safe.
- Damage to the body interrupts the active channel but does **not** auto-close the shell. Shell remains open; the player can continue or exit.
- An alarm on the LAN does not auto-yank — it only interrupts an active channel (same rule). The shell stays open. Player decides whether to continue or exit.

### 3b. In-Grid doorway (cyberdeck ssh after spatial entry)

**Trigger.** Player has jacked into a LAN spatial sector via a `(hack) Jack In` interactable on a `JackInPort` fixture (this is the *only* way to enter a LAN spatially in v1 — `jack <ip>` from cyberdeck is mod-gated; see §15). Avatar is in the LAN sector. To shell a device, the avatar must walk to a tile **adjacent** to that device's gateway tile.

**Sequence.**

1. Player walks avatar adjacent to target device's gateway tile.
2. Player opens the cyberdeck shell (existing PDA toggle key) and types `ssh <user>@<ip>` — OR uses an in-Grid `(hack) Shell Access` interactable on the adjacent device, which autoruns ssh.
3. SSH validates: target IP must be the IP of the device whose gateway the avatar is currently adjacent to. If not: `ssh: <ip>: host unreachable (out of range)`.
4. Connection ritual fires (per §5). The Tron window content swaps from the spatial sector view to the device shell. HUD chrome (Trace/Heat, log pane) stays visible — this is critical for seeing Trace tick during long-channels.
5. Banner renders. Prompt appears.

**Exit.** `exit` or Esc at prompt closes the shell. Tron window swaps back to spatial sector view. Avatar at the same tile.

**During the shell.** Avatar is frozen at its tile in the spatial sector while the shell is open. ICE present in the same LAN sector continues moving and may attack the avatar. HP damage from Black ICE breaks the active channel (per §7). The player sees this in the log pane, e.g., `[!] Black ICE adjacent — channel will break on next strike`.

**Differences from real-world.**
- Body phased out per Plan 4. No physical-vulnerability state.
- Long-channel ticks the **Grid clock** (Trace, Heat), not the world clock.
- Avatar lives in the LAN sector and can be attacked by ICE while shell is open.

The shell *itself* — banner, commands, flavor, channel mechanics, interrupt rules — is identical in both doorways. Only the surrounding context differs.

---

## 4. Auth model — guest vs root

### Two-tier privilege

`ssh` always *attempts*; whether root succeeds depends on `Hackable` state.

| Tier | When | Visible as | Allowed |
|---|---|---|---|
| `guest` | `Hackable` is `Locked` and not yet escalated, OR player typed `guest@<ip>` | `<DEVICE>:guest$` and `Welcome Guest.` | Reads (`ls`, `cat`, `whoami`, `help`, `clear`, `history`), and the `Locked`-tag commands (`hashcat`, `unlock`) |
| `root` | `Hackable` is unlocked OR has been escalated this session | `<DEVICE>:root#` and `Welcome <root-name>.` | All tag-derived commands + reads + universals |

A device with no `Locked` tag opens directly at `root` on `ssh root@<ip>`.

### Manual vs autorun ssh

**Manual ssh (player typing at `pda>`):** strict semantics.
- `ssh root@<ip>` to a Locked-and-not-escalated device: rejects with
  ```
  ssh: <ip>: permission denied (root login disabled).
        try: ssh guest@<ip>
  ```
  No shell opens. The player tries again as guest.
- `ssh guest@<ip>`: always succeeds at guest tier (or root tier if device has no `Locked` tag — the privilege requested floors at what the player asked for).
- `ssh <ip>` (no user): defaults to `root@`.

The `nmap -l` listing exposes lock state per device, so the player knows which user to type without trial-and-error. See §15.

**Autorun ssh (from `Shell Access` interactable):** smart semantics.
- The cyberdeck inspects `Hackable.locked` and `Hackable.escalated`, then autotypes `ssh guest@<ip>` for locked-unescalated devices, `ssh root@<ip>` otherwise.
- The player always lands a shell — no rejection beat from an autorun.
- Diegesis: the cyberdeck is yours and knows what it's connecting to.

### Escalation

`hashcat --fast` is a long-channel command (typical: 8–14 turns, 4–8 Heat, +10–20 Detection scaled by INT/skill). On success: `Hackable.escalated = true`, prompt switches to `root#`, full command set unlocks. On failure (skill check): Heat is spent, `cracked_digits` increments by 1–2 (partial-state for next attempt), prompt stays `guest$`.

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

### Successful ssh

Hard-coded sequence, ~1 second, non-skippable. Output streams character-by-character at ~60 chars/sec:

```
Connecting to <DOTTED-IP>.... [SUCCESS]
Negotiating cipher... [SUCCESS]
Authentication accepted. Welcome <TIER-NAME>.

<BANNER-FROM-FIXTURE>

<MOTD-FROM-FACTION-PACK>

<DEVICE-NAME>:<TIER>$ _
```

`TIER-NAME` is `Guest` (guest tier) or one of the faction's authored root names (e.g., `Welcome root.` for Corp; `Welcome ROOT_USER (mikko)` for Cartel).

`MOTD-FROM-FACTION-PACK` is one randomly-selected line from the faction's MOTD list. Examples below in §10.

### Manual-ssh root rejection

When the player manually types `ssh root@<locked-ip>`, the ritual aborts at the auth step:

```
Connecting to <DOTTED-IP>.... [SUCCESS]
Negotiating cipher... [SUCCESS]
ssh: <ip>: permission denied (root login disabled).
      try: ssh guest@<ip>
pda> _
```

No shell opens. The player retypes as guest.

### Cursor rendering

The shell prompt's cursor is an **inverted-color block**, mirroring the existing `dev_console` cursor:

- Single cell.
- Background = the prompt's foreground color (typically `Color::White`).
- Foreground = the prompt's background color (typically `Color::Black`).
- Drawn *on top of* the character at the cursor position — so a cursor over `firmwa█e` shows the `r` as black-on-white in that one cell. End-of-line cursor shows a solid block over a space.
- Implementation reference: `dev_console::render` at `src/dev_console.cpp:1730–1732`.
- Behavior: arrow keys move the cursor through the input buffer, Home/End jumps to ends, Backspace/Delete edit, characters insert at cursor.
- During an active long-channel the prompt is busy (showing the progress bar); the input cursor is suppressed.

In sample blocks, the trailing `_` represents "cursor here, line empty" — read it as the inverted block at end-of-line.

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

Commands self-register at static-init time into a global registry. The registry exposes:

```cpp
const HackCommand* find_command(std::string_view name);
std::vector<const HackCommand*> commands_for(HackTagMask tags, bool is_root);
```

### Universal commands

Tag-less, always available:

| Command | Behavior |
|---|---|
| `help` | Lists all commands available on this device (current tier). |
| `<cmd> --help` | Synopsis + description + scaled cost. |
| `whoami` | Prints current user (`guest@TURRET-OS-2.7` or `root@TURRET-OS-2.7`). |
| `clear` | Clears the visible scroll. |
| `history` | Prints command history (this session). |
| `exit` | Closes the shell. |

No `pivot` — see §8.

### Tag → command starting map

| Tag | Commands |
|---|---|
| `Locked` | `hashcat`, `unlock` |
| `HasOptics` | `blind`, `feed`, `restream`, `purge` |
| `Weaponized` | `disarm`, `lockout`, `friendly_fire`, `targetlist` |
| `PowerNode` | `surge`, `kill`, `reroute`, `dim` |
| `DataStore` | `ls`, `cat`, `grep`, `dump`, `wipe`, `find` |
| `Mobile` | `halt`, `redirect`, `gps` |
| `AlienTech` | *(deferred to Plan 11 — see §16)* |

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

The cost numbers are computed live from `base_*` × INT/skill modifiers.

---

## 7. Long-channels

### Mechanics

A **long-channel** is any command where `base_turns > 0`. When executed:

1. Cost is paid up front: Heat += scaled, Detection += scaled (real-world only).
2. `HackChannel` becomes active. The shell renders an inline progress bar that updates every world tick.
3. Each world tick (or Grid tick, in-Grid) advances progress by `1 / scaled_turns`.
4. World/Grid simulation **continues** while the channel runs.
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

- **Heat-spent-is-spent.** No refund. Detection already added stays added.
- Effect is not applied — *unless* the command is `allow_partial = true`.
- For partial-state commands (`hashcat`, `dump`), the command's `on_partial(progress, Hackable&)` hook fires.

Authored partial-state commands in v1: `hashcat`, `dump`. Everything else aborts atomically.

### Body during channel (real-world)

- **Frozen.** No movement, no attacks, no item use. Esc and read commands only.
- Wired-in state shown by `@` glyph cyan pulse / `[⟳]` superscript.
- Enemies can target the player normally. AI sees the player as a normal target (no special "is hacking" awareness for v1).

### Avatar during channel (in-Grid)

- Avatar phased per Plan 4. Frozen at its tile in the LAN sector while shell is open.
- ICE present in the same sector continues moving. Black ICE adjacency triggers a log warning; HP damage breaks the channel.

---

## 8. SSH adjacency rule

This replaces the original spec's "Pivots" section (deleted).

**Rule.** SSH succeeds against a target IP only if:

1. The player is **wired into that exact device** via Shell Access (real-world doorway), OR
2. The player's avatar is **adjacent to that device's gateway tile** in the LAN spatial sector (in-Grid doorway).

Anything else: `ssh: <ip>: host unreachable (out of range)`.

### Consequences

- **No pivots.** A device shell cannot ssh another device. To go elsewhere, the player exits, walks the avatar to the next device's gateway, and ssh's from cyberdeck.
- **No remote ssh from a corner.** Walking is required for every device interaction.
- **Region firewalls (tier 2/3) act as walls** — they block avatar movement, which automatically blocks ssh-reach to devices on the far side. To reach those devices, breach the firewall in the spatial sector first (`breach.exe`, walked-up-to).
- **Wired-in real-world is single-device.** The wire grants no LAN reach beyond the device it's plugged into.

### Why this is the model

The original spec experimented with `pivot` (device-to-device ssh) as a way to make a "Sysadmin" playstyle. In design review (2026-05-03), this was rejected as creating a parallel path that bypassed Plan 5/6's spatial work. The chosen rule (adjacency-only ssh) collapses the playstyle space: walking is the only traversal, shells are what you do at the destinations.

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
    const char* root_user_name;    // "root" / "ROOT_USER (mikko)" / "greta"
    std::span<const char*> motd_lines;     // pool, one selected at random
    std::span<const char*> log_lines;      // templates with %TIME%, %USER%, %IP%
    std::span<const char*> user_names;     // for fake home dirs and fs entries
    std::span<const char*> file_contents;  // templates for `cat` results
    std::span<const char*> banner_chrome;  // box-drawing decorations / ASCII art
};
const HackFlavorPack& flavor_for(Faction f);
```

**Stored in code**, not external files.

### Authoring budget (v1 — three packs)

| Asset | Per-faction count (target) |
|---|---|
| MOTD lines | 8 |
| Log line templates | 6 |
| User-name pool | 12 |
| File-content templates | 10 |
| Banner chrome variants | 3 |

Three factions × ~40 lines per asset class = ~600 string literals. Linear in code, no parser.

### Civilian as fallback

Any device whose `Faction` does not match an authored pack falls back to **Civilian**. This includes `Faction::None` and any faction whose pack isn't shipped yet (Precursor, Conclave, future factions). Civilian's "you-are-in-an-ungated-place" tone is the safe default.

---

## 10. v1 faction packs (Plan 7)

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

### Civilian / Outpost (also serves as Faction::None fallback)

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

### Deferred to Plan 11

Precursor and Conclave (Stellari) ship as the **alien hacking dialect** — see §16. Not faction flavor packs but a parallel command vocabulary with their own filesystem-equivalent and escalation analog.

---

## 11. Filesystem (`ls` / `cat` / `grep` / `find`)

### Procedural generation

When the shell opens, `DeviceFsView` builds an in-memory map of `path → content` for the device:

- `/etc/version` — always present. One-line OS string.
- `/etc/motd` — always present. Selected MOTD line.
- `/var/log/auth.log` — always present. 5–10 generated log lines using faction templates.
- `/var/log/<system>.log` — one per relevant tag (`optics.log` for `HasOptics`, `power.log` for `PowerNode`, etc.).
- `/home/<user>/` — one user from the faction pool. Contains 1–3 small files (`notes.txt`, `todo.txt`).
- `/firmware/<tag>.fw` — one entry per tag. Permission denied at guest, readable as a hex blob at root.
- `/data/` — present only for `DataStore`-tagged devices. Contains the "loot": lore fragments (Plan 4), credit balances (vending), encrypted archives.

Generation is seeded from `(network_id, hackable_id)` so re-opening the shell always shows the same files.

### Permission model

- `guest` — `/etc/version`, `/etc/motd`, top-level `/var/log/auth.log` only. Anything else: `permission denied`.
- `root` — everything except files explicitly marked `kernel-only` (rare, plot-gated).

### Reads stay free

`ls`, `cat`, `grep`, `find` are all instant. The whole filesystem is a flavor + lore vehicle.

### `dump` and `wipe`

`dump <path>` — privileged sibling, leeches contents to player inventory as a `data fragment` item (long-channel, partial-state via `dumped_bytes`). `wipe <path>` — permanently removes the file from the device (long-channel, atomic).

---

## 12. UX walkthroughs

### Walkthrough A — real-world Shell Access door bypass

```
[player approaches a locked corp door, has Cat_Hacking + cyberdeck]

> press E (interactables widget)
  → menu: [Open (locked)] [(hack) Shell Access] [(hack) Jack In  -- not on this door]
> select [(hack) Shell Access]
  → body becomes wired-in
  → PDA opens, hacking tab active

pda> ssh guest@10.0.4.22    # smart-typed: door is locked, autorun chose guest

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

### Walkthrough B — in-Grid spatial walk + ssh chain

```
[player has jacked into a station LAN via a JackInPort fixture]
[avatar in LAN sector; player walked to a CAM-07 gateway tile]

pda> nmap -l
HOST          OS         TIER             FACTION
10.0.4.07     CAM-OS     2 (locked)       cartel
10.0.4.17     TUR-OS     2 (locked)       cartel
10.0.4.22     DOR-OS     1 (cracked)      cartel
...

pda> ssh root@10.0.4.07
ssh: 10.0.4.07: permission denied (root login disabled).
      try: ssh guest@10.0.4.07

pda> ssh guest@10.0.4.07
[connection ritual — Tron window swaps to shell view]
CAM-07:guest$ hashcat --fast
[+] Recovered.
CAM-07:root# ls /home/mikko/
notes.txt  todo.txt
CAM-07:root# cat /home/mikko/notes.txt
"the turret in 4-A is on default creds. dont tell denis."

CAM-07:root# exit
[Tron window swaps back to spatial sector]
[avatar still at the CAM-07 gateway tile]

[player walks avatar through the sector to the TUR-17 gateway tile]

pda> ssh root@10.0.4.17
[autorun? no — manual; root@ on locked turret rejects]
ssh: 10.0.4.17: permission denied (root login disabled).
      try: ssh guest@10.0.4.17

pda> ssh guest@10.0.4.17
CAM-... wait, actually the turret notes said "default creds"

[player tries hashcat on TUR-17 — Cartel default-creds shortcut applies, instant]
TUR-17:guest$ hashcat --fast
[+] Recovered immediately (default credentials).
TUR-17:root# friendly_fire --target=cartel
[*] Reconfiguring target priority... [▓▓▓▓▓▓▓▓▓▓] 100%
[+] Turret will engage Cartel-tagged actors as hostile.

TUR-17:root# exit
[player walks to next device, repeats]
```

The Sysadmin-who-never-walks chain from the original spec is gone. Each new device requires the avatar to physically walk to its gateway tile.

---

## 13. Persistence

All shell-driven mutations persist via Plan 5's tile-mutation persistence:

- `Hackable.escalated` — sticks across sessions.
- `Hackable.cracked_digits` — partial-crack progress.
- `Hackable.firmware_state` — Stock/Wiped/Glitched.
- `Hackable.dumped_bytes` — partial dump progress.
- Filesystem mutations (`wipe`) — encoded as a small bitmask of "wiped-paths" on the Hackable.

Plus `CyberdeckMods` slots — see §15. Empty in v1 saves; populated when mods land.

Schema bump on `galaxy_*.dat` (per project rule, reject old saves).

---

## 14. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Command count balloons during authoring | High | Hard cap at "one cmd file per concept." If a tag wants more than 5 commands, redesign the tag. |
| Faction flavor packs feel samey in play | Medium | Three-pack v1 was specifically scoped to maximize voice contrast (sterile / personal / friendly). Playtest after Plan 7 ships before committing to alien-tech dialect work. |
| Long-channel pacing wrong on first play | High | All `base_turns` and `base_heat` are constants in `cmd_*.cpp`. Tunable post-merge. |
| Real-world frozen body feels frustrating | Medium | The flow is opt-in (skill-gated) and short-loop (most channels < 15 turns). |
| `--help` cost numbers drift from runtime values | Medium | Single source: scaling lives in one helper; both `--help` and channel-runtime call it. |
| Walking-to-each-gateway feels grindy in dense regions | Medium | LAN gen sets gateway placement; if dense regions feel like a slog, sparsify gateways or cluster devices. Tune in playtest. |
| Filesystem becomes lore-firehose / dilutes signal | Low | Per-device file count capped; lore fragments live behind `DataStore` only; rest is texture. |
| In-code flavor packs balloon `hack_flavor.cpp` | Low | Split per-faction file the moment any single pack crosses ~150 lines. |

---

## 15. Cyberdeck mod system (gate only in v1)

### Concept

The cyberdeck has installable mods that unlock new capabilities. v1 ships **only the gate** for one mod category — `WirelessJackIn` — used to gate `jack <ip>` from the cyberdeck shell. The mod system itself (slots, install UI, tinkerer NPCs, mod balance) is deferred to Plan 11+.

### `WirelessJackIn` category

When installed, allows `pda> jack <ip>` to enter the LAN spatial sector at the IP's location. When NOT installed, the command returns:

```
pda> jack 10.0.4.17
jack: no wireless jack-in device installed.
       (requires Wireless Jack-In Module.)
```

### Brand variants

Two brands ship as items in v1 — both functionally identical at install time (since the install UI doesn't exist yet) but defined so they can be tested:

- **Aerojack** — short, punchy, a one-word inventory entry. Tier 1.
- **Untether** — alternative brand from a different in-world manufacturer. Tier 1.

Both items must be:
1. Defined in `docs/items.md` with stats stub (range, trace cost, install slot — values TBD; v1 stats can be placeholder).
2. Registered in the item database so they can be spawned.
3. Wired through `CyberdeckMods` such that having one in inventory (as a placeholder for "installed") enables the `jack <ip>` command.

For v1 testing: the mod is "installed" simply by being present in inventory. No install ritual. When Plan 11 lands the proper mod system, this rule changes.

---

## 16. Alien tech (Precursor / Conclave / Stellari) — deferred to Plan 11

Precursor and Conclave (the Stellari) are non-human civilizations. Their devices are not running BSD with a different MOTD; POSIX `ls`/`cat`/`grep` is a *human* idiom. Plan 7 punts on alien tech entirely — `AlienTech`-tagged devices are not hackable in v1.

### v1 behavior for AlienTech-tagged devices

- The `interactables_widget` does **not** show `(hack) Shell Access` on `AlienTech`-tagged devices.
- Manual `ssh root@<alien-ip>` returns: `ssh: <ip>: protocol not understood (alien tech).`
- They still appear in `nmap` listings — marked as `OS: ??? (unknown)` — so the player knows they exist but can't interact.

### Plan 11 scope (preview)

When Plan 11 lands the alien hacking dialect, it ships:
- A parallel command vocabulary (`attune`, `resonate`, `mirror`, `query`, `decode` instead of `ls`/`cat`/`grep`).
- A glyph-node "filesystem" analog (no `/etc/motd`; instead, addressable echo nodes).
- An escalation analog (`attune` as the alien `hashcat`).
- Precursor and Conclave/Stellari packs (formerly intended as faction flavor packs, now full command sets).
- Reuses the same `HackChannel`, partial-state, and persistence machinery as v1's POSIX shells.

---

## 17. Plan 5 amendments

Plan 5's hacking-terminal CLI ships with affordances that conflict with this spec's model. The amendments below land **in the same plan** as Plan 7 implementation.

### A1. `jack <ip>` is now mod-gated

Plan 5 (current): `jack <ip>` lands the player's avatar in the LAN sector for any reachable IP.

Plan 7 (revised): `jack <ip>` requires a Wireless Jack-In Module mod installed in the cyberdeck. v1 ships no mod (or ships Aerojack/Untether as findable items), so the default behavior is the error in §15.

The §16 amendment from the original Plan 7 spec ("jack always succeeds for any IP regardless of lock") is superseded — `jack` doesn't run at all without the mod.

### A2. Spatial entry to a LAN requires a `(hack) Jack In` interactable

The only way to enter a LAN spatially in v1 is via a `(hack) Jack In` interactable on a `JackInPort`-tagged fixture in the real world. Walk up, select Jack In, avatar lands in the LAN.

### A3. `nmap -m` `b` key removed

Per the original Plan 7 spec §16: `nmap -m`'s `b` key (per-device breach via netmap) is removed. `breach.exe` survives only for region-scope firewalls walked-up-to in the spatial sector.

### A4. `nmap -l` lock-state column kept

`nmap -l` and `ping <ip>` continue to show per-device tier and lock state. This is canonical info — players use it to plan ssh attempts (manual ssh strict means knowing lock state up front avoids reject beats).

### A5. `breach.exe` retained for region firewalls only

`breach.exe` survives but scoped to region/zone firewalls (tier-2/3 walls in the LAN spatial sector that gate a *region*) and the deep-Grid Atlas↔Frontier and inter-Frontier firewalls. Walked-up-to-and-pressed in the spatial sector. Not netmap-side.

### What this preserves

- LAN sector firewalls between regions still exist as spatial obstacles.
- `breach.exe` stays useful — for region-scope obstacles.
- Plan 5's existing `apply_breach_grid` machinery stays; its callers shrink.

### What this breaks (and that's fine)

- Players who used `nmap -m` + `b` + Enter as a fast unlock path lose it. Replacement: `(hack) Jack In` to enter spatially → walk → `(hack) Shell Access` adjacent → `hashcat --fast`. Richer, deliberately so.
- `jack <ip>` from cyberdeck no longer works in v1. Players use the physical `Jack In` interactable.

---

## 18. Open questions for the implementation plan

These are tactical, not design — they belong in the plan file:

- Visual rendering of the inline progress bar — terminal cell width, refresh rate, character set.
- AI awareness of "hacking in progress" — v1 keeps AI dumb to it; revisit if playtest shows guards walk past frozen players too obliviously.
- Tinkerer / install-ritual UX for cyberdeck mods — out of scope for Plan 7; ships in Plan 11+.
- Aerojack vs Untether stat curves — both placeholder-equivalent in v1 since install UI doesn't exist; differentiate in Plan 11+.
- Real-world Shell Access on a `JackInPort` fixture — does it offer Shell Access AND Jack In as two options, or only Jack In? Recommend: both, since `JackInPort` devices have their own `Hackable` state.

---

## 19. Cross-references

- Plan 4 spec — `2026-04-30-hacking-deep-grid-design.md` — body-phased-out behavior, Trace/Heat semantics.
- Plan 5 spec — `2026-05-01-grid-expansion-design.md` — tag taxonomy, LAN sector, `Hackable.ip`, persistence machinery.
- Plan 6 spec — `2026-05-02-grid-hud-design.md` — Tron HUD overlay, log pane, render context.
- Root hacking spec — `2026-04-29-hacking-design.md` — `Cat_Hacking` skill tree, PDA structure.
- Plan 7 roadmap — `docs/plans/2026-05-03-plan-7-roadmap.md` — sub-project map.

---

## 20. Status

Approved. Implementation plan to follow at `docs/superpowers/plans/2026-05-03-device-shells.md`.
