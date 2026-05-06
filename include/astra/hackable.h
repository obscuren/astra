#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

// Forward-declared to avoid circular dependency (tilemap.h includes hackable.h)
enum class FixtureType : uint8_t;

enum class HackTag : uint32_t {
    None        = 0,
    Electronic  = 1u << 0,
    Locked      = 1u << 1,
    PowerNode   = 1u << 2,
    DataStore   = 1u << 3,
    HasOptics   = 1u << 4,
    Weaponized  = 1u << 5,
    Mobile      = 1u << 6,
    AlienTech   = 1u << 7,
    JackInPort  = 1u << 8,
};

using HackTagMask = uint32_t;
using TagSet      = HackTagMask;   // alias used at the program-filter site for AND-within readability

inline HackTagMask operator|(HackTag a, HackTag b) {
    return static_cast<HackTagMask>(a) | static_cast<HackTagMask>(b);
}
inline HackTagMask operator|(HackTagMask m, HackTag t) {
    return m | static_cast<HackTagMask>(t);
}
inline bool has_tag(HackTagMask m, HackTag t) {
    return (m & static_cast<HackTagMask>(t)) != 0;
}
inline bool covers(HackTagMask device, TagSet required) {
    return (device & required) == required;
}

HackTagMask tags_for_fixture(FixtureType type);

// Short human-readable label for a tag mask. Picks a single dominant tag
// for display: "turret" (Weaponized+HasOptics) > "camera" (HasOptics) >
// "lock" (Locked) > "power" (PowerNode) > "alien" (AlienTech) >
// "data" (DataStore) > "device" (else). Use for UI/log purposes that need a
// single human-readable label for an already-populated Hackable's dominant tag.
const char* tag_summary(HackTagMask tags);

// Render a TagSet (program target_filter entry) as a human-readable
// requirement, e.g. "HasOptics" or "Weaponized+Mobile". Returns a static
// string; suitable for UI lists of "Targets: ..." in QH info screens.
// Distinct from tag_summary because filters describe REQUIREMENTS
// (multi-tag conjunctions matter) rather than a single dominant capability.
const char* tag_set_describe(TagSet required);

enum class HackState : uint8_t {
    Clean,         // never been hacked
    Compromised,   // at least one QH applied; effect timer running
    Alarmed,       // detected; broadcasts to faction
};

// v63 — Plan 7: device firmware state (shell-driven mutations).
enum class FirmwareState : uint8_t {
    Stock,         // factory firmware
    Wiped,         // permanently bricked via shell `firmware --wipe`
    Glitched,      // partially corrupted (future)
};

// Forward-declared in headers that don't need ProgramId (program.h includes hackable.h).
enum class ProgramId : uint16_t;

// v57 — Plan 4: each Precursor console carries 1..4 of these.
struct LoreFragmentSeed {
    std::string archive_id;        // e.g. "ARCH-7x12-0"; encodes position + index
    bool committed = false;        // true once written to consciousness.dat
};

struct Hackable {
    HackTagMask tags          = 0;
    uint32_t    ip            = 0;        // packed 10.X.Y.host; assigned by LAN registration in Task 9
    int         security_tier = 1;        // 1..3 — gates QH/jack-in availability
    uint32_t    network_id    = 0;        // 0 = unwired (subnet of one)
    HackState   state         = HackState::Clean;

    // For PrecursorConsole only — Plan 3 will use this; Plan 2 stubs the verb.
    int jack_in_node_id   = -1;

    // Compromised-state cooldown timer in ticks. Decremented per game tick;
    // when it hits 0 the state collapses back to Clean (or to Alarmed if a
    // detection event flagged it).
    int state_ticks_left  = 0;

    // v57 — Plan 4 (PrecursorConsole only): lore fragments + Soul Mirror progress.
    std::vector<LoreFragmentSeed> lore_fragments;
    int soul_mirror_progress = 0;

    // v61 — Plan 5 Cut 2.6: source FixtureType, used to pick a wall-mounted
    // device-avatar glyph inside the per-Hackable subnet sector.
    FixtureType source_type = static_cast<FixtureType>(0);

    // v63 — Plan 7: device-shell driven persistent state.
    FirmwareState firmware_state = FirmwareState::Stock;
    uint8_t       cracked_digits = 0;     // hashcat partial-state (0..N revealed)
    bool          escalated      = false; // true once root via hashcat success
    uint32_t      dumped_bytes   = 0;     // dump partial-state accumulator

    // v63 — Plan 7 Phase B: shell-driven mutations that persist across save.
    // `wiped_paths`: filesystem entries removed via `wipe` (rebuilt on every
    // shell open by DeviceFsView, then filtered by this list).
    std::vector<std::string> wiped_paths;
    // Persisted target priority override set by `friendly_fire`. Empty = default.
    std::string friendly_fire_target_faction;

    // Phase B: ephemeral countdowns + flags driven by privileged shell
    // commands. Decremented per world tick by tick_runtime_state(). NOT
    // persisted (cleared on save/load) per spec §13 — only the four canon
    // persisted fields (escalated, cracked_digits, firmware_state,
    // dumped_bytes) plus wiped_paths and friendly_fire_target_faction stick.
    int  optics_blind_ticks    = 0;   // cmd_blind: vision cone disabled
    int  optics_restream_ticks = 0;   // cmd_restream: looped feed
    int  optics_dim_ticks      = 0;   // cmd_dim (PowerNode): downstream cone halved
    int  disarmed_ticks        = 0;   // cmd_disarm: turret won't fire
    bool locked_out_to_player  = false; // cmd_lockout: cosmetic
    int  surged_ticks          = 0;   // cmd_surge: brief power buff
    int  power_off_ticks       = 0;   // cmd_kill: downstream powerless
    int  halt_ticks            = 0;   // cmd_halt: mobile fixture stopped

    // Decrement all tick-based runtime fields by `dt` (typically 1/world tick).
    // Called from HackingSystem::tick(). Free function below.

    // Spec 1: per-corpse Imprint deep-dive state (only meaningful on corpse fixtures).
    bool     corpse_imprint_exhausted = false;
    uint32_t corpse_imprint_seed      = 0;
};

// Decrement Hackable's per-tick runtime countdowns. No-op for fields == 0.
void tick_runtime_state(Hackable& h, int dt = 1);

// Returns a Hackable populated from the fixture type's tag mask. Returns
// Hackable with tags=0 if the fixture is not hackable (caller checks).
Hackable make_hackable(FixtureType type, int tier);

} // namespace astra
