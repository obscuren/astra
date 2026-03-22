#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace astra {

enum class TimePhase : uint8_t {
    Dawn,
    Day,
    Dusk,
    Night,
};

// Global calendar constants
constexpr int ticks_per_global_day = 200;
constexpr int days_per_cycle = 30;

// Phase boundaries as percentage of day_length
constexpr float dawn_start  = 0.05f;  //  5%
constexpr float day_start   = 0.15f;  // 15%
constexpr float dusk_start  = 0.65f;  // 65%
constexpr float night_start = 0.85f;  // 85%
// Night wraps: 85-100% + 0-5%

struct DayClock {
    int local_ticks_per_day = 200;
    int local_tick = 0;

    void advance(int ticks) {
        local_tick = (local_tick + ticks) % local_ticks_per_day;
    }

    void set_body_day_length(int len) {
        if (len <= 0) len = 200;
        // Preserve proportional position in the day
        float ratio = static_cast<float>(local_tick) / local_ticks_per_day;
        local_ticks_per_day = len;
        local_tick = static_cast<int>(ratio * len) % len;
    }

    float day_fraction() const {
        return static_cast<float>(local_tick) / local_ticks_per_day;
    }

    TimePhase phase() const {
        float f = day_fraction();
        if (f < dawn_start)  return TimePhase::Night;  // 0-5% is still night
        if (f < day_start)   return TimePhase::Dawn;
        if (f < dusk_start)  return TimePhase::Day;
        if (f < night_start) return TimePhase::Dusk;
        return TimePhase::Night;
    }

    float phase_progress() const {
        float f = day_fraction();
        switch (phase()) {
            case TimePhase::Dawn:
                return (f - dawn_start) / (day_start - dawn_start);
            case TimePhase::Day:
                return (f - day_start) / (dusk_start - day_start);
            case TimePhase::Dusk:
                return (f - dusk_start) / (night_start - dusk_start);
            case TimePhase::Night: {
                // Night spans night_start..1.0 + 0..dawn_start
                float night_len = (1.0f - night_start) + dawn_start;
                float pos = (f >= night_start) ? (f - night_start)
                                               : (1.0f - night_start + f);
                return pos / night_len;
            }
        }
        return 0.0f;
    }

    // max_radius = full map visibility (e.g. max(width,height))
    // light_radius = player's personal light source
    int effective_view_radius(int max_radius, int light_radius) const {
        switch (phase()) {
            case TimePhase::Day:
                return max_radius;
            case TimePhase::Night:
                return light_radius;
            case TimePhase::Dawn: {
                float t = phase_progress();
                return light_radius + static_cast<int>(
                    std::round(t * (max_radius - light_radius)));
            }
            case TimePhase::Dusk: {
                float t = phase_progress();
                return max_radius - static_cast<int>(
                    std::round(t * (max_radius - light_radius)));
            }
        }
        return max_radius;
    }

    int ticks_until_dawn() const {
        float f = day_fraction();
        float target = dawn_start;
        float dist = (f < target) ? (target - f) : (1.0f - f + target);
        return std::max(1, static_cast<int>(dist * local_ticks_per_day));
    }

};

// Global calendar helpers — computed from world_tick
inline int global_day(int world_tick) {
    return (world_tick / ticks_per_global_day) + 1;
}

inline int global_cycle(int world_tick) {
    return ((global_day(world_tick) - 1) / days_per_cycle) + 1;
}

inline int day_in_cycle(int world_tick) {
    return ((global_day(world_tick) - 1) % days_per_cycle) + 1;
}

inline std::string format_calendar(int world_tick) {
    return "C" + std::to_string(global_cycle(world_tick))
         + " D" + std::to_string(day_in_cycle(world_tick));
}

// Phase display helpers
inline const char* phase_icon(TimePhase p) {
    switch (p) {
        case TimePhase::Dawn: return "\xe2\x97\x90"; // ◐
        case TimePhase::Day:  return "\xe2\x98\x80"; // ☀
        case TimePhase::Dusk: return "\xe2\x97\x91"; // ◑
        case TimePhase::Night:return "\xe2\x98\xbd"; // ☽
    }
    return "?";
}

inline const char* phase_name(TimePhase p) {
    switch (p) {
        case TimePhase::Dawn: return "Dawn";
        case TimePhase::Day:  return "Day";
        case TimePhase::Dusk: return "Dusk";
        case TimePhase::Night:return "Night";
    }
    return "?";
}

// Day length derivation from celestial body properties
inline int derive_day_length(int body_type, int size, float /*orbital_distance*/) {
    // body_type maps to BodyType enum values
    // 0=Rocky, 1=GasGiant, 2=IceGiant, 3=Terrestrial, 4=DwarfPlanet, 5=AsteroidBelt
    int base = 200;
    switch (body_type) {
        case 0: base = 150; break; // Rocky — short days
        case 1: base = 280; break; // GasGiant — longer
        case 2: base = 260; break; // IceGiant
        case 3: base = 200; break; // Terrestrial — moderate
        case 4: base = 180; break; // DwarfPlanet
        case 5: base = 160; break; // AsteroidBelt
    }
    // Scale by size (1-10): larger = slightly longer
    base += size * 8;
    // Clamp
    if (base < 100) base = 100;
    if (base > 400) base = 400;
    return base;
}

// Moon day length — tidally locked or very long
inline int derive_moon_day_length(int parent_type) {
    (void)parent_type;
    return 400;
}

} // namespace astra
