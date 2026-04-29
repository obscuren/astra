# Hacking Plan 3 — The Grid (A-layer) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the full A-layer of the Hacking & The Grid feature: jacking into Precursor consoles uploads consciousness into a Tron-styled tile sector, populated by ICE actors and connected via a per-galaxy network graph; Heat, Trace, RAM, voluntary/forced disconnects, and the 5 inert `.exe` programs all become live; one hand-authored deep-Grid Consciousness Anchor sector ships.

**Architecture:** Add a new `GameState::Grid` driven by a `HackingSystem`-owned `GridSession`. The session holds avatar state, the active `GridSector`, and active ICE actors. A per-galaxy `GridNetwork` graph is stored on `WorldManager` and seeded from Precursor consoles in the world. Sectors generate procedurally per `(network_id, device_id)` tuple for subnets, hand-tuned per-station for regional darknets, and from a single hand-authored layout for the deep-Grid anchor. Render reuses the existing tilemap renderer with a `GridTheme` palette/glyph-set swap. ICE actors share `Npc`-like shape but live on the session, not the world. Programs live in `program_effects.cpp` and dispatch via Grid context.

**Tech Stack:** C++20, CMake, terminal renderer. No new dependencies. Save schema bumps `SAVE_FILE_VERSION` 53→54.

**Spec reference:** `docs/superpowers/specs/2026-04-29-hacking-design.md` §2 (architecture), §3 (Grid tiers/ICE/Trace), §4 (PDA terminal), §5 (persistence), §6 (v1 cut).

**Locked decisions (do not re-debate):**
- Body phased-out + invulnerable while jacked in (Q4a-B). Faint `@` ghost glyph rendered at jack-in console.
- Black-ICE death = real HP damage + possible IRL death (Q4b-B). Non-black-ICE death = avatar wipe + body wakes with debuff + unsaved loot lost.
- Cyberdeck required for any quickhack; `Cat_Hacking` skill required to jack in.
- Programs are loadable items, not learned abilities.
- Save: bump `SAVE_FILE_VERSION`, no backcompat. Mid-Grid sessions are NOT resumable across save/load (soft-disconnect on load).
- Ship ONE hand-authored deep-Grid anchor sector (the Consciousness Anchor). Plan 4 ships more.

**Project conventions to follow:**
- Build with dev mode: `cmake -B build -DDEV=ON && cmake --build build`. (Per `feedback_dev_mode.md`.)
- Headers: `#pragma once`, in `include/astra/`. Namespace `astra`. Member vars `snake_case_`. Classes `PascalCase`.
- Don't auto-push.
- Containerize code: `Game` is a coordinator. New runtime systems get their own files. File-size target ≤ ~600 lines.
- All mechanics/formulas → `docs/mechanics.md`. All item stats → `docs/items.md`. Update `docs/roadmap.md` for shipped features.
- Comments earn their place. Only WHY where non-obvious.

**Out of scope (Plan 4 or future):**
- `consciousness.dat` cross-Sgr-A* save file and rebirth wiring.
- Non-hacker access (Cybernetic Neural Backup implant, Precursor "soul mirror").
- Multiple deep-Grid anchor sectors.
- `Cat_Hacking` capstones (`CodeCraft`, `ConsciousnessAnchor` runtime).
- All hackable kinds beyond v1 (Drone, Light, Vendor, Elevator, etc.).
- Vulnerable-body model.
- T3+ cyberdecks, advanced programs (`pulse_hammer.exe`, `daemon_hijack.exe`, etc.).

---

## File map

**Files created:**

| File | Responsibility |
|---|---|
| `include/astra/grid_session.h` / `src/grid_session.cpp` | Live runtime: avatar pos/HP, RAM, Heat, Trace, current sector pointer, current node id, status flags. Owned by `HackingSystem` while jacked in. |
| `include/astra/grid_network.h` / `src/grid_network.cpp` | Per-galaxy graph of nodes (subnet / regional darknet / deep-Grid anchor). Edges encode gateway lock state. Stored on `WorldManager`. |
| `include/astra/grid_sector.h` / `src/grid_sector.cpp` | One Grid sector: tile grid (`std::vector<GridTile>`), ICE actor list, data-node positions, exit positions, gateway target node ids. Procedural seed-based generation entry points. |
| `include/astra/grid_ice.h` / `src/grid_ice.cpp` | `GridIce` actor + AI: white (patrol+vision), gray (engage avatar HP), black (engage avatar HP + bleed-through real HP). One-of-each in v1. |
| `include/astra/grid_renderer.h` / `src/grid_renderer.cpp` | Tron-palette tilemap render of the current sector + HUD bars (RAM, Heat, Trace, avatar HP). Reuses existing `Renderer` interface. |
| `include/astra/grid_input.h` / `src/grid_input.cpp` | Grid-mode input: movement, fire program, jack out (`Q` voluntary, `Shift+Q` hard), help. Owned by `InputManager` dispatch. |
| `include/astra/grid_theme.h` | Color and glyph constants for Grid mode. Pure data header. |
| `src/grid_anchor_layout.cpp` | Hand-authored Consciousness Anchor sector layout (data only). |

**Files modified:**

| File | Change |
|---|---|
| `include/astra/effect.h` | Add Grid-side EffectIds: `OpticsRebooted` already exists as `Hijacked` model — add `IceBreakerCharge`, `GhostCloak`, `GridInvulnerable` (body phase-out), `BlackIceShock` (debuff after non-black death). |
| `include/astra/game.h` | Add `GameState::Grid`. |
| `src/game.cpp` | Drive Grid loop (`update`, render, input dispatch) when `state_ == GameState::Grid`. |
| `src/game_input.cpp` | Replace "Grid not yet implemented" stub at line 259 with real `HackingSystem::jack_in()` call. |
| `src/game_rendering.cpp` | Add render branch for `GameState::Grid` → `grid_renderer::render(...)`. Render faint ghost `@` at saved jack-in console position when Grid active. |
| `include/astra/hacking_system.h` / `src/hacking_system.cpp` | Add `jack_in(network_id, node_id)`, `jack_out(JackOutKind)`, `session()`, `tick_grid(Game&)`. Owns `std::optional<GridSession>`. |
| `include/astra/world_manager.h` / `src/world_manager.cpp` | Add `GridNetwork& grid_network()` accessor + storage. Populate from world Precursor consoles on galaxy generation. |
| `include/astra/cyberdeck.h` / `src/cyberdeck.cpp` | Heat add/decay helpers; forced-reboot detection (heat > heat_cap). Existing fields are already declared. |
| `include/astra/program.h` / `src/program.cpp` | (No new program ids — the 5 `.exe` ids already exist.) |
| `include/astra/program_effects.h` / `src/program_effects.cpp` | Add `apply_grid_*` dispatch for the 5 `.exe` programs: `icebreaker_lite`, `ghost_trace`, `cooldown`, `breach`, `decrypt`. Real-world dispatch unchanged. |
| `src/pda_hacking_tab.cpp` | `hack_term_cmd_jack` becomes real (resolves netmap node, calls `jack_in`). `netmap` lists actual known nodes. Update `tab_help_body(PdaTab::Hacking)` accordingly. |
| `include/astra/save_file.h` / `src/save_file.cpp` | Bump `SAVE_FILE_VERSION` 53→54. Persist `GridNetwork` graph state (gateway crack flags, regional darknet seed). On load, soft-disconnect any active session. |
| `include/astra/dev_console.h` / `src/dev_console.cpp` | Add `jack <network_id>`, `trace <n>`, `spawn-ice <kind>`. |
| `include/astra/skill_defs.h` (data only) | No new skills (all enum values already exist). Consume them in Plan 3. |
| `src/player.cpp` (or wherever skill effects are queried) | Wire skill runtime effects: Intrusion / IceBreaking / DaemonMastery / GhostProtocol / DeepGridNavigator / NeuralFortitude. |
| `docs/mechanics.md` | Add "Grid" section: Trace formula, Heat/Trace coupling, ICE behavior, disconnect outcomes, skill effects. |
| `docs/items.md` | Update 5 `.exe` programs' descriptions with their now-real Grid effects. |
| `docs/roadmap.md` | Mark Plan 3 shipped under Hacking. |

---

## Type contracts (used across multiple tasks — define once, reuse exactly)

These are the interfaces every later task must match. Tasks 1–4 establish them; tasks 5+ consume them.

```cpp
// grid_network.h
namespace astra {

enum class GridNodeKind : uint8_t {
    Subnet,           // 1 device, 1 small sector
    RegionalDarknet,  // station/asteroid scope, 3-4 sectors
    DeepGridAnchor,   // hand-authored, persists across rebirth (Plan 4)
};

struct GridNodeId {
    uint32_t value = 0;
    bool valid() const { return value != 0; }
    bool operator==(const GridNodeId&) const = default;
};

struct GridEdge {
    GridNodeId from;
    GridNodeId to;
    int        gateway_tier   = 0;     // 0 = open, 1+ = needs breach
    bool       cracked        = false; // set true once breached
};

struct GridNode {
    GridNodeId        id;
    GridNodeKind      kind          = GridNodeKind::Subnet;
    uint32_t          source_seed   = 0;        // (network_id<<16)|device_id for subnets
    int               security_tier = 1;        // 1..3 — drives ICE composition
    std::string       label;                    // "Hangar.Turret-7", "Station.Spine"
    // For RegionalDarknet+: pre-generated sector list (one per "room")
    std::vector<uint32_t> sector_seeds;
};

class GridNetwork {
public:
    GridNodeId add_node(GridNode node);
    void       add_edge(GridEdge edge);
    const GridNode* find(GridNodeId id) const;
    GridNode*       find_mut(GridNodeId id);
    std::vector<GridNodeId> neighbors(GridNodeId id) const;
    const std::vector<GridNode>& nodes() const { return nodes_; }
    const std::vector<GridEdge>& edges() const { return edges_; }
    void clear();
private:
    std::vector<GridNode> nodes_;
    std::vector<GridEdge> edges_;
    uint32_t              next_id_ = 1;
};

} // namespace astra
```

```cpp
// grid_sector.h
namespace astra {

enum class GridTile : uint8_t {
    Floor,            // ░
    Firewall,         // ▓ (impassable, breachable)
    DataNode,         // $
    Gateway,          // ⌬
    ExitNode,         // ⊙
    EncryptedFile,    // ⊘
    Wall,             // outside-of-sector
};

struct GridSector {
    int                  w = 0;
    int                  h = 0;
    std::vector<GridTile> tiles;       // size w*h, row-major
    int                  spawn_x = 0;
    int                  spawn_y = 0;
    GridNodeId           source_node;  // which network node this sector belongs to
    // Resolved targets for special tiles. Each entry maps an (x,y) gateway tile
    // to the destination node id it leads to (cracked or not).
    struct GatewayLink { int x, y; GridNodeId dst; };
    std::vector<GatewayLink> gateways;

    GridTile at(int x, int y) const;
    void     set(int x, int y, GridTile t);
    bool     in_bounds(int x, int y) const;
    bool     passable(int x, int y) const; // floor + walked-through gateway/exit tiles
};

// Procedural generators. Same seed → same layout (stable revisits).
GridSector gen_subnet_sector(uint32_t seed, int security_tier);
GridSector gen_regional_sector(uint32_t seed, int security_tier);
// Hand-authored — see grid_anchor_layout.cpp.
GridSector make_consciousness_anchor_sector();

} // namespace astra
```

```cpp
// grid_ice.h
namespace astra {

enum class IceColor : uint8_t { White, Gray, Black };

struct GridIce {
    int       x = 0;
    int       y = 0;
    int       hp = 1;
    IceColor  color = IceColor::White;
    int       patrol_dir = 0;          // 0..3 (white only)
    bool      sees_avatar = false;     // white only — refreshed each turn
    int       attack_cooldown = 0;     // gray/black
};

struct GridSession; // fwd

namespace grid_ice {

void spawn_for_sector(GridSession& s, uint32_t seed, int security_tier);
void tick_all(GridSession& s, class Game& game);

// Damage hooks. Called from program effects.
void damage(GridSession& s, GridIce& ice, int dmg);
bool kill_if_dead(GridSession& s, GridIce& ice);

} // namespace grid_ice

} // namespace astra
```

```cpp
// grid_session.h
namespace astra {

enum class JackOutKind : uint8_t {
    Voluntary,        // walked to ⊙ — full loot, no penalty
    HardJackOut,      // hotkey — Trace +10, drop 50% loot
    NonBlackDeath,    // avatar HP=0 by gray/white ICE — body debuff, unsaved loot lost
    BlackIceDeath,    // avatar HP=0 by black ICE — real HP damage (lethal possible)
    SoftDisconnect,   // load-time recovery — Trace cleared, no penalty
};

struct GridLootBuffer {
    int credits             = 0;
    int code_fragments_t1   = 0;
    int code_fragments_t2   = 0;
    std::vector<uint16_t> programs_acquired;   // ProgramId values (cast)
    std::vector<std::string> lore_unlocked;
    bool empty() const;
};

struct GridSession {
    // Identity
    GridNodeId entry_node;
    GridNodeId current_node;

    // Body
    int body_x = 0;             // saved overworld/dungeon position
    int body_y = 0;
    GameState body_state = GameState::Playing;

    // Avatar
    int avatar_x = 0;
    int avatar_y = 0;
    int avatar_hp_max = 3;
    int avatar_hp = 3;

    // Resources
    int ram_max = 4;
    int ram = 4;
    int trace = 0;              // [0, 100]
    int trace_alert_pulses = 0; // bookkeeping for breakpoint side effects

    // Tier-derived turn ticks
    int trace_tick_per_turn = 1;

    // Skill flags (cached at jack-in)
    bool skill_intrusion          = false;
    bool skill_icebreaking        = false;
    bool skill_daemon_mastery     = false;
    bool skill_ghost_protocol     = false;
    bool skill_deepgrid_navigator = false;
    bool skill_neural_fortitude   = false;
    bool ghost_protocol_used      = false;  // set true after first program of session

    // Sector
    GridSector sector;
    std::vector<GridIce> ice;

    // Loot accumulated this session (committed on voluntary disconnect).
    GridLootBuffer loot;
};

} // namespace astra
```

```cpp
// hacking_system.h additions
class HackingSystem {
public:
    // …existing public API…

    // ── Grid lifecycle ──
    bool jacked_in() const { return session_.has_value(); }
    GridSession* session() { return session_ ? &*session_ : nullptr; }
    const GridSession* session() const { return session_ ? &*session_ : nullptr; }

    // Returns true if jack-in succeeded (preconditions met). Logs reason on failure.
    bool jack_in(Game& game, GridNodeId entry_node);

    // Drains/persists loot per kind, restores body, returns to previous game state.
    void jack_out(Game& game, JackOutKind kind);

    // Per-turn Grid update. Called from Game::advance_world when state == Grid.
    void tick_grid(Game& game);

private:
    std::optional<GridSession> session_;
    // …existing private fields…
};
```

```cpp
// program_effects.h additions
// apply_<id>_grid functions are added for the 5 .exe ids.
// Signature mirrors the QH ones but takes a GridSession + optional ice target.
struct GridProgramContext {
    Game&        game;
    GridSession& session;
    int          target_x;     // -1 if N/A
    int          target_y;
};
std::string apply_program_in_grid(ProgramId id, GridProgramContext ctx);
```

---

## Task 1 — Skeleton headers + EffectIds + GameState (compile-clean stubs)

**Files:**
- Create: `include/astra/grid_network.h`, `include/astra/grid_sector.h`, `include/astra/grid_ice.h`, `include/astra/grid_session.h`, `include/astra/grid_theme.h`
- Modify: `include/astra/effect.h`, `include/astra/game.h`, `include/astra/hacking_system.h`
- Create empty: `src/grid_network.cpp`, `src/grid_sector.cpp`, `src/grid_ice.cpp`, `src/grid_session.cpp`
- Modify: `CMakeLists.txt`

Establish the type surface from the Type Contracts section. No behavior yet — stub bodies return defaults. Goal: `cmake --build build` is clean after this task.

- [ ] **Step 1: Add Grid-related EffectIds**

In `include/astra/effect.h`, append to the Debuffs (400+) block:

```cpp
    Hijacked            = 401,    // existing
    GridInvulnerable    = 402,    // body phased-out while jacked in
    BlackIceShock       = 403,    // debuff after non-black avatar death
    IceBreakerCharge    = 404,    // pending damage applied next turn
    GhostCloak          = 405,    // invisible to white ICE for N turns
```

- [ ] **Step 2: Add `GameState::Grid`**

In `include/astra/game.h`, modify the `GameState` enum:

```cpp
enum class GameState {
    MainMenu,
    Playing,
    GameOver,
    LoadMenu,
    HallOfFame,
    Grid,           // jacked in — HackingSystem owns the loop
};
```

- [ ] **Step 3: Create `include/astra/grid_network.h`**

Paste the `GridNetwork` contract from the Type Contracts section verbatim. File should compile standalone with `<cstdint>`, `<string>`, `<vector>` includes.

- [ ] **Step 4: Create `include/astra/grid_sector.h`**

Paste the `GridSector` contract verbatim. Add includes: `<cstdint>`, `<vector>`, `"astra/grid_network.h"`.

- [ ] **Step 5: Create `include/astra/grid_ice.h`**

Paste `GridIce` + free-function declarations verbatim. Add `<cstdint>`, `<vector>`. Forward-declare `GridSession` and `Game`.

- [ ] **Step 6: Create `include/astra/grid_session.h`**

Paste `GridSession` + `JackOutKind` + `GridLootBuffer` verbatim. Includes: `<vector>`, `<string>`, `<cstdint>`, `"astra/grid_network.h"`, `"astra/grid_sector.h"`, `"astra/grid_ice.h"`, `"astra/game.h"` (for `GameState`).

- [ ] **Step 7: Create `include/astra/grid_theme.h`**

```cpp
#pragma once
#include "astra/color.h"

namespace astra::grid_theme {

constexpr Color floor       = Color::DarkBlue;
constexpr Color firewall    = Color::Magenta;
constexpr Color avatar      = Color::Cyan;
constexpr Color white_ice   = Color::White;
constexpr Color gray_ice    = Color::Gray;
constexpr Color black_ice   = Color::DarkRed;
constexpr Color data_node   = Color::Yellow;
constexpr Color gateway     = Color::BrightMagenta;
constexpr Color exit_node   = Color::BrightCyan;
constexpr Color encrypted   = Color::BrightBlue;

constexpr char floor_glyph     = '.';   // bg-tile via renderer; UTF-8 ░ supplied as ascii fallback
constexpr char firewall_glyph  = '#';
constexpr char avatar_glyph    = '@';
constexpr char white_ice_glyph = 'w';
constexpr char gray_ice_glyph  = 'G';
constexpr char black_ice_glyph = 'B';
constexpr char data_node_glyph = '$';
constexpr char gateway_glyph   = '*';
constexpr char exit_glyph      = 'O';
constexpr char encrypted_glyph = '?';

} // namespace astra::grid_theme
```

> Why ASCII glyphs and not the spec's UTF-8 characters: terminal renderer Cell uses `char` (per memory `project_cell_refactor.md`). Use ASCII now; revisit when multi-byte support lands.

- [ ] **Step 8: Add HackingSystem Grid surface**

In `include/astra/hacking_system.h`, add the public Grid lifecycle methods + `std::optional<GridSession>` member declared in the contract block above. Forward-declare or include `grid_session.h`. Don't implement bodies yet — only declarations.

- [ ] **Step 9: Create empty source stubs**

Each new `.cpp`:

```cpp
#include "astra/<corresponding_header>.h"
namespace astra {
// Implementations land in later tasks.
} // namespace astra
```

- [ ] **Step 10: Add stub `jack_in`/`jack_out`/`tick_grid` to `src/hacking_system.cpp`**

```cpp
bool HackingSystem::jack_in(Game& /*game*/, GridNodeId /*entry_node*/) {
    return false;   // wired in Task 5
}
void HackingSystem::jack_out(Game& /*game*/, JackOutKind /*kind*/) {
    // wired in Task 5
}
void HackingSystem::tick_grid(Game& /*game*/) {
    // wired in Task 6
}
```

- [ ] **Step 11: Update CMakeLists.txt**

Add to the `astra` target source list:

```cmake
    src/grid_network.cpp
    src/grid_sector.cpp
    src/grid_ice.cpp
    src/grid_session.cpp
```

(Leave `src/grid_renderer.cpp`, `src/grid_input.cpp`, `src/grid_anchor_layout.cpp` for later tasks; add when those files are created.)

- [ ] **Step 12: Build**

```bash
cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -30
```

Expected: clean. If `Color::DarkBlue` etc. don't exist, replace with closest existing palette entries (`grep -n "enum class Color" include/astra/color.h`).

- [ ] **Step 13: Commit**

```bash
git add include/astra/grid_network.h include/astra/grid_sector.h \
        include/astra/grid_ice.h include/astra/grid_session.h \
        include/astra/grid_theme.h include/astra/effect.h \
        include/astra/game.h include/astra/hacking_system.h \
        src/grid_network.cpp src/grid_sector.cpp src/grid_ice.cpp \
        src/grid_session.cpp src/hacking_system.cpp \
        CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(grid): scaffold Grid mode types + GameState::Grid

Adds GridNetwork, GridSector, GridIce, GridSession, GridTheme headers
plus empty cpp stubs. GameState::Grid + HackingSystem::jack_in/out/tick_grid
declared but unimplemented. Compile-clean foundation for Plan 3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2 — `GridNetwork` graph + per-galaxy storage

**Files:**
- Modify: `src/grid_network.cpp`
- Modify: `include/astra/world_manager.h`, `src/world_manager.cpp`

Implement the graph itself + plumb storage on `WorldManager`. No population logic yet (Task 3 does that).

- [ ] **Step 1: Implement GridNetwork in `src/grid_network.cpp`**

```cpp
#include "astra/grid_network.h"

namespace astra {

GridNodeId GridNetwork::add_node(GridNode node) {
    node.id.value = next_id_++;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

void GridNetwork::add_edge(GridEdge edge) {
    edges_.push_back(edge);
}

const GridNode* GridNetwork::find(GridNodeId id) const {
    for (const auto& n : nodes_) if (n.id == id) return &n;
    return nullptr;
}

GridNode* GridNetwork::find_mut(GridNodeId id) {
    for (auto& n : nodes_) if (n.id == id) return &n;
    return nullptr;
}

std::vector<GridNodeId> GridNetwork::neighbors(GridNodeId id) const {
    std::vector<GridNodeId> out;
    for (const auto& e : edges_) {
        if (e.from == id) out.push_back(e.to);
        else if (e.to == id) out.push_back(e.from);
    }
    return out;
}

void GridNetwork::clear() {
    nodes_.clear();
    edges_.clear();
    next_id_ = 1;
}

} // namespace astra
```

- [ ] **Step 2: Add storage to WorldManager**

In `include/astra/world_manager.h`, add at top: `#include "astra/grid_network.h"`. In the class body (private section): `GridNetwork grid_network_;`. Public accessor:

```cpp
GridNetwork&       grid_network()       { return grid_network_; }
const GridNetwork& grid_network() const { return grid_network_; }
```

- [ ] **Step 3: Clear network on galaxy generation**

In `src/world_manager.cpp`, find the galaxy-generation entry point (`grep -n "generate.*galaxy\|new_galaxy\|reset_galaxy" src/world_manager.cpp`). At the start of fresh galaxy creation (after seed assignment, before space generation), call `grid_network_.clear();`. This ensures rebirth/load gives a fresh graph.

- [ ] **Step 4: Build**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean.

- [ ] **Step 5: Commit**

```bash
git add include/astra/world_manager.h src/world_manager.cpp src/grid_network.cpp
git commit -m "feat(grid): GridNetwork graph stored on WorldManager

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3 — Populate network from world Precursor consoles

**Files:**
- Modify: `src/world_manager.cpp` (or wherever station/asteroid generation places fixtures)
- Modify: `src/grid_network.cpp` (or new `src/grid_network_populate.cpp` if it grows)

When a Precursor console is created in the world, register its node into `GridNetwork`, set `Hackable.jack_in_node_id` to the new node's id, and link it to a regional darknet node for its station.

- [ ] **Step 1: Find Precursor console placement sites**

```bash
grep -rn "PrecursorConsole\|DeviceKind::PrecursorConsole" src/ include/
```

Identify each call site that creates a `Hackable` of kind `PrecursorConsole`. Common sites: station generators, archive POI generators, and any tutorial fixture. Note the file/line for each.

- [ ] **Step 2: Add helper in `grid_network.h`**

```cpp
// Adds (or reuses) a regional darknet node for the given (galaxy_seed, region_key).
// Returns the regional node's id.
GridNodeId ensure_regional_darknet(GridNetwork& net,
                                   const std::string& region_label,
                                   uint32_t region_seed,
                                   int security_tier);

// Registers a Precursor console as a deep-Grid gateway. Creates a deep-Grid
// anchor node if none exists yet (Plan 3: at most one per galaxy). Wires:
//   regional_darknet ─── (gateway tier T) ─── deep_grid_anchor
// Returns the regional node id (the actual jack-in target).
GridNodeId register_precursor_console(GridNetwork& net,
                                      const std::string& region_label,
                                      uint32_t region_seed,
                                      int security_tier);
```

- [ ] **Step 3: Implement helpers**

In `src/grid_network.cpp`, append the bodies. The "one anchor per galaxy" constraint is satisfied by checking `nodes_` for existing `DeepGridAnchor` kind:

```cpp
GridNodeId ensure_regional_darknet(GridNetwork& net,
                                   const std::string& region_label,
                                   uint32_t region_seed,
                                   int security_tier) {
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::RegionalDarknet && n.label == region_label) {
            return n.id;
        }
    }
    GridNode node;
    node.kind          = GridNodeKind::RegionalDarknet;
    node.source_seed   = region_seed;
    node.security_tier = security_tier;
    node.label         = region_label;
    return net.add_node(std::move(node));
}

GridNodeId register_precursor_console(GridNetwork& net,
                                      const std::string& region_label,
                                      uint32_t region_seed,
                                      int security_tier) {
    GridNodeId regional = ensure_regional_darknet(net, region_label, region_seed,
                                                  security_tier);

    // Anchor — create lazily, exactly one per galaxy.
    GridNodeId anchor;
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) { anchor = n.id; break; }
    }
    if (!anchor.valid()) {
        GridNode a;
        a.kind          = GridNodeKind::DeepGridAnchor;
        a.label         = "Consciousness.Anchor";
        a.security_tier = 3;
        anchor = net.add_node(std::move(a));
    }

    // Gateway tier 2: needs DeepGridNavigator skill OR breach.exe to crack.
    GridEdge e;
    e.from         = regional;
    e.to           = anchor;
    e.gateway_tier = 2;
    e.cracked      = false;
    net.add_edge(e);
    return regional;
}
```

- [ ] **Step 4: Wire console placement**

For each Precursor console site found in Step 1, after the `Hackable` is created, replace placeholder `jack_in_node_id` assignment with:

```cpp
// region_label e.g. "Heavens.Above" / "Asteroid.A12.Archive"
auto& net = world_.grid_network();   // or world_manager_.grid_network()
GridNodeId nid = register_precursor_console(net, region_label,
                                            region_seed, security_tier);
hackable.jack_in_node_id = static_cast<int>(nid.value);
```

If the call site doesn't have direct access to `WorldManager`, thread it via the surrounding generator's existing parameters. For one-shot console placements you can call `register_precursor_console` lazily on first jack-in attempt instead — but **do it at placement time** when possible so that `netmap` lists already-known nodes.

- [ ] **Step 5: Build + smoke test**

```bash
cmake --build build && ./build/astra
```

Walk to a Precursor console (use dev verbs to teleport if needed). The console must remain interactable; "Jack In" still logs the stub from Plan 2 — that's wired in Task 5.

- [ ] **Step 6: Commit**

```bash
git add include/astra/grid_network.h src/grid_network.cpp \
        src/world_manager.cpp <other touched generator files>
git commit -m "feat(grid): register Precursor consoles into GridNetwork

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4 — Sector generation: subnet, regional, anchor

**Files:**
- Modify: `src/grid_sector.cpp`
- Create: `src/grid_anchor_layout.cpp`
- Modify: `CMakeLists.txt`

Implement the three sector generators. Each must be deterministic by seed.

- [ ] **Step 1: Implement core `GridSector` methods**

In `src/grid_sector.cpp`:

```cpp
#include "astra/grid_sector.h"
#include <algorithm>
#include <random>

namespace astra {

GridTile GridSector::at(int x, int y) const {
    if (!in_bounds(x, y)) return GridTile::Wall;
    return tiles[y * w + x];
}

void GridSector::set(int x, int y, GridTile t) {
    if (!in_bounds(x, y)) return;
    tiles[y * w + x] = t;
}

bool GridSector::in_bounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < w && y < h;
}

bool GridSector::passable(int x, int y) const {
    GridTile t = at(x, y);
    return t == GridTile::Floor || t == GridTile::DataNode ||
           t == GridTile::ExitNode || t == GridTile::EncryptedFile ||
           t == GridTile::Gateway;
}

} // namespace astra
```

- [ ] **Step 2: Implement `gen_subnet_sector`**

Single 8x8 room with floor, one data-node, one exit-node, walls all around. Optional one firewall block based on tier.

```cpp
GridSector gen_subnet_sector(uint32_t seed, int security_tier) {
    std::mt19937 rng(seed);
    GridSector s;
    s.w = 8;
    s.h = 8;
    s.tiles.assign(s.w * s.h, GridTile::Wall);
    for (int y = 1; y < s.h - 1; ++y)
        for (int x = 1; x < s.w - 1; ++x)
            s.set(x, y, GridTile::Floor);

    s.spawn_x = 1;
    s.spawn_y = s.h - 2;
    // Exit opposite spawn
    s.set(s.w - 2, 1, GridTile::ExitNode);
    // One data-node mid
    s.set(s.w / 2, s.h / 2, GridTile::DataNode);
    // Optional firewall obstacle for tier >= 2
    if (security_tier >= 2) {
        int fx = 2 + (rng() % (s.w - 4));
        int fy = 2 + (rng() % (s.h - 4));
        s.set(fx, fy, GridTile::Firewall);
    }
    return s;
}
```

- [ ] **Step 3: Implement `gen_regional_sector`**

Larger 16x12 room with 2-3 sub-zones split by firewalls, multiple data-nodes, one gateway tile reserved for cracking the deep-Grid:

```cpp
GridSector gen_regional_sector(uint32_t seed, int security_tier) {
    std::mt19937 rng(seed);
    GridSector s;
    s.w = 16;
    s.h = 12;
    s.tiles.assign(s.w * s.h, GridTile::Wall);
    for (int y = 1; y < s.h - 1; ++y)
        for (int x = 1; x < s.w - 1; ++x)
            s.set(x, y, GridTile::Floor);

    // Vertical firewall divider with one gap.
    int dx = s.w / 2;
    int gap = 2 + (rng() % (s.h - 4));
    for (int y = 1; y < s.h - 1; ++y)
        if (y != gap) s.set(dx, y, GridTile::Firewall);

    s.spawn_x = 1;
    s.spawn_y = s.h - 2;
    s.set(s.w - 2, 1, GridTile::ExitNode);
    s.set(3, 3, GridTile::DataNode);
    s.set(s.w - 3, s.h - 3, GridTile::DataNode);
    s.set(dx + 2, 1, GridTile::Gateway);

    if (security_tier >= 2) {
        s.set(s.w / 4, s.h / 2, GridTile::EncryptedFile);
    }
    return s;
}
```

- [ ] **Step 4: Implement Consciousness Anchor in `src/grid_anchor_layout.cpp`**

Hand-authored 14x10 sector. Safe room (no ICE will spawn here in v1), one data-node displaying a lore archive shelf, one exit-node back to its parent regional darknet.

```cpp
#include "astra/grid_sector.h"

namespace astra {

GridSector make_consciousness_anchor_sector() {
    GridSector s;
    s.w = 14;
    s.h = 10;
    s.tiles.assign(s.w * s.h, GridTile::Wall);
    for (int y = 1; y < s.h - 1; ++y)
        for (int x = 1; x < s.w - 1; ++x)
            s.set(x, y, GridTile::Floor);

    // Centerpiece: ASCII "shrine" of firewalls forming a diamond around a data
    // node — the lore archive interface.
    s.set(6, 3, GridTile::Firewall);
    s.set(8, 3, GridTile::Firewall);
    s.set(5, 4, GridTile::Firewall);
    s.set(9, 4, GridTile::Firewall);
    s.set(5, 5, GridTile::Firewall);
    s.set(9, 5, GridTile::Firewall);
    s.set(6, 6, GridTile::Firewall);
    s.set(8, 6, GridTile::Firewall);
    s.set(7, 4, GridTile::DataNode);   // lore archive

    s.spawn_x = 1;
    s.spawn_y = s.h / 2;
    s.set(s.w - 2, s.h / 2, GridTile::ExitNode);
    return s;
}

} // namespace astra
```

- [ ] **Step 5: Add `src/grid_anchor_layout.cpp` to CMakeLists.txt**

- [ ] **Step 6: Build**

```bash
cmake --build build 2>&1 | tail -10
```

- [ ] **Step 7: Commit**

```bash
git add src/grid_sector.cpp src/grid_anchor_layout.cpp CMakeLists.txt
git commit -m "feat(grid): sector generators (subnet, regional, anchor)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5 — `HackingSystem::jack_in` / `jack_out` lifecycle

**Files:**
- Modify: `src/hacking_system.cpp`, `include/astra/hacking_system.h`

Implement the lifecycle. No render/input yet — those are Tasks 7-8. After this task, dev verbs (Task 17) can already trigger jack-in.

- [ ] **Step 1: Add `<optional>` and includes**

`#include <optional>` and `#include "astra/grid_session.h"` to `src/hacking_system.cpp`.

- [ ] **Step 2: Implement `jack_in`**

```cpp
bool HackingSystem::jack_in(Game& game, GridNodeId entry_node) {
    if (session_) {
        game.log("Already jacked in.");
        return false;
    }
    if (!game.player().skills.has_unlocked(SkillId::Cat_Hacking)) {
        game.log("You lack the Hacking skill category.");
        return false;
    }
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->cyberdeck) {
        game.log("No cyberdeck equipped.");
        return false;
    }
    const auto& cd = *(*deck_slot)->cyberdeck;
    auto& net = game.world().grid_network();
    auto* node = net.find(entry_node);
    if (!node) {
        game.log("Unknown network node.");
        return false;
    }

    GridSession s;
    s.entry_node   = entry_node;
    s.current_node = entry_node;
    s.body_x       = game.player().x;
    s.body_y       = game.player().y;
    s.body_state   = GameState::Playing;

    // Avatar HP from skill+deck — small. NeuralFortitude raises max by 1.
    bool nf = game.player().skills.has_unlocked(SkillId::NeuralFortitude);
    s.avatar_hp_max = 3 + (nf ? 1 : 0);
    s.avatar_hp     = s.avatar_hp_max;

    s.ram_max = cd.stats.ram_max;
    s.ram     = cd.ram_current;   // reuse current charge — refilled on disconnect

    // Tier-driven Trace tick
    switch (node->kind) {
        case GridNodeKind::Subnet:           s.trace_tick_per_turn = 1; break;
        case GridNodeKind::RegionalDarknet:  s.trace_tick_per_turn = 2; break;
        case GridNodeKind::DeepGridAnchor:   s.trace_tick_per_turn = 3; break;
    }

    // Cache skill flags
    s.skill_intrusion          = game.player().skills.has_unlocked(SkillId::Intrusion);
    s.skill_icebreaking        = game.player().skills.has_unlocked(SkillId::IceBreaking);
    s.skill_daemon_mastery     = game.player().skills.has_unlocked(SkillId::DaemonMastery);
    s.skill_ghost_protocol     = game.player().skills.has_unlocked(SkillId::GhostProtocol);
    s.skill_deepgrid_navigator = game.player().skills.has_unlocked(SkillId::DeepGridNavigator);
    s.skill_neural_fortitude   = nf;

    // Resolve sector
    if (node->kind == GridNodeKind::DeepGridAnchor) {
        s.sector = make_consciousness_anchor_sector();
    } else if (node->kind == GridNodeKind::RegionalDarknet) {
        s.sector = gen_regional_sector(node->source_seed, node->security_tier);
    } else {
        s.sector = gen_subnet_sector(node->source_seed, node->security_tier);
    }
    s.sector.source_node = entry_node;
    s.avatar_x = s.sector.spawn_x;
    s.avatar_y = s.sector.spawn_y;

    // Spawn ICE per tier (anchor stays empty in v1)
    if (node->kind != GridNodeKind::DeepGridAnchor) {
        grid_ice::spawn_for_sector(s, node->source_seed, node->security_tier);
    }

    // Body phase-out
    add_effect(game.player().effects, make_grid_invulnerable_effect());

    session_ = std::move(s);
    game.set_state(GameState::Grid);
    game.log("Uploading consciousness... You jack in.");
    return true;
}
```

> `Game::set_state(GameState)` must exist or be added — `grep -n "state_ =\|set_state" include/astra/game.h src/game.cpp`. If not present, add a public `void set_state(GameState s) { state_ = s; }` method.

> `make_grid_invulnerable_effect()` is a one-line factory like other `make_*_ge` helpers in the codebase. Add to `effect.cpp`/`effect.h`:
> ```cpp
> Effect make_grid_invulnerable_effect();
> ```
> Body sets `id = EffectId::GridInvulnerable`, `name = "Phased Out"`, `duration = -1`, `show_in_bar = true`.

- [ ] **Step 3: Implement `jack_out`**

```cpp
void HackingSystem::jack_out(Game& game, JackOutKind kind) {
    if (!session_) return;
    auto& s = *session_;

    // Apply consequences per kind.
    switch (kind) {
        case JackOutKind::Voluntary:
            commit_loot_(game, s.loot, /*pct*/100);
            game.log("Disconnect channel complete. You wake at the console.");
            break;
        case JackOutKind::HardJackOut:
            commit_loot_(game, s.loot, /*pct*/50);
            detection_.value = std::min(100, detection_.value + 10);
            game.log("Hard jack-out. Your trail blares.");
            break;
        case JackOutKind::NonBlackDeath:
            // Avatar wiped. Body wakes with a debuff.
            add_effect(game.player().effects, make_blackice_shock_effect_short());
            game.log("Avatar wiped. You wake disoriented at the console.");
            break;
        case JackOutKind::BlackIceDeath: {
            // Real HP damage + possible IRL death.
            int bleed = s.skill_neural_fortitude ? 5 : 10;
            game.player().take_damage(bleed);
            if (game.player().hp <= 0) {
                game.set_death_message("Killed by black ICE in the Grid.");
                game.set_state(GameState::GameOver);
                session_.reset();
                return;
            }
            add_effect(game.player().effects, make_blackice_shock_effect_long());
            game.log("BLACK ICE BLEED-THROUGH. You convulse and slump.");
            break;
        }
        case JackOutKind::SoftDisconnect:
            // Load-time recovery — no penalty, no loot.
            break;
    }

    // Restore body state
    remove_effect(game.player().effects, EffectId::GridInvulnerable);
    game.set_state(s.body_state);
    session_.reset();
}
```

> `commit_loot_` is a private helper added in this task — credits to player money, code-fragments to inventory, programs as Item drops, lore unlocks logged. The `pct` arg drops a fraction. Add a private declaration to `hacking_system.h`.

> `make_blackice_shock_effect_short/long` factories in `effect.cpp`. Short = 20 ticks. Long = 60 ticks. Stat malus: -1 to all attributes.

- [ ] **Step 4: Build**

```bash
cmake --build build 2>&1 | tail -15
```

- [ ] **Step 5: Commit**

```bash
git add include/astra/hacking_system.h src/hacking_system.cpp \
        include/astra/effect.h src/effect.cpp
git commit -m "feat(grid): HackingSystem jack_in/jack_out lifecycle

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6 — `tick_grid` + Trace + Heat decay

**Files:**
- Modify: `src/hacking_system.cpp`
- Modify: `include/astra/cyberdeck.h`, `src/cyberdeck.cpp`

Per-turn Grid update: ICE acts, Trace ticks, Heat decays, breakpoints fire.

- [ ] **Step 1: Add Heat helpers in cyberdeck.cpp**

```cpp
void cyberdeck_add_heat(CyberdeckData& cd, int amount);
bool cyberdeck_decay_heat(CyberdeckData& cd);    // true if at zero
bool cyberdeck_overheated(const CyberdeckData& cd); // heat > heat_cap
void cyberdeck_force_reboot(CyberdeckData& cd);  // ram=0, heat=0
```

Bodies: clamp non-negative, cooling_rate decay per call.

- [ ] **Step 2: Implement `tick_grid`**

```cpp
void HackingSystem::tick_grid(Game& game) {
    if (!session_) return;
    auto& s = *session_;

    // 1. ICE actions (gray/black approach + attack; white patrols).
    grid_ice::tick_all(s, game);

    // 2. Heat decay on equipped deck.
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->cyberdeck) {
        auto& cd = *(*deck_slot)->cyberdeck;
        cyberdeck_decay_heat(cd);

        // 3. Heat → Trace coupling. While heat > 5, +1 Trace tick / turn.
        if (cd.heat_current > 5) {
            s.trace = std::min(100, s.trace + 1);
        }

        // 4. Forced reboot if over cap.
        if (cyberdeck_overheated(cd)) {
            cyberdeck_force_reboot(cd);
            s.ram = 0;
            s.trace = std::min(100, s.trace + 10);
            game.log("Deck overheated — forced reboot. RAM lost. Trace +10.");
        }
    }

    // 5. Tier baseline trace tick.
    s.trace = std::min(100, s.trace + s.trace_tick_per_turn);

    // 6. White ICE visibility bonus already added by grid_ice::tick_all.

    // 7. Breakpoint side effects (50, 75, 100).
    if (s.trace >= 100 && s.trace_alert_pulses < 3) {
        spawn_black_ice_(s);
        s.trace_alert_pulses = 3;
        game.log("BLACK ICE CONVERGING.");
    } else if (s.trace >= 75 && s.trace_alert_pulses < 2) {
        spawn_gray_ice_reinforcement_(s);
        s.trace_alert_pulses = 2;
        game.log("Gray ICE reinforcements detected.");
    } else if (s.trace >= 50 && s.trace_alert_pulses < 1) {
        s.trace_alert_pulses = 1;
        game.log("Alert: Trace at 50%.");
    }

    // 8. Avatar HP zero check.
    if (s.avatar_hp <= 0) {
        // grid_ice::tick_all sets s.last_killer_color when it kills the avatar.
        JackOutKind kind = (s.last_killer_color == IceColor::Black)
                         ? JackOutKind::BlackIceDeath
                         : JackOutKind::NonBlackDeath;
        jack_out(game, kind);
    }
}
```

> Add `IceColor last_killer_color = IceColor::White;` to `GridSession`.
> Add private helpers `spawn_black_ice_`, `spawn_gray_ice_reinforcement_` to `HackingSystem`. Pick a random walkable tile far from the avatar; insert a `GridIce` of the right color.

- [ ] **Step 3: Wire `tick_grid` into the main loop**

In `src/game.cpp`, find `advance_world` and the main update path. Add the Grid branch:

```cpp
void Game::advance_world(int cost) {
    if (state_ == GameState::Grid) {
        hacking_system_.tick_grid(*this);
        return;
    }
    // existing body unchanged
}
```

Also ensure regular `tick` (`HackingSystem::tick`) does NOT run while `state_ == GameState::Grid` (Detection counter is real-world only).

- [ ] **Step 4: Build + commit**

```bash
cmake --build build 2>&1 | tail -10
```

```bash
git add include/astra/hacking_system.h src/hacking_system.cpp \
        include/astra/cyberdeck.h src/cyberdeck.cpp \
        include/astra/grid_session.h src/game.cpp
git commit -m "feat(grid): per-turn tick — Trace, Heat decay, breakpoints

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7 — Grid renderer (sector + HUD)

**Files:**
- Create: `include/astra/grid_renderer.h`, `src/grid_renderer.cpp`
- Modify: `src/game_rendering.cpp`, `CMakeLists.txt`

Render the active sector (tiles + ICE + avatar) in Tron palette plus a top/bottom HUD with Trace, Heat, RAM, HP bars.

- [ ] **Step 1: Create `include/astra/grid_renderer.h`**

```cpp
#pragma once
namespace astra {
class Game;
class Renderer;
namespace grid_renderer {
    void render(Game& game, Renderer& r);
}
}
```

- [ ] **Step 2: Implement render**

In `src/grid_renderer.cpp`, walk the sector tile grid, render each tile via the existing renderer's draw cell API at the appropriate screen coords (centered on the play area). Render ICE and avatar on top. Render HUD bars at the right side panel, mirroring the existing HUD layout pattern from `src/hud_renderer.cpp` (or wherever the play HUD lives — `grep -rn "HUD\|hud_render" src/`).

Pattern (paraphrased from existing tilemap rendering):

```cpp
namespace astra::grid_renderer {

static char glyph_for(GridTile t) {
    using namespace grid_theme;
    switch (t) {
        case GridTile::Floor:         return floor_glyph;
        case GridTile::Firewall:      return firewall_glyph;
        case GridTile::DataNode:      return data_node_glyph;
        case GridTile::Gateway:       return gateway_glyph;
        case GridTile::ExitNode:      return exit_glyph;
        case GridTile::EncryptedFile: return encrypted_glyph;
        case GridTile::Wall:          return ' ';
    }
    return ' ';
}

static Color color_for(GridTile t) {
    using namespace grid_theme;
    switch (t) {
        case GridTile::Floor:         return floor;
        case GridTile::Firewall:      return firewall;
        case GridTile::DataNode:      return data_node;
        case GridTile::Gateway:       return gateway;
        case GridTile::ExitNode:      return exit_node;
        case GridTile::EncryptedFile: return encrypted;
        case GridTile::Wall:          return Color::Black;
    }
    return Color::White;
}

void render(Game& game, Renderer& r) {
    auto* sess = game.hacking_system().session();
    if (!sess) return;
    const auto& s = *sess;

    // Layout: tiles take left ~80%, HUD on right (same as Playing).
    int origin_x = 1;
    int origin_y = 1;

    for (int y = 0; y < s.sector.h; ++y) {
        for (int x = 0; x < s.sector.w; ++x) {
            GridTile t = s.sector.at(x, y);
            r.draw_char(origin_x + x, origin_y + y, glyph_for(t), color_for(t));
        }
    }
    for (const auto& ice : s.ice) {
        char g = ice.color == IceColor::White ? grid_theme::white_ice_glyph
               : ice.color == IceColor::Gray  ? grid_theme::gray_ice_glyph
               :                                 grid_theme::black_ice_glyph;
        Color c = ice.color == IceColor::White ? grid_theme::white_ice
                : ice.color == IceColor::Gray  ? grid_theme::gray_ice
                :                                 grid_theme::black_ice;
        r.draw_char(origin_x + ice.x, origin_y + ice.y, g, c);
    }
    r.draw_char(origin_x + s.avatar_x, origin_y + s.avatar_y,
                grid_theme::avatar_glyph, grid_theme::avatar);

    // HUD: hp/ram/trace/heat bars.
    int hud_x = origin_x + s.sector.w + 2;
    int hy = origin_y;
    auto bar = [&](const char* label, int v, int max, Color c) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%-6s %3d/%3d", label, v, max);
        r.draw_text(hud_x, hy, buf, c);
        ++hy;
    };
    bar("HP",    s.avatar_hp, s.avatar_hp_max, Color::Red);
    bar("RAM",   s.ram,        s.ram_max,       Color::Cyan);
    bar("Trace", s.trace,      100,             s.trace >= 75 ? Color::Red
                                              : s.trace >= 50 ? Color::Yellow
                                              :                 Color::Green);

    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (deck_slot && *deck_slot && (*deck_slot)->cyberdeck) {
        const auto& cd = *(*deck_slot)->cyberdeck;
        bar("Heat",  cd.heat_current, cd.stats.heat_cap,
            cd.heat_current > 5 ? Color::Yellow : Color::Cyan);
    }
}

} // namespace astra::grid_renderer
```

> If `Renderer::draw_char` / `draw_text` don't match these exact names, find the actual API: `grep -n "void draw\|Renderer::" include/astra/renderer.h`.

- [ ] **Step 3: Hook into game_rendering.cpp**

Find the `render_play()` switch (line ~857). Add:

```cpp
        case GameState::Grid:       grid_renderer::render(*this, *renderer_); break;
```

Add `#include "astra/grid_renderer.h"`.

- [ ] **Step 4: Render ghost `@` at body when in Grid**

In the play renderer, when rendering the player at their position, also render a faint `@` if `state_ == GameState::Grid`. (Body is invisible during Grid, but adding a "ghost glyph" shown only when the camera/player tile would be rendered ensures the player can locate the body when they jack out.) Plan 4 may revisit; v1 just leaves a literal cosmetic at the saved `body_x, body_y`.

> Implementation: in `render_play`, before rendering the player, check `state_ == GameState::Grid` — if true, render `@` in `Color::DarkGray` at `(player_.x, player_.y)` and skip the live player-glyph render. (Player is anyway frozen there.) During Grid mode the play view isn't rendered at all, so this is a no-op until Plan 4 adds split rendering. Skip this step for now if it complicates the renderer — file a follow-up TODO in `project_grid_followup.md` if so.

- [ ] **Step 5: Add `src/grid_renderer.cpp` to CMakeLists.txt**

- [ ] **Step 6: Build + smoke test**

```bash
cmake --build build && ./build/astra
```

Use dev verb (Task 17) to jack in. Confirm sector renders with avatar at spawn, HUD shows HP/RAM/Trace/Heat bars.

- [ ] **Step 7: Commit**

```bash
git add include/astra/grid_renderer.h src/grid_renderer.cpp \
        src/game_rendering.cpp CMakeLists.txt
git commit -m "feat(grid): tilemap + HUD renderer for Grid mode

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8 — Grid input (movement, fire, jack out)

**Files:**
- Create: `include/astra/grid_input.h`, `src/grid_input.cpp`
- Modify: `src/game_input.cpp`, `CMakeLists.txt`

Grid input lives in its own translation unit. `game_input.cpp` only dispatches when `state_ == GameState::Grid`.

- [ ] **Step 1: Create grid_input.h**

```cpp
#pragma once
namespace astra {
class Game;
namespace grid_input {
    // Returns true if the input consumed a turn (drives advance_world).
    bool handle(Game& game, int key);
}
}
```

- [ ] **Step 2: Implement grid_input.cpp**

```cpp
namespace astra::grid_input {

static bool try_move(GridSession& s, int dx, int dy) {
    int nx = s.avatar_x + dx;
    int ny = s.avatar_y + dy;
    if (!s.sector.passable(nx, ny)) return false;
    s.avatar_x = nx;
    s.avatar_y = ny;
    return true;
}

bool handle(Game& game, int key) {
    auto* sess = game.hacking_system().session();
    if (!sess) return false;
    auto& s = *sess;

    auto act_after_move = [&](){
        // Walking onto special tiles — handled inline.
        GridTile here = s.sector.at(s.avatar_x, s.avatar_y);
        if (here == GridTile::ExitNode) {
            game.log("Disconnect channel...");
            game.hacking_system().jack_out(game, JackOutKind::Voluntary);
        } else if (here == GridTile::DataNode) {
            // pickup loot — small credit drip; remove the node tile.
            int credits = 5 + 5 * s.trace_tick_per_turn;
            s.loot.credits += credits;
            s.sector.set(s.avatar_x, s.avatar_y, GridTile::Floor);
            game.log("Data node ripped: +" + std::to_string(credits) + " credits.");
        } else if (here == GridTile::EncryptedFile) {
            // Lore unlock requires decrypt.exe. For now: if avatar holds a
            // pending GhostCloak/GHost cipher, autoread; else hint.
            game.log("Encrypted file. Run decrypt.exe to read.");
        } else if (here == GridTile::Gateway) {
            // Walking onto a gateway tile attempts to traverse.
            // Looks up the matching GatewayLink, opens new sector.
            grid_input::try_traverse_gateway_(game, s);
        }
    };

    switch (key) {
        case KEY_UP:    case 'k': if (try_move(s, 0, -1)) act_after_move(); return true;
        case KEY_DOWN:  case 'j': if (try_move(s, 0,  1)) act_after_move(); return true;
        case KEY_LEFT:  case 'h': if (try_move(s, -1, 0)) act_after_move(); return true;
        case KEY_RIGHT: case 'l': if (try_move(s,  1, 0)) act_after_move(); return true;
        case '.':                 return true;                    // wait
        case 'f': case 'F':       grid_input::open_program_picker_(game, s); return false;
        case 'q':                 // voluntary disconnect requires standing on ⊙
            // Fallthrough handled by walking onto ExitNode; q while not on one is hard.
            game.hacking_system().jack_out(game, JackOutKind::HardJackOut);
            return false;
        case '?':                 grid_input::show_help_(game);   return false;
    }
    return false;
}

} // namespace astra::grid_input
```

> Helpers `try_traverse_gateway_`, `open_program_picker_`, `show_help_` live in this same file as static functions.

- [ ] **Step 3: Wire dispatch in game_input.cpp**

In the main `handle_input` (or whatever the top-level dispatch is — `grep -n "handle_input\|switch.*state_" src/game_input.cpp`), add a branch:

```cpp
if (state_ == GameState::Grid) {
    if (grid_input::handle(*this, key)) {
        advance_world(ActionCost::move);
    }
    return;
}
```

> If `ActionCost::move` doesn't exist, use `ActionCost::default_step` or whatever the codebase calls a one-tile move.

- [ ] **Step 4: Replace the Plan-2 stub at game_input.cpp:259**

The stub currently reads:

```cpp
log("The Grid is not yet implemented (Plan 3 will add it).");
```

Replace with:

```cpp
GridNodeId target{ static_cast<uint32_t>(hk.jack_in_node_id) };
hacking_system_.jack_in(*this, target);
```

> `hk` is the local `Hackable&`. Confirm the surrounding context — the hackable_menu Selected branch with `slot_idx == -1` per kickoff prompt.

- [ ] **Step 5: Build**

```bash
cmake --build build 2>&1 | tail -10
```

- [ ] **Step 6: Smoke test**

Walk to a Precursor console (or use dev verb to spawn one), select "Jack In". Move avatar around the sector. Walk onto the ⊙ tile — should disconnect and return to play state.

- [ ] **Step 7: Commit**

```bash
git add include/astra/grid_input.h src/grid_input.cpp src/game_input.cpp CMakeLists.txt
git commit -m "feat(grid): input dispatch — movement, jack-out, exit-node disconnect

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9 — ICE actors (white, gray, black)

**Files:**
- Modify: `src/grid_ice.cpp`

Three flavors. Spawn one of each per regional sector; subnets get one white only.

- [ ] **Step 1: Implement spawn**

```cpp
namespace astra::grid_ice {

static bool place_random(GridSession& s, std::mt19937& rng,
                         int& out_x, int& out_y) {
    for (int tries = 0; tries < 64; ++tries) {
        int x = rng() % s.sector.w;
        int y = rng() % s.sector.h;
        if (!s.sector.passable(x, y)) continue;
        if (x == s.avatar_x && y == s.avatar_y) continue;
        bool occupied = false;
        for (auto& i : s.ice) if (i.x == x && i.y == y) { occupied = true; break; }
        if (occupied) continue;
        out_x = x; out_y = y;
        return true;
    }
    return false;
}

void spawn_for_sector(GridSession& s, uint32_t seed, int security_tier) {
    std::mt19937 rng(seed ^ 0xDECAFC0DEu);
    int n_white = 1;
    int n_gray  = security_tier >= 2 ? 1 : 0;
    int n_black = 0;   // Plan 3: black ICE only via Trace = 100 trigger or anchor (deferred)

    for (int i = 0; i < n_white; ++i) {
        int x, y; if (!place_random(s, rng, x, y)) break;
        GridIce ice; ice.x = x; ice.y = y; ice.color = IceColor::White; ice.hp = 1;
        ice.patrol_dir = rng() % 4;
        s.ice.push_back(ice);
    }
    for (int i = 0; i < n_gray; ++i) {
        int x, y; if (!place_random(s, rng, x, y)) break;
        GridIce ice; ice.x = x; ice.y = y; ice.color = IceColor::Gray; ice.hp = 2;
        s.ice.push_back(ice);
    }
    (void)n_black;
}

} // namespace astra::grid_ice
```

- [ ] **Step 2: Implement tick_all**

```cpp
void tick_all(GridSession& s, Game& game) {
    static const int dx[4] = { 0, 0, -1, 1 };
    static const int dy[4] = { -1, 1, 0, 0 };

    for (auto& ice : s.ice) {
        if (ice.hp <= 0) continue;

        bool sees = manhattan(ice.x, ice.y, s.avatar_x, s.avatar_y) <= 5;
        ice.sees_avatar = sees;

        switch (ice.color) {
            case IceColor::White: {
                // Patrol; if sees avatar, +2 Trace per turn.
                if (sees) {
                    int bonus = s.skill_intrusion ? 1 : 2;
                    s.trace = std::min(100, s.trace + bonus);
                } else {
                    int d = ice.patrol_dir;
                    int nx = ice.x + dx[d];
                    int ny = ice.y + dy[d];
                    if (s.sector.passable(nx, ny)) {
                        ice.x = nx; ice.y = ny;
                    } else {
                        ice.patrol_dir = (d + 1) % 4;
                    }
                }
                break;
            }
            case IceColor::Gray: {
                if (!sees) break;
                if (manhattan(ice.x, ice.y, s.avatar_x, s.avatar_y) == 1) {
                    s.avatar_hp -= 1;
                    s.last_killer_color = IceColor::Gray;
                } else {
                    step_toward(s, ice, s.avatar_x, s.avatar_y);
                }
                break;
            }
            case IceColor::Black: {
                if (manhattan(ice.x, ice.y, s.avatar_x, s.avatar_y) == 1) {
                    int dmg = s.skill_neural_fortitude ? 1 : 2;
                    s.avatar_hp -= dmg;
                    s.last_killer_color = IceColor::Black;
                } else {
                    step_toward(s, ice, s.avatar_x, s.avatar_y);
                }
                break;
            }
        }
    }

    // Cull dead ICE.
    s.ice.erase(std::remove_if(s.ice.begin(), s.ice.end(),
        [](const GridIce& i){ return i.hp <= 0; }), s.ice.end());
}
```

> Helpers `manhattan(...)` and `step_toward(s, ice, tx, ty)` are static functions in this TU. `step_toward` is greedy: pick the cardinal step that reduces distance and is passable + unoccupied.

- [ ] **Step 3: Implement damage / kill_if_dead**

```cpp
void damage(GridSession& s, GridIce& ice, int dmg) {
    ice.hp -= dmg;
}
bool kill_if_dead(GridSession& s, GridIce& ice) {
    if (ice.hp > 0) return false;
    s.trace = std::min(100, s.trace + 3);   // killing ICE raises Trace
    return true;
}
```

- [ ] **Step 4: Build + smoke test**

Jack in via dev verb, watch avatar HP drop if you stand near a Gray ICE for several turns.

- [ ] **Step 5: Commit**

```bash
git add src/grid_ice.cpp
git commit -m "feat(grid): ICE actors — white patrol, gray engage, black bleed

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 10 — Wire 5 `.exe` programs into Grid

**Files:**
- Modify: `include/astra/program_effects.h`, `src/program_effects.cpp`
- Modify: `src/grid_input.cpp` (open_program_picker_ + fire flow)

Make the inert `.exe` programs do work: `icebreaker_lite`, `ghost_trace`, `cooldown`, `breach`, `decrypt`.

- [ ] **Step 1: Add Grid dispatch in program_effects.h**

Append the `GridProgramContext` struct and `apply_program_in_grid` declaration from the Type Contracts section.

- [ ] **Step 2: Implement each apply_grid in program_effects.cpp**

```cpp
namespace {

std::string apply_icebreaker_lite_grid(GridProgramContext c) {
    // 1d4 + IceBreaking +1 to one ICE in LOS — pick nearest seen enemy.
    GridIce* tgt = nullptr;
    int best = INT_MAX;
    for (auto& i : c.session.ice) {
        int d = std::abs(i.x - c.session.avatar_x) + std::abs(i.y - c.session.avatar_y);
        if (d <= 5 && d < best) { tgt = &i; best = d; }
    }
    if (!tgt) return "icebreaker_lite: no target in line of sight.";
    int dmg = 1 + (c.game.rng().roll(4)) + (c.session.skill_icebreaking ? 1 : 0);
    grid_ice::damage(c.session, *tgt, dmg);
    grid_ice::kill_if_dead(c.session, *tgt);
    return "icebreaker_lite: " + std::to_string(dmg) + " damage to ICE.";
}

std::string apply_ghost_trace_grid(GridProgramContext c) {
    c.session.trace = std::max(0, c.session.trace - 3);
    add_effect(c.game.player().effects, make_ghost_cloak_effect(3));
    return "ghost_trace: invisible to white ICE for 3 turns. Trace -3.";
}

std::string apply_cooldown_grid(GridProgramContext c) {
    auto* slot = c.game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->cyberdeck) return "no deck.";
    auto& cd = *(*slot)->cyberdeck;
    cd.heat_current = std::max(0, cd.heat_current - 4);
    return "cooldown: heat -4.";
}

std::string apply_breach_grid(GridProgramContext c) {
    // Open one adjacent firewall tile, OR crack one gateway link.
    static const int dx[4] = { 0, 0, -1, 1 };
    static const int dy[4] = { -1, 1, 0, 0 };
    for (int d = 0; d < 4; ++d) {
        int nx = c.session.avatar_x + dx[d];
        int ny = c.session.avatar_y + dy[d];
        if (c.session.sector.at(nx, ny) == GridTile::Firewall) {
            c.session.sector.set(nx, ny, GridTile::Floor);
            c.session.trace = std::min(100, c.session.trace + 5);
            return "breach: firewall down. Trace +5.";
        }
        if (c.session.sector.at(nx, ny) == GridTile::Gateway) {
            // Crack the matching edge in the network.
            for (auto& e : c.game.world().grid_network().edges()) {
                if ((e.from == c.session.current_node || e.to == c.session.current_node) && !e.cracked) {
                    auto& e_mut = const_cast<GridEdge&>(e); // edges() returns const&
                    e_mut.cracked = true;
                    c.session.trace = std::min(100, c.session.trace + 5);
                    return "breach: gateway cracked. Trace +5.";
                }
            }
        }
    }
    return "breach: nothing adjacent to break.";
}

std::string apply_decrypt_grid(GridProgramContext c) {
    static const int dx[5] = { 0, 0, 0, -1, 1 };
    static const int dy[5] = { 0, -1, 1, 0, 0 };
    for (int d = 0; d < 5; ++d) {
        int nx = c.session.avatar_x + dx[d];
        int ny = c.session.avatar_y + dy[d];
        if (c.session.sector.at(nx, ny) == GridTile::EncryptedFile) {
            c.session.sector.set(nx, ny, GridTile::Floor);
            c.session.loot.lore_unlocked.push_back("ARCH-" +
                std::to_string(c.session.entry_node.value) + "-" + std::to_string(d));
            return "decrypt: archive read.";
        }
    }
    return "decrypt: no encrypted file in range.";
}

} // namespace

std::string apply_program_in_grid(ProgramId id, GridProgramContext c) {
    switch (id) {
        case ProgramId::IcebreakerLite: return apply_icebreaker_lite_grid(c);
        case ProgramId::GhostTrace:     return apply_ghost_trace_grid(c);
        case ProgramId::Cooldown:       return apply_cooldown_grid(c);
        case ProgramId::Breach:         return apply_breach_grid(c);
        case ProgramId::Decrypt:        return apply_decrypt_grid(c);
        default:                        return "Program is not Grid-side.";
    }
}
```

> `make_ghost_cloak_effect(int turns)` lives in `effect.cpp` like the other factories.
> `c.game.rng().roll(n)` — confirm exact RNG API: `grep -n "class.*Rng\|roll(" include/astra/`. Adapt as needed.

- [ ] **Step 3: Implement program picker in grid_input.cpp**

```cpp
void open_program_picker_(Game& game, GridSession& s) {
    auto* slot = game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->cyberdeck) {
        game.log("No deck."); return;
    }
    auto& cd = *(*slot)->cyberdeck;
    // Build candidates: loaded slots that are .exe (Atk/Stl/Utl).
    std::vector<std::pair<int, ProgramId>> opts;
    for (int i = 0; i < cd.stats.slots; ++i) {
        if (cd.loaded[i].program_def_id == 0) continue;
        ProgramId pid = static_cast<ProgramId>(cd.loaded[i].program_def_id);
        const auto* def = find_program(pid);
        if (!def) continue;
        if (def->kind == ProgramKind::Qh) continue;   // QH only fires real-world
        opts.push_back({i, pid});
    }
    if (opts.empty()) { game.log("No Grid programs loaded."); return; }
    // For v1, fire the first .exe. Full picker UI deferred to a follow-up
    // unless trivially achievable here.
    // TODO(plan-3 polish): proper modal picker.
    auto [slot_idx, pid] = opts.front();
    const auto* def = find_program(pid);

    // RAM check — first program of session is heatless for GhostProtocol users.
    if (s.ram < def->ram_cost) { game.log("Not enough RAM."); return; }
    s.ram -= def->ram_cost;
    int heat = def->heat_cost;
    if (s.skill_ghost_protocol && !s.ghost_protocol_used) {
        heat = 0;
        s.ghost_protocol_used = true;
    }
    cyberdeck_add_heat(cd, heat);

    GridProgramContext ctx{game, s, -1, -1};
    auto msg = apply_program_in_grid(pid, ctx);
    game.log("[" + std::string(def->filename) + "] " + msg);
}
```

> A real picker UI (selectable by hotkey) is a follow-up; v1 fires the first loaded `.exe`. Track in `project_grid_followup.md`.

- [ ] **Step 4: Build + smoke test**

Jack in (dev verb). Press `f` — first loaded `.exe` fires. RAM drops, heat rises.

- [ ] **Step 5: Commit**

```bash
git add include/astra/program_effects.h src/program_effects.cpp src/grid_input.cpp
git commit -m "feat(grid): wire 5 .exe programs (icebreaker, ghost, cool, breach, decrypt)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11 — Voluntary, hard, and forced disconnect flows

**Files:**
- Modify: `src/grid_input.cpp`, `src/hacking_system.cpp`

Tighten the disconnect surfaces. Voluntary already works (walk onto ⊙). Hard jack-out is `Q` (capital). Forced is HP=0 path in `tick_grid`.

- [ ] **Step 1: Distinguish `q` (info) from `Q` (hard jack-out)**

In `grid_input::handle`, change the `'q'` case:

```cpp
case 'q':
    if (s.sector.at(s.avatar_x, s.avatar_y) == GridTile::ExitNode) {
        game.log("(walk; you're already on an exit. Step onto it to disconnect.)");
    } else {
        game.log("(use Shift+Q for hard jack-out; walk to ⊙ for safe exit.)");
    }
    return false;
case 'Q':
    game.hacking_system().jack_out(game, JackOutKind::HardJackOut);
    return false;
```

- [ ] **Step 2: Confirm forced flow**

Re-read Task 6 step 2 — after `tick_grid` decrements `avatar_hp`, the forced flow already calls `jack_out` with the right `JackOutKind`. No new code; confirm this works in smoke test by spawning two gray ICE adjacent and waiting (`.`).

- [ ] **Step 3: Smoke test**

```bash
./build/astra
```

- Walk onto ⊙: voluntary flow, no penalty.
- Press Shift+Q: Trace +10, body wakes.
- Get killed by gray ICE: `BlackIceShock` short debuff applied.
- (Black-ICE death tested in Task 9 follow-up via dev `spawn-ice black`.)

- [ ] **Step 4: Commit**

```bash
git add src/grid_input.cpp src/hacking_system.cpp
git commit -m "feat(grid): polish voluntary/hard/forced disconnect distinctions

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 12 — Skill runtime effects

**Files:**
- Modify: places that compute skill-affected behavior (`src/grid_ice.cpp`, `src/program_effects.cpp`, `src/grid_input.cpp`, `src/hacking_system.cpp`, possibly `src/cyberdeck.cpp`)

Audit every skill flag in `GridSession` and verify it has a consumer:

| Skill | Effect | Consumer |
|---|---|---|
| Intrusion | white-ICE-visibility Trace add halved (2→1) | `grid_ice::tick_all` (already) |
| IceBreaking | +1 dmg to ICE | `apply_icebreaker_lite_grid` (already) |
| DaemonMastery | +1 deck slot iteration cap | `open_program_picker_` + paper-doll preview |
| GhostProtocol | first program is heatless | `open_program_picker_` (already) |
| DeepGridNavigator | gateway crack chance 50% w/o breach.exe; netmap reveal | `apply_breach_grid` (auto-crack adjacent gateway no-cost) + `pda_hacking_tab netmap` |
| NeuralFortitude | halve black-ICE bleed-through; +1 avatar HP max | `jack_in` (already), `tick_all` black-branch (already) |

- [ ] **Step 1: DaemonMastery slot bonus**

In `open_program_picker_`, change `cd.stats.slots` upper bound to:

```cpp
int eff_slots = std::min(kCyberdeckMaxSlots,
                         cd.stats.slots + (s.skill_daemon_mastery ? 1 : 0));
for (int i = 0; i < eff_slots; ++i) { ... }
```

Also surface this in the PDA Equipment look overlay and the deck info terminal (cosmetic — show "(+1 from DaemonMastery)" suffix).

- [ ] **Step 2: DeepGridNavigator gateway free-crack**

When `apply_breach_grid` finds an adjacent gateway, if `c.session.skill_deepgrid_navigator` is true and a 50/50 RNG roll succeeds, crack it without consuming the program (still consume RAM + Heat — the spell still runs, but the breach is "for free" in the sense that the gateway costs no extra check). Document in mechanics.md.

- [ ] **Step 3: Build + commit**

```bash
cmake --build build 2>&1 | tail -10
git add src/grid_input.cpp src/program_effects.cpp src/grid_ice.cpp
git commit -m "feat(grid): wire skill runtime effects (Intrusion, IceBreaking, etc.)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 13 — Save schema bump v53→v54

**Files:**
- Modify: `include/astra/save_file.h`, `src/save_file.cpp`
- Modify: `src/world_manager.cpp` (if it owns galaxy load hook)

Persist `GridNetwork` graph state. Soft-disconnect any active session on load.

- [ ] **Step 1: Bump SAVE_FILE_VERSION**

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 54;   // v54: Plan 3 — GridNetwork
```

- [ ] **Step 2: Add `write_grid_network` / `read_grid_network` helpers**

In `src/save_file.cpp`, mirror the pattern of existing helpers (e.g. `write_hackable`/`read_hackable`):

```cpp
static void write_grid_network(BinaryWriter& w, const GridNetwork& net) {
    w.u32(static_cast<uint32_t>(net.nodes().size()));
    for (const auto& n : net.nodes()) {
        w.u32(n.id.value);
        w.u8(static_cast<uint8_t>(n.kind));
        w.u32(n.source_seed);
        w.u8(static_cast<uint8_t>(n.security_tier));
        w.string(n.label);
        w.u32(static_cast<uint32_t>(n.sector_seeds.size()));
        for (uint32_t s : n.sector_seeds) w.u32(s);
    }
    w.u32(static_cast<uint32_t>(net.edges().size()));
    for (const auto& e : net.edges()) {
        w.u32(e.from.value);
        w.u32(e.to.value);
        w.u8(static_cast<uint8_t>(e.gateway_tier));
        w.u8(e.cracked ? 1 : 0);
    }
}

static void read_grid_network(BinaryReader& r, GridNetwork& net) {
    net.clear();
    uint32_t n_nodes = r.u32();
    for (uint32_t i = 0; i < n_nodes; ++i) {
        GridNode n;
        n.id.value      = r.u32();
        n.kind          = static_cast<GridNodeKind>(r.u8());
        n.source_seed   = r.u32();
        n.security_tier = r.u8();
        n.label         = r.string();
        uint32_t ns = r.u32();
        for (uint32_t j = 0; j < ns; ++j) n.sector_seeds.push_back(r.u32());
        // bypass next_id_ assignment — preserve original ids on load
        net.nodes_push_raw(n);
    }
    uint32_t n_edges = r.u32();
    for (uint32_t i = 0; i < n_edges; ++i) {
        GridEdge e;
        e.from.value    = r.u32();
        e.to.value      = r.u32();
        e.gateway_tier  = r.u8();
        e.cracked       = r.u8() != 0;
        net.add_edge(e);
    }
}
```

> Add `void nodes_push_raw(GridNode)` + `void set_next_id_max()` private/public helpers in `GridNetwork` so loaded nodes preserve ids; or rebuild ids consistently.

- [ ] **Step 3: Hook into save/load entry points**

In the galaxy save body — typically in `save_file.cpp` after the world's main blob — add:

```cpp
write_grid_network(w, world.grid_network());
```

In load body, mirror:

```cpp
read_grid_network(r, world.grid_network());
```

- [ ] **Step 4: Soft-disconnect on load**

Just after loading state, if the loaded `state_ == GameState::Grid`, force `state_ = GameState::Playing` and call `hacking_system_.session_.reset()` (or expose an `abandon_session_for_load()` helper). Log "Soft disconnect — Grid session not resumable." per spec §5.

> Per project rule: per `feedback_no_backcompat_pre_ship`, no migration. Old v53 saves are rejected at the existing `if (h.version != SAVE_FILE_VERSION)` check.

- [ ] **Step 5: Build + smoke test**

```bash
cmake --build build && ./build/astra
```

Save game in non-Grid state; load — graph survives. Force Grid state via dev verb, save, quit, reload — confirm soft disconnect log.

- [ ] **Step 6: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp \
        include/astra/grid_network.h src/grid_network.cpp \
        include/astra/hacking_system.h src/hacking_system.cpp
git commit -m "feat(save): bump v53→v54 — persist GridNetwork; soft-disconnect on load

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 14 — PDA Hacking tab integration: live netmap + real `jack -t`

**Files:**
- Modify: `src/pda_hacking_tab.cpp`

`netmap` becomes real (lists known nodes). `jack -t <node>` actually jacks in. Update `tab_help_body`.

- [ ] **Step 1: Implement real `netmap`**

Replace the placeholder netmap renderer with a list of nodes from `world().grid_network()`:

```cpp
void PdaScreen::hack_term_cmd_netmap(const std::vector<std::string>&) {
    const auto& net = game_->world().grid_network();
    if (net.nodes().empty()) {
        hack_term_emit("(netmap empty — discover Precursor consoles to populate)");
        return;
    }
    for (const auto& n : net.nodes()) {
        const char* kind = n.kind == GridNodeKind::Subnet ? "[subnet]"
                         : n.kind == GridNodeKind::RegionalDarknet ? "[regional]"
                         : "[deep-grid]";
        char buf[160];
        std::snprintf(buf, sizeof(buf), "  %-32s %s  T%d", n.label.c_str(), kind, n.security_tier);
        hack_term_emit(buf);
    }
}
```

- [ ] **Step 2: Implement real `jack -t`**

```cpp
void PdaScreen::hack_term_cmd_jack(const std::vector<std::string>& args) {
    if (args.size() < 2 || args[0] != "-t") {
        hack_term_emit("usage: jack -t <node-label>");
        return;
    }
    auto& net = game_->world().grid_network();
    const GridNode* match = nullptr;
    for (const auto& n : net.nodes()) {
        if (n.label == args[1]) { match = &n; break; }
    }
    if (!match) { hack_term_emit("Unknown node: " + args[1]); return; }

    bool ok = game_->hacking_system().jack_in(*game_, match->id);
    if (ok) {
        close();   // close PDA — jack_in switches state to Grid
    }
}
```

> Hook tab-completion: add node labels to the autocomplete pool.

- [ ] **Step 3: Update `tab_help_body(PdaTab::Hacking)`**

Append to the help text:

```
netmap            list known networks
jack -t <node>    jack into a network node (requires Cat_Hacking)
```

Remove the "Plan 3 will add it" stub placeholder.

- [ ] **Step 4: Build + smoke test**

Open PDA → Hacking → type `netmap` (lists nodes) → type `jack -t Heavens.Above` (or whatever label is shown).

- [ ] **Step 5: Commit**

```bash
git add src/pda_hacking_tab.cpp
git commit -m "feat(pda-hacking): wire netmap + jack -t to GridNetwork

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 15 — Dev console verbs: jack, trace, spawn-ice

**Files:**
- Modify: `src/dev_console.cpp`

- [ ] **Step 1: Add dispatchers**

```cpp
// jack <network-label>   — force jack-in to the named node (must exist).
// trace <n>              — set Grid trace to n.
// spawn-ice <kind>       — spawn one ICE of kind (white|gray|black) near avatar.
```

Wire each as a new verb in the existing dev_console parsing pattern. Match the signatures used by other verbs in the file. Each should `game.log()` outcomes.

- [ ] **Step 2: Build + smoke test**

`:jack Heavens.Above` jacks in. `:trace 49` then wait — at next tick should hit 50 alert. `:spawn-ice black` adjacent to avatar.

- [ ] **Step 3: Commit**

```bash
git add src/dev_console.cpp include/astra/dev_console.h
git commit -m "feat(dev): jack/trace/spawn-ice verbs for Grid testing

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 16 — Black ICE summon on Trace = 100

**Files:**
- Modify: `src/hacking_system.cpp`

Implement `spawn_black_ice_` private helper (referenced in Task 6). One black ICE only. Pursues avatar.

- [ ] **Step 1: Implement helpers**

```cpp
void HackingSystem::spawn_black_ice_(GridSession& s) {
    int x = 0, y = 0;
    std::mt19937 rng(0xB1ACC1CEu ^ s.entry_node.value);
    for (int tries = 0; tries < 64; ++tries) {
        x = rng() % s.sector.w;
        y = rng() % s.sector.h;
        if (s.sector.passable(x, y) &&
            std::abs(x - s.avatar_x) + std::abs(y - s.avatar_y) >= 4) break;
    }
    GridIce ice; ice.x = x; ice.y = y; ice.color = IceColor::Black; ice.hp = 4;
    s.ice.push_back(ice);
}

void HackingSystem::spawn_gray_ice_reinforcement_(GridSession& s) {
    // Same as above with Gray and hp=2.
}
```

- [ ] **Step 2: Smoke test**

`:trace 99` then `:wait` once — black ICE appears, log line fires.

- [ ] **Step 3: Commit**

```bash
git add src/hacking_system.cpp
git commit -m "feat(grid): black ICE summon at Trace 100

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 17 — Mechanics + items + roadmap docs

**Files:**
- Modify: `docs/mechanics.md`, `docs/items.md`, `docs/roadmap.md`

- [ ] **Step 1: docs/mechanics.md — append Grid section**

Add a new top-level "## Hacking — The Grid" section (or extend an existing Hacking section) with subsections:

- **Trace** — formula, sources (subnet +1, regional +2, deep +3, white visibility +1/+2 (Intrusion), gateway breach +5 burst, ICE kill +3, heat coupling +1 above 5).
- **Heat** — per-deck, decay = cooling_rate, forced reboot at heat > heat_cap (RAM lost, Trace +10).
- **ICE behavior** — white patrols+raises Trace, gray engages avatar HP, black bleeds real HP (NeuralFortitude halves).
- **Disconnect outcomes** — voluntary (full loot), hard (Trace +10, 50% loot), non-black death (avatar wiped, body debuff), black death (real HP damage; possible IRL death).
- **Skill effects** — table mapping each unlock to its runtime effect (per Task 12 audit).

- [ ] **Step 2: docs/items.md — refresh program descriptions**

For the 5 `.exe`s, replace the "(inert in Plan 2; Plan 3 will activate)" notes with their actual Grid effects.

- [ ] **Step 3: docs/roadmap.md — mark Plan 3 done**

Add line under Hacking section:

```markdown
- [x] **Plan 3 — The Grid (A-layer)** — jack-in, GridSession runtime, Trace + Heat coupling, ICE actors (white/gray/black), 5 .exe programs wired, Consciousness Anchor anchor sector, save schema v54.
```

- [ ] **Step 4: Commit**

```bash
git add docs/mechanics.md docs/items.md docs/roadmap.md
git commit -m "docs(grid): mechanics + items + roadmap for Plan 3

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 18 — Final verification

Run a full Plan-3 acceptance pass:

- [ ] **Clean build**

```bash
rm -rf build && cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -20
```

Expected: zero warnings introduced by Plan 3.

- [ ] **End-to-end smoke**

1. Start a new game (rejects v53 saves).
2. Walk to a Precursor console (use `:tp` if needed).
3. PDA → Hacking → `netmap` lists the regional darknet.
4. Interact with console → "Jack In" → enters Grid.
5. Sector renders. HUD shows HP / RAM / Trace / Heat.
6. Move avatar around. Step on data-node — credits ticker rises.
7. Get adjacent to gray ICE — avatar HP drops.
8. Press `f` — first loaded `.exe` fires (RAM drops, Heat rises, log line).
9. Walk to ⊙ — voluntary disconnect. Body wakes at console.
10. Inventory: credits added; lore archive unlocks (if decrypt was used).
11. `:trace 100` while jacked in — black ICE summons.
12. Black ICE adjacent kill — real HP damage applied to body.
13. Save + quit + reload — graph persists; old v53 saves rejected.
14. Force-state save while jacked in (`:save`) → reload → soft-disconnect log.
15. PDA tab cycle still works ([/]).

- [ ] **File-size discipline check**

```bash
wc -l src/grid_*.cpp src/hacking_system.cpp src/program_effects.cpp
```

Expected: each ≤ ~600 lines.

- [ ] **Branch state**

```bash
git log --oneline main..HEAD
```

Expected: ~17 commits, each scoped to a task.

If everything passes, hand off to `superpowers:finishing-a-development-branch` to choose merge / PR / cleanup.

---

## Self-review

**Spec coverage** — Plan 3 addresses spec §3 (tiers, sectors, ICE, Trace), §4 (terminal `jack -t` real, body phase-out via GridInvulnerable effect), §5 (galaxy save bump + soft-disconnect; `consciousness.dat` deferred to Plan 4 per kickoff), §6 v1 cut (5 `.exe` live, anchor sector, 3 of 8 `Cat_Hacking` skills wired — but this plan wires 6 of 8 since the kickoff prompt locks `CodeCraft` and `ConsciousnessAnchor` to Plan 4).

**Open ambiguities flagged for executor:**
- `Color::DarkBlue` etc. in `grid_theme.h` may need swapping for actual palette names in `include/astra/color.h`.
- `Renderer::draw_char` exact name — verify before Task 7.
- `c.game.rng().roll(n)` — confirm RNG API.
- `EquipSlot` / `equipment.equipped_cyberdeck()` is verified in current main.
- The DataNode "credits ticker" formula in Task 8 is a placeholder; tune in playtest.
- Heat icon glyphs use ASCII fallbacks because terminal Cell is `char`. Revisit when multi-byte support lands.

**Risks:**
- Sector tile count vs render area — first run may have layout overlap with the existing right-side HUD. If the play-area width is too narrow, shrink sector to 14x10. Adjust in `gen_*_sector` and Anchor.
- Save format change can lock out playtesters — per project rule, intentional.
- Black ICE difficulty tuning may be brutal first pass; v1 ships with `NeuralFortitude` halving so the safety is in.
