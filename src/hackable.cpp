#include "astra/hackable.h"
#include "astra/tilemap.h"

#include <cstring>

namespace astra {

// Static base tags per FixtureType. Variant-aware overrides (e.g. PrecursorConsole adding
// AlienTech, electric-only Door variants, electric-only Torch variants) land in Task 11
// of Plan 5 alongside the map-gen content pass. See spec §14.
HackTagMask tags_for_fixture(FixtureType type) {
    using H = HackTag;
    switch (type) {
        // JackInPort + DataStore terminals
        case FixtureType::Console:
        case FixtureType::CommandTerminal:
        case FixtureType::ShipTerminal:
        case FixtureType::DataTerminal:
        case FixtureType::StarChart:
        case FixtureType::StarChartL:
        case FixtureType::StarChartR:
            return H::Electronic | H::DataStore | H::JackInPort;

        // Locked electronic surfaces.
        // Doors are NOT auto-tagged: most doors in the world are wood /
        // structural, and a generic Electronic|Locked sweep would surface them
        // in nmap and quickhack menus when they shouldn't be hackable. Once
        // Plan 6/7 introduces explicit BlastDoor / AirlockDoor variants those
        // get electrical tags here. Wood Gate stays untagged for the same
        // reason. See spec §14.
        case FixtureType::Gate:    return H::Electronic | H::Locked;

        // Power / lighting.
        // Torch is NOT auto-tagged: in most contexts it's wall-mounted FIRE,
        // not electrical. An electric Lamp/HoloLight variant may be added
        // later but Torch's default semantics aren't a power node.
        case FixtureType::Conduit:    return H::Electronic | H::PowerNode;
        case FixtureType::Lamp:       return H::Electronic | H::PowerNode;
        case FixtureType::HoloLight:  return H::Electronic | H::PowerNode;

        // Commerce + stash
        case FixtureType::HealPod:       return H::Electronic | H::DataStore;
        case FixtureType::FoodTerminal:  return H::Electronic | H::DataStore;
        case FixtureType::WeaponDisplay: return H::Electronic | H::DataStore;
        // Single-tag returns need an explicit cast — there's no implicit HackTag→HackTagMask
        // conversion (only the operator| overloads bridge the types).
        case FixtureType::RepairBench:   return static_cast<HackTagMask>(H::Electronic);
        case FixtureType::SupplyLocker:  return H::Electronic | H::Locked | H::DataStore;
        case FixtureType::Locker:        return H::Electronic | H::Locked | H::DataStore;
        case FixtureType::RestPod:       return static_cast<HackTagMask>(H::Electronic);

        // Everything else: not hackable.
        default: return 0;
    }
}

const char* tag_summary(HackTagMask tags) {
    using H = HackTag;
    if (has_tag(tags, H::Weaponized) && has_tag(tags, H::HasOptics)) return "turret";
    if (has_tag(tags, H::HasOptics))                                 return "camera";
    if (has_tag(tags, H::Locked))                                    return "lock";
    if (has_tag(tags, H::PowerNode))                                 return "power";
    if (has_tag(tags, H::AlienTech))                                 return "alien";
    if (has_tag(tags, H::DataStore))                                 return "data";
    return "device";
}

const char* tag_set_describe(TagSet required) {
    using H = HackTag;
    // Static buffer rotation so callers can use up to 4 results in one log line
    // without overlapping. Output forms like "HasOptics", "Weaponized+Mobile",
    // "Locked+Electronic". Order: most distinctive tag first.
    static char bufs[4][96];
    static int  next = 0;
    char* out = bufs[next++ & 3];
    out[0] = '\0';
    auto append = [&](const char* name) {
        if (out[0] != '\0') {
            std::strncat(out, "+", sizeof(bufs[0]) - std::strlen(out) - 1);
        }
        std::strncat(out, name, sizeof(bufs[0]) - std::strlen(out) - 1);
    };
    if (has_tag(required, H::Weaponized)) append("Weaponized");
    if (has_tag(required, H::HasOptics))  append("HasOptics");
    if (has_tag(required, H::Mobile))     append("Mobile");
    if (has_tag(required, H::Locked))     append("Locked");
    if (has_tag(required, H::PowerNode))  append("PowerNode");
    if (has_tag(required, H::DataStore))  append("DataStore");
    if (has_tag(required, H::AlienTech))  append("AlienTech");
    if (has_tag(required, H::JackInPort)) append("JackInPort");
    if (has_tag(required, H::Electronic)) append("Electronic");
    if (out[0] == '\0') std::strncpy(out, "Any", sizeof(bufs[0]) - 1);
    return out;
}

Hackable make_hackable(FixtureType type, int tier) {
    Hackable h;
    h.tags = tags_for_fixture(type);
    h.security_tier = tier;
    h.source_type = type;
    return h;
}

} // namespace astra
