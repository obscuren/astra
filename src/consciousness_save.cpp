#include "astra/consciousness_save.h"
#include "astra/save_file.h"  // for save_directory()

#include <fstream>
#include <vector>

namespace astra {

std::filesystem::path consciousness_save_path() {
    return save_directory() / "consciousness.dat";
}

namespace {
class Writer {
public:
    explicit Writer(std::ofstream& out) : out_(out) {}
    void write_u8(uint8_t v)   { out_.put(static_cast<char>(v)); }
    void write_u32(uint32_t v) { out_.write(reinterpret_cast<const char*>(&v), 4); }
    void write_u64(uint64_t v) { out_.write(reinterpret_cast<const char*>(&v), 8); }
    void write_i32(int32_t v)  { out_.write(reinterpret_cast<const char*>(&v), 4); }
    void write_str(const std::string& s) {
        write_u32(static_cast<uint32_t>(s.size()));
        out_.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
private:
    std::ofstream& out_;
};

class Reader {
public:
    explicit Reader(std::ifstream& in) : in_(in) {}
    uint8_t  read_u8()  { char c; in_.get(c); return static_cast<uint8_t>(c); }
    uint32_t read_u32() { uint32_t v; in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    uint64_t read_u64() { uint64_t v; in_.read(reinterpret_cast<char*>(&v), 8); return v; }
    int32_t  read_i32() { int32_t v;  in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    std::string read_str() {
        uint32_t n = read_u32();
        std::string s(n, '\0');
        in_.read(s.data(), static_cast<std::streamsize>(n));
        return s;
    }
private:
    std::ifstream& in_;
};
} // namespace

bool write_consciousness(const ConsciousnessSave& cs) {
    auto dir = save_directory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto tmp        = dir / "consciousness.dat.tmp";
    auto final_path = consciousness_save_path();

    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out) return false;
        Writer w(out);
        w.write_u32(cs.version);
        w.write_u64(cs.consciousness_id);
        w.write_u32(cs.rebirth_count);
        w.write_u8(cs.seen_first_rebirth ? 1 : 0);

        w.write_u32(static_cast<uint32_t>(cs.lore_archive.size()));
        for (const auto& f : cs.lore_archive) {
            w.write_str(f.archive_id);
            w.write_u32(f.galaxy_seed_origin);
            w.write_i32(f.world_tick_origin);
        }

        w.write_i32(cs.grid_currency);

        w.write_u32(static_cast<uint32_t>(cs.ai_contacts.size()));
        for (const auto& c : cs.ai_contacts) {
            w.write_u32(c.faction_id);
            w.write_i32(c.reputation);
        }

        // Body fields deferred to Task 9 — write flag/count only.
        w.write_u8(cs.deep_grid_base.has_value() ? 1 : 0);
        w.write_u32(static_cast<uint32_t>(cs.signature_program_rack.size()));
    }

    std::filesystem::rename(tmp, final_path, ec);
    return !ec;
}

bool read_consciousness(ConsciousnessSave& out) {
    auto path = consciousness_save_path();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    Reader r(in);

    ConsciousnessSave tmp;

    uint32_t ver = r.read_u32();
    if (ver != CONSCIOUSNESS_SAVE_VERSION) return false;

    tmp.version            = ver;
    tmp.consciousness_id   = r.read_u64();
    tmp.rebirth_count      = r.read_u32();
    tmp.seen_first_rebirth = r.read_u8() != 0;

    uint32_t lore_n = r.read_u32();
    tmp.lore_archive.reserve(lore_n);
    for (uint32_t i = 0; i < lore_n; ++i) {
        LoreFragmentRef f;
        f.archive_id         = r.read_str();
        f.galaxy_seed_origin = r.read_u32();
        f.world_tick_origin  = r.read_i32();
        tmp.lore_archive.push_back(std::move(f));
    }

    tmp.grid_currency = r.read_i32();

    uint32_t ai_n = r.read_u32();
    tmp.ai_contacts.reserve(ai_n);
    for (uint32_t i = 0; i < ai_n; ++i) {
        AiContact c;
        c.faction_id = r.read_u32();
        c.reputation = r.read_i32();
        tmp.ai_contacts.push_back(std::move(c));
    }

    uint8_t base_present = r.read_u8();
    (void)base_present;   // body deferred to Task 9
    tmp.deep_grid_base = std::nullopt;

    uint32_t rack_n = r.read_u32();
    (void)rack_n;
    // tmp.signature_program_rack is already empty from default construction

    if (!in) return false;

    out = std::move(tmp);
    return true;
}

bool delete_consciousness() {
    std::error_code ec;
    return std::filesystem::remove(consciousness_save_path(), ec) && !ec;
}

} // namespace astra
