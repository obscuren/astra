#pragma once

#include <cstdint>
#include <vector>

namespace astra {

// Kind of hackable device. Identifies which quickhack target_filter set
// applies and which interaction effects fire. Decoupled from FixtureType:
// a security camera and a console may render the same glyph, but their
// device_kind differs.
enum class DeviceKind : uint8_t {
    Turret,
    Camera,
    Door,
    PowerConduit,
    PrecursorConsole,
    // Future (Plan 3+): Drone, MineTrap, Vendor, Light, Elevator,
    // Hazard, NpcImplant, ShipSystem, ReputationServer, Wreckage.
};

const char* device_kind_name(DeviceKind k);

enum class HackState : uint8_t {
    Clean,         // never been hacked
    Compromised,   // at least one QH applied; effect timer running
    Alarmed,       // detected; broadcasts to faction
};

// Forward-declared in headers that don't need ProgramId (program.h includes hackable.h).
enum class ProgramId : uint16_t;

struct Hackable {
    DeviceKind device_kind = DeviceKind::Turret;
    int security_tier = 1;        // 1..3 — gates QH/jack-in availability
    uint32_t network_id = 0;      // 0 = unwired (subnet of one)
    HackState state = HackState::Clean;

    // Program ids that are valid against this device.
    // Filled by make_hackable() based on device_kind.
    std::vector<ProgramId> available_qh;

    // For PrecursorConsole only — Plan 3 will use this; Plan 2 stubs the verb.
    int jack_in_node_id = -1;

    // Compromised-state cooldown timer in ticks. Decremented per game tick;
    // when it hits 0 the state collapses back to Clean (or to Alarmed if a
    // detection event flagged it).
    int state_ticks_left = 0;
};

// Default-constructs a Hackable with device-appropriate available_qh
// programs filled in. Use this everywhere a Hackable is added to a
// fixture or NPC — never hand-fill the available_qh list.
Hackable make_hackable(DeviceKind kind, int tier);

} // namespace astra
