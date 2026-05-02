#include "astra/ip.h"

#include <cstdio>
#include <string>

namespace astra {

uint32_t derive_subnet_base(uint32_t map_seed) {
    // 10.X.Y.0  where X.Y = (map_seed >> 8) & 0xFFFF, host octet zeroed.
    return 0x0A000000u | ((map_seed >> 8) & 0x00FFFF00u);
}

std::string format_ip(uint32_t ip) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%u.%u.%u.%u",
                  (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return buf;
}

std::optional<uint32_t> parse_ip(std::string_view s) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(std::string(s).c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return std::nullopt;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) return std::nullopt;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

} // namespace astra
