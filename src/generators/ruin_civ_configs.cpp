#include "astra/ruin_types.h"

namespace astra {

CivConfig civ_config_monolithic() {
    CivConfig c;
    c.name = "Monolithic";
    c.civ_index = CIV_MONOLITHIC;
    c.wall_glyphs = {"\xe2\x96\x88", "\xe2\x96\x93", "\xe2\x96\x91"};
    c.accent_glyphs = {"\xe2\x96\xaa", "\xe2\x96\xa0"};
    c.color_primary = 250;    // near-white
    c.color_secondary = 245;  // light gray
    c.color_tint = 245;
    c.wall_thickness_bias = 1.3f;
    c.split_regularity = 0.3f;  // somewhat organic
    c.preferred_rooms = {
        BuildingType::GreatHall, BuildingType::GreatHall,
        BuildingType::Temple, BuildingType::Vault,
    };
    c.architecture = Architecture::Geometric;
    return c;
}

CivConfig civ_config_baroque() {
    CivConfig c;
    c.name = "Baroque";
    c.civ_index = CIV_BAROQUE;
    c.wall_glyphs = {"\xe2\x95\x94", "\xe2\x95\x90", "\xe2\x95\x97", "\xe2\x95\x91", "\xe2\x95\xac", "\xe2\x95\xa0", "\xe2\x95\xa3"};
    c.accent_glyphs = {"\xe2\x97\x86", "\xe2\x97\x87"};
    c.color_primary = 178;    // warm gold
    c.color_secondary = 124;  // deep red
    c.color_tint = 178;
    c.wall_thickness_bias = 0.8f;
    c.split_regularity = 0.8f;  // very geometric
    c.preferred_rooms = {
        BuildingType::Archive, BuildingType::Archive,
        BuildingType::Temple, BuildingType::Observatory,
    };
    c.architecture = Architecture::Crystalline;
    return c;
}

CivConfig civ_config_crystal() {
    CivConfig c;
    c.name = "Crystal";
    c.civ_index = CIV_CRYSTAL;
    c.wall_glyphs = {"\xe2\x94\x8c", "\xe2\x94\x80", "\xe2\x94\x90", "\xe2\x94\x82", "\xe2\x94\xbc", "\xe2\x94\x9c", "\xe2\x94\xa4"};
    c.accent_glyphs = {"\xe2\x97\x88", "\xe2\x97\x8e", "\xe2\x97\x89"};
    c.color_primary = 51;     // bright cyan
    c.color_secondary = 231;  // white
    c.color_tint = 51;
    c.wall_thickness_bias = 0.6f;
    c.split_regularity = 0.5f;
    c.preferred_rooms = {
        BuildingType::Observatory, BuildingType::Observatory,
        BuildingType::Archive, BuildingType::Vault,
    };
    c.architecture = Architecture::LightWoven;
    return c;
}

CivConfig civ_config_precursor() {
    CivConfig c = civ_config_crystal();  // start from Crystal as a base
    c.name = "Precursor";
    c.civ_index = CIV_PRECURSOR;
    // Deep violet primary, Stellari-resonance cyan accent
    c.color_primary   = 93;    // deep violet
    c.color_secondary = 135;   // Stellari resonance cyan
    c.color_tint      = 93;
    // Sparser, more open feel: thin walls, regular geometry
    c.wall_thickness_bias = 0.5f;
    c.split_regularity    = 0.65f;
    // Accent glyphs: resonance markers
    c.accent_glyphs = {"\xe2\x97\x8a", "\xe2\x97\x87", "\xe2\x96\xbd"};
    // Preferred rooms: archives and observatories, befitting an ancient archive
    c.preferred_rooms = {
        BuildingType::Archive, BuildingType::Archive,
        BuildingType::Observatory, BuildingType::GreatHall,
    };
    c.architecture = Architecture::LightWoven;
    c.inscription_text_pool = {
        "The silence here is older than breath. We were asked to listen; we obeyed.",
        "Seven stars, seven seals. One we broke ourselves.",
        "Those who walk the nave without gift are asked only to remember.",
        "The crystal hums the first name. The second was never written.",
        "We sealed this vault so that hunger could not enter it.",
        "Do not kneel. The architects did not want kneeling.",
        "The rite of descent is simple: go down until the light changes.",
        "What you carry out of this place you must carry forever.",
    };
    return c;
}

CivConfig civ_config_industrial() {
    CivConfig c;
    c.name = "Industrial";
    c.civ_index = CIV_INDUSTRIAL;
    c.wall_glyphs = {"\xe2\x96\x93", "\xe2\x96\x88", "\xe2\x96\x91"};
    c.accent_glyphs = {"\xe2\x8a\x9e", "\xe2\x8a\xa0"};
    c.color_primary = 166;    // corroded orange
    c.color_secondary = 124;  // rust red
    c.color_tint = 124;
    c.wall_thickness_bias = 1.1f;
    c.split_regularity = 0.6f;
    c.preferred_rooms = {
        BuildingType::Vault, BuildingType::Vault,
        BuildingType::Workshop, BuildingType::Storage,
    };
    c.architecture = Architecture::VoidCarved;
    return c;
}

CivConfig civ_config_natural() {
    CivConfig c;
    c.name = "Natural";
    c.civ_index = CIV_MONOLITHIC;  // no dedicated renderer slot yet
    c.wall_glyphs = {"\xe2\x96\x93"};   // ▓ dark shade — natural stone
    c.accent_glyphs = {"\xe2\x96\x92"}; // ▒ medium shade
    c.color_primary = 240;
    c.color_secondary = 244;
    c.color_tint = 240;
    c.backdrop_tint = 237;
    c.wall_thickness_bias = 1.0f;
    c.max_wall_thickness = 3;
    c.split_regularity = 0.15f;
    c.preferred_rooms = {};
    c.architecture = Architecture::Geometric;
    return c;
}

CivConfig civ_config_for_architecture(Architecture arch) {
    switch (arch) {
        case Architecture::Geometric:   return civ_config_monolithic();
        case Architecture::Crystalline: return civ_config_baroque();
        case Architecture::LightWoven:  return civ_config_crystal();
        case Architecture::VoidCarved:  return civ_config_industrial();
        case Architecture::Organic:     return civ_config_monolithic(); // fallback
        default:                        return civ_config_monolithic();
    }
}

CivConfig civ_config_by_name(const std::string& name) {
    if (name == "monolithic") return civ_config_monolithic();
    if (name == "baroque")    return civ_config_baroque();
    if (name == "crystal")    return civ_config_crystal();
    if (name == "industrial") return civ_config_industrial();
    if (name == "Precursor")  return civ_config_precursor();
    if (name == "Natural")    return civ_config_natural();
    return civ_config_monolithic();  // default
}

const std::string& pick_inscription(const CivConfig& civ, std::mt19937& rng) {
    static const std::string empty{};
    if (civ.inscription_text_pool.empty()) return empty;
    std::uniform_int_distribution<size_t> d(0, civ.inscription_text_pool.size() - 1);
    return civ.inscription_text_pool[d(rng)];
}

} // namespace astra
