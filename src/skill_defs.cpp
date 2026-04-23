#include "astra/skill_defs.h"
#include "astra/player.h"
#include "astra/renderer.h"

namespace astra {

bool player_has_skill(const Player& player, SkillId id) {
    for (auto sid : player.learned_skills)
        if (sid == id) return true;
    return false;
}

static std::string acrobatics_category_description() {
    std::string s = "Mastery of agile movement and evasion in any environment.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("1 DV", Color::Cyan);
    s += " while this category is learned.";
    return s;
}

static std::string tinkering_category_description() {
    std::string s = "The art of repairing, modifying, and disassembling technology and equipment.\n\n";
    s += colored("Passive:", Color::White);
    s += " Grants the ";
    s += colored("Salvage", Color::Yellow);
    s += " ability: ";
    s += colored("40%", Color::Cyan);
    s += " chance on mechanical kills to recover 1-2 ";
    s += colored("Spare Parts", Color::Yellow);
    s += " and ";
    s += colored("30%", Color::Cyan);
    s += " chance of ";
    s += colored("Circuitry", Color::Cyan);
    s += ".";
    return s;
}

// ── Acrobatics sub-skills ───────────────────────────────────────
static std::string swiftness_description() {
    std::string s = "Training to shed momentum and slip out of a line of fire. Posture, footwork, and shallow pivots.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("5 DV", Color::Cyan);
    s += " when attacked by ranged weapons.";
    return s;
}

static std::string sidestep_description() {
    std::string s = "Close-quarters footwork for crowded fights. Steps off the attacker's line without breaking stance.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("2 DV", Color::Cyan);
    s += " while at least one hostile is adjacent. Stacks with ";
    s += colored("Swiftness", Color::Yellow);
    s += " against adjacent ranged foes.";
    return s;
}

static std::string sure_footed_description() {
    std::string s = "Footwork drilled on ship gantries, rubble, and loose scree. The surface beneath you stops mattering.\n\n";
    s += colored("Passive:", Color::White);
    s += " Your movement is lithe and efficient. Dungeon move actions cost ";
    s += colored("10%", Color::Cyan);
    s += " less time.";
    return s;
}

static std::string tumble_description() {
    std::string s = "Acrobatic combat roll used to cross exposed ground quickly. Telegraphs the direction before committing.\n\n";
    s += colored("Active:", Color::White);
    s += " Dash up to ";
    s += colored("3 tiles", Color::Cyan);
    s += " in any direction (telegraphed), ignoring anything in between. Cooldown ";
    s += colored("25 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("50", Color::Cyan);
    s += ".";
    return s;
}

static std::string adrenaline_rush_description() {
    std::string s = "A conditioned stress response. The body's panic reflex channelled into a short burst of speed and clarity.\n\n";
    s += colored("Active:", Color::White);
    s += " Self-cast surge: +";
    s += colored("2 DV", Color::Cyan);
    s += " and +";
    s += colored("25%", Color::Cyan);
    s += " quickness for ";
    s += colored("3 ticks", Color::Cyan);
    s += ". Cooldown ";
    s += colored("40 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("25", Color::Cyan);
    s += ".";
    return s;
}

// ── Short Blade sub-skills ──────────────────────────────────────
static std::string short_blade_expertise_description() {
    std::string s = "Close-quarters blade work drilled on boarding parties and station decks. Knife, shortsword, razorglass.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("1 hit", Color::Cyan);
    s += " with short blades. Action cost of attacking with a short blade in your primary hand is reduced by ";
    s += colored("25%", Color::Cyan);
    s += ".";
    return s;
}

static std::string jab_description() {
    std::string s = "A drilled off-hand strike used to open a cut before the main blow.\n\n";
    s += colored("Active:", Color::White);
    s += " Immediately jab with your off-hand short blade for a quick strike. Cooldown ";
    s += colored("3 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("25", Color::Cyan);
    s += ".";
    return s;
}

// ── Long Blade sub-skills ───────────────────────────────────────
static std::string long_blade_expertise_description() {
    std::string s = "Technique for swords and sabres. Parries, committed strikes, and footwork that keeps the reach.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("1 hit", Color::Cyan);
    s += " with long blades. Increased parry chance when wielding a long blade.";
    return s;
}

static std::string cleave_description() {
    std::string s = "A wide two-handed arc used to clear a corridor or a press of bodies.\n\n";
    s += colored("Active:", Color::White);
    s += " Strike all adjacent enemies in an arc with your long blade. Cooldown ";
    s += colored("5 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("50", Color::Cyan);
    s += ".";
    return s;
}

// ── Pistol sub-skills ───────────────────────────────────────────
static std::string steady_hand_description() {
    std::string s = "Sidearm range-training. Grip, breath, and sight alignment held together under pressure.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("1 accuracy", Color::Cyan);
    s += " with pistols. Reduced sway when firing at range.";
    return s;
}

static std::string quickdraw_description() {
    std::string s = "Draw and fire drilled as a single motion. For when the fight starts before you're ready.\n\n";
    s += colored("Active:", Color::White);
    s += " Draw and fire your pistol in a single action at reduced cost. Cooldown ";
    s += colored("3 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("25", Color::Cyan);
    s += ".";
    return s;
}

// ── Rifle sub-skills ────────────────────────────────────────────
static std::string marksman_description() {
    std::string s = "Long-gun marksmanship. Range calls, elevation, and lead on moving targets.\n\n";
    s += colored("Passive:", Color::White);
    s += " Effective range with rifles increased by +";
    s += colored("2", Color::Cyan);
    s += ". Better accuracy at long range.";
    return s;
}

static std::string suppressing_fire_description() {
    std::string s = "Sustained fire used to pin a group and deny them angles of advance.\n\n";
    s += colored("Active:", Color::White);
    s += " Lay down a barrage that pins enemies in a cone, reducing their movement and accuracy.";
    return s;
}

// ── Tinkering sub-skills ────────────────────────────────────────
static std::string basic_repair_description() {
    std::string s = "Field-maintenance taught on long-haul crews. Patches, welds, and salvage fittings on the fly.\n\n";
    s += colored("Passive:", Color::White);
    s += " Repair damaged equipment using scrap materials. Restores durability over time.";
    return s;
}

static std::string disassemble_description() {
    std::string s = "A methodical approach to stripping a device without ruining the parts inside.\n\n";
    s += colored("Active:", Color::White);
    s += " Break down items into component parts. Higher ";
    s += colored("Intelligence", Color::Yellow);
    s += " yields better salvage.";
    return s;
}

static std::string synthesize_description() {
    std::string s = "Assembling items from learned blueprints and raw crafting stock at a workbench.\n\n";
    s += colored("Active:", Color::White);
    s += " Combine learned blueprints and crafting materials to create entirely new items. Requires advanced molecular understanding.";
    return s;
}

// ── Endurance sub-skills ────────────────────────────────────────
static std::string thick_skin_description() {
    std::string s = "Callused hide and conditioned tolerance. The kind of toughness that builds through repeated small injuries.\n\n";
    s += colored("Passive:", Color::White);
    s += " Natural armor value increased by +";
    s += colored("1", Color::Cyan);
    s += ". Shrug off minor wounds more easily.";
    return s;
}

static std::string iron_will_description() {
    std::string s = "Mental discipline against coercion, fear, and psionic intrusion.\n\n";
    s += colored("Passive:", Color::White);
    s += " Mental resistance strengthened. +";
    s += colored("5", Color::Cyan);
    s += " to all resistance checks against psionic effects.";
    return s;
}

// ── Persuasion sub-skills ───────────────────────────────────────
static std::string haggle_description() {
    std::string s = "The commercial art of reading a seller's floor and a buyer's ceiling.\n\n";
    s += colored("Passive:", Color::White);
    s += " Merchants offer better prices. Buy values reduced by ";
    s += colored("10%", Color::Cyan);
    s += ", sell values increased by ";
    s += colored("10%", Color::Cyan);
    s += ".";
    return s;
}

static std::string intimidate_description() {
    std::string s = "Posture, voice, and measured threat — enough to make a fight unnecessary.\n\n";
    s += colored("Active:", Color::White);
    s += " Frighten a hostile creature, causing it to flee for several turns. Effectiveness scales with level. Cooldown ";
    s += colored("10 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("50", Color::Cyan);
    s += ".";
    return s;
}

// ── Wayfinding sub-skills ───────────────────────────────────────
static std::string camp_making_description() {
    std::string s = "Field skill for pitching a quick, warm shelter almost anywhere. Enough fire for a meal and a rest.\n\n";
    s += colored("Active:", Color::White);
    s += " Set up a makeshift camp with a fire for resting and cooking. Cooldown ";
    s += colored("300 ticks", Color::Cyan);
    s += ", action cost ";
    s += colored("100", Color::Cyan);
    s += ".";
    return s;
}

static std::string compass_sense_description() {
    std::string s = "Orienteering instinct sharpened by travel. Direction holds through rough ground and bad weather.\n\n";
    s += colored("Passive:", Color::White);
    s += " Your natural sense of direction is sharpened. Grace period before regaining bearings is halved and recovery rate doubled.";
    return s;
}

static std::string lore_plains_description() {
    std::string s = colored("Passive:", Color::White);
    s += " Familiarity with open terrain. ";
    s += colored("50%", Color::Cyan);
    s += " less likely to get lost on ";
    s += colored("Plains", Color::Yellow);
    s += " and ";
    s += colored("Desert", Color::Yellow);
    s += ". Travel time halved.";
    return s;
}

static std::string lore_forest_description() {
    std::string s = colored("Passive:", Color::White);
    s += " You read forest trails instinctively. ";
    s += colored("50%", Color::Cyan);
    s += " less likely to get lost in ";
    s += colored("Forests", Color::Yellow);
    s += " and ";
    s += colored("Fungal Growth", Color::Yellow);
    s += ". Travel time halved.";
    return s;
}

static std::string lore_wetlands_description() {
    std::string s = colored("Passive:", Color::White);
    s += " Navigating bogs and waterways comes naturally. ";
    s += colored("50%", Color::Cyan);
    s += " less likely to get lost in ";
    s += colored("Swamps", Color::Yellow);
    s += ", ";
    s += colored("Rivers", Color::Yellow);
    s += ", and ";
    s += colored("Lakeside", Color::Yellow);
    s += ". Travel time halved.";
    return s;
}

static std::string lore_mountains_description() {
    std::string s = colored("Passive:", Color::White);
    s += " Highland pathfinding expertise. ";
    s += colored("50%", Color::Cyan);
    s += " less likely to get lost in ";
    s += colored("Mountains", Color::Yellow);
    s += " and ";
    s += colored("Craters", Color::Yellow);
    s += ". Travel time halved.";
    return s;
}

static std::string lore_tundra_description() {
    std::string s = colored("Passive:", Color::White);
    s += " Survival knowledge for extreme environments. ";
    s += colored("50%", Color::Cyan);
    s += " less likely to get lost on ";
    s += colored("Ice Fields", Color::Yellow);
    s += " and ";
    s += colored("Volcanic", Color::Yellow);
    s += " terrain. Travel time halved.";
    return s;
}

static std::string scouts_eye_description() {
    std::string s = "Awareness trained on movement, silhouettes, and breaks in the horizon line.\n\n";
    s += colored("Passive:", Color::White);
    s += " Your keen awareness reveals nearby creatures on the minimap. Hostile and friendly NPCs appear as colored markers.";
    return s;
}

static std::string cartographer_description() {
    std::string s = "An eye for landmarks and spatial memory. Terrain, once crossed, stays with you.\n\n";
    s += colored("Passive:", Color::White);
    s += " Your trained eye spots points of interest and valuables. Items and landmarks appear on the minimap.";
    return s;
}

// ── Archaeology sub-skills ──────────────────────────────────────
static std::string ruin_reader_description() {
    std::string s = "Practiced interpretation of weathered markings and fragmented inscriptions.\n\n";
    s += colored("Passive:", Color::White);
    s += " Lore fragments found in ruins reveal their full text instead of partial translations.";
    return s;
}

static std::string artifact_identification_description() {
    std::string s = colored("Passive:", Color::White);
    s += " Ancient items are automatically identified on pickup.";
    return s;
}

static std::string excavation_description() {
    std::string s = colored("Active:", Color::White);
    s += " Search ruin tiles for hidden caches. Chance to discover lore fragments and sealed chambers.";
    return s;
}

static std::string cultural_attunement_description() {
    std::string s = colored("Passive:", Color::White);
    s += " Bonus when using artifacts from civilizations you have studied.";
    return s;
}

static std::string precursor_linguist_description() {
    std::string s = "Working familiarity with the dead languages of the ancients.\n\n";
    s += colored("Passive:", Color::White);
    s += " Read ancient inscriptions. Unlocks sealed doors and reveals vault locations.";
    return s;
}

static std::string beacon_sense_description() {
    std::string s = "An attunement to the low-frequency pull of beacons left by the ancients.\n\n";
    s += colored("Passive:", Color::White);
    s += " ";
    s += colored("Sgr A*", Color::Yellow);
    s += " beacon nodes glow on the star chart before visiting. Feel the ancient signal toward the center.";
    return s;
}

// ── Cooking sub-skills ──────────────────────────────────────────
static std::string advanced_fire_making_description() {
    std::string s = "Efficient fire-discipline for travelling cooks. Faster to light, longer-lasting coal bed.\n\n";
    s += colored("Passive:", Color::White);
    s += " Requires ";
    s += colored("Camp Making", Color::Yellow);
    s += ". Reduces the ";
    s += colored("Camp Making", Color::Yellow);
    s += " cooldown by ";
    s += colored("40%", Color::Cyan);
    s += ".";
    return s;
}

const std::vector<SkillCategory>& skill_catalog() {
    static const std::vector<SkillCategory> catalog = {
        {SkillId::Cat_Acrobatics, "Acrobatics",
         acrobatics_category_description(), 50, {
            {SkillId::Swiftness, "Swiftness",
             swiftness_description(),
             true, 50, 0, nullptr},
            {SkillId::Sidestep, "Sidestep",
             sidestep_description(),
             true, 75, 13, "Agility"},
            {SkillId::SureFooted, "Sure-Footed",
             sure_footed_description(),
             true, 75, 15, "Agility"},
            {SkillId::Tumble, "Tumble",
             tumble_description(),
             false, 100, 17, "Agility"},
            {SkillId::AdrenalineRush, "Adrenaline Rush",
             adrenaline_rush_description(),
             false, 150, 14, "Willpower"},
        }},
        {SkillId::Cat_ShortBlade, "Short Blade",
         "Proficiency with knives, daggers, and other short-edged weapons. Fast and precise.", 50, {
            {SkillId::ShortBladeExpertise, "Short Blade Expertise",
             short_blade_expertise_description(),
             true, 50, 0, nullptr},
            {SkillId::Jab, "Jab",
             jab_description(),
             false, 100, 15, "Agility"},
        }},
        {SkillId::Cat_LongBlade, "Long Blade",
         "Expertise with swords and long-edged weapons. Powerful strikes and parrying.", 75, {
            {SkillId::LongBladeExpertise, "Long Blade Expertise",
             long_blade_expertise_description(),
             true, 50, 0, nullptr},
            {SkillId::Cleave, "Cleave",
             cleave_description(),
             false, 150, 19, "Strength"},
        }},
        {SkillId::Cat_Pistol, "Pistol",
         "Training with sidearms and handguns. Quick draws and accurate fire at close range.", 50, {
            {SkillId::SteadyHand, "Steady Hand",
             steady_hand_description(),
             true, 50, 0, nullptr},
            {SkillId::Quickdraw, "Quickdraw",
             quickdraw_description(),
             false, 100, 15, "Agility"},
        }},
        {SkillId::Cat_Rifle, "Rifle",
         "Mastery of long-range firearms. Increased accuracy and effective range.", 75, {
            {SkillId::Marksman, "Marksman",
             marksman_description(),
             true, 50, 0, nullptr},
            {SkillId::SuppressingFire, "Suppressing Fire",
             suppressing_fire_description(),
             false, 150, 17, "Willpower"},
        }},
        {SkillId::Cat_Tinkering, "Tinkering",
         tinkering_category_description(), 100, {
            {SkillId::BasicRepair, "Basic Repair",
             basic_repair_description(),
             true, 100, 15, "Intelligence"},
            {SkillId::Disassemble, "Disassemble",
             disassemble_description(),
             false, 100, 17, "Intelligence"},
            {SkillId::Synthesize, "Synthesize",
             synthesize_description(),
             false, 150, 19, "Intelligence"},
        }},
        {SkillId::Cat_Endurance, "Endurance",
         "Physical resilience and mental fortitude. Shrug off damage and resist effects.", 50, {
            {SkillId::ThickSkin, "Thick Skin",
             thick_skin_description(),
             true, 50, 15, "Toughness"},
            {SkillId::IronWill, "Iron Will",
             iron_will_description(),
             true, 100, 17, "Willpower"},
        }},
        {SkillId::Cat_Persuasion, "Persuasion",
         "Social manipulation and negotiation. Better prices and the power to intimidate.", 100, {
            {SkillId::Haggle, "Haggle",
             haggle_description(),
             true, 50, 0, nullptr},
            {SkillId::Intimidate, "Intimidate",
             intimidate_description(),
             false, 100, 15, "Willpower"},
        }},
        {SkillId::Cat_Wayfinding, "Wayfinding",
         "Knowledge of navigation, terrain, and survival in the wild. "
         "Reduces the chance of getting lost and improves overland travel.", 75, {
            {SkillId::CampMaking, "Camp Making",
             camp_making_description(),
             false, 50, 12, "Intelligence"},
            {SkillId::CompassSense, "Compass Sense",
             compass_sense_description(),
             true, 50, 13, "Intelligence"},
            {SkillId::LorePlains, "Terrain Lore: Plains",
             lore_plains_description(),
             true, 50, 12, "Intelligence"},
            {SkillId::LoreForest, "Terrain Lore: Forest",
             lore_forest_description(),
             true, 50, 13, "Intelligence"},
            {SkillId::LoreWetlands, "Terrain Lore: Wetlands",
             lore_wetlands_description(),
             true, 75, 14, "Intelligence"},
            {SkillId::LoreMountains, "Terrain Lore: Mountains",
             lore_mountains_description(),
             true, 75, 15, "Intelligence"},
            {SkillId::LoreTundra, "Terrain Lore: Tundra",
             lore_tundra_description(),
             true, 75, 15, "Intelligence"},
            {SkillId::ScoutsEye, "Scout's Eye",
             scouts_eye_description(),
             true, 75, 13, "Intelligence"},
            {SkillId::Cartographer, "Cartographer",
             cartographer_description(),
             true, 100, 14, "Intelligence"},
        }},
        {SkillId::Cat_Archaeology, "Archaeology",
         "The study of ancient civilizations and their remains. "
         "Identify ruins, decipher inscriptions, and uncover lost knowledge.", 75, {
            {SkillId::RuinReader, "Ruin Reader",
             ruin_reader_description(),
             true, 50, 12, "Intelligence"},
            {SkillId::ArtifactIdentification, "Artifact Identification",
             artifact_identification_description(),
             true, 75, 13, "Intelligence"},
            {SkillId::Excavation, "Excavation",
             excavation_description(),
             false, 50, 12, "Intelligence"},
            {SkillId::CulturalAttunement, "Cultural Attunement",
             cultural_attunement_description(),
             true, 75, 14, "Intelligence"},
            {SkillId::PrecursorLinguist, "Precursor Linguist",
             precursor_linguist_description(),
             true, 100, 15, "Intelligence"},
            {SkillId::BeaconSense, "Beacon Sense",
             beacon_sense_description(),
             true, 100, 16, "Intelligence"},
        }},
        {SkillId::Cat_Cooking, "Cooking",
         "The art of turning raw ingredients into nourishing meals. Unlocks the "
         "kitchen tab for preparing recipes at a cooking fire.", 50, {
            {SkillId::AdvancedFireMaking, "Advanced Fire Making",
             advanced_fire_making_description(),
             true, 75, 0, nullptr},
        }},
    };
    return catalog;
}

const SkillDef* find_skill(SkillId id) {
    for (const auto& cat : skill_catalog()) {
        for (const auto& sk : cat.skills) {
            if (sk.id == id) return &sk;
        }
    }
    return nullptr;
}

SkillDetail skill_detail(SkillId id) {
    SkillDetail d;
    const auto& cat_list = skill_catalog();

    for (const auto& cat : cat_list) {
        if (cat.unlock_id == id) {
            d.header = cat.name + " [Category]";
            d.body = cat.description;
            d.cost_line = std::to_string(cat.sp_cost) + " SP";
            return d;
        }
    }

    const SkillDef* sk = find_skill(id);
    if (!sk) { d.header = "Unknown Skill"; return d; }
    d.header = sk->name + (sk->passive ? " [Passive]" : " [Active]");
    d.body = sk->description;
    d.cost_line = std::to_string(sk->sp_cost) + " SP";
    if (sk->attribute_req > 0 && sk->attribute_name) {
        d.requirement_line = "Requires " + std::to_string(sk->attribute_req) +
                             " " + sk->attribute_name;
    }
    return d;
}

SkillId terrain_lore_for(Tile terrain) {
    switch (terrain) {
        case Tile::OW_Plains: case Tile::OW_Desert:
            return SkillId::LorePlains;
        case Tile::OW_Forest: case Tile::OW_Fungal:
            return SkillId::LoreForest;
        case Tile::OW_Swamp: case Tile::OW_River: case Tile::OW_Lake:
            return SkillId::LoreWetlands;
        case Tile::OW_Mountains: case Tile::OW_Crater:
            return SkillId::LoreMountains;
        case Tile::OW_IceField: case Tile::OW_LavaFlow:
            return SkillId::LoreTundra;
        default:
            return static_cast<SkillId>(0); // no matching lore
    }
}

} // namespace astra
