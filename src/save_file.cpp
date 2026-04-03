#include "astra/save_file.h"
#include "astra/item_ids.h"
#include "astra/world_manager.h"

#include <unordered_map>

#include <chrono>
#include <cstring>
#include <fstream>

namespace astra {

// ---------------------------------------------------------------------------
// Binary helpers
// ---------------------------------------------------------------------------

class BinaryWriter {
public:
    explicit BinaryWriter(std::ofstream& out) : out_(out) {}

    void write_u8(uint8_t v)   { out_.write(reinterpret_cast<const char*>(&v), 1); }
    void write_u16(uint16_t v) { out_.write(reinterpret_cast<const char*>(&v), 2); }
    void write_u32(uint32_t v) { out_.write(reinterpret_cast<const char*>(&v), 4); }
    void write_i32(int32_t v)  { out_.write(reinterpret_cast<const char*>(&v), 4); }

    void write_string(const std::string& s) {
        uint16_t len = static_cast<uint16_t>(s.size());
        write_u16(len);
        if (len > 0) out_.write(s.data(), len);
    }

    void write_f32(float v) {
        out_.write(reinterpret_cast<const char*>(&v), 4);
    }

    void write_bytes(const void* data, size_t n) {
        out_.write(static_cast<const char*>(data), static_cast<std::streamsize>(n));
    }

    // Section support: write tag + placeholder size, return position of size field
    std::streampos begin_section(const char tag[4]) {
        out_.write(tag, 4);
        std::streampos pos = out_.tellp();
        uint32_t placeholder = 0;
        write_u32(placeholder);
        return pos;
    }

    void end_section(std::streampos size_pos) {
        std::streampos end = out_.tellp();
        uint32_t size = static_cast<uint32_t>(end - size_pos - 4);
        out_.seekp(size_pos);
        write_u32(size);
        out_.seekp(end);
    }

    bool good() const { return out_.good(); }

private:
    std::ofstream& out_;
};

class BinaryReader {
public:
    explicit BinaryReader(std::ifstream& in) : in_(in) {}

    uint8_t  read_u8()  { uint8_t v = 0;  in_.read(reinterpret_cast<char*>(&v), 1); return v; }
    uint16_t read_u16() { uint16_t v = 0; in_.read(reinterpret_cast<char*>(&v), 2); return v; }
    uint32_t read_u32() { uint32_t v = 0; in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    int32_t  read_i32() { int32_t v = 0;  in_.read(reinterpret_cast<char*>(&v), 4); return v; }

    std::string read_string() {
        uint16_t len = read_u16();
        std::string s(len, '\0');
        if (len > 0) in_.read(s.data(), len);
        return s;
    }

    float read_f32() {
        float v = 0;
        in_.read(reinterpret_cast<char*>(&v), 4);
        return v;
    }

    void read_bytes(void* data, size_t n) {
        in_.read(static_cast<char*>(data), static_cast<std::streamsize>(n));
    }

    // Read 4-char tag + u32 size
    bool read_section_header(char tag[4], uint32_t& size) {
        in_.read(tag, 4);
        if (!in_.good()) return false;
        size = read_u32();
        return in_.good();
    }

    void skip(uint32_t n) {
        in_.seekg(n, std::ios::cur);
    }

    bool good() const { return in_.good(); }

private:
    std::ifstream& in_;
};

// ---------------------------------------------------------------------------
// Save directory
// ---------------------------------------------------------------------------

std::filesystem::path save_directory() {
    std::filesystem::path dir;
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home) {
        dir = std::filesystem::path(home) / ".astra" / "saves";
    } else {
        dir = std::filesystem::path(".astra") / "saves";
    }
    return dir;
}

// ---------------------------------------------------------------------------
// Header layout (128 bytes)
// ---------------------------------------------------------------------------

static constexpr uint32_t SAVE_MAGIC = 0x52545341; // "ASTR" little-endian
static constexpr int HEADER_SIZE = 128;

struct SaveHeader {
    char magic[4];              // 0..3
    uint32_t version;           // 4..7
    uint32_t seed;              // 8..11
    int32_t world_tick;         // 12..15
    int32_t player_level;       // 16..19
    uint32_t map_count;         // 20..23
    uint32_t current_map_id;    // 24..27
    uint32_t timestamp;         // 28..31
    uint8_t dead;               // 32
    int32_t kills;              // 33..36
    int32_t xp;                 // 37..40
    int32_t money;              // 41..44
    char location[32];          // 45..76
    char death_cause[48];       // 77..124
    char reserved[3];           // 125..127
};

static void write_header(std::ofstream& out, const SaveData& data) {
    SaveHeader h{};
    std::memcpy(h.magic, "ASTR", 4);
    h.version = data.version;
    h.seed = data.seed;
    h.world_tick = data.world_tick;
    h.player_level = data.player.level;
    h.map_count = static_cast<uint32_t>(data.maps.size());
    h.current_map_id = data.current_map_id;

    auto now = std::chrono::system_clock::now();
    h.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

    h.dead = data.dead ? 1 : 0;
    h.kills = data.player.kills;
    h.xp = data.player.xp;
    h.money = data.player.money;

    // Copy location (first map's location name)
    std::string loc;
    if (!data.maps.empty()) {
        loc = data.maps[0].tilemap.location_name();
    }
    std::strncpy(h.location, loc.c_str(), sizeof(h.location) - 1);

    std::strncpy(h.death_cause, data.death_message.c_str(), sizeof(h.death_cause) - 1);

    out.write(reinterpret_cast<const char*>(&h), HEADER_SIZE);
}

static bool read_header(std::ifstream& in, SaveHeader& h) {
    in.read(reinterpret_cast<char*>(&h), HEADER_SIZE);
    if (!in.good()) return false;
    if (std::memcmp(h.magic, "ASTR", 4) != 0) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Item serialization helpers
// ---------------------------------------------------------------------------

static void write_stat_modifiers(BinaryWriter& w, const StatModifiers& m) {
    w.write_i32(m.attack);
    w.write_i32(m.defense);
    w.write_i32(m.max_hp);
    w.write_i32(m.view_radius);
    w.write_i32(m.quickness);
}

static StatModifiers read_stat_modifiers(BinaryReader& r) {
    StatModifiers m;
    m.attack = r.read_i32();
    m.defense = r.read_i32();
    m.max_hp = r.read_i32();
    m.view_radius = r.read_i32();
    m.quickness = r.read_i32();
    return m;
}

// Reconstruct item_def_id from name for pre-v19 saves
static uint16_t item_def_id_from_name(const std::string& name) {
    static const std::unordered_map<std::string, uint16_t> lookup = {
        {"Plasma Pistol", ITEM_PLASMA_PISTOL},
        {"Ion Blaster", ITEM_ION_BLASTER},
        {"Pulse Rifle", ITEM_PULSE_RIFLE},
        {"Arc Caster", ITEM_ARC_CASTER},
        {"Void Lance", ITEM_VOID_LANCE},
        {"Energy Cell", ITEM_BATTERY},
        {"Battery", ITEM_BATTERY},
        {"Ration Pack", ITEM_RATION_PACK},
        {"Combat Stim", ITEM_COMBAT_STIM},
        {"Combat Knife", ITEM_COMBAT_KNIFE},
        {"Vibro Blade", ITEM_VIBRO_BLADE},
        {"Vibro-Blade", ITEM_VIBRO_BLADE},
        {"Plasma Saber", ITEM_PLASMA_SABER},
        {"Stun Baton", ITEM_STUN_BATON},
        {"Ancient Mono-Edge", ITEM_ANCIENT_MONO_EDGE},
        {"Padded Vest", ITEM_PADDED_VEST},
        {"Composite Armor", ITEM_COMPOSITE_ARMOR},
        {"Exo-Suit", ITEM_EXO_SUIT},
        {"Flight Helmet", ITEM_FLIGHT_HELMET},
        {"Tactical Helmet", ITEM_TACTICAL_HELMET},
        {"Combat Boots", ITEM_COMBAT_BOOTS},
        {"Mag-Lock Boots", ITEM_MAG_LOCK_BOOTS},
        {"Arm Guard", ITEM_ARM_GUARD},
        {"Riot Shield", ITEM_RIOT_SHIELD},
        {"Recon Visor", ITEM_RECON_VISOR},
        {"Night Goggles", ITEM_NIGHT_GOGGLES},
        {"Jetpack", ITEM_JETPACK},
        {"Cargo Pack", ITEM_CARGO_PACK},
        {"Frag Grenade", ITEM_FRAG_GRENADE},
        {"EMP Grenade", ITEM_EMP_GRENADE},
        {"Cryo Grenade", ITEM_CRYO_GRENADE},
        {"Scrap Metal", ITEM_SCRAP_METAL},
        {"Broken Circuit", ITEM_BROKEN_CIRCUIT},
        {"Empty Casing", ITEM_EMPTY_CASING},
        {"Nano-Fiber", ITEM_NANO_FIBER},
        {"Power Core", ITEM_POWER_CORE},
        {"Circuit Board", ITEM_CIRCUIT_BOARD},
        {"Alloy Ingot", ITEM_ALLOY_INGOT},
        {"Engine Coil Mk1", ITEM_ENGINE_COIL_MK1},
        {"Engine Coil Mk.I", ITEM_ENGINE_COIL_MK1},
        {"Hull Plate", ITEM_HULL_PLATE},
        {"Hull Plate Mk1", ITEM_HULL_PLATE},
        {"Shield Generator", ITEM_SHIELD_GENERATOR},
        {"Navi Computer Mk2", ITEM_NAVI_COMPUTER_MK2},
        {"Navi-Computer Mk.II", ITEM_NAVI_COMPUTER_MK2},
    };
    auto it = lookup.find(name);
    if (it != lookup.end()) return it->second;
    // Try matching synthesized items by prefix
    if (name.find("Plasma Edge") != std::string::npos) return ITEM_SYNTH_PLASMA_EDGE;
    if (name.find("Thruster Plate") != std::string::npos) return ITEM_SYNTH_THRUSTER_PLATE;
    if (name.find("Targeting Array") != std::string::npos) return ITEM_SYNTH_TARGETING_ARRAY;
    if (name.find("Dual-Edge") != std::string::npos) return ITEM_SYNTH_DUAL_EDGE;
    if (name.find("Reinforced Pack") != std::string::npos) return ITEM_SYNTH_REINFORCED_PACK;
    if (name.find("Overcharged Engine") != std::string::npos) return ITEM_SYNTH_OVERCHARGED_ENGINE;
    if (name.find("Articulated Armor") != std::string::npos) return ITEM_SYNTH_ARTICULATED_ARMOR;
    if (name.find("Guided Blaster") != std::string::npos) return ITEM_SYNTH_GUIDED_BLASTER;
    if (name.find("Combat Gauntlet") != std::string::npos) return ITEM_SYNTH_COMBAT_GAUNTLET;
    if (name.find("Armored Blade") != std::string::npos) return ITEM_SYNTH_ARMORED_BLADE;
    return 0; // unknown — renders as '?' Magenta
}

static void write_item(BinaryWriter& w, const Item& item) {
    w.write_u32(item.id);
    w.write_string(item.name);
    w.write_string(item.description);
    w.write_u8(static_cast<uint8_t>(item.type));
    w.write_u8(item.slot.has_value() ? 1 : 0);
    if (item.slot) w.write_u8(static_cast<uint8_t>(*item.slot));
    w.write_u8(static_cast<uint8_t>(item.rarity));
    // v19: write item_def_id instead of glyph/color (same 2 bytes)
    w.write_u16(item.item_def_id);
    w.write_i32(item.weight);
    w.write_u8(item.stackable ? 1 : 0);
    w.write_i32(item.stack_count);
    w.write_i32(item.buy_value);
    w.write_i32(item.sell_value);
    write_stat_modifiers(w, item.modifiers);
    w.write_i32(item.item_level);
    w.write_i32(item.level_requirement);
    w.write_i32(item.durability);
    w.write_i32(item.max_durability);
    w.write_u8(item.usable ? 1 : 0);
    w.write_u8(item.ranged.has_value() ? 1 : 0);
    if (item.ranged) {
        w.write_i32(item.ranged->charge_capacity);
        w.write_i32(item.ranged->charge_per_shot);
        w.write_i32(item.ranged->current_charge);
        w.write_i32(item.ranged->max_range);
    }
    // Enhancement slots
    w.write_i32(item.enhancement_slots);
    w.write_u32(static_cast<uint32_t>(item.enhancements.size()));
    for (const auto& enh : item.enhancements) {
        w.write_u8(enh.filled ? 1 : 0);
        w.write_u8(enh.committed ? 1 : 0);
        w.write_u32(enh.material_id);
        w.write_string(enh.material_name);
        w.write_i32(enh.bonus.attack);
        w.write_i32(enh.bonus.defense);
        w.write_i32(enh.bonus.max_hp);
        w.write_i32(enh.bonus.view_radius);
        w.write_i32(enh.bonus.quickness);
    }
    // v14: ship component fields
    w.write_u8(item.ship_slot.has_value() ? 1 : 0);
    if (item.ship_slot) w.write_u8(static_cast<uint8_t>(*item.ship_slot));
    w.write_i32(item.ship_modifiers.hull_hp);
    w.write_i32(item.ship_modifiers.shield_hp);
    w.write_i32(item.ship_modifiers.warp_range);
    w.write_i32(item.ship_modifiers.cargo_capacity);
}

static Item read_item(BinaryReader& r, uint32_t version = 14) {
    Item item;
    item.id = r.read_u32();
    item.name = r.read_string();
    item.description = r.read_string();
    item.type = static_cast<ItemType>(r.read_u8());
    bool has_slot = r.read_u8() != 0;
    if (has_slot) item.slot = static_cast<EquipSlot>(r.read_u8());
    item.rarity = static_cast<Rarity>(r.read_u8());
    // v19: item_def_id replaces glyph+color (same 2 bytes)
    if (version >= 19) {
        item.item_def_id = r.read_u16();
    } else {
        r.read_u8();  // skip legacy glyph
        r.read_u8();  // skip legacy color
        // Reconstruct item_def_id from name after full read (below)
    }
    item.weight = r.read_i32();
    item.stackable = r.read_u8() != 0;
    item.stack_count = r.read_i32();
    item.buy_value = r.read_i32();
    item.sell_value = r.read_i32();
    item.modifiers = read_stat_modifiers(r);
    item.item_level = r.read_i32();
    item.level_requirement = r.read_i32();
    item.durability = r.read_i32();
    item.max_durability = r.read_i32();
    item.usable = r.read_u8() != 0;
    bool has_ranged = r.read_u8() != 0;
    if (has_ranged) {
        RangedData rd;
        rd.charge_capacity = r.read_i32();
        rd.charge_per_shot = r.read_i32();
        rd.current_charge = r.read_i32();
        rd.max_range = r.read_i32();
        item.ranged = rd;
    }
    // Enhancement slots
    item.enhancement_slots = r.read_i32();
    uint32_t enh_count = r.read_u32();
    item.enhancements.resize(enh_count);
    for (uint32_t i = 0; i < enh_count; ++i) {
        item.enhancements[i].filled = r.read_u8() != 0;
        item.enhancements[i].committed = r.read_u8() != 0;
        item.enhancements[i].material_id = r.read_u32();
        item.enhancements[i].material_name = r.read_string();
        item.enhancements[i].bonus.attack = r.read_i32();
        item.enhancements[i].bonus.defense = r.read_i32();
        item.enhancements[i].bonus.max_hp = r.read_i32();
        item.enhancements[i].bonus.view_radius = r.read_i32();
        item.enhancements[i].bonus.quickness = r.read_i32();
    }
    // v14: ship component fields
    if (version >= 14) {
        bool has_ship_slot = r.read_u8() != 0;
        if (has_ship_slot) item.ship_slot = static_cast<ShipSlot>(r.read_u8());
        item.ship_modifiers.hull_hp = r.read_i32();
        item.ship_modifiers.shield_hp = r.read_i32();
        item.ship_modifiers.warp_range = r.read_i32();
        item.ship_modifiers.cargo_capacity = r.read_i32();
    }
    // Reconstruct item_def_id for pre-v19 saves
    if (version < 19 && item.item_def_id == 0) {
        item.item_def_id = item_def_id_from_name(item.name);
    }
    return item;
}

static void write_optional_item(BinaryWriter& w, const std::optional<Item>& opt) {
    w.write_u8(opt.has_value() ? 1 : 0);
    if (opt) write_item(w, *opt);
}

static std::optional<Item> read_optional_item(BinaryReader& r, uint32_t version = 14) {
    if (r.read_u8() != 0) return read_item(r, version);
    return std::nullopt;
}

static void write_equipment(BinaryWriter& w, const Equipment& eq) {
    // v12: write all 11 slots
    write_optional_item(w, eq.face);
    write_optional_item(w, eq.head);
    write_optional_item(w, eq.body);
    write_optional_item(w, eq.left_arm);
    write_optional_item(w, eq.right_arm);
    write_optional_item(w, eq.left_hand);
    write_optional_item(w, eq.right_hand);
    write_optional_item(w, eq.back);
    write_optional_item(w, eq.feet);
    write_optional_item(w, eq.thrown);
    write_optional_item(w, eq.missile);
}

static void read_equipment(BinaryReader& r, Equipment& eq, uint32_t version) {
    if (version >= 12) {
        eq.face = read_optional_item(r, version);
        eq.head = read_optional_item(r, version);
        eq.body = read_optional_item(r, version);
        eq.left_arm = read_optional_item(r, version);
        eq.right_arm = read_optional_item(r, version);
        eq.left_hand = read_optional_item(r, version);
        eq.right_hand = read_optional_item(r, version);
        eq.back = read_optional_item(r, version);
        eq.feet = read_optional_item(r, version);
        eq.thrown = read_optional_item(r, version);
        eq.missile = read_optional_item(r, version);
    } else {
        // Old 8-slot format: map to new slots
        eq.head = read_optional_item(r, version);
        eq.body = read_optional_item(r, version);
        (void)read_optional_item(r, version);
        eq.feet = read_optional_item(r, version);
        eq.left_hand = read_optional_item(r, version);
        eq.right_hand = read_optional_item(r, version);
        eq.missile = read_optional_item(r, version);
        (void)read_optional_item(r, version);
    }
}

static void write_inventory(BinaryWriter& w, const Inventory& inv) {
    w.write_i32(inv.max_carry_weight);
    w.write_u32(static_cast<uint32_t>(inv.items.size()));
    for (const auto& item : inv.items) write_item(w, item);
}

static void read_inventory(BinaryReader& r, Inventory& inv, uint32_t version = 14) {
    inv.max_carry_weight = r.read_i32();
    uint32_t count = r.read_u32();
    inv.items.resize(count);
    for (uint32_t i = 0; i < count; ++i) inv.items[i] = read_item(r, version);
}

// ---------------------------------------------------------------------------
// Section writers
// ---------------------------------------------------------------------------

static void write_player_section(BinaryWriter& w, const Player& p) {
    auto pos = w.begin_section("PLYR");
    w.write_i32(p.x);
    w.write_i32(p.y);
    w.write_i32(p.hp);
    w.write_i32(p.max_hp);
    w.write_i32(p.depth);
    w.write_i32(p.view_radius);
    w.write_i32(p.temperature);
    w.write_u8(static_cast<uint8_t>(p.hunger));
    w.write_i32(p.money);
    w.write_i32(p.quickness);
    w.write_i32(p.move_speed);
    w.write_i32(p.attack_value);
    w.write_i32(p.defense_value);
    w.write_i32(p.level);
    w.write_i32(p.xp);
    w.write_i32(p.max_xp);
    w.write_i32(p.energy);
    w.write_i32(p.kills);
    w.write_i32(p.regen_counter);
    // v10: light_radius
    w.write_i32(p.light_radius);
    write_equipment(w, p.equipment);
    write_inventory(w, p.inventory);
    // v12: character identity, attributes, skills, reputation
    w.write_string(p.name);
    w.write_u8(static_cast<uint8_t>(p.race));
    w.write_u8(static_cast<uint8_t>(p.player_class));
    w.write_i32(p.attributes.strength);
    w.write_i32(p.attributes.agility);
    w.write_i32(p.attributes.toughness);
    w.write_i32(p.attributes.intelligence);
    w.write_i32(p.attributes.willpower);
    w.write_i32(p.attributes.luck);
    w.write_i32(p.attribute_points);
    w.write_i32(p.dodge_value);
    w.write_i32(p.resistances.acid);
    w.write_i32(p.resistances.electrical);
    w.write_i32(p.resistances.cold);
    w.write_i32(p.resistances.heat);
    w.write_i32(p.skill_points);
    w.write_u32(static_cast<uint32_t>(p.learned_skills.size()));
    for (const auto& sid : p.learned_skills) {
        w.write_u32(static_cast<uint32_t>(sid));
    }
    w.write_u32(static_cast<uint32_t>(p.reputation.size()));
    for (const auto& f : p.reputation) {
        w.write_string(f.faction_name);
        w.write_i32(f.reputation);
    }
    // Blueprints
    w.write_u32(static_cast<uint32_t>(p.learned_blueprints.size()));
    for (const auto& bp : p.learned_blueprints) {
        w.write_u32(bp.source_item_id);
        w.write_string(bp.name);
        w.write_string(bp.description);
    }
    // Journal
    w.write_u32(static_cast<uint32_t>(p.journal.size()));
    for (const auto& je : p.journal) {
        w.write_u8(static_cast<uint8_t>(je.category));
        w.write_string(je.title);
        w.write_string(je.technical);
        w.write_string(je.personal);
        w.write_string(je.timestamp);
        w.write_i32(je.world_tick);
        // v16: quest_id link
        w.write_string(je.quest_id);
    }
    // v14: starship
    w.write_string(p.ship.name);
    w.write_string(p.ship.type);
    for (int i = 0; i < ship_slot_count; ++i) {
        write_optional_item(w, p.ship.slot_ref(static_cast<ShipSlot>(i)));
    }
    // Ship cargo
    w.write_u32(static_cast<uint32_t>(p.ship.cargo.size()));
    for (const auto& item : p.ship.cargo) write_item(w, item);
    // v15: tab help seen bitfield
    w.write_u16(p.tab_help_seen);
    w.end_section(pos);
}

static void write_npc(BinaryWriter& w, const Npc& npc) {
    w.write_i32(npc.x);
    w.write_i32(npc.y);
    // v18: write npc_role instead of legacy glyph/color
    w.write_u8(static_cast<uint8_t>(npc.npc_role));
    w.write_string(npc.name);
    w.write_string(npc.role);
    w.write_u8(static_cast<uint8_t>(npc.race));
    w.write_i32(npc.hp);
    w.write_i32(npc.max_hp);
    w.write_u8(static_cast<uint8_t>(npc.disposition));
    w.write_u8(has_effect(npc.effects, EffectId::Invulnerable) ? 1 : 0); // back-compat
    w.write_i32(npc.quickness);
    w.write_i32(npc.energy);
    w.write_i32(npc.level);
    w.write_u8(npc.elite ? 1 : 0);
    w.write_i32(npc.base_xp);
    w.write_i32(npc.base_damage);

    // Interaction traits presence flags
    uint8_t has_talk = npc.interactions.talk ? 1 : 0;
    uint8_t has_shop = npc.interactions.shop ? 1 : 0;
    uint8_t has_quest = npc.interactions.quest ? 1 : 0;
    w.write_u8(has_talk);
    w.write_u8(has_shop);
    w.write_u8(has_quest);

    if (npc.interactions.talk) {
        const auto& t = *npc.interactions.talk;
        w.write_string(t.greeting);
        w.write_u32(static_cast<uint32_t>(t.nodes.size()));
        for (const auto& node : t.nodes) {
            w.write_string(node.text);
            w.write_u32(static_cast<uint32_t>(node.choices.size()));
            for (const auto& c : node.choices) {
                w.write_string(c.label);
                w.write_i32(c.next_node);
            }
        }
    }
    if (npc.interactions.shop) {
        w.write_string(npc.interactions.shop->shop_name);
        w.write_u32(static_cast<uint32_t>(npc.interactions.shop->inventory.size()));
        for (const auto& item : npc.interactions.shop->inventory) write_item(w, item);
    }
    if (npc.interactions.quest) {
        const auto& q = *npc.interactions.quest;
        w.write_string(q.quest_intro);
        w.write_u32(static_cast<uint32_t>(q.nodes.size()));
        for (const auto& node : q.nodes) {
            w.write_string(node.text);
            w.write_u32(static_cast<uint32_t>(node.choices.size()));
            for (const auto& c : node.choices) {
                w.write_string(c.label);
                w.write_i32(c.next_node);
            }
        }
    }
}

static void write_map_section(BinaryWriter& w, const MapState& ms) {
    auto pos = w.begin_section("MPDT");

    w.write_u32(ms.map_id);
    const auto& tm = ms.tilemap;
    w.write_u8(static_cast<uint8_t>(tm.map_type()));
    w.write_u8(static_cast<uint8_t>(tm.biome()));
    w.write_i32(tm.width());
    w.write_i32(tm.height());
    w.write_string(tm.location_name());

    // Tiles as raw u8
    const auto& tiles = tm.tiles();
    for (const auto& t : tiles) w.write_u8(static_cast<uint8_t>(t));

    // Region IDs
    const auto& rids = tm.region_ids();
    for (int rid : rids) w.write_i32(rid);

    // Regions
    const auto& regions = tm.regions_vec();
    w.write_u32(static_cast<uint32_t>(regions.size()));
    for (const auto& r : regions) {
        w.write_u8(static_cast<uint8_t>(r.type));
        w.write_u8(r.lit ? 1 : 0);
        w.write_u8(static_cast<uint8_t>(r.flavor));
        w.write_u16(static_cast<uint16_t>(r.features));
        w.write_string(r.name);
        w.write_string(r.enter_message);
    }

    // Backdrop
    const auto& backdrop = tm.backdrop_data();
    w.write_bytes(backdrop.data(), backdrop.size());

    // Glyph overrides (v8+)
    const auto& glyph_ov = tm.glyph_overrides();
    size_t tile_area = static_cast<size_t>(tm.width()) * tm.height();
    if (!glyph_ov.empty()) {
        w.write_bytes(glyph_ov.data(), tile_area);
    } else {
        std::vector<uint8_t> zeros(tile_area, 0);
        w.write_bytes(zeros.data(), tile_area);
    }

    // Visibility
    const auto& vis = ms.visibility;
    const auto& cells = vis.cells();
    for (const auto& c : cells) w.write_u8(static_cast<uint8_t>(c));

    // NPCs
    w.write_u32(static_cast<uint32_t>(ms.npcs.size()));
    for (const auto& npc : ms.npcs) {
        write_npc(w, npc);
    }

    // Ground items
    w.write_u32(static_cast<uint32_t>(ms.ground_items.size()));
    for (const auto& gi : ms.ground_items) {
        w.write_i32(gi.x);
        w.write_i32(gi.y);
        write_item(w, gi.item);
    }

    // Fixtures (v3+)
    const auto& fixtures = tm.fixtures_vec();
    w.write_u32(static_cast<uint32_t>(fixtures.size()));
    for (const auto& f : fixtures) {
        w.write_u8(static_cast<uint8_t>(f.type));
        // v17: glyph and color no longer written (renderer-resolved)
        w.write_u8(f.passable ? 1 : 0);
        w.write_u8(f.interactable ? 1 : 0);
        w.write_i32(f.cooldown);
        w.write_i32(f.last_used_tick);
    }
    // Fixture IDs (parallel to tiles)
    const auto& fids = tm.fixture_ids();
    for (int fid : fids) w.write_i32(fid);

    // Hub flag
    w.write_u8(tm.is_hub() ? 1 : 0);

    w.end_section(pos);
}

static void write_messages_section(BinaryWriter& w, const std::deque<std::string>& msgs) {
    auto pos = w.begin_section("MSGS");
    w.write_u32(static_cast<uint32_t>(msgs.size()));
    for (const auto& m : msgs) w.write_string(m);
    w.end_section(pos);
}

static void write_stash_section(BinaryWriter& w, const std::vector<Item>& stash) {
    auto pos = w.begin_section("STSH");
    w.write_u32(static_cast<uint32_t>(stash.size()));
    for (const auto& item : stash) write_item(w, item);
    w.end_section(pos);
}

static void write_navigation_section(BinaryWriter& w, const NavigationData& nav) {
    auto pos = w.begin_section("STAR");
    w.write_u32(nav.current_system_id);
    w.write_i32(nav.navi_range);
    w.write_i32(nav.current_body_index);
    w.write_i32(nav.current_moon_index);
    w.write_u8(nav.at_station ? 1 : 0);
    w.write_u8(nav.on_ship ? 1 : 0);
    w.write_u32(static_cast<uint32_t>(nav.systems.size()));
    for (const auto& sys : nav.systems) {
        w.write_u32(sys.id);
        w.write_string(sys.name);
        w.write_u8(static_cast<uint8_t>(sys.star_class));
        w.write_u8(sys.binary ? 1 : 0);
        w.write_u8(sys.has_station ? 1 : 0);
        w.write_i32(sys.planet_count);
        w.write_i32(sys.asteroid_belts);
        w.write_i32(sys.danger_level);
        w.write_f32(sys.gx);
        w.write_f32(sys.gy);
        w.write_u8(sys.discovered ? 1 : 0);

        // v4: celestial bodies
        w.write_u8(sys.bodies_generated ? 1 : 0);
        if (sys.bodies_generated) {
            w.write_u16(static_cast<uint16_t>(sys.bodies.size()));
            for (const auto& body : sys.bodies) {
                w.write_string(body.name);
                w.write_u8(static_cast<uint8_t>(body.type));
                w.write_u8(static_cast<uint8_t>(body.atmosphere));
                w.write_u8(static_cast<uint8_t>(body.temperature));
                w.write_u16(body.resources);
                w.write_u8(body.size);
                w.write_u8(body.moons);
                w.write_f32(body.orbital_distance);
                w.write_u8(body.landable ? 1 : 0);
                w.write_u8(body.explored ? 1 : 0);
                w.write_u8(body.has_dungeon ? 1 : 0);
                w.write_i32(body.danger_level);
                // v10: day length
                w.write_i32(body.day_length);
            }
        }
    }
    w.end_section(pos);
}

// ---------------------------------------------------------------------------
// Quest serialization (v13)
// ---------------------------------------------------------------------------

static void write_quest(BinaryWriter& w, const Quest& q) {
    w.write_string(q.id);
    w.write_string(q.title);
    w.write_string(q.description);
    w.write_string(q.giver_npc);
    w.write_u8(static_cast<uint8_t>(q.status));
    w.write_u8(q.is_story ? 1 : 0);
    w.write_i32(q.accepted_tick);
    w.write_u32(q.target_system_id);
    w.write_i32(q.target_body_index);

    // Objectives
    w.write_u32(static_cast<uint32_t>(q.objectives.size()));
    for (const auto& obj : q.objectives) {
        w.write_u8(static_cast<uint8_t>(obj.type));
        w.write_string(obj.description);
        w.write_i32(obj.target_count);
        w.write_i32(obj.current_count);
        w.write_string(obj.target_id);
    }

    // Reward
    w.write_i32(q.reward.xp);
    w.write_i32(q.reward.credits);
    w.write_i32(q.reward.skill_points);
    w.write_string(q.reward.item_name);
    w.write_string(q.reward.faction_name);
    w.write_i32(q.reward.reputation_change);
}

static Quest read_quest(BinaryReader& r) {
    Quest q;
    q.id = r.read_string();
    q.title = r.read_string();
    q.description = r.read_string();
    q.giver_npc = r.read_string();
    q.status = static_cast<QuestStatus>(r.read_u8());
    q.is_story = r.read_u8() != 0;
    q.accepted_tick = r.read_i32();
    q.target_system_id = r.read_u32();
    q.target_body_index = r.read_i32();

    uint32_t obj_count = r.read_u32();
    q.objectives.resize(obj_count);
    for (auto& obj : q.objectives) {
        obj.type = static_cast<ObjectiveType>(r.read_u8());
        obj.description = r.read_string();
        obj.target_count = r.read_i32();
        obj.current_count = r.read_i32();
        obj.target_id = r.read_string();
    }

    q.reward.xp = r.read_i32();
    q.reward.credits = r.read_i32();
    q.reward.skill_points = r.read_i32();
    q.reward.item_name = r.read_string();
    q.reward.faction_name = r.read_string();
    q.reward.reputation_change = r.read_i32();

    return q;
}

static void write_quest_section(BinaryWriter& w, const SaveData& data) {
    auto pos = w.begin_section("QUST");

    // Active quests
    w.write_u32(static_cast<uint32_t>(data.active_quests.size()));
    for (const auto& q : data.active_quests) {
        write_quest(w, q);
    }

    // Completed quests
    w.write_u32(static_cast<uint32_t>(data.completed_quests.size()));
    for (const auto& q : data.completed_quests) {
        write_quest(w, q);
    }

    // Quest locations map
    w.write_u32(static_cast<uint32_t>(data.quest_locations.size()));
    for (const auto& [key, meta] : data.quest_locations) {
        // LocationKey: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth, zone_x, zone_y}
        auto [sys_id, body_idx, moon_idx, is_station, ow_x, ow_y, depth, zone_x, zone_y] = key;
        w.write_u32(sys_id);
        w.write_i32(body_idx);
        w.write_i32(moon_idx);
        w.write_u8(is_station ? 1 : 0);
        w.write_i32(ow_x);
        w.write_i32(ow_y);
        w.write_i32(depth);
        w.write_i32(zone_x);
        w.write_i32(zone_y);

        // QuestLocationMeta
        w.write_string(meta.quest_id);
        w.write_string(meta.quest_title);
        w.write_i32(meta.difficulty_override);
        w.write_u32(static_cast<uint32_t>(meta.npc_roles.size()));
        for (const auto& role : meta.npc_roles) w.write_string(role);
        w.write_u32(static_cast<uint32_t>(meta.quest_items.size()));
        for (const auto& item : meta.quest_items) w.write_string(item);
        w.write_u16(static_cast<uint16_t>(meta.poi_type));
        w.write_u8(meta.remove_on_completion ? 1 : 0);
        w.write_u32(meta.target_system_id);
        w.write_i32(meta.target_body_index);
    }

    w.end_section(pos);
}

static void read_quest_section(BinaryReader& r, SaveData& data) {
    uint32_t active_count = r.read_u32();
    data.active_quests.resize(active_count);
    for (auto& q : data.active_quests) {
        q = read_quest(r);
    }

    uint32_t completed_count = r.read_u32();
    data.completed_quests.resize(completed_count);
    for (auto& q : data.completed_quests) {
        q = read_quest(r);
    }

    uint32_t loc_count = r.read_u32();
    for (uint32_t i = 0; i < loc_count; ++i) {
        uint32_t sys_id = r.read_u32();
        int body_idx = r.read_i32();
        int moon_idx = r.read_i32();
        bool is_station = r.read_u8() != 0;
        int ow_x = r.read_i32();
        int ow_y = r.read_i32();
        int depth = r.read_i32();
        int zone_x = -1, zone_y = -1;
        if (data.version >= 16) {
            zone_x = r.read_i32();
            zone_y = r.read_i32();
        }
        LocationKey key = LocationKey{sys_id, body_idx, moon_idx, is_station, ow_x, ow_y, depth, zone_x, zone_y};

        QuestLocationMeta meta;
        meta.quest_id = r.read_string();
        meta.quest_title = r.read_string();
        meta.difficulty_override = r.read_i32();
        uint32_t role_count = r.read_u32();
        meta.npc_roles.resize(role_count);
        for (auto& role : meta.npc_roles) role = r.read_string();
        uint32_t item_count = r.read_u32();
        meta.quest_items.resize(item_count);
        for (auto& item : meta.quest_items) item = r.read_string();
        meta.poi_type = static_cast<Tile>(r.read_u16());
        meta.remove_on_completion = r.read_u8() != 0;
        meta.target_system_id = r.read_u32();
        meta.target_body_index = r.read_i32();

        data.quest_locations[key] = std::move(meta);
    }
}

static void write_game_state_section(BinaryWriter& w, const SaveData& data) {
    auto pos = w.begin_section("GSTA");
    w.write_i32(data.current_region);
    // v20: widget bitfield replaces active_tab
    w.write_u8(data.active_widgets);
    w.write_u8(data.focused_widget);
    w.write_u8(data.panel_visible ? 1 : 0);
    w.write_string(data.death_message);
    // v7/v9: surface mode (was on_overworld bool)
    w.write_u8(data.surface_mode);
    w.write_i32(data.overworld_x);
    w.write_i32(data.overworld_y);
    // v16: zone position within 3x3 grid
    w.write_i32(data.zone_x);
    w.write_i32(data.zone_y);
    w.write_u8(data.lost ? 1 : 0);
    w.write_i32(data.lost_moves);
    // v10: day clock
    w.write_i32(data.local_tick);
    w.write_i32(data.local_ticks_per_day);
    w.end_section(pos);
}

// ---------------------------------------------------------------------------
// Section readers
// ---------------------------------------------------------------------------

static void read_dialog_nodes(BinaryReader& r, std::vector<DialogNode>& nodes) {
    uint32_t count = r.read_u32();
    nodes.resize(count);
    for (auto& node : nodes) {
        node.text = r.read_string();
        uint32_t nchoices = r.read_u32();
        node.choices.resize(nchoices);
        for (auto& c : node.choices) {
            c.label = r.read_string();
            c.next_node = r.read_i32();
        }
    }
}

static void read_player_section(BinaryReader& r, Player& p, uint32_t version) {
    p.x = r.read_i32();
    p.y = r.read_i32();
    p.hp = r.read_i32();
    p.max_hp = r.read_i32();
    p.depth = r.read_i32();
    p.view_radius = r.read_i32();
    p.temperature = r.read_i32();
    p.hunger = static_cast<HungerState>(r.read_u8());
    p.money = r.read_i32();
    p.quickness = r.read_i32();
    p.move_speed = r.read_i32();
    p.attack_value = r.read_i32();
    p.defense_value = r.read_i32();
    p.level = r.read_i32();
    p.xp = r.read_i32();
    p.max_xp = r.read_i32();
    p.energy = r.read_i32();
    p.kills = r.read_i32();
    p.regen_counter = r.read_i32();
    if (version >= 10) {
        p.light_radius = r.read_i32();
    }
    read_equipment(r, p.equipment, version);
    read_inventory(r, p.inventory, version);
    // v12: character identity, attributes, skills, reputation
    if (version >= 12) {
        p.name = r.read_string();
        p.race = static_cast<Race>(r.read_u8());
        p.player_class = static_cast<PlayerClass>(r.read_u8());
        p.attributes.strength = r.read_i32();
        p.attributes.agility = r.read_i32();
        p.attributes.toughness = r.read_i32();
        p.attributes.intelligence = r.read_i32();
        p.attributes.willpower = r.read_i32();
        p.attributes.luck = r.read_i32();
        p.attribute_points = r.read_i32();
        p.dodge_value = r.read_i32();
        p.resistances.acid = r.read_i32();
        p.resistances.electrical = r.read_i32();
        p.resistances.cold = r.read_i32();
        p.resistances.heat = r.read_i32();
        p.skill_points = r.read_i32();
        uint32_t skill_count = r.read_u32();
        p.learned_skills.resize(skill_count);
        for (uint32_t i = 0; i < skill_count; ++i) {
            p.learned_skills[i] = static_cast<SkillId>(r.read_u32());
        }
        uint32_t rep_count = r.read_u32();
        p.reputation.resize(rep_count);
        for (uint32_t i = 0; i < rep_count; ++i) {
            p.reputation[i].faction_name = r.read_string();
            p.reputation[i].reputation = r.read_i32();
        }
        // Blueprints
        uint32_t bp_count = r.read_u32();
        p.learned_blueprints.resize(bp_count);
        for (uint32_t i = 0; i < bp_count; ++i) {
            p.learned_blueprints[i].source_item_id = r.read_u32();
            p.learned_blueprints[i].name = r.read_string();
            p.learned_blueprints[i].description = r.read_string();
        }
        // Journal
        uint32_t journal_count = r.read_u32();
        p.journal.resize(journal_count);
        for (uint32_t i = 0; i < journal_count; ++i) {
            p.journal[i].category = static_cast<JournalCategory>(r.read_u8());
            p.journal[i].title = r.read_string();
            p.journal[i].technical = r.read_string();
            p.journal[i].personal = r.read_string();
            p.journal[i].timestamp = r.read_string();
            p.journal[i].world_tick = r.read_i32();
            if (version >= 16) {
                p.journal[i].quest_id = r.read_string();
            }
        }
    }
    // v14: starship
    if (version >= 14) {
        p.ship.name = r.read_string();
        p.ship.type = r.read_string();
        for (int i = 0; i < ship_slot_count; ++i) {
            p.ship.slot_ref(static_cast<ShipSlot>(i)) = read_optional_item(r, version);
        }
        // Ship cargo
        uint32_t cargo_count = r.read_u32();
        p.ship.cargo.resize(cargo_count);
        for (uint32_t i = 0; i < cargo_count; ++i) {
            p.ship.cargo[i] = read_item(r, version);
        }
    }
    // v15: tab help seen
    if (version >= 15) {
        p.tab_help_seen = r.read_u16();
    }
}

static Npc read_npc(BinaryReader& r, uint32_t version) {
    Npc npc;
    npc.x = r.read_i32();
    npc.y = r.read_i32();
    if (version >= 18) {
        npc.npc_role = static_cast<NpcRole>(r.read_u8());
    } else {
        r.read_u8();  // skip legacy glyph
        r.read_u8();  // skip legacy color
    }
    npc.name = r.read_string();
    npc.role = r.read_string();
    npc.race = static_cast<Race>(r.read_u8());

    // Reconstruct npc_role from role string for pre-v18 saves
    if (version < 18) {
        if (npc.role == "Station Keeper") npc.npc_role = NpcRole::StationKeeper;
        else if (npc.role == "Merchant") npc.npc_role = NpcRole::Merchant;
        else if (npc.role == "Drifter") npc.npc_role = NpcRole::Drifter;
        else if (npc.role == "Xytomorph") npc.npc_role = NpcRole::Xytomorph;
        else if (npc.role == "Food Merchant") npc.npc_role = NpcRole::FoodMerchant;
        else if (npc.role == "Medic") npc.npc_role = NpcRole::Medic;
        else if (npc.role == "Station Commander") npc.npc_role = NpcRole::Commander;
        else if (npc.role == "Arms Dealer") npc.npc_role = NpcRole::ArmsDealer;
        else if (npc.role == "Astronomer") npc.npc_role = NpcRole::Astronomer;
        else if (npc.role == "Engineer") npc.npc_role = NpcRole::Engineer;
        else if (npc.role == "Stellar Engineer") npc.npc_role = NpcRole::Nova;
        // Default is Civilian (covers all civilian role titles)
    }
    npc.hp = r.read_i32();
    npc.max_hp = r.read_i32();
    npc.disposition = static_cast<Disposition>(r.read_u8());
    { bool was_invulnerable = r.read_u8() != 0;
      if (was_invulnerable) add_effect(npc.effects, make_invulnerable()); }
    npc.quickness = r.read_i32();
    npc.energy = r.read_i32();
    npc.level = r.read_i32();
    npc.elite = r.read_u8() != 0;
    npc.base_xp = r.read_i32();
    npc.base_damage = r.read_i32();

    uint8_t has_talk = r.read_u8();
    uint8_t has_shop = r.read_u8();
    uint8_t has_quest = r.read_u8();

    if (has_talk) {
        TalkTrait t;
        t.greeting = r.read_string();
        read_dialog_nodes(r, t.nodes);
        npc.interactions.talk = std::move(t);
    }
    if (has_shop) {
        ShopTrait s;
        s.shop_name = r.read_string();
        if (version >= 11) {
            uint32_t count = r.read_u32();
            s.inventory.resize(count);
            for (uint32_t i = 0; i < count; ++i) s.inventory[i] = read_item(r, version);
        }
        npc.interactions.shop = std::move(s);
    }
    if (has_quest) {
        QuestTrait q;
        q.quest_intro = r.read_string();
        read_dialog_nodes(r, q.nodes);
        npc.interactions.quest = std::move(q);
    }

    return npc;
}

static void read_map_section(BinaryReader& r, MapState& ms, uint32_t version) {
    ms.map_id = r.read_u32();
    auto map_type = static_cast<MapType>(r.read_u8());
    Biome biome = Biome::Station;
    if (version >= 2) {
        biome = static_cast<Biome>(r.read_u8());
    } else {
        biome = (map_type == MapType::SpaceStation) ? Biome::Station : Biome::Rocky;
    }
    int width = r.read_i32();
    int height = r.read_i32();
    std::string location = r.read_string();

    int area = width * height;

    std::vector<Tile> tiles(area);
    for (int i = 0; i < area; ++i) tiles[i] = static_cast<Tile>(r.read_u8());

    std::vector<int> rids(area);
    for (int i = 0; i < area; ++i) rids[i] = r.read_i32();

    uint32_t region_count = r.read_u32();
    std::vector<Region> regions(region_count);
    for (auto& reg : regions) {
        reg.type = static_cast<RegionType>(r.read_u8());
        reg.lit = r.read_u8() != 0;
        reg.flavor = static_cast<RoomFlavor>(r.read_u8());
        if (version >= 3) {
            reg.features = static_cast<RoomFeature>(r.read_u16());
        } else {
            reg.features = default_features(reg.flavor);
        }
        reg.name = r.read_string();
        reg.enter_message = r.read_string();
    }

    std::vector<char> backdrop(area);
    r.read_bytes(backdrop.data(), area);

    // Glyph overrides (v8+)
    std::vector<uint8_t> glyph_ov(area, 0);
    if (version >= 8) {
        r.read_bytes(glyph_ov.data(), area);
    }

    ms.tilemap.load_from(width, height, map_type, biome, std::move(location),
                         std::move(tiles), std::move(rids),
                         std::move(regions), std::move(backdrop));
    ms.tilemap.load_glyph_overrides(std::move(glyph_ov));

    // Visibility
    std::vector<Visibility> cells(area);
    for (int i = 0; i < area; ++i) cells[i] = static_cast<Visibility>(r.read_u8());
    ms.visibility.load_from(width, height, std::move(cells));

    // NPCs
    uint32_t npc_count = r.read_u32();
    ms.npcs.resize(npc_count);
    for (uint32_t i = 0; i < npc_count; ++i) {
        ms.npcs[i] = read_npc(r, version);
    }

    // Ground items — may be absent in old saves (section guard handles it)
    uint32_t gi_count = r.read_u32();
    ms.ground_items.resize(gi_count);
    for (uint32_t i = 0; i < gi_count; ++i) {
        ms.ground_items[i].x = r.read_i32();
        ms.ground_items[i].y = r.read_i32();
        ms.ground_items[i].item = read_item(r, version);
    }

    // Fixtures (v3+)
    if (version >= 3) {
        uint32_t fixture_count = r.read_u32();
        std::vector<FixtureData> fixtures(fixture_count);
        for (auto& f : fixtures) {
            f.type = static_cast<FixtureType>(r.read_u8());
            if (version < 17) {
                r.read_u8();  // skip legacy glyph
                r.read_u8();  // skip legacy color
            }
            f.passable = r.read_u8() != 0;
            f.interactable = r.read_u8() != 0;
            f.cooldown = r.read_i32();
            f.last_used_tick = r.read_i32();
        }
        std::vector<int> fids(area);
        for (int i = 0; i < area; ++i) fids[i] = r.read_i32();

        // Migrate Debris fixtures from old saves (v3-v16)
        if (version < 17) {
            for (int i = 0; i < area; ++i) {
                int fid = fids[i];
                if (fid >= 0 && fid < static_cast<int>(fixtures.size())) {
                    auto& f = fixtures[fid];
                    if (f.type == FixtureType::Debris) {
                        if (f.passable) {
                            // Passable debris becomes floor — remove fixture
                            fids[i] = -1;
                            ms.tilemap.set(i % width, i / width, Tile::Floor);
                        } else {
                            f.type = FixtureType::NaturalObstacle;
                        }
                    }
                }
            }
        }

        ms.tilemap.load_fixtures(std::move(fixtures), std::move(fids));

        bool hub = r.read_u8() != 0;
        ms.tilemap.set_hub(hub);
    }
}

static void read_messages_section(BinaryReader& r, std::deque<std::string>& msgs) {
    uint32_t count = r.read_u32();
    msgs.clear();
    for (uint32_t i = 0; i < count; ++i) {
        msgs.push_back(r.read_string());
    }
}

static void read_stash_section(BinaryReader& r, std::vector<Item>& stash, uint32_t version = 14) {
    uint32_t count = r.read_u32();
    stash.resize(count);
    for (uint32_t i = 0; i < count; ++i) stash[i] = read_item(r, version);
}

static void read_navigation_section(BinaryReader& r, NavigationData& nav, uint32_t version) {
    nav.current_system_id = r.read_u32();
    nav.navi_range = r.read_i32();
    if (version >= 5) {
        nav.current_body_index = r.read_i32();
        if (version >= 6) {
            nav.current_moon_index = r.read_i32();
            nav.at_station = r.read_u8() != 0;
            nav.on_ship = r.read_u8() != 0;
        } else {
            nav.current_moon_index = -1;
            nav.at_station = r.read_u8() != 0;
            nav.on_ship = false;
        }
    } else {
        nav.current_body_index = -1;
        nav.current_moon_index = -1;
        nav.at_station = true;
        nav.on_ship = false;
    }
    uint32_t count = r.read_u32();
    nav.systems.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        auto& sys = nav.systems[i];
        sys.id = r.read_u32();
        sys.name = r.read_string();
        sys.star_class = static_cast<StarClass>(r.read_u8());
        sys.binary = r.read_u8() != 0;
        sys.has_station = r.read_u8() != 0;
        sys.planet_count = r.read_i32();
        sys.asteroid_belts = r.read_i32();
        sys.danger_level = r.read_i32();
        sys.gx = r.read_f32();
        sys.gy = r.read_f32();
        sys.discovered = r.read_u8() != 0;

        // v4: celestial bodies
        if (version >= 4) {
            sys.bodies_generated = r.read_u8() != 0;
            if (sys.bodies_generated) {
                uint16_t body_count = r.read_u16();
                sys.bodies.resize(body_count);
                for (auto& body : sys.bodies) {
                    body.name = r.read_string();
                    body.type = static_cast<BodyType>(r.read_u8());
                    body.atmosphere = static_cast<Atmosphere>(r.read_u8());
                    body.temperature = static_cast<Temperature>(r.read_u8());
                    body.resources = r.read_u16();
                    body.size = r.read_u8();
                    body.moons = r.read_u8();
                    body.orbital_distance = r.read_f32();
                    body.landable = r.read_u8() != 0;
                    body.explored = r.read_u8() != 0;
                    body.has_dungeon = r.read_u8() != 0;
                    body.danger_level = r.read_i32();
                    // v10: day length
                    if (version >= 10) {
                        body.day_length = r.read_i32();
                    }
                }
            }
        }
    }
}

static void read_game_state_section(BinaryReader& r, SaveData& data) {
    data.current_region = r.read_i32();
    // v20: widget bitfield replaces active_tab (was i32)
    if (data.version >= 20) {
        data.active_widgets = r.read_u8();
        data.focused_widget = r.read_u8();
    } else {
        // Migrate: old active_tab was i32 (0=Messages,1=Equip,2=Ship,3=Wait)
        int old_tab = r.read_i32();
        data.active_widgets = 1; // Messages on
        if (old_tab == 3) data.active_widgets |= (1 << 1); // Wait was active
        data.focused_widget = (old_tab == 3) ? 1 : 0;
    }
    data.panel_visible = r.read_u8() != 0;
    data.death_message = r.read_string();
    // v7/v9: surface mode (v7-8 stored bool on_overworld, v9+ stores surface_mode u8)
    if (data.version >= 9) {
        data.surface_mode = r.read_u8();
        data.overworld_x = r.read_i32();
        data.overworld_y = r.read_i32();
    } else if (data.version >= 7) {
        bool on_ow = r.read_u8() != 0;
        data.surface_mode = on_ow ? 2 : 0; // 2=Overworld, 0=Dungeon
        data.overworld_x = r.read_i32();
        data.overworld_y = r.read_i32();
    }
    // v16: zone position + lost state
    if (data.version >= 16) {
        data.zone_x = r.read_i32();
        data.zone_y = r.read_i32();
        data.lost = r.read_u8() != 0;
        data.lost_moves = r.read_i32();
    }
    // v10: day clock
    if (data.version >= 10) {
        data.local_tick = r.read_i32();
        data.local_ticks_per_day = r.read_i32();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<SaveSlot> list_saves() {
    std::vector<SaveSlot> slots;
    auto dir = save_directory();
    if (!std::filesystem::exists(dir)) return slots;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".astra") continue;

        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) continue;

        SaveHeader h{};
        if (!read_header(in, h)) continue;

        SaveSlot slot;
        slot.filename = entry.path().stem().string();
        slot.location = h.location;
        slot.player_level = h.player_level;
        slot.world_tick = h.world_tick;
        slot.kills = h.kills;
        slot.xp = h.xp;
        slot.money = h.money;
        slot.timestamp = h.timestamp;
        slot.dead = h.dead != 0;
        slot.valid = true;
        slot.death_message = h.death_cause;
        slots.push_back(std::move(slot));
    }

    return slots;
}

bool write_save(const std::string& name, const SaveData& data) {
    auto dir = save_directory();
    std::filesystem::create_directories(dir);

    auto path = dir / (name + ".astra");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    write_header(out, data);

    BinaryWriter w(out);
    write_player_section(w, data.player);
    for (const auto& ms : data.maps) {
        write_map_section(w, ms);
    }
    write_messages_section(w, data.messages);
    write_game_state_section(w, data);
    if (!data.stash.empty()) {
        write_stash_section(w, data.stash);
    }
    if (!data.navigation.systems.empty()) {
        write_navigation_section(w, data.navigation);
    }
    if (!data.active_quests.empty() || !data.completed_quests.empty() ||
        !data.quest_locations.empty()) {
        write_quest_section(w, data);
    }

    // Sentinel
    out.write("END\0", 4);
    uint32_t zero = 0;
    out.write(reinterpret_cast<const char*>(&zero), 4);

    return out.good();
}

bool read_save(const std::string& name, SaveData& data) {
    auto path = save_directory() / (name + ".astra");
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    SaveHeader h{};
    if (!read_header(in, h)) return false;

    data.version = h.version;
    data.seed = h.seed;
    data.world_tick = h.world_tick;
    data.dead = h.dead != 0;
    data.death_message = h.death_cause;

    BinaryReader r(in);

    // Read sections by tag
    while (in.good()) {
        char tag[4]{};
        uint32_t size = 0;
        if (!r.read_section_header(tag, size)) break;

        if (std::memcmp(tag, "END\0", 4) == 0) break;

        std::streampos section_start = in.tellg();

        if (std::memcmp(tag, "PLYR", 4) == 0) {
            read_player_section(r, data.player, data.version);
        } else if (std::memcmp(tag, "MPDT", 4) == 0) {
            MapState ms;
            read_map_section(r, ms, data.version);
            data.maps.push_back(std::move(ms));
        } else if (std::memcmp(tag, "MSGS", 4) == 0) {
            read_messages_section(r, data.messages);
        } else if (std::memcmp(tag, "GSTA", 4) == 0) {
            read_game_state_section(r, data);
        } else if (std::memcmp(tag, "STSH", 4) == 0) {
            read_stash_section(r, data.stash, data.version);
        } else if (std::memcmp(tag, "STAR", 4) == 0) {
            read_navigation_section(r, data.navigation, data.version);
        } else if (std::memcmp(tag, "QUST", 4) == 0) {
            read_quest_section(r, data);
        } else {
            // Unknown section — skip
            r.skip(size);
            continue;
        }

        // Ensure we consumed exactly the right number of bytes
        std::streampos expected = section_start + static_cast<std::streamoff>(size);
        std::streampos actual = in.tellg();
        if (actual != expected) {
            in.seekg(expected);
        }
    }

    data.current_map_id = h.current_map_id;
    return in.good() || in.eof();
}

bool delete_save(const std::string& name) {
    auto path = save_directory() / (name + ".astra");
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

} // namespace astra
