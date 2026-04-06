#include "astra/name_generator.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

namespace astra {

// ── Syllable pools ──────────────────────────────────────────────────────────

static constexpr std::array<const char*, 40> sharp_syllables = {{
    "Keth", "Vor",  "Thex", "Zan",  "Cyr",  "Qal",  "Drex", "Kryn",
    "Vex",  "Zar",  "Thra", "Kol",  "Xen",  "Rax",  "Vyr",  "Dral",
    "Kren", "Zyx",  "Phar", "Qex",  "Thul", "Vrex", "Kaz",  "Xar",
    "Dren", "Zek",  "Krix", "Val",  "Tark", "Qyn",  "Vryn", "Skex",
    "Khar", "Zul",  "Thran","Vok",  "Dryn", "Krex", "Xyl",  "Zorn",
}};

static constexpr std::array<const char*, 40> flowing_syllables = {{
    "Ael",  "Vyn",  "Osa",  "Leri", "Ithae","Myr",  "Aelu", "Elyn",
    "Syl",  "Ova",  "Luin", "Aeri", "Ytha", "Nael", "Orin", "Sae",
    "Elu",  "Mira", "Yen",  "Alae", "Riel", "Vin",  "Lira", "Thae",
    "Ume",  "Sira", "Ylen", "Orae", "Nyl",  "Aleu", "Vael", "Ira",
    "Lune", "Ysae", "Mae",  "Elan", "Oly",  "Reva", "Nyra", "Sael",
}};

static constexpr std::array<const char*, 40> guttural_syllables = {{
    "Groth","Durr", "Khar", "Mog",  "Zhul", "Brek", "Grum", "Drak",
    "Krug", "Vorg", "Throk","Gurm", "Braz", "Drog", "Khur", "Murg",
    "Zog",  "Grak", "Brunt","Durg", "Krom", "Vrug", "Ghaz", "Morr",
    "Thruk","Ghorr","Brug", "Drem", "Khol", "Zugg", "Grath","Bror",
    "Dukk", "Mhur", "Zrog", "Grun", "Brakk","Drakh","Khur", "Vurm",
}};

static constexpr std::array<const char*, 40> ethereal_syllables = {{
    "Phi",  "Sei",  "Lua",  "Wen",  "Tia",  "Nev",  "Zhi",  "Rei",
    "Mua",  "Yei",  "Fia",  "Lae",  "Shi",  "Nua",  "Wei",  "Zia",
    "Hei",  "Rae",  "Lui",  "Tei",  "Nia",  "Fei",  "Yua",  "Mei",
    "Sae",  "Lii",  "Wua",  "Rhi",  "Dei",  "Pua",  "Kei",  "Nai",
    "Thei", "Xia",  "Yue",  "Wai",  "Sui",  "Lai",  "Hue",  "Ria",
}};

static constexpr std::array<const char*, 40> harmonic_syllables = {{
    "Zyn",  "Mael", "Thal", "Orn",  "Vel",  "Ryn",  "Kael", "Shal",
    "Dur",  "Ven",  "Tael", "Nym",  "Lor",  "Kyn",  "Maer", "Ral",
    "Thel", "Syn",  "Vael", "Kor",  "Dael", "Ren",  "Tyn",  "Mal",
    "Sel",  "Kal",  "Rael", "Thyn", "Vol",  "Nyr",  "Dyn",  "Mael",
    "Syr",  "Vel",  "Tor",  "Lyn",  "Kael", "Ral",  "Ven",  "Thal",
}};

static constexpr std::array<const char*, 40> staccato_syllables = {{
    "Kix",  "Tuk",  "Prel", "Chak", "Rik",  "Tok",  "Drik", "Klak",
    "Bix",  "Tep",  "Krek", "Pik",  "Chit", "Rak",  "Duk",  "Klik",
    "Prak", "Tik",  "Brek", "Chek", "Rix",  "Trak", "Kip",  "Dek",
    "Plik", "Chuk", "Brik", "Tak",  "Krek", "Drik", "Pek",  "Trix",
    "Chik", "Bak",  "Rek",  "Klip", "Dix",  "Prak", "Tok",  "Krit",
}};

// ── Polity suffixes ─────────────────────────────────────────────────────────

static constexpr std::array<const char*, 6> sharp_polities = {{
    "Dominion", "Collective", "Accord", "Lattice", "Matrix", "Nexus",
}};

static constexpr std::array<const char*, 6> flowing_polities = {{
    "Convergence", "Ascendancy", "Communion", "Lumen", "Eternity", "Resonance",
}};

static constexpr std::array<const char*, 6> guttural_polities = {{
    "Horde", "Dominion", "Throng", "Maw", "Forge", "Bastion",
}};

static constexpr std::array<const char*, 6> ethereal_polities = {{
    "Chorus", "Whisper", "Drift", "Veil", "Shimmer", "Radiance",
}};

static constexpr std::array<const char*, 6> harmonic_polities = {{
    "Harmony", "Symphony", "Resonance", "Accord", "Chorus", "Cadence",
}};

static constexpr std::array<const char*, 6> staccato_polities = {{
    "Swarm", "Cluster", "Hive", "Pack", "Network", "Array",
}};

// ── Place suffixes ──────────────────────────────────────────────────────────

static constexpr std::array<const char*, 5> place_suffixes = {{
    " Prime", " Major", " Reach", " Deep", " Spire",
}};

// ── Artifact type names by category ─────────────────────────────────────────

static constexpr std::array<const char*, 5> weapon_types = {{
    "Blade", "Lance", "Cannon", "Edge", "Fang",
}};

static constexpr std::array<const char*, 5> navigation_types = {{
    "Compass", "Sextant", "Chart", "Lens", "Beacon",
}};

static constexpr std::array<const char*, 5> knowledge_types = {{
    "Codex", "Archive", "Tome", "Chronicle", "Index",
}};

static constexpr std::array<const char*, 5> key_types = {{
    "Key", "Cipher", "Seal", "Glyph", "Sigil",
}};

static constexpr std::array<const char*, 5> anomaly_types = {{
    "Paradox", "Singularity", "Rift", "Echo", "Anomaly",
}};

// ── Title options by archetype ──────────────────────────────────────────────

static constexpr std::array<const char*, 5> founder_titles = {{
    "the Founder", "the First", "the Progenitor", "the Architect", "the Origin",
}};

static constexpr std::array<const char*, 5> conqueror_titles = {{
    "the Conqueror", "the Unyielding", "the Scourge", "the Iron Will", "the Warlord",
}};

static constexpr std::array<const char*, 5> sage_titles = {{
    "the Wise", "the Seer", "the Oracle", "the Illuminated", "the Keeper",
}};

static constexpr std::array<const char*, 5> traitor_titles = {{
    "the Betrayer", "the Fallen", "the Oathbreaker", "the Hollow", "the Deceiver",
}};

static constexpr std::array<const char*, 5> explorer_titles = {{
    "the Wayfinder", "the Voyager", "the Wanderer", "the Pathfinder", "the Pioneer",
}};

static constexpr std::array<const char*, 5> last_titles = {{
    "the Last", "the Final", "the Remnant", "the Enduring", "the Twilight",
}};

static constexpr std::array<const char*, 5> builder_titles = {{
    "the Builder", "the Forgemaster", "the Shaper", "the Artificer", "the Maker",
}};

// ── Helpers ─────────────────────────────────────────────────────────────────

static std::span<const char* const> syllables_for(PhonemePool pool) {
    switch (pool) {
    case PhonemePool::Sharp:    return sharp_syllables;
    case PhonemePool::Flowing:  return flowing_syllables;
    case PhonemePool::Guttural: return guttural_syllables;
    case PhonemePool::Ethereal: return ethereal_syllables;
    case PhonemePool::Harmonic: return harmonic_syllables;
    case PhonemePool::Staccato: return staccato_syllables;
    }
    return sharp_syllables;
}

static std::span<const char* const> polities_for(PhonemePool pool) {
    switch (pool) {
    case PhonemePool::Sharp:    return sharp_polities;
    case PhonemePool::Flowing:  return flowing_polities;
    case PhonemePool::Guttural: return guttural_polities;
    case PhonemePool::Ethereal: return ethereal_polities;
    case PhonemePool::Harmonic: return harmonic_polities;
    case PhonemePool::Staccato: return staccato_polities;
    }
    return sharp_polities;
}

static std::span<const char* const> artifact_types_for(ArtifactCategory cat) {
    switch (cat) {
    case ArtifactCategory::Weapon:         return weapon_types;
    case ArtifactCategory::NavigationTool: return navigation_types;
    case ArtifactCategory::KnowledgeStore: return knowledge_types;
    case ArtifactCategory::Key:            return key_types;
    case ArtifactCategory::Anomaly:        return anomaly_types;
    }
    return anomaly_types;
}

static std::span<const char* const> titles_for(FigureArchetype arch) {
    switch (arch) {
    case FigureArchetype::Founder:   return founder_titles;
    case FigureArchetype::Conqueror: return conqueror_titles;
    case FigureArchetype::Sage:      return sage_titles;
    case FigureArchetype::Traitor:   return traitor_titles;
    case FigureArchetype::Explorer:  return explorer_titles;
    case FigureArchetype::Last:      return last_titles;
    case FigureArchetype::Builder:   return builder_titles;
    }
    return founder_titles;
}

/// Build a word from `count` random syllables, capitalize first letter, lowercase rest.
static std::string compose_word(std::mt19937& rng, std::span<const char* const> syls,
                                int count) {
    std::string result;
    std::uniform_int_distribution<size_t> dist(0, syls.size() - 1);
    for (int i = 0; i < count; ++i) {
        result += syls[dist(rng)];
    }
    // Capitalize first, lowercase rest.
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        for (size_t i = 1; i < result.size(); ++i) {
            result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
        }
    }
    return result;
}

// ── NameGenerator ───────────────────────────────────────────────────────────

NameGenerator::NameGenerator(PhonemePool pool) : pool_(pool) {}

std::string NameGenerator::name(std::mt19937& rng) const {
    auto syls = syllables_for(pool_);
    std::uniform_int_distribution<int> len_dist(1, 3);
    return compose_word(rng, syls, len_dist(rng));
}

std::string NameGenerator::place(std::mt19937& rng) const {
    auto syls = syllables_for(pool_);
    std::uniform_int_distribution<int> len_dist(2, 3);
    std::string result = compose_word(rng, syls, len_dist(rng));

    // 30% chance of a place suffix.
    std::uniform_int_distribution<int> suffix_chance(0, 9);
    if (suffix_chance(rng) < 3) {
        std::uniform_int_distribution<size_t> suffix_dist(0, place_suffixes.size() - 1);
        result += place_suffixes[suffix_dist(rng)];
    }
    return result;
}

std::string NameGenerator::civilization(std::mt19937& rng) const {
    auto syls = syllables_for(pool_);
    auto pols = polities_for(pool_);

    std::uniform_int_distribution<int> len_dist(2, 3);
    std::string civ_name = compose_word(rng, syls, len_dist(rng));

    std::uniform_int_distribution<size_t> pol_dist(0, pols.size() - 1);
    return "The " + civ_name + " " + pols[pol_dist(rng)];
}

std::string NameGenerator::artifact(std::mt19937& rng, ArtifactCategory category) const {
    auto syls = syllables_for(pool_);
    auto types = artifact_types_for(category);

    std::uniform_int_distribution<int> len_dist(1, 2);
    std::string art_name = compose_word(rng, syls, len_dist(rng));

    std::uniform_int_distribution<size_t> type_dist(0, types.size() - 1);
    std::string type_name = types[type_dist(rng)];

    // 50/50: "The [Name] [Type]" vs "[Type] of [Name]"
    std::uniform_int_distribution<int> form(0, 1);
    if (form(rng) == 0) {
        return "The " + art_name + " " + type_name;
    } else {
        return std::string(type_name) + " of " + art_name;
    }
}

std::string NameGenerator::title(std::mt19937& rng, FigureArchetype archetype) const {
    auto opts = titles_for(archetype);
    std::uniform_int_distribution<size_t> dist(0, opts.size() - 1);
    return std::string(opts[dist(rng)]);
}

} // namespace astra
