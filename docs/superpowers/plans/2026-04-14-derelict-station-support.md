# Derelict Station Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development.

**Goal:** Let `CustomSystemSpec` create systems with abandoned/derelict stations, so Nova Stage 2 Echo 2 ("The Quiet Shell") can exist.

**Architecture:** `StationType::Abandoned`, `MapType::DerelictStation`, and the derelict-station map generator already exist. The only gap is that `CustomSystemSpec` doesn't expose a `StationInfo` field, so custom systems can't set the station type to Abandoned. Two tiny changes: add the field, wire it through `add_custom_system`, and expose it via a new dev-console `derelict` kind.

**No save schema change.** `StationInfo` already round-trips.

**Worktree:** `.worktrees/derelict-station`, branch `feat/derelict-station`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/star_chart.h` | MODIFY | `CustomSystemSpec.station` field |
| `src/star_chart.cpp` | MODIFY | Copy the station info in `add_custom_system` |
| `src/dev_console.cpp` | MODIFY | `derelict` kind in `chart create` |

---

### Task 1: Expose `StationInfo` on `CustomSystemSpec`

**Files:**
- Modify: `include/astra/star_chart.h` — `CustomSystemSpec`
- Modify: `src/star_chart.cpp` — `add_custom_system`

- [ ] **Step 1: Add the field**

In `include/astra/star_chart.h`, locate `struct CustomSystemSpec` (around lines 67-77). After `bool has_station = false;`, add:

```cpp
    StationInfo station;   // honoured when has_station=true; default NormalHub/Generic
```

(`StationInfo` is already declared earlier in the same header, around line 28.)

- [ ] **Step 2: Copy it through in `add_custom_system`**

In `src/star_chart.cpp::add_custom_system` (around line 720), find the line that sets `sys.has_station = spec.has_station;`. Immediately after, add:

```cpp
    sys.station = spec.station;
```

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/star_chart.h src/star_chart.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): expose StationInfo on CustomSystemSpec

Custom systems can now declare an Abandoned or Scav/Pirate station
by populating spec.station. When spec.has_station is true,
add_custom_system copies the station info verbatim. Previously
the station defaulted to NormalHub regardless of caller intent,
which made quest-created derelict stations impossible.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Dev-console `derelict` kind

**Files:**
- Modify: `src/dev_console.cpp`

- [ ] **Step 1: Extend the kind whitelist**

Find the `chart create` validator that currently reads:

```cpp
                if (a2 != "asteroid" && a2 != "scar" &&
                    a2 != "rock" && a2 != "neutron") {
                    log("chart create: unknown kind '" + a2 +
                        "' (expected asteroid|scar|rock|neutron)");
                    return;
                }
```

Replace with:

```cpp
                if (a2 != "asteroid" && a2 != "scar" &&
                    a2 != "rock" && a2 != "neutron" && a2 != "derelict") {
                    log("chart create: unknown kind '" + a2 +
                        "' (expected asteroid|scar|rock|neutron|derelict)");
                    return;
                }
```

- [ ] **Step 2: Add the kind branch**

Before the final `else { // "rock"` branch in the kind-dispatch chain, insert:

```cpp
            } else if (kind == "derelict") {
                spec.has_station = true;
                spec.station.type = StationType::Abandoned;
                spec.station.specialty = StationSpecialty::Generic;
                spec.station.name = name + " Outpost";
                spec.star_class = StarClass::ClassG;
                spec.bodies = {};   // no planets; station is the only docking target
            }
```

The `spec.star_class = StarClass::ClassM;` line earlier in the handler stays; the derelict branch overrides it to G (more neutral for an abandoned shipping-lane station).

- [ ] **Step 3: Update help line**

Replace:

```cpp
log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock|neutron)");
```

with:

```cpp
log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock|neutron|derelict)");
```

- [ ] **Step 4: Add include if needed**

At the top of `src/dev_console.cpp`, confirm `#include "astra/station_type.h"` is present (needed for `StationType::Abandoned` / `StationSpecialty::Generic`). If missing, add it.

- [ ] **Step 5: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 6: Smoke test**

```
./build/astra-dev --term
```

1. New game → backtick → `chart create derelict QuietShell`.
2. Expect log: `Created custom derelict system 'QuietShell' id=...`.
3. Open chart (`m`). Find `QuietShell` near Sol as a G-class system (`*` in yellow).
4. Dock to the station. Detail map should render as a **derelict-station interior** (corridors, broken equipment, no live NPCs) — not a pirate station or normal hub.
5. Save → reload → confirm QuietShell still renders correctly.

- [ ] **Step 7: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): chart create derelict <name> — dev-console derelict-station spawn

Adds 'derelict' to the kind whitelist. Creates a G-class system
with an Abandoned station and no planets, so the quest can place
a drone fixture on the derelict interior via QuestLocationMeta.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Acceptance Criteria

- `cmake --build build --target astra-dev` clean at every commit.
- `chart create derelict Foo` creates an Abandoned station in a new system.
- Docking with that station opens the existing derelict-station generator's detail map.
- Save/reload preserves the station type.
- Existing `chart create` kinds continue to work unchanged.

---

## Out of Scope

- Quest wiring / Nova Echo 2 fixture placement (next plan).
- Custom station generator variants beyond Abandoned.
- System-name templating for derelict stations (just uses `<name> Outpost`).
