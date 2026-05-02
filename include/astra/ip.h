#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace astra {

// Packed 10.X.Y.0/24 derived deterministically from a map seed.
// Used by LAN auto-registration to allocate per-Hackable IPs.
uint32_t derive_subnet_base(uint32_t map_seed);

// Compose 10.X.Y.host from base + host octet (1..253 valid; 0/255 reserved
// network/broadcast; 254 reserved for the deep-Grid gateway).
inline uint32_t pack_ip(uint32_t base, uint8_t host) {
    return (base & 0xFFFFFF00u) | host;
}

// "10.42.7.5"
std::string format_ip(uint32_t ip);

// Parse "10.X.Y.Z" into packed; nullopt on malformed input.
std::optional<uint32_t> parse_ip(std::string_view s);

} // namespace astra
