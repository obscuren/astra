#pragma once

#include <cstdint>

namespace astra {

struct Npc;

// Bitflags describing intrinsic creature traits. Stored as uint64_t on Npc.
// Room for 64 future traits (undead, psionic, synthetic, etc.).
enum class CreatureFlag : uint64_t {
    None       = 0,
    Mechanical = 1ull << 0,
    Biological = 1ull << 1,
};

constexpr uint64_t operator|(CreatureFlag a, CreatureFlag b) {
    return static_cast<uint64_t>(a) | static_cast<uint64_t>(b);
}

constexpr uint64_t operator|(uint64_t a, CreatureFlag b) {
    return a | static_cast<uint64_t>(b);
}

inline bool has_flag(uint64_t flags, CreatureFlag f) {
    return (flags & static_cast<uint64_t>(f)) != 0;
}

bool is_mechanical(const Npc& npc);
bool is_biological(const Npc& npc);

} // namespace astra
