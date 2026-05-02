#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/tilemap.h"

#include <array>

namespace astra {

const char* race_name(Race r) {
    switch (r) {
        case Race::Human:     return "Human";
        case Race::Veldrani:  return "Veldrani";
        case Race::Kreth:     return "Kreth";
        case Race::Sylphari:  return "Sylphari";
        case Race::Xytomorph: return "Xytomorph";
        case Race::Stellari:  return "Stellari";
        case Race::Mechanical: return "Mechanical";
    }
    return "Unknown";
}

// --- Name tables per race ---

static constexpr std::array human_first = {
    "Marcus", "Elena", "Jin", "Sable", "Harlan",
    "Nessa", "Dorian", "Yara", "Cole", "Mira",
    "Oren", "Tessa", "Vik", "Lena", "Ash",
};

static constexpr std::array veldrani_first = {
    "Vel'thar", "Syndara", "Ael'wyn", "Koriel", "Thalune",
    "Nyxara", "Vel'moth", "Iridael", "Sorynn", "Zephael",
    "Quorath", "Lythane", "Orivael", "Cael'is", "Vytheran",
};

static constexpr std::array kreth_first = {
    "Gromm", "Thukka", "Brekk", "Torga", "Drukk",
    "Yagga", "Krell", "Magda", "Borul", "Skarn",
    "Hekka", "Zugga", "Fenn", "Borg", "Rikka",
};

static constexpr std::array sylphari_first = {
    "Aelith", "Lumyn", "Whisp", "Faelora", "Cirrus",
    "Ethyn", "Myst", "Solara", "Nimbus", "Zephyra",
    "Gleam", "Vellyn", "Aura", "Drift", "Haze",
};

static constexpr std::array xytomorph_first = {
    "Skrix", "Chitter", "Vex", "Gnash", "Thrax",
    "Klik", "Razz", "Zzik", "Morph", "Scythe",
};

template <std::size_t N>
static const char* pick(const std::array<const char*, N>& arr, std::mt19937& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);
    return arr[dist(rng)];
}

std::string generate_name(Race race, std::mt19937& rng) {
    switch (race) {
        case Race::Human:     return pick(human_first, rng);
        case Race::Veldrani:  return pick(veldrani_first, rng);
        case Race::Kreth:     return pick(kreth_first, rng);
        case Race::Sylphari:  return pick(sylphari_first, rng);
        case Race::Xytomorph: return pick(xytomorph_first, rng);
        case Race::Stellari:  return "Nova";
        case Race::Mechanical: return "Unit";  // drones/automatons don't have personal names
    }
    return "Unknown";
}

// --- Level scaling ---

void Npc::scale_to_level(int lvl, bool is_elite) {
    level = lvl;
    elite = is_elite;
    hp = hp * level;
    max_hp = hp;
    dv += (level - 1);
    av += (level - 1) / 2;
    if (elite) {
        hp *= 2;
        max_hp *= 2;
        quickness = quickness * 3 / 2;
        dv += 2;
        av += 1;
    }
}

// --- Display name ---

std::string Npc::label() const {
    std::string prefix = elite ? "Elite " : "";
    if (name.empty()) {
        return prefix + role;
    }
    if (role.empty()) {
        return prefix + name;
    }
    if (name == role) {
        // Machines / unnamed units set both fields to the model string.
        // Drop the "the" interstitial — otherwise the label reads
        // "Sentry Drone the Sentry Drone".
        return prefix + role;
    }
    return prefix + name + " the " + role;
}

// --- Factory dispatcher ---

namespace {

// Plan 5 Task 13 — attach a per-NPC cybernetic implant `Hackable`.
// Mechanical NPCs (drones, sentinels, automatons) are inherently electronic
// and mobile; the spec §14 calls for `Electronic | Mobile` tags, plus
// `Weaponized` for hostile combat platforms so `friendly_fire.qh` (filter:
// Weaponized | Mobile) can target them.
//
// We override the default Console-derived tags from `make_hackable` because
// Console's JackInPort+DataStore mask is wrong for an NPC implant — implants
// are bypass / disable / reroute targets, not jack-in entry points.
//
// All current mechanical NPCs are weapon platforms (Sentry Drones, Archon
// Sentinels, Rust Hounds, etc.) so we apply Weaponized to all of them.
// A future faction-cybernetic biological NPC might NOT be Weaponized — at
// that point a per-NpcRole tag pass via `make_implant_hackable()` will
// pick out the non-weaponised cases.
void maybe_attach_implant(Npc& npc) {
    if (!has_flag(npc.flags, CreatureFlag::Mechanical)) return;
    Hackable h = make_hackable(FixtureType::Console, /*tier*/ 1);
    h.tags = static_cast<HackTagMask>(HackTag::Electronic)
           | static_cast<HackTagMask>(HackTag::Mobile)
           | static_cast<HackTagMask>(HackTag::Weaponized);
    npc.cyber = std::move(h);
}

} // namespace

Npc create_npc(NpcRole npc_role, Race race, std::mt19937& rng) {
    Npc npc;
    switch (npc_role) {
        case NpcRole::StationKeeper: npc = build_station_keeper(race, rng); break;
        case NpcRole::Merchant:      npc = build_merchant(race, rng); break;
        case NpcRole::Drifter:       npc = build_drifter(race, rng); break;
        case NpcRole::Xytomorph:     npc = build_xytomorph(rng); break;
        case NpcRole::FoodMerchant:  npc = build_food_merchant(race, rng); break;
        case NpcRole::Medic:         npc = build_medic(race, rng); break;
        case NpcRole::Commander:     npc = build_commander(race, rng); break;
        case NpcRole::ArmsDealer:    npc = build_arms_dealer(race, rng); break;
        case NpcRole::Astronomer:    npc = build_astronomer(race, rng); break;
        case NpcRole::Engineer:      npc = build_engineer(race, rng); break;
        case NpcRole::Nova:          npc = build_nova(); break;
        case NpcRole::Civilian:      npc = build_civilian(race, rng); break;
        case NpcRole::Scavenger:     npc = build_scavenger(race, rng); break;
        case NpcRole::Prospector:    npc = build_prospector(race, rng); break;
        case NpcRole::ArchonRemnant: npc = build_archon_remnant(rng); break;
        case NpcRole::VoidReaver:    npc = build_void_reaver(rng); break;
        case NpcRole::ArchonSentinel: npc = build_archon_sentinel(rng); break;
        case NpcRole::ConclaveSentry: npc = build_conclave_sentry(rng); break;
        case NpcRole::HeavyConclaveSentry: npc = build_heavy_conclave_sentry(rng); break;
        case NpcRole::RustHound:       npc = build_rust_hound(rng); break;
        case NpcRole::SentryDrone:     npc = build_sentry_drone(rng); break;
        case NpcRole::ArchonAutomaton: npc = build_archon_automaton(rng); break;
        case NpcRole::ConclaveSentryDrone: npc = build_conclave_sentry_drone(rng); break;
        case NpcRole::ArchonSentryDrone:   npc = build_archon_sentry_drone(rng); break;
    }
    maybe_attach_implant(npc);
    return npc;
}

Npc create_npc_by_role(const std::string& role_name, std::mt19937& rng) {
    if (role_name == "Xytomorph")        return create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
    if (role_name == "Young Xytomorph")  return create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
    if (role_name == "Station Keeper")   return create_npc(NpcRole::StationKeeper, Race::Human, rng);
    if (role_name == "Merchant")         return create_npc(NpcRole::Merchant, Race::Human, rng);
    if (role_name == "Drifter")          return create_npc(NpcRole::Drifter, Race::Human, rng);
    if (role_name == "Archon Remnant")   return create_npc(NpcRole::ArchonRemnant, Race::Mechanical, rng);
    if (role_name == "Void Reaver")      return create_npc(NpcRole::VoidReaver, Race::Human, rng);
    if (role_name == "Archon Sentinel")  return create_npc(NpcRole::ArchonSentinel, Race::Mechanical, rng);
    if (role_name == "Conclave Sentry")  return create_npc(NpcRole::ConclaveSentry, Race::Stellari, rng);
    if (role_name == "Heavy Conclave Sentry") return create_npc(NpcRole::HeavyConclaveSentry, Race::Stellari, rng);
    if (role_name == "Rust Hound")       return create_npc(NpcRole::RustHound, Race::Mechanical, rng);
    if (role_name == "Sentry Drone")     return create_npc(NpcRole::SentryDrone, Race::Mechanical, rng);
    if (role_name == "Conclave Sentry Drone") return create_npc(NpcRole::ConclaveSentryDrone, Race::Mechanical, rng);
    if (role_name == "Archon Sentry Drone")   return create_npc(NpcRole::ArchonSentryDrone, Race::Mechanical, rng);
    if (role_name == "Archon Automaton") return create_npc(NpcRole::ArchonAutomaton, Race::Mechanical, rng);
    // Fallback: hostile xytomorph
    return create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
}

bool is_mechanical(const Npc& npc) {
    return has_flag(npc.flags, CreatureFlag::Mechanical);
}

bool is_biological(const Npc& npc) {
    return has_flag(npc.flags, CreatureFlag::Biological);
}

} // namespace astra
