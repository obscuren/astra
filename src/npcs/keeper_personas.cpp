#include "astra/keeper_personas.h"
#include <array>

namespace astra {

namespace {

const std::array<const char*, 16> FIRST_NAMES = {
    "Alva","Brix","Cass","Doran","Elin","Ferro","Gale","Haru",
    "Iska","Jonn","Koda","Lira","Mox","Nessa","Orin","Pell",
};
const std::array<const char*, 16> LAST_NAMES = {
    "Vance","Okoro","Sato","Reyes","Mogg","Ilyin","Hale","Crane",
    "Drex","Kato","Starr","Vega","Quill","Nash","Orlov","Park",
};
const std::array<const char*, 8> SCAV_NAMES = {
    "Rust","Chum","Wire","Bolts","Pike","Hex","Kettle","Crank",
};
const std::array<const char*, 8> PIRATE_NAMES = {
    "Blackjaw","Red Nils","Captain Vex","Ironsight","Ash","Kestrel","Marrow","Scar",
};

} // namespace

std::string pick_keeper_name(uint64_t seed) {
    auto a = FIRST_NAMES[(seed >> 4) % FIRST_NAMES.size()];
    auto b = LAST_NAMES[(seed >> 20) % LAST_NAMES.size()];
    return std::string(a) + " " + b;
}

std::string pick_scav_keeper_name(uint64_t seed) {
    return SCAV_NAMES[(seed >> 8) % SCAV_NAMES.size()];
}

std::string pick_pirate_captain_name(uint64_t seed) {
    return PIRATE_NAMES[(seed >> 12) % PIRATE_NAMES.size()];
}

KeeperArchetype pick_keeper_archetype(uint64_t seed) {
    return (KeeperArchetype)((seed >> 28) % 6);
}

const char* archetype_voice(KeeperArchetype a) {
    switch (a) {
        case KeeperArchetype::GruffVeteran:     return "gruff, terse, military cadence";
        case KeeperArchetype::ChattyBureaucrat: return "polite, over-explains forms";
        case KeeperArchetype::NervousNewcomer:  return "nervous, hedges every sentence";
        case KeeperArchetype::RetiredSpacer:    return "rambling, stories about the old days";
        case KeeperArchetype::CorporateStiff:   return "formal, scripted, brand-forward";
        case KeeperArchetype::EccentricLoner:   return "cryptic, odd non-sequiturs";
    }
    return "";
}

std::string keeper_specialty_hook(const StationContext& ctx) {
    switch (ctx.specialty) {
        case StationSpecialty::Mining:
            return "A shipment of ore went missing on the belt run last cycle. Worth looking into.";
        case StationSpecialty::Research:
            return "We lost a data core when the relay burned out. If you're handy, it's still down there.";
        case StationSpecialty::Frontier:
            return "A patrol didn't come back. If you head out that way, keep your eyes open.";
        case StationSpecialty::Trade:
            return "An overdue convoy's got everyone nervous — dock fees piling up.";
        case StationSpecialty::Industrial:
            return "The long-range relay's been flaking. If you know parts, there's pay in it.";
        default:
            return "Nothing urgent. Enjoy the station.";
    }
}

}  // namespace astra
