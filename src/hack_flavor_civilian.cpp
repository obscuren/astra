#include "astra/hack_flavor.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace astra {

// Forward — defined in hack_flavor_corp.cpp / hack_flavor_cartel.cpp.
const HackFlavorPack& corp_flavor_pack();
const HackFlavorPack& cartel_flavor_pack();

namespace {

// 8 MOTDs — friendly / mundane / careless ("password is on the fridge")
// per spec §10 Civilian.
const char* k_motd[] = {
    "hi! please dont mess with the settings",
    "wifi password is on the fridge",
    "if you find a bug pls let greta know",
    "the spare key is under the mat",
    "don't forget the back door auto-locks at sundown",
    "if it beeps just unplug it for 10 seconds",
    "this thing was working last week, who knows",
    "PLEASE clean up after yourself, thanks!",
};

// 6 log line templates — uses %TIME%, %USER%, %IP% loosely.
const char* k_logs[] = {
    "apr 12 8:14am - greta logged in (front porch cam)",
    "apr 11 6:02pm - jonas accessed config",
    "apr 11 1:18pm - greta from kitchen tablet",
    "apr 10 11:55am - autoupdate skipped (manual override)",
    "apr 10 7:30am - power cycle (scheduled)",
    "apr 09 9:47pm - guest session (unknown)",
};

// 12 user names — ordinary, neutral, lower-case.
const char* k_users[] = {
    "greta", "jonas", "milo", "anya", "rex", "sam",
    "del",   "petra", "kasper", "iva", "tomas", "ren",
};

// 10 file content templates — texture, mostly innocent, occasional secret.
const char* k_files[] = {
    "shopping list:\n  bread\n  hydroponic carrots\n  filter (size 4)",
    "todo:\n  fix the leak in the rad coil\n  call jonas\n  re-key the back door",
    "remember: backup wifi pw is the dog's birthday minus 3",
    "wifi: outpost-2.4g  pw: gretasdog2461",
    "kasper said the patch tuesday was bunk - skip the auto-update",
    "i think the camera is rebooting on its own. weird.",
    "milo's birthday on the 14th - decorations in the closet",
    "if anya asks about the package, it's in the back room",
    "note to self: never trust the cheap relays. always.",
    "ssh-keys/notes: dont share the laptop's key with anyone, ok?",
};

// 3 banner chrome variants — light box-drawing, friendly tone.
const char* k_chrome[] = {
    "----- %FACTION% / %FIXTURE_NAME% %VERSION% -----",
    "(* hi from %FACTION% *)  %FIXTURE_NAME% %VERSION%",
    ":: %FACTION% :: %FIXTURE_NAME% %VERSION% :: stay friendly ::",
};

const HackFlavorPack k_civilian{
    "Outpost",
    "greta",
    std::span<const char* const>(k_motd),
    std::span<const char* const>(k_logs),
    std::span<const char* const>(k_users),
    std::span<const char* const>(k_files),
    std::span<const char* const>(k_chrome),
};

} // namespace

namespace {
// Case-insensitive substring search.
bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return false;
    auto cmp = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    };
    return std::search(hay.begin(), hay.end(),
                       needle.begin(), needle.end(), cmp) != hay.end();
}
} // namespace

const HackFlavorPack& flavor_for(std::string_view faction) {
    // Plan 7 §9 — single-rule fallback: any unmatched faction (including
    // Faction::None, Precursor, Conclave, future) falls back to Civilian.
    // Match Corp first (more specific keywords), then Cartel, else Civilian.
    if (icontains(faction, "Cartel") ||
        icontains(faction, "Pirate") ||
        icontains(faction, "Reaver") ||
        icontains(faction, "Drift")) {
        return cartel_flavor_pack();
    }
    if (icontains(faction, "Kaguya") ||
        icontains(faction, "Helion") ||
        icontains(faction, "Corp") ||
        icontains(faction, "KHI") ||
        icontains(faction, "Federation") ||
        icontains(faction, "Terran")) {
        return corp_flavor_pack();
    }
    return k_civilian;
}

} // namespace astra
