#include "astra/npc_defs.h"
#include "astra/creature_flags.h"

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
    return prefix + name + " the " + role;
}

// --- Factory dispatcher ---

Npc create_npc(NpcRole npc_role, Race race, std::mt19937& rng) {
    switch (npc_role) {
        case NpcRole::StationKeeper: return build_station_keeper(race, rng);
        case NpcRole::Merchant:      return build_merchant(race, rng);
        case NpcRole::Drifter:       return build_drifter(race, rng);
        case NpcRole::Xytomorph:     return build_xytomorph(rng);
        case NpcRole::FoodMerchant:  return build_food_merchant(race, rng);
        case NpcRole::Medic:         return build_medic(race, rng);
        case NpcRole::Commander:     return build_commander(race, rng);
        case NpcRole::ArmsDealer:    return build_arms_dealer(race, rng);
        case NpcRole::Astronomer:    return build_astronomer(race, rng);
        case NpcRole::Engineer:      return build_engineer(race, rng);
        case NpcRole::Nova:          return build_nova();
        case NpcRole::Civilian:      return build_civilian(race, rng);
        case NpcRole::Scavenger:     return build_scavenger(race, rng);
        case NpcRole::Prospector:    return build_prospector(race, rng);
        case NpcRole::ArchonRemnant: return build_archon_remnant(rng);
        case NpcRole::VoidReaver:    return build_void_reaver(rng);
        case NpcRole::ArchonSentinel: return build_archon_sentinel(rng);
        case NpcRole::ConclaveSentry: return build_conclave_sentry(rng);
        case NpcRole::HeavyConclaveSentry: return build_heavy_conclave_sentry(rng);
        case NpcRole::RustHound:       return build_rust_hound(rng);
        case NpcRole::SentryDrone:     return build_sentry_drone(rng);
        case NpcRole::ArchonAutomaton: return build_archon_automaton(rng);
    }
    return {};
}

Npc create_npc_by_role(const std::string& role_name, std::mt19937& rng) {
    if (role_name == "Xytomorph")        return create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
    if (role_name == "Young Xytomorph")  return create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
    if (role_name == "Station Keeper")   return create_npc(NpcRole::StationKeeper, Race::Human, rng);
    if (role_name == "Merchant")         return create_npc(NpcRole::Merchant, Race::Human, rng);
    if (role_name == "Drifter")          return create_npc(NpcRole::Drifter, Race::Human, rng);
    if (role_name == "Archon Remnant")   return create_npc(NpcRole::ArchonRemnant, Race::Human, rng);
    if (role_name == "Void Reaver")      return create_npc(NpcRole::VoidReaver, Race::Human, rng);
    if (role_name == "Archon Sentinel")  return create_npc(NpcRole::ArchonSentinel, Race::Human, rng);
    if (role_name == "Conclave Sentry")  return create_npc(NpcRole::ConclaveSentry, Race::Stellari, rng);
    if (role_name == "Heavy Conclave Sentry") return create_npc(NpcRole::HeavyConclaveSentry, Race::Stellari, rng);
    if (role_name == "Rust Hound")       return create_npc(NpcRole::RustHound, Race::Human, rng);
    if (role_name == "Sentry Drone")     return create_npc(NpcRole::SentryDrone, Race::Human, rng);
    if (role_name == "Archon Automaton") return create_npc(NpcRole::ArchonAutomaton, Race::Human, rng);
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
