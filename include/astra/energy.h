#pragma once

#include <cstdint>

namespace astra {

// Anything that holds energy.
struct EnergyStore {
    int current = 0;
    int capacity = 0;
};

// Anything that spends energy on use.
struct EnergyConsumer {
    int energy_per_use = 1;
};

// Tinkering bonuses for energy items.
struct EnergyModifiers {
    int capacity_bonus = 0;        // +X to max
    int charge_rate_bonus = 0;     // +X% to incoming energy per tick
    int discharge_efficiency = 0;  // every N units transferred yields +1 free; 0 = disabled
};

// Per-slot Solar Panel state. Sits inside EnhancementSlot.
struct SolarPanelData {
    bool active = true;
    int energy_per_tick = 5;     // tier-based: 5 / 8 / 12
    int tick_interval = 2;       // game-ticks between deposits
    int accumulator = 0;         // ticks accrued since last deposit
};

// What a cell does when its proc fires.
enum class CellProcKind : uint8_t {
    None,
    ShieldOvercharge,    // +magnitude to shield current (allowed past capacity)
    WeaponOvercharge,    // +magnitude to weapon current (allowed past capacity)
    DefenseBoost,        // grants attack-boost-style buff for duration turns
    AdrenalineRush,      // grants adrenaline_rush effect for duration turns
};

// Bonus a cell fires once per `threshold` units actually drained from it.
// Stored on Item. The accumulator persists across drains (per-instance state).
struct CellProc {
    CellProcKind kind = CellProcKind::None;
    int magnitude = 0;     // overcharge units, or stat amount for buffs
    int duration = 0;      // turns (for status-effect kinds)
    int threshold = 100;   // fires once per N units drained
    int accumulator = 0;   // per-instance accumulated drain
};

inline bool is_full(const EnergyStore& s)  { return s.current >= s.capacity; }
inline bool is_empty(const EnergyStore& s) { return s.current <= 0; }
inline int  deficit(const EnergyStore& s)  { return s.capacity - s.current; }

// Transfer energy from src to dst. Drains up to `requested` units from src.
// `efficiency_bonus_per_n` from EnergyModifiers::discharge_efficiency on src
// adds +1 free to dst for every N units actually drained.
// Returns the units actually deposited into dst.
int transfer_energy(EnergyStore& src, EnergyStore& dst, int requested,
                    int efficiency_bonus_per_n = 0);

} // namespace astra
