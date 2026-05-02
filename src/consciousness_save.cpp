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
    void write_u16(uint16_t v) { out_.write(reinterpret_cast<const char*>(&v), 2); }
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
    uint16_t read_u16() { uint16_t v = 0; in_.read(reinterpret_cast<char*>(&v), 2); return v; }
    uint32_t read_u32() { uint32_t v = 0; in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    uint64_t read_u64() { uint64_t v = 0; in_.read(reinterpret_cast<char*>(&v), 8); return v; }
    int32_t  read_i32() { int32_t v = 0; in_.read(reinterpret_cast<char*>(&v), 4); return v; }
    std::string read_str() {
        uint32_t n = read_u32();
        std::string s(n, '\0');
        in_.read(s.data(), static_cast<std::streamsize>(n));
        return s;
    }
private:
    std::ifstream& in_;
};

// ---------------------------------------------------------------------------
// GridSector body serialization.
// Only persists the fields that matter for consciousness.dat: grid geometry,
// tiles, and spawn position. GatewayLinks are runtime-resolved and rebuilt
// when the sector is visited, so they are not persisted here.
// ---------------------------------------------------------------------------

static void write_grid_sector(Writer& w, const GridSector& s) {
    w.write_i32(s.w);
    w.write_i32(s.h);
    w.write_u32(static_cast<uint32_t>(s.tiles.size()));
    for (GridTile t : s.tiles) w.write_u8(static_cast<uint8_t>(t));
    w.write_i32(s.spawn_x);
    w.write_i32(s.spawn_y);
}

static GridSector read_grid_sector(Reader& r) {
    GridSector s;
    s.w = r.read_i32();
    s.h = r.read_i32();
    uint32_t tile_n = r.read_u32();
    s.tiles.resize(tile_n);
    for (uint32_t i = 0; i < tile_n; ++i)
        s.tiles[i] = static_cast<GridTile>(r.read_u8());
    s.spawn_x = r.read_i32();
    s.spawn_y = r.read_i32();
    return s;
}

// ---------------------------------------------------------------------------
// SectorRuntimeState serialization (v2, Task 15).
// Mirrors save_file.cpp logic for mutations and killed_ice overlay.
// ---------------------------------------------------------------------------

static void write_sector_runtime_state(Writer& w, const SectorRuntimeState& s) {
    w.write_u32(static_cast<uint32_t>(s.mutations.size()));
    for (const auto& m : s.mutations) {
        w.write_u8(m.x);
        w.write_u8(m.y);
        w.write_u8(static_cast<uint8_t>(m.new_tile));
    }
    w.write_u32(static_cast<uint32_t>(s.killed_ice.size()));
    for (const auto& [x, y] : s.killed_ice) {
        w.write_u8(x);
        w.write_u8(y);
    }
}

static SectorRuntimeState read_sector_runtime_state(Reader& r) {
    SectorRuntimeState s;
    uint32_t nm = r.read_u32();
    s.mutations.resize(nm);
    for (auto& m : s.mutations) {
        m.x = r.read_u8();
        m.y = r.read_u8();
        m.new_tile = static_cast<GridTile>(r.read_u8());
    }
    uint32_t ni = r.read_u32();
    s.killed_ice.resize(ni);
    for (auto& [x, y] : s.killed_ice) {
        x = r.read_u8();
        y = r.read_u8();
    }
    return s;
}

// ---------------------------------------------------------------------------
// Item body serialization — mirrors save_file.cpp's write_item / read_item.
// Duplicated here because those helpers are static (file-local). When Plan 4
// is complete and save_file helpers are lifted to a shared header, this copy
// should be removed. Keep in sync with save_file.cpp's item schema.
// ---------------------------------------------------------------------------

static void write_stat_mods(Writer& w, const StatModifiers& m) {
    w.write_i32(m.av);
    w.write_i32(m.dv);
    w.write_i32(m.max_hp);
    w.write_i32(m.view_radius);
    w.write_i32(m.quickness);
    w.write_i32(m.willpower);
}

static StatModifiers read_stat_mods(Reader& r) {
    StatModifiers m;
    m.av          = r.read_i32();
    m.dv          = r.read_i32();
    m.max_hp      = r.read_i32();
    m.view_radius = r.read_i32();
    m.quickness   = r.read_i32();
    m.willpower   = r.read_i32();
    return m;
}

static void write_item(Writer& w, const Item& item) {
    w.write_u32(item.id);
    w.write_str(item.name);
    w.write_str(item.description);
    w.write_u8(static_cast<uint8_t>(item.type));
    w.write_u8(item.slot.has_value() ? 1 : 0);
    if (item.slot) w.write_u8(static_cast<uint8_t>(*item.slot));
    w.write_u8(static_cast<uint8_t>(item.rarity));
    w.write_u16(item.item_def_id);
    w.write_i32(item.weight);
    w.write_u8(item.stackable ? 1 : 0);
    w.write_i32(item.stack_count);
    w.write_i32(item.buy_value);
    w.write_i32(item.sell_value);
    write_stat_mods(w, item.modifiers);
    w.write_i32(item.item_level);
    w.write_i32(item.level_requirement);
    w.write_i32(item.durability);
    w.write_i32(item.max_durability);
    w.write_u8(item.usable ? 1 : 0);
    w.write_u8(item.ranged.has_value() ? 1 : 0);
    if (item.ranged) w.write_i32(item.ranged->max_range);
    // energy / consumer
    w.write_u8(item.energy.has_value() ? 1 : 0);
    if (item.energy) {
        w.write_i32(item.energy->current);
        w.write_i32(item.energy->capacity);
    }
    w.write_u8(item.consumer.has_value() ? 1 : 0);
    if (item.consumer) w.write_i32(item.consumer->energy_per_use);
    // cell proc
    w.write_u8(item.proc.has_value() ? 1 : 0);
    if (item.proc) {
        w.write_u8(static_cast<uint8_t>(item.proc->kind));
        w.write_i32(item.proc->magnitude);
        w.write_i32(item.proc->duration);
        w.write_i32(item.proc->threshold);
        w.write_i32(item.proc->accumulator);
    }
    // toggleable
    w.write_u8(item.toggleable ? 1 : 0);
    w.write_u8(item.active ? 1 : 0);
    w.write_i32(item.drain_accumulator);
    // enhancement slots
    w.write_i32(item.enhancement_slots);
    w.write_u32(static_cast<uint32_t>(item.enhancements.size()));
    for (const auto& enh : item.enhancements) {
        w.write_u8(enh.filled ? 1 : 0);
        w.write_u8(enh.committed ? 1 : 0);
        w.write_u32(enh.material_id);
        w.write_str(enh.material_name);
        w.write_i32(enh.stat_bonus.av);
        w.write_i32(enh.stat_bonus.dv);
        w.write_i32(enh.stat_bonus.max_hp);
        w.write_i32(enh.stat_bonus.view_radius);
        w.write_i32(enh.stat_bonus.quickness);
        w.write_i32(enh.energy_bonus.capacity_bonus);
        w.write_i32(enh.energy_bonus.charge_rate_bonus);
        w.write_i32(enh.energy_bonus.discharge_efficiency);
        w.write_u8(enh.solar_panel.has_value() ? 1 : 0);
        if (enh.solar_panel) {
            w.write_u8(enh.solar_panel->active ? 1 : 0);
            w.write_i32(enh.solar_panel->energy_per_tick);
            w.write_i32(enh.solar_panel->tick_interval);
            w.write_i32(enh.solar_panel->accumulator);
        }
        w.write_u8(static_cast<uint8_t>(enh.module_kind));
    }
    // ship fields
    w.write_u8(item.ship_slot.has_value() ? 1 : 0);
    if (item.ship_slot) w.write_u8(static_cast<uint8_t>(*item.ship_slot));
    w.write_i32(item.ship_modifiers.hull_hp);
    w.write_i32(item.ship_modifiers.shield_hp);
    w.write_i32(item.ship_modifiers.warp_range);
    w.write_i32(item.ship_modifiers.cargo_capacity);
    // dice combat
    w.write_u8(static_cast<uint8_t>(item.damage_type));
    w.write_i32(item.damage_dice.count);
    w.write_i32(item.damage_dice.sides);
    w.write_i32(item.damage_dice.modifier);
    w.write_i32(item.type_affinity.kinetic);
    w.write_i32(item.type_affinity.plasma);
    w.write_i32(item.type_affinity.electrical);
    w.write_i32(item.type_affinity.cryo);
    w.write_i32(item.type_affinity.acid);
    // cooking
    w.write_u8(item.dish.has_value() ? 1 : 0);
    if (item.dish) {
        w.write_i32(item.dish->hunger_shift);
        w.write_i32(item.dish->hp_restore);
        w.write_u32(static_cast<uint32_t>(item.dish->granted.size()));
        for (EffectId eid : item.dish->granted) w.write_u32(static_cast<uint32_t>(eid));
    }
    w.write_u16(item.teaches_recipe_id);
    w.write_u16(item.teaches_schematic_id);
    // cyberdeck payload
    w.write_u8(item.deck.has_value() ? 1 : 0);
    if (item.deck) {
        const auto& d = *item.deck;
        w.write_i32(d.stats.ram_max);
        w.write_i32(d.stats.cpu);
        w.write_i32(d.stats.slots);
        w.write_i32(d.stats.stealth);
        w.write_i32(d.stats.cooling_rate);
        w.write_i32(d.stats.heat_cap);
        w.write_i32(d.ram_current);
        w.write_i32(d.heat_current);
        for (int i = 0; i < kCyberdeckMaxSlots; ++i) w.write_u16(d.loaded[i].program_def_id);
    }
    // program payload
    w.write_u8(item.program.has_value() ? 1 : 0);
    if (item.program) w.write_u16(static_cast<uint16_t>(item.program->id));
}

static Item read_item(Reader& r) {
    Item item;
    item.id          = r.read_u32();
    item.name        = r.read_str();
    item.description = r.read_str();
    item.type        = static_cast<ItemType>(r.read_u8());
    if (r.read_u8() != 0) item.slot = static_cast<EquipSlot>(r.read_u8());
    item.rarity       = static_cast<Rarity>(r.read_u8());
    item.item_def_id  = r.read_u16();
    item.weight       = r.read_i32();
    item.stackable    = r.read_u8() != 0;
    item.stack_count  = r.read_i32();
    item.buy_value    = r.read_i32();
    item.sell_value   = r.read_i32();
    item.modifiers    = read_stat_mods(r);
    item.item_level        = r.read_i32();
    item.level_requirement = r.read_i32();
    item.durability        = r.read_i32();
    item.max_durability    = r.read_i32();
    item.usable = r.read_u8() != 0;
    if (r.read_u8() != 0) { RangedData rd; rd.max_range = r.read_i32(); item.ranged = rd; }
    // energy / consumer
    if (r.read_u8() != 0) {
        EnergyStore e; e.current = r.read_i32(); e.capacity = r.read_i32(); item.energy = e;
    }
    if (r.read_u8() != 0) {
        EnergyConsumer c; c.energy_per_use = r.read_i32(); item.consumer = c;
    }
    // cell proc
    if (r.read_u8() != 0) {
        CellProc p;
        p.kind        = static_cast<CellProcKind>(r.read_u8());
        p.magnitude   = r.read_i32();
        p.duration    = r.read_i32();
        p.threshold   = r.read_i32();
        p.accumulator = r.read_i32();
        item.proc = p;
    }
    // toggleable
    item.toggleable       = r.read_u8() != 0;
    item.active           = r.read_u8() != 0;
    item.drain_accumulator = r.read_i32();
    // enhancement slots
    item.enhancement_slots = r.read_i32();
    uint32_t enh_n = r.read_u32();
    item.enhancements.resize(enh_n);
    for (uint32_t i = 0; i < enh_n; ++i) {
        item.enhancements[i].filled       = r.read_u8() != 0;
        item.enhancements[i].committed    = r.read_u8() != 0;
        item.enhancements[i].material_id   = r.read_u32();
        item.enhancements[i].material_name = r.read_str();
        item.enhancements[i].stat_bonus.av          = r.read_i32();
        item.enhancements[i].stat_bonus.dv          = r.read_i32();
        item.enhancements[i].stat_bonus.max_hp      = r.read_i32();
        item.enhancements[i].stat_bonus.view_radius = r.read_i32();
        item.enhancements[i].stat_bonus.quickness   = r.read_i32();
        item.enhancements[i].energy_bonus.capacity_bonus       = r.read_i32();
        item.enhancements[i].energy_bonus.charge_rate_bonus    = r.read_i32();
        item.enhancements[i].energy_bonus.discharge_efficiency = r.read_i32();
        if (r.read_u8() != 0) {
            SolarPanelData sp;
            sp.active           = r.read_u8() != 0;
            sp.energy_per_tick  = r.read_i32();
            sp.tick_interval    = r.read_i32();
            sp.accumulator      = r.read_i32();
            item.enhancements[i].solar_panel = sp;
        }
        item.enhancements[i].module_kind = static_cast<ModuleKind>(r.read_u8());
    }
    // ship fields
    if (r.read_u8() != 0) item.ship_slot = static_cast<ShipSlot>(r.read_u8());
    item.ship_modifiers.hull_hp        = r.read_i32();
    item.ship_modifiers.shield_hp      = r.read_i32();
    item.ship_modifiers.warp_range     = r.read_i32();
    item.ship_modifiers.cargo_capacity = r.read_i32();
    // dice combat
    item.damage_type            = static_cast<DamageType>(r.read_u8());
    item.damage_dice.count      = r.read_i32();
    item.damage_dice.sides      = r.read_i32();
    item.damage_dice.modifier   = r.read_i32();
    item.type_affinity.kinetic  = r.read_i32();
    item.type_affinity.plasma   = r.read_i32();
    item.type_affinity.electrical = r.read_i32();
    item.type_affinity.cryo     = r.read_i32();
    item.type_affinity.acid     = r.read_i32();
    // cooking
    if (r.read_u8() != 0) {
        DishOutput d;
        d.hunger_shift = r.read_i32();
        d.hp_restore   = r.read_i32();
        uint32_t gn = r.read_u32();
        d.granted.resize(gn);
        for (uint32_t i = 0; i < gn; ++i) d.granted[i] = static_cast<EffectId>(r.read_u32());
        item.dish = std::move(d);
    }
    item.teaches_recipe_id    = r.read_u16();
    item.teaches_schematic_id = r.read_u16();
    // cyberdeck payload
    if (r.read_u8() != 0) {
        CyberdeckData d;
        d.stats.ram_max      = r.read_i32();
        d.stats.cpu          = r.read_i32();
        d.stats.slots        = r.read_i32();
        d.stats.stealth      = r.read_i32();
        d.stats.cooling_rate = r.read_i32();
        d.stats.heat_cap     = r.read_i32();
        d.ram_current        = r.read_i32();
        d.heat_current       = r.read_i32();
        for (int i = 0; i < kCyberdeckMaxSlots; ++i)
            d.loaded[i].program_def_id = r.read_u16();
        item.deck = std::move(d);
    }
    // program payload
    if (r.read_u8() != 0) {
        ProgramData p; p.id = static_cast<ProgramId>(r.read_u16()); item.program = p;
    }
    return item;
}

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

        // deep_grid_base body (Task 9).
        w.write_u8(cs.deep_grid_base.has_value() ? 1 : 0);
        if (cs.deep_grid_base) write_grid_sector(w, *cs.deep_grid_base);

        // signature_program_rack body (Task 9).
        w.write_u32(static_cast<uint32_t>(cs.signature_program_rack.size()));
        for (const auto& item : cs.signature_program_rack) write_item(w, item);

        // v2 additions (Task 15) — empty until Cut 3/4 populate
        write_grid_sector(w, cs.deep_grid_base_v2);
        write_sector_runtime_state(w, cs.deep_grid_sector_state);

        w.write_u32(static_cast<uint32_t>(cs.warp_anchors.size()));
        for (const auto& a : cs.warp_anchors) {
            w.write_u16(a.galaxy_id);
            w.write_u32(a.region_seed);
            w.write_str(a.lan_display_name);
            w.write_i32(a.nodes_total);
            w.write_i32(a.nodes_cracked);
            w.write_u8(a.warpable ? 1 : 0);
        }

        w.write_u32(static_cast<uint32_t>(cs.ai_contacts_v2.size()));
        for (const auto& a : cs.ai_contacts_v2) {
            w.write_str(a.id);
            w.write_str(a.display_name);
            w.write_u16(a.origin_galaxy_id);
        }
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
    if (ver != CONSCIOUSNESS_SAVE_VERSION) {
        // v1 saves are rejected on Plan 5 launch (no migration)
        // This is acceptable per spec: "v1 saves are rejected (per no-backcompat)"
        return false;
    }

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

    // deep_grid_base body (Task 9).
    uint8_t base_present = r.read_u8();
    if (base_present) tmp.deep_grid_base = read_grid_sector(r);

    // signature_program_rack body (Task 9).
    uint32_t rack_n = r.read_u32();
    tmp.signature_program_rack.reserve(rack_n);
    for (uint32_t i = 0; i < rack_n; ++i)
        tmp.signature_program_rack.push_back(read_item(r));

    // v2 additions (Task 15) — empty until Cut 3/4 populate
    tmp.deep_grid_base_v2 = read_grid_sector(r);
    tmp.deep_grid_sector_state = read_sector_runtime_state(r);

    uint32_t na = r.read_u32();
    tmp.warp_anchors.resize(na);
    for (auto& a : tmp.warp_anchors) {
        a.galaxy_id = r.read_u16();
        a.region_seed = r.read_u32();
        a.lan_display_name = r.read_str();
        a.nodes_total = r.read_i32();
        a.nodes_cracked = r.read_i32();
        a.warpable = r.read_u8() != 0;
    }

    uint32_t nac = r.read_u32();
    tmp.ai_contacts_v2.resize(nac);
    for (auto& a : tmp.ai_contacts_v2) {
        a.id = r.read_str();
        a.display_name = r.read_str();
        a.origin_galaxy_id = r.read_u16();
    }

    if (!in) return false;

    out = std::move(tmp);
    return true;
}

bool delete_consciousness() {
    std::error_code ec;
    return std::filesystem::remove(consciousness_save_path(), ec) && !ec;
}

} // namespace astra
