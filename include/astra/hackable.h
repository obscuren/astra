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
};

// Returns a Hackable populated from the fixture type's tag mask. Returns
// Hackable with tags=0 if the fixture is not hackable (caller checks).
Hackable make_hackable(FixtureType type, int tier);

} // namespace astra
