#include "astra/save_file.h"

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
    w.end_section(pos);
}

static void write_npc(BinaryWriter& w, const Npc& npc) {
    w.write_i32(npc.x);
    w.write_i32(npc.y);
    w.write_u8(static_cast<uint8_t>(npc.glyph));
    w.write_u8(static_cast<uint8_t>(npc.color));
    w.write_string(npc.name);
    w.write_string(npc.role);
    w.write_u8(static_cast<uint8_t>(npc.race));
    w.write_i32(npc.hp);
    w.write_i32(npc.max_hp);
    w.write_u8(static_cast<uint8_t>(npc.disposition));
    w.write_u8(npc.invulnerable ? 1 : 0);
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
        w.write_string(r.name);
        w.write_string(r.enter_message);
    }

    // Backdrop
    const auto& backdrop = tm.backdrop_data();
    w.write_bytes(backdrop.data(), backdrop.size());

    // Visibility
    const auto& vis = ms.visibility;
    const auto& cells = vis.cells();
    for (const auto& c : cells) w.write_u8(static_cast<uint8_t>(c));

    // NPCs
    w.write_u32(static_cast<uint32_t>(ms.npcs.size()));
    for (const auto& npc : ms.npcs) {
        write_npc(w, npc);
    }

    w.end_section(pos);
}

static void write_messages_section(BinaryWriter& w, const std::deque<std::string>& msgs) {
    auto pos = w.begin_section("MSGS");
    w.write_u32(static_cast<uint32_t>(msgs.size()));
    for (const auto& m : msgs) w.write_string(m);
    w.end_section(pos);
}

static void write_game_state_section(BinaryWriter& w, const SaveData& data) {
    auto pos = w.begin_section("GSTA");
    w.write_i32(data.current_region);
    w.write_i32(data.active_tab);
    w.write_u8(data.panel_visible ? 1 : 0);
    w.write_string(data.death_message);
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

static void read_player_section(BinaryReader& r, Player& p) {
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
}

static Npc read_npc(BinaryReader& r) {
    Npc npc;
    npc.x = r.read_i32();
    npc.y = r.read_i32();
    npc.glyph = static_cast<char>(r.read_u8());
    npc.color = static_cast<Color>(r.read_u8());
    npc.name = r.read_string();
    npc.role = r.read_string();
    npc.race = static_cast<Race>(r.read_u8());
    npc.hp = r.read_i32();
    npc.max_hp = r.read_i32();
    npc.disposition = static_cast<Disposition>(r.read_u8());
    npc.invulnerable = r.read_u8() != 0;
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

static void read_map_section(BinaryReader& r, MapState& ms) {
    ms.map_id = r.read_u32();
    auto map_type = static_cast<MapType>(r.read_u8());
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
        reg.name = r.read_string();
        reg.enter_message = r.read_string();
    }

    std::vector<char> backdrop(area);
    r.read_bytes(backdrop.data(), area);

    ms.tilemap.load_from(width, height, map_type, std::move(location),
                         std::move(tiles), std::move(rids),
                         std::move(regions), std::move(backdrop));

    // Visibility
    std::vector<Visibility> cells(area);
    for (int i = 0; i < area; ++i) cells[i] = static_cast<Visibility>(r.read_u8());
    ms.visibility.load_from(width, height, std::move(cells));

    // NPCs
    uint32_t npc_count = r.read_u32();
    ms.npcs.resize(npc_count);
    for (uint32_t i = 0; i < npc_count; ++i) {
        ms.npcs[i] = read_npc(r);
    }
}

static void read_messages_section(BinaryReader& r, std::deque<std::string>& msgs) {
    uint32_t count = r.read_u32();
    msgs.clear();
    for (uint32_t i = 0; i < count; ++i) {
        msgs.push_back(r.read_string());
    }
}

static void read_game_state_section(BinaryReader& r, SaveData& data) {
    data.current_region = r.read_i32();
    data.active_tab = r.read_i32();
    data.panel_visible = r.read_u8() != 0;
    data.death_message = r.read_string();
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
            read_player_section(r, data.player);
        } else if (std::memcmp(tag, "MPDT", 4) == 0) {
            MapState ms;
            read_map_section(r, ms);
            data.maps.push_back(std::move(ms));
        } else if (std::memcmp(tag, "MSGS", 4) == 0) {
            read_messages_section(r, data.messages);
        } else if (std::memcmp(tag, "GSTA", 4) == 0) {
            read_game_state_section(r, data);
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
