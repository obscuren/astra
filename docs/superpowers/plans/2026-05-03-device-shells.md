# Device Shells (Plan 7) — Implementation Plan

**Date:** 2026-05-03
**Spec:** `docs/superpowers/specs/2026-05-01-device-shells-design.md`
**Branch:** `feature/device-shells`
**Format:** Flat task list (no cuts). Tasks ordered by dependency.

## Goal

Land per-device diegetic shells: tag-driven CLI per electronic fixture, two doorways (real-world Shell Access, in-Grid ssh adjacent to gateway), three faction flavor packs (Corp/Cartel/Civilian), and the cyberdeck-mod gate for `jack <ip>` with two brand items (Aerojack, Untether) defined and wired.

## Conventions

- Build with `-DDEV=ON` per CLAUDE.md.
- Each task is self-contained and incrementally compilable. Run `cmake --build build` after each task or task-group; fix breaks before moving on.
- File-size budget per spec §2: `device_shell.cpp ≤ ~400 LOC`, `hack_flavor.cpp` split at ~800 LOC threshold, `cmd_*.cpp ≤ ~150 LOC`, `device_fs.cpp ≤ ~250 LOC`.
- Update `docs/mechanics.md` whenever a numeric formula or rule lands.
- Update `docs/items.md` for Aerojack and Untether when item-defs land.

## Task list

### Foundations

1. **Extend `Hackable` struct** with `firmware_state` (enum Stock/Wiped/Glitched), `cracked_digits: uint8_t`, `escalated: bool`, `dumped_bytes: uint32_t`. Default-init all to zero/Stock. Update relevant constructors and serialization stubs.

2. **Bump save schema** for `galaxy_*.dat`. Per project rule (`feedback_no_backcompat_pre_ship`): reject older saves at load with a clear "save schema vN required" error. Bump version constant; audit `World::save_to`/`World::load_from` for the new fields.

3. **Define `HackCommand` struct** in `include/astra/hack_command.h` per spec §6. Fields: name, synopsis, description, required_tag, requires_root, base_turns, base_heat, base_detection, allow_partial, execute fn-ptr.

4. **Build `HackCommandRegistry`** with `register(HackCommand*)`, `find_command(name)`, `commands_for(HackTagMask, is_root)`. Static-init friendly: each `cmd_*.cpp` file registers itself on load.

5. **Create `HackChannel` state machine** (member of `DeviceShell`) per spec §7. Tracks: command, args, scaled_turns, scaled_heat, scaled_detection, progress, started_at_tick, allow_partial, on_partial hook. Add `tick(int ticks)` and `abort()` methods.

6. **Create `DeviceShell` class** in `device_shell.h/cpp`. Fields: `Hackable* target`, `tier (Guest/Root)`, `via (RealWorld/Grid)`, `history`, `active_channel: optional<HackChannel>`, `output_lines`, `input_buffer`, `cursor_pos`. Methods: `open()`, `submit_input()`, `tick()`, `render(ctx)`, `close()`.

7. **Implement DeviceShell prompt rendering** with the inverted-cursor block per spec §5. Reuse the `dev_console::render` pattern (`src/dev_console.cpp:1730–1732`). Arrow keys, Home/End, Backspace/Delete, mid-line insert.

8. **Implement long-channel inline progress bar rendering** in DeviceShell. Per spec §7: `[*] <verb>... [▓▓▓░░░░░░░] 30%` at the prompt while channel active. Suppress the input cursor during channel.

9. **Implement connection ritual streaming** per spec §5. Char-by-char output at ~60 chars/sec. Successful path: `Connecting → Negotiating → Authentication accepted → banner → MOTD → prompt`. Manual-ssh root-rejection path: `Connecting → Negotiating → ssh: permission denied (root login disabled). try: ssh guest@<ip>` and shell does NOT open.

### DeviceFsView + Filesystem

10. **Create `DeviceFsView`** in `device_fs.h/cpp` per spec §11. Build `path → content` map seeded from `(network_id, hackable_id)`. Always-present paths: `/etc/version`, `/etc/motd`, `/var/log/auth.log`. Tag-conditional paths: `/var/log/<system>.log` per tag, `/firmware/<tag>.fw` per tag, `/data/...` for `DataStore` only. `/home/<user>/notes.txt|todo.txt` from faction pool.

11. **Implement permission gate** in `DeviceFsView`: guest sees only `/etc/version`, `/etc/motd`, `/var/log/auth.log`. Root sees everything except `kernel-only` paths. Anything else: `permission denied`.

### HackFlavor packs

12. **Define `HackFlavorPack` struct** in `hack_flavor.h` per spec §9. Fields: faction_name, root_user_name, motd_lines, log_lines, user_names, file_contents, banner_chrome.

13. **Implement Civilian flavor pack** (`hack_flavor_civilian.cpp`) — voice tone per spec §10. 8 MOTDs / 6 log templates / 12 user names / 10 file content templates / 3 banner chromes.

14. **Implement Corp flavor pack** (`hack_flavor_corp.cpp`) — same asset budget. Sample content per spec §10.

15. **Implement Cartel flavor pack** (`hack_flavor_cartel.cpp`) — same asset budget. Sample content per spec §10.

16. **Wire faction lookup** with Civilian fallback: `flavor_for(Faction f)` returns the matching pack or Civilian if no pack registered. Single-rule fallback covers `Faction::None`, Precursor, Conclave, future.

17. **Define `FixtureOsId` table** per spec §9. Per-FixtureType: os_name, version, prompt_user, prompt_host, prompt_glyph. ASCII-art banner template with `%FACTION%`, `%VERSION%`, `%FIXTURE_NAME%` substitution slots.

### Universal commands

18. **Implement `cmd_help.cpp`** — lists commands available on this device at current tier. One line per command: `name — short description`.

19. **Implement `cmd_whoami.cpp`** — prints `<tier>@<host>-<version>`.

20. **Implement `cmd_clear.cpp`** — clears the visible scroll.

21. **Implement `cmd_history.cpp`** — prints command history this session.

22. **Implement `cmd_exit.cpp`** — closes shell. Real-world: yanks cable. In-Grid: returns to spatial sector view.

23. **Implement `<cmd> --help` rendering** per spec §6. Synopsis + description + per-player scaled cost (computed live from `base_*` × INT/skill modifiers via a single `scaled_cost(...)` helper). Single source of truth: same helper is used by channel runtime.

### Locked-tag commands

24. **Implement `cmd_hashcat.cpp`** — escalation long-channel. Skill check based on `INT_mod` + `Cat_Hacking_rank` + `RootKit_rank`. Partial-state (`allow_partial = true`): on abort, increment `cracked_digits`. Reveals digits one-at-a-time per spec §4.

25. **Implement `cmd_unlock.cpp`** — root-only single-tick action. For doors: cycles the lock. For other Locked devices: sets `escalated = true` (effectively "I have admin rights now").

### DataStore commands

26. **Implement `cmd_ls.cpp`** — instant. Lists files at the given path. Permission-gated via `DeviceFsView`.

27. **Implement `cmd_cat.cpp`** — instant. Prints file content. Permission-gated.

28. **Implement `cmd_grep.cpp`** — instant. Substring search across the device's filesystem.

29. **Implement `cmd_find.cpp`** — instant. Path-pattern search.

30. **Implement `cmd_dump.cpp`** — privileged long-channel. Leeches content to inventory as a `data fragment` item. Partial-state (`allow_partial = true`): on abort, accumulate `dumped_bytes`.

31. **Implement `cmd_wipe.cpp`** — privileged long-channel atomic. On success: encodes the wiped path into a per-device `wiped_paths` bitmask, persisted via Plan 5 tile-mutation persistence.

### HasOptics commands

32. **Implement `cmd_blind.cpp`** — privileged long-channel. Disables the camera's vision cone for N turns.

33. **Implement `cmd_feed.cpp`** — root long-channel. Opens a snapshot of the camera's current vision cone in the shell scroll.

34. **Implement `cmd_restream.cpp`** — root long-channel. Loops a recorded frame; the camera reports nothing until ended.

35. **Implement `cmd_purge.cpp`** — root long-channel. Wipes the camera's recording buffer.

### Weaponized commands

36. **Implement `cmd_disarm.cpp`** — privileged. Turret disarmed for N turns.

37. **Implement `cmd_lockout.cpp`** — privileged. Turret refuses inputs from anyone but the player.

38. **Implement `cmd_friendly_fire.cpp`** — privileged long-channel. Reconfigures target priority by faction tag (e.g., `--target=cartel`).

39. **Implement `cmd_targetlist.cpp`** — instant read. Prints current target priority list.

### PowerNode commands

40. **Implement `cmd_surge.cpp`** — privileged long-channel. Briefly powers up adjacent devices.

41. **Implement `cmd_kill.cpp`** — privileged. Cuts power to downstream devices for N turns.

42. **Implement `cmd_reroute.cpp`** — privileged long-channel. Switches downstream device set.

43. **Implement `cmd_dim.cpp`** — privileged. Reduces vision-cone radius of downstream `HasOptics` devices.

### Mobile commands

44. **Implement `cmd_halt.cpp`** — privileged. Mobile fixture stops moving for N turns.

45. **Implement `cmd_redirect.cpp`** — privileged long-channel. Sets a new patrol target.

46. **Implement `cmd_gps.cpp`** — instant read. Prints current location.

### SSH command + cyberdeck shell

47. **Add `ssh` command to `pda>` cyberdeck shell** in `pda_hacking_tab`. Parse `ssh [<user>@]<ip>`. Default user = `root`. Validate target IP is reachable per the adjacency rule (§8): wired-into-this-device OR avatar-adjacent-to-this-device-gateway. If unreachable: `ssh: <ip>: host unreachable (out of range)`.

48. **Implement manual-ssh strict semantics** per spec §4. `root@<locked-unescalated>` → reject with `permission denied (root login disabled). try: ssh guest@<ip>` and DO NOT open shell. Other paths: open shell at the requested tier (capped by device state for guest-on-locked-no-auth-needed).

49. **Implement autorun-ssh smart semantics** per spec §4. Cyberdeck inspects target's `locked` and `escalated`; autotypes `ssh guest@<ip>` if locked-unescalated, `ssh root@<ip>` otherwise. Always lands a shell.

50. **Wire AlienTech opt-out** per spec §16. `ssh root@<alien-ip>` returns `ssh: <ip>: protocol not understood (alien tech).` `nmap -l` marks them `OS: ??? (unknown)`. They appear in listings but cannot be sshed.

### Interactables

51. **Add `(hack) Shell Access` interactable** on any `Hackable` with `Electronic` tag (and not `AlienTech`) when player has `Cat_Hacking`. Triggering wires body in (`player.is_jacked_into = hackable_id`, freeze movement/attack/item-use), opens PDA Hacking tab, autoruns smart-ssh per task 49.

52. **Add `(hack) Jack In` interactable** on `Hackable` with `JackInPort` tag (existing). Avatar lands in the LAN spatial sector. Reuses Plan 5 spatial entry.

53. **Render real-world wired-in body state**: `@` glyph with cyan pulse / `[⟳]` superscript per spec §3a. Movement/attack/item-use disabled while wired. Esc at prompt yanks cable.

54. **Render in-Grid Tron-window shell takeover** per spec §3b. While shell is open, swap Tron window content from the spatial-sector renderer to the device-shell renderer. HUD chrome (Trace/Heat panes, log pane) stays visible. On `exit`: swap back; avatar at same tile.

### Cyberdeck mod gate

55. **Create `CyberdeckMods` system** in `cyberdeck_mods.h/cpp`. Single field for v1: `wireless_jackin_installed: bool`. Computed from inventory presence of any `WirelessJackInModule` item. (Plan 11+ replaces inventory-check with proper install slot.)

56. **Gate `jack <ip>` from cyberdeck shell** on `CyberdeckMods.wireless_jackin_installed`. When false: print the spec §15 error block:
    ```
    jack: no wireless jack-in device installed.
           (requires Wireless Jack-In Module.)
    ```
    Strip the existing Plan 5 lock-error path (`jack: locked — try breach.exe`).

### Items: Aerojack & Untether

57. **Define `WirelessJackInModule` item category** in the item-data layer. Tier 1, install-slot=`cyberdeck`, range/trace-cost stubs (placeholder values for v1; differentiated when mod-system lands in Plan 11).

58. **Define `Aerojack` item** as a brand variant of `WirelessJackInModule`. Name: "Aerojack". Tier 1.

59. **Define `Untether` item** as a brand variant of `WirelessJackInModule`. Name: "Untether (Mod)". Tier 1.

60. **Register both items** in the item database so they can be spawned (dev console + future loot tables).

61. **Update `docs/items.md`** with both items: stats stub, brand attribution, "v1 placeholder" note. Keep in sync with code-side item defs.

62. **Verify mod-gate end-to-end:** dev-spawn an Aerojack into inventory, confirm `pda> jack 10.0.4.17` works; remove it, confirm the error fires; do the same for Untether.

### Skills

63. **Add `ColdHands` skill node** under `Cat_Hacking`. Effect: reduce per-command Detection (real-world only) per rank. Wire into `scaled_cost(...)`.

64. **Add `RootKit` skill node** under `Cat_Hacking`. Effect: reduce `hashcat` `base_turns` per rank. Wire into `scaled_cost(...)`.

### Plan 5 amendments

65. **Remove `nmap -m` `b` key handler** per spec §17 amendment A3 / Plan 5 §16. Per-device netmap-side breach is gone. The `b` key is unbound on per-device gateway edges. Region-firewall edges keep `breach.exe` accessible only spatially (walked-up-to).

66. **Update `nmap -l` and `ping <ip>` formatting** to make tier/lock state visually obvious per spec §17 amendment A4. `tier:N (locked|cracked|unlocked)`. AlienTech rows: `OS: ??? (unknown)`.

### Documentation

67. **Update `docs/mechanics.md`** with the new shell mechanics: long-channel scaling formula (`scaled_turns`, `scaled_heat`, `scaled_detection`), `hashcat` partial-state behavior, ssh adjacency rule, cyberdeck mod gate behavior. Cross-reference the spec.

68. **Update `docs/roadmap.md`** to reflect Plan 7 status (in-progress → done when merged) and check off the relevant boxes.

### Verification

69. **Manual playtest — real-world doorway:** spawn a Cartel turret in dev mode, walk up, select `(hack) Shell Access`, watch ritual, hashcat, friendly_fire, exit. Expected: body frozen during shell, channel ticks while world ticks, exit unwires.

70. **Manual playtest — in-Grid doorway:** find a JackInPort fixture, select `(hack) Jack In`, walk avatar to a device's gateway tile, ssh adjacent device, do crack/wipe loop. Verify ssh on a non-adjacent device fails with `out of range`.

71. **Manual playtest — manual ssh strict reject:** `pda> ssh root@<locked-ip>` → see reject + try-guest hint. `pda> ssh guest@<ip>` → land at guest. `hashcat --fast` → escalate to root.

72. **Manual playtest — autorun ssh smart:** Shell Access on a locked turret → autoruns `ssh guest@`. Shell Access on the same turret after escalation → autoruns `ssh root@`.

73. **Manual playtest — jack mod-gate:** without Aerojack/Untether in inventory, `pda> jack <ip>` → see the §15 error. With Aerojack: works. With Untether instead: works.

74. **Manual playtest — AlienTech opt-out:** spawn a Precursor console (AlienTech tag), confirm interactables widget hides Shell Access, confirm `pda> ssh root@<alien-ip>` returns `protocol not understood`.

75. **Manual playtest — three faction packs:** open shells on Corp, Cartel, and Civilian devices in turn. Verify banner chrome, MOTD voice, log line cadence, and `/home/<user>/notes.txt` content all read distinct. Confirm `Faction::None` device falls back to Civilian.

76. **Manual playtest — Tron HUD render:** in-Grid, ssh into a device. Verify Tron window content swaps to shell view. Verify Trace/Heat HUD pane stays visible during a long-channel. On `exit`, verify avatar returns to its tile in the spatial sector.

## Out of scope (deferred)

- Alien-tech hacking dialect (Precursor, Conclave/Stellari) — Plan 11.
- Cyberdeck-mod install UI / tinkerer NPCs / mod balance — Plan 11+.
- `PivotMaster` skill — dropped (no pivot in v1).
- New programs / quickhacks — none in this plan.
- Real-body damage while wired-in beyond existing combat damage rules — design only.

## References

- Spec: `docs/superpowers/specs/2026-05-01-device-shells-design.md`
- Plan 5 spec: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- Plan 6 spec: `docs/superpowers/specs/2026-05-02-grid-hud-design.md`
- Roadmap: `docs/plans/2026-05-03-plan-7-roadmap.md`
- Project conventions: `CLAUDE.md`
