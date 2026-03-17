#include "astra/npc_defs.h"

#include <array>

namespace astra {

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
    if (elite) {
        hp *= 2;
        max_hp *= 2;
        quickness = quickness * 3 / 2;
    }
}

// --- Display name ---

std::string Npc::display_name() const {
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
    }
    return {};
}

} // namespace astra
