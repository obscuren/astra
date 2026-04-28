#include "astra/tinkering.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/item_ids.h"
#include "astra/loot_table.h"
#include "astra/player.h"

#include <algorithm>
#include <cmath>

namespace astra {

// ---------------------------------------------------------------------------
// Material catalog (24 entries — populated in a later task)
// ---------------------------------------------------------------------------

const std::vector<MaterialDef>& material_catalog() {
    using T = MaterialTier;
    // Color values use renderer.h Color enum cast to uint8_t.
    // glyph: '~' for junk-typed reagents, ',' for T1 pure-mats, '+' for T2, '*' for T3.
    static const std::vector<MaterialDef> catalog = {
        // --- T1 (junk-typed reagents) ---
        { 30,   "Scrap Metal",     T::Common,   '~', static_cast<uint8_t>(Color::DarkGray),     1, true  },
        { 31,   "Broken Circuit",  T::Common,   '~', static_cast<uint8_t>(Color::DarkGray),     2, true  },
        { 32,   "Empty Casing",    T::Common,   '~', static_cast<uint8_t>(Color::DarkGray),     1, true  },
        // --- T1 (pure-mat) ---
        { 7010, "Copper Wire",     T::Common,   ',', static_cast<uint8_t>(Color::Yellow),       2, false },
        { 7011, "Polymer Strip",   T::Common,   ',', static_cast<uint8_t>(Color::White),        2, false },
        { 7012, "Glass Shard",     T::Common,   ',', static_cast<uint8_t>(Color::Cyan),         1, false },
        { 7013, "Adhesive Resin",  T::Common,   ',', static_cast<uint8_t>(Color::BrightYellow), 2, false },
        { 7014, "Coolant Vial",    T::Common,   ',', static_cast<uint8_t>(Color::Blue),         3, false },
        // --- T2 (existing pure-mat) ---
        { 7001, "Nano-Fiber",      T::Uncommon, '+', static_cast<uint8_t>(Color::Cyan),         8, false },
        { 7002, "Power Core",      T::Uncommon, '+', static_cast<uint8_t>(Color::Yellow),      12, false },
        { 7003, "Circuit Board",   T::Uncommon, '+', static_cast<uint8_t>(Color::Green),       10, false },
        { 7004, "Alloy Ingot",     T::Uncommon, '+', static_cast<uint8_t>(Color::White),       10, false },
        // --- T2 (junk-typed reagents) ---
        { 47,   "Spare Parts",     T::Uncommon, '~', static_cast<uint8_t>(Color::Yellow),       6, true  },
        { 48,   "Circuitry",       T::Uncommon, '~', static_cast<uint8_t>(Color::Cyan),         8, true  },
        // --- T2 (new pure-mat) ---
        { 7020, "Nano Lattice",    T::Uncommon, '+', static_cast<uint8_t>(Color::BrightWhite), 14, false },
        { 7021, "Polished Lens",   T::Uncommon, '+', static_cast<uint8_t>(Color::Cyan),        12, false },
        { 7022, "Micro-Servo",     T::Uncommon, '+', static_cast<uint8_t>(Color::BrightYellow),14, false },
        { 7023, "Plasma Cartridge",T::Uncommon, '+', static_cast<uint8_t>(Color::Red),         16, false },
        // --- T3 ---
        { 7030, "Quantum Resonance Crystal", T::Rare, '*', static_cast<uint8_t>(Color::BrightMagenta), 50, false },
        { 7031, "Strange Strobing Crystal",  T::Rare, '*', static_cast<uint8_t>(Color::BrightWhite),   60, false },
        { 7032, "Prime Catalyst",            T::Rare, '*', static_cast<uint8_t>(Color::BrightYellow),  55, false },
        { 7033, "Prime Filament",            T::Rare, '*', static_cast<uint8_t>(Color::Cyan),          55, false },
        { 7034, "Voidshard",                 T::Rare, '*', static_cast<uint8_t>(Color::Magenta),       70, false },
        { 7035, "Phase Coil",                T::Rare, '*', static_cast<uint8_t>(Color::Blue),          65, false },
    };
    return catalog;
}

const MaterialDef* find_material(uint32_t material_id) {
    for (const auto& m : material_catalog())
        if (m.material_id == material_id) return &m;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Material effects
// ---------------------------------------------------------------------------

static const MaterialEffect s_material_effects[] = {
    {7002, "Power Core",    {2, 0, 0, 0, 0}, {}, std::nullopt},  // +2 AV
    {7003, "Circuit Board", {0, 0, 0, 1, 0}, {}, std::nullopt},  // +1 view
    {7004, "Alloy Ingot",   {0, 2, 0, 0, 0}, {}, std::nullopt},  // +2 DV
    // material_id matches Item::id from build_solar_panel_*, not the item_def_id constant.
    {2050, "Solar Panel",           {}, {},                      SolarPanelData{ true,  5, 2, 0 }},
    {2051, "Polished Solar Panel",  {}, {},                      SolarPanelData{ true,  8, 2, 0 }},
    {2052, "Prismatic Solar Panel", {}, {},                      SolarPanelData{ true, 12, 2, 0 }},
    // Energy mods: capacity / charge_rate / discharge_efficiency
    {2053, "Capacitor Coil",        {}, {30, 0, 0},              std::nullopt},
    {2054, "Charge Catalyst",       {}, { 0, 25, 0},             std::nullopt},
    {2055, "Polished Conduit",      {}, { 0, 0, 5},              std::nullopt},
    // Minor energy mods for custom cell builds (smaller magnitudes; meant to stack).
    {2056, "Reinforced Casing",     {}, {10, 0, 0},              std::nullopt},
    {2057, "Receptor Plate",        {}, { 0, 10, 0},             std::nullopt},
    {2058, "Brass Conduit",         {}, { 0, 0, 10},             std::nullopt},
    {2059, "Power Junction",        {}, {15, 10, 0},             std::nullopt},
    {2060, "Tuned Catalyst",        {}, { 0, 15, 8},             std::nullopt},
    // Accessory modules — no stat/energy bonus; promote host item to auto-mode.
    {2070, "AI Module",             {}, {},                      std::nullopt, ModuleKind::AiModule},
    {2071, "Light Sensor",          {}, {},                      std::nullopt, ModuleKind::LightSensor},
};

const MaterialEffect* get_material_effect(uint32_t material_id) {
    for (const auto& me : s_material_effects) {
        if (me.material_id == material_id) return &me;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Blueprint catalog
// ---------------------------------------------------------------------------

const std::vector<BlueprintEntry>& blueprint_catalog() {
    static const std::vector<BlueprintEntry> catalog = {
        // Ranged weapons
        {ItemType::RangedWeapon, "Plasma Emitter",
         "A superheated plasma projection system."},
        {ItemType::RangedWeapon, "Grip Assembly",
         "Ergonomic weapon grip with recoil dampening."},
        {ItemType::RangedWeapon, "Power Conduit",
         "Energy routing system for ranged weapons."},
        // Melee weapons
        {ItemType::MeleeWeapon, "Blade Housing",
         "Structural frame for edged weapons."},
        {ItemType::MeleeWeapon, "Hilt Assembly",
         "Balanced weapon handle with shock absorption."},
        {ItemType::MeleeWeapon, "Edge Material",
         "Molecular-honed cutting surface."},
        // Armor
        {ItemType::Armor, "Plating Alloy",
         "Composite metal alloy for defensive plating."},
        {ItemType::Armor, "Padding Weave",
         "Impact-absorbing fiber weave."},
        {ItemType::Armor, "Joint Mechanism",
         "Flexible joint system for armored mobility."},
        // Accessories
        {ItemType::Accessory, "Optic Module",
         "Enhanced optical sensor array."},
        {ItemType::Accessory, "Thruster Core",
         "Miniaturized propulsion system."},
        {ItemType::Accessory, "Storage Frame",
         "Structural frame for cargo containment."},
    };
    return catalog;
}

// ---------------------------------------------------------------------------
// Init enhancement slots
// ---------------------------------------------------------------------------

void init_enhancement_slots(Item& item) {
    switch (item.rarity) {
        case Rarity::Common:    item.enhancement_slots = 1; break;
        case Rarity::Uncommon:  item.enhancement_slots = 2; break;
        case Rarity::Rare:
        case Rarity::Epic:
        case Rarity::Legendary: item.enhancement_slots = 3; break;
    }
}

// ---------------------------------------------------------------------------
// Repair
// ---------------------------------------------------------------------------

int repair_cost(const Item& item) {
    if (item.max_durability <= 0) return 0;
    int missing = item.max_durability - item.durability;
    if (missing <= 0) return 0;
    return std::max(1, static_cast<int>(std::ceil(missing / 10.0)));
}

TinkerResult repair_item(Item& item, Player& player) {
    if (!player_has_skill(player, SkillId::BasicRepair))
        return {false, false, "Requires Basic Repair skill."};

    if (item.max_durability <= 0)
        return {false, false, "This item cannot be repaired."};

    if (item.durability >= item.max_durability)
        return {false, false, "Already at full durability."};

    int cost = repair_cost(item);

    // Find Nano-Fiber in inventory
    int fiber_idx = -1;
    for (int i = 0; i < static_cast<int>(player.inventory.items.size()); ++i) {
        if (player.inventory.items[i].id == 7001) {
            fiber_idx = i;
            break;
        }
    }
    if (fiber_idx < 0 || player.inventory.items[fiber_idx].stack_count < cost)
        return {false, false, "Need " + std::to_string(cost) + " Nano-Fiber. (have " +
                (fiber_idx >= 0 ? std::to_string(player.inventory.items[fiber_idx].stack_count) : "0") + ")"};

    // Consume Nano-Fiber
    player.inventory.items[fiber_idx].stack_count -= cost;
    if (player.inventory.items[fiber_idx].stack_count <= 0)
        player.inventory.items.erase(player.inventory.items.begin() + fiber_idx);

    item.durability = item.max_durability;
    return {true, false, "Repaired! Used " + std::to_string(cost) + " Nano-Fiber."};
}

// ---------------------------------------------------------------------------
// Enhancement
// ---------------------------------------------------------------------------

TinkerResult enhance_item(Item& item, int slot_index, uint32_t material_id, Player& player) {
    if (!player_has_skill(player, SkillId::BasicRepair))
        return {false, false, "Requires Basic Repair skill."};

    if (slot_index < 0 || slot_index >= item.enhancement_slots)
        return {false, false, "Slot is locked."};

    // Ensure enhancements vector is large enough
    while (static_cast<int>(item.enhancements.size()) <= slot_index)
        item.enhancements.push_back({});

    if (item.enhancements[slot_index].filled)
        return {false, false, "Slot already filled."};

    const MaterialEffect* effect = get_material_effect(material_id);
    if (!effect)
        return {false, false, "This material cannot be used for enhancement."};

    // Find and consume material from inventory
    int mat_idx = -1;
    for (int i = 0; i < static_cast<int>(player.inventory.items.size()); ++i) {
        if (player.inventory.items[i].id == material_id) {
            mat_idx = i;
            break;
        }
    }
    if (mat_idx < 0)
        return {false, false, "You don't have this material."};

    // Consume 1
    if (player.inventory.items[mat_idx].stack_count > 1) {
        player.inventory.items[mat_idx].stack_count--;
    } else {
        player.inventory.items.erase(player.inventory.items.begin() + mat_idx);
    }

    // Stage enhancement (pending — not applied until commit)
    auto& slot = item.enhancements[slot_index];
    slot.filled = true;
    slot.material_id = material_id;
    slot.material_name = effect->name;
    slot.stat_bonus = effect->stat_bonus;
    slot.energy_bonus = effect->energy_bonus;
    slot.solar_panel  = effect->solar_panel;
    slot.module_kind  = effect->module_kind;

    return {true, false, "Slotted " + std::string(effect->name) + ". [f] Assemble to apply."};
}

TinkerResult commit_enhancements(Item& item) {
    int applied = 0;
    for (auto& slot : item.enhancements) {
        if (slot.filled && !slot.committed) {
            // Apply bonus permanently
            item.modifiers.av += slot.stat_bonus.av;
            item.modifiers.dv += slot.stat_bonus.dv;
            item.modifiers.max_hp += slot.stat_bonus.max_hp;
            item.modifiers.view_radius += slot.stat_bonus.view_radius;
            item.modifiers.quickness += slot.stat_bonus.quickness;
            slot.committed = true;
            applied++;
        }
    }
    if (applied == 0)
        return {false, false, "Nothing to assemble."};
    return {true, false, "Assembled! " + std::to_string(applied) + " enhancement(s) applied to " + item.name + "."};
}

TinkerResult clear_enhancement_slot(Item& item, int slot_index, Player& player) {
    if (slot_index < 0 || slot_index >= static_cast<int>(item.enhancements.size()))
        return {false, false, "Invalid slot."};

    auto& slot = item.enhancements[slot_index];
    if (!slot.filled)
        return {false, false, "Slot is empty."};
    if (slot.committed)
        return {false, false, "Cannot remove committed enhancements."};

    // Return material to inventory
    const MaterialEffect* effect = get_material_effect(slot.material_id);
    if (effect) {
        bool merged = false;
        for (auto& inv_item : player.inventory.items) {
            if (inv_item.id == slot.material_id) {
                inv_item.stack_count++;
                merged = true;
                break;
            }
        }
        if (!merged) {
            // Rebuild the material item
            Item mat;
            mat.id = slot.material_id;
            mat.name = slot.material_name;
            mat.type = ItemType::CraftingMaterial;
            // Resolve item_def_id from material_id
            if (slot.material_id == 7001) mat.item_def_id = ITEM_NANO_FIBER;
            else if (slot.material_id == 7002) mat.item_def_id = ITEM_POWER_CORE;
            else if (slot.material_id == 7003) mat.item_def_id = ITEM_CIRCUIT_BOARD;
            else if (slot.material_id == 7004) mat.item_def_id = ITEM_ALLOY_INGOT;
            else if (slot.material_id == 2050) mat.item_def_id = ITEM_SOLAR_PANEL_COMMON;
            else if (slot.material_id == 2051) mat.item_def_id = ITEM_SOLAR_PANEL_UNCOMMON;
            else if (slot.material_id == 2052) mat.item_def_id = ITEM_SOLAR_PANEL_RARE;
            else if (slot.material_id == 2053) mat.item_def_id = ITEM_CAPACITOR_COIL;
            else if (slot.material_id == 2054) mat.item_def_id = ITEM_CHARGE_CATALYST;
            else if (slot.material_id == 2055) mat.item_def_id = ITEM_POLISHED_CONDUIT;
            else if (slot.material_id == 2056) mat.item_def_id = ITEM_REINFORCED_CASING;
            else if (slot.material_id == 2057) mat.item_def_id = ITEM_RECEPTOR_PLATE;
            else if (slot.material_id == 2058) mat.item_def_id = ITEM_BRASS_CONDUIT;
            else if (slot.material_id == 2059) mat.item_def_id = ITEM_POWER_JUNCTION;
            else if (slot.material_id == 2060) mat.item_def_id = ITEM_TUNED_CATALYST;
            mat.stackable = true;
            mat.stack_count = 1;
            mat.weight = 1;
            player.inventory.items.push_back(std::move(mat));
        }
    }

    std::string name = slot.material_name;
    slot = {}; // reset slot
    return {true, false, "Removed " + name + " from slot."};
}

bool has_pending_enhancements(const Item& item) {
    for (const auto& slot : item.enhancements)
        if (slot.filled && !slot.committed) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Analyze
// ---------------------------------------------------------------------------

TinkerResult analyze_item(Item& item, Player& player, std::mt19937& rng) {
    // Pre-flight failures: action didn't run, item is untouched.
    if (!player_has_skill(player, SkillId::Cat_Tinkering))
        return {false, false, "Requires Tinkering skill unlocked."};

    if (!item.slot.has_value())
        return {false, false, "Can only analyze equipment."};

    // Find blueprints for this item's type
    std::vector<const BlueprintEntry*> candidates;
    for (const auto& bp : blueprint_catalog()) {
        if (bp.category == item.type) candidates.push_back(&bp);
    }
    // Also check legacy Equipment type
    if (candidates.empty() && item.type == ItemType::Equipment) {
        for (const auto& bp : blueprint_catalog()) {
            if (bp.category == ItemType::RangedWeapon) candidates.push_back(&bp);
        }
    }
    if (candidates.empty())
        return {false, false, "Nothing to learn from this item."};

    // Pick a random blueprint not already known
    std::vector<const BlueprintEntry*> unknown;
    for (auto* bp : candidates) {
        bool known = false;
        for (const auto& learned : player.learned_blueprints) {
            if (learned.name == bp->name) { known = true; break; }
        }
        if (!known) unknown.push_back(bp);
    }
    if (unknown.empty())
        return {false, false, "You already know all blueprints from this type."};

    auto* chosen = unknown[std::uniform_int_distribution<size_t>(0, unknown.size() - 1)(rng)];

    // Survival chance: 50% base + 3% per INT above 10
    int survive_chance = 50 + std::max(0, (player.attributes.intelligence - 10)) * 3;
    bool survived = std::uniform_int_distribution<int>(0, 99)(rng) < survive_chance;

    // Learn the blueprint
    player.learned_blueprints.push_back({item.id, chosen->name, chosen->description});

    std::string msg = "Learned blueprint: " + std::string(chosen->name) + "!";
    if (!survived) {
        msg += " The item was destroyed in the process.";
    }

    // Action succeeded (blueprint learned). consumed=true only when the item
    // was destroyed by the survival roll.
    return {true, !survived, msg};
}

// ---------------------------------------------------------------------------
// Salvage
// ---------------------------------------------------------------------------

TinkerResult salvage_item(const Item& item, Player& player, std::mt19937& rng) {
    if (!player_has_skill(player, SkillId::Disassemble))
        return {false, false, "Requires Disassemble skill."};
    if (item.type == ItemType::QuestItem)
        return {false, false, "Cannot salvage quest items."};

    // Yield + tier weights per rarity (spec §5).
    int yield_min = 1, yield_max = 2;
    struct TierWeights { int t1, t2, t3; };
    TierWeights w{100, 0, 0};
    switch (item.rarity) {
        case Rarity::Common:    yield_min = 1; yield_max = 2; w = { 100,  0,  0 }; break;
        case Rarity::Uncommon:  yield_min = 2; yield_max = 3; w = {  70, 30,  0 }; break;
        case Rarity::Rare:      yield_min = 2; yield_max = 3; w = {  40, 60,  0 }; break;
        case Rarity::Epic:      yield_min = 3; yield_max = 3; w = {  20, 70, 10 }; break;
        case Rarity::Legendary: yield_min = 3; yield_max = 4; w = {   0, 60, 40 }; break;
    }
    int yield = std::uniform_int_distribution<int>(yield_min, yield_max)(rng);

    // Build per-tier candidate pools from the catalog.
    std::vector<uint32_t> t1_ids, t2_ids, t3_ids;
    for (const auto& m : material_catalog()) {
        switch (m.tier) {
            case MaterialTier::Common:   t1_ids.push_back(m.material_id); break;
            case MaterialTier::Uncommon: t2_ids.push_back(m.material_id); break;
            case MaterialTier::Rare:     t3_ids.push_back(m.material_id); break;
        }
    }

    auto draw_from = [&](const std::vector<uint32_t>& pool) -> uint32_t {
        if (pool.empty()) return 0;
        return pool[std::uniform_int_distribution<size_t>(0, pool.size() - 1)(rng)];
    };

    int total = w.t1 + w.t2 + w.t3;
    auto pick_tier = [&]() -> int {
        int roll = std::uniform_int_distribution<int>(0, total - 1)(rng);
        if (roll < w.t1) return 1;
        if (roll < w.t1 + w.t2) return 2;
        return 3;
    };

    int produced = 0;
    for (int i = 0; i < yield; ++i) {
        int tier = pick_tier();
        const auto& pool = (tier == 1) ? t1_ids : (tier == 2) ? t2_ids : t3_ids;
        uint32_t mid = draw_from(pool);
        if (mid == 0) {
            mid = draw_from(t1_ids);
            if (mid == 0) continue;
        }

        // Merge into existing stack if possible.
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == mid) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            // Map material_id (Item::id) to its item_def_id by walking the loot table.
            uint16_t def_id = 0;
            for (const auto& entry : loot_table_all_entries()) {
                Item probe = build_by_def_id(entry.item_def_id);
                if (probe.id == mid) { def_id = entry.item_def_id; break; }
            }
            if (def_id == 0) continue;
            player.inventory.items.push_back(build_by_def_id(def_id));
        }
        ++produced;
    }

    return {true, true, "Salvaged " + item.name + ". Received " + std::to_string(produced) + " materials."};
}

// ---------------------------------------------------------------------------
// Synthesizer
// ---------------------------------------------------------------------------

// Material IDs (now sourced from material_catalog()):
//   T1: 30=Scrap, 31=Broken Circuit, 32=Empty Casing,
//       7010=Copper Wire, 7011=Polymer Strip, 7012=Glass Shard,
//       7013=Adhesive Resin, 7014=Coolant Vial
//   T2: 7001=Nano-Fiber, 7002=Power Core, 7003=Circuit Board,
//       7004=Alloy Ingot, 47=Spare Parts, 48=Circuitry,
//       7020=Nano Lattice, 7021=Polished Lens, 7022=Micro-Servo,
//       7023=Plasma Cartridge
//   T3: 7030=QRC, 7031=Strange Strobing Crystal, 7032=Prime Catalyst,
//       7033=Prime Filament, 7034=Voidshard, 7035=Phase Coil
const std::vector<SynthesisRecipe>& synthesis_recipes() {
    static const std::vector<SynthesisRecipe> recipes = {
        {"Plasma Emitter", "Blade Housing", "Plasma Edge",
         "A blade wreathed in plasma energy. Burns on contact.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {8, 0, 0, 0, 0}, 60,
         // 2 Scrap + 1 Circuitry + 1 Power Core + 1 Alloy Ingot + 1 Plasma Cartridge
         { {30, 2}, {48, 1}, {7002, 1}, {7004, 1}, {7023, 1} }},

        {"Plating Alloy", "Thruster Core", "Thruster Plate",
         "Armored plating with integrated micro-thrusters for agile combat.",
         ItemType::Armor, EquipSlot::Body, ']',
         {0, 4, 0, 0, 3}, 80,
         // 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo
         { {30, 3}, {48, 1}, {7004, 2}, {7022, 1} }},

        {"Optic Module", "Power Conduit", "Targeting Array",
         "Advanced optics fused with a power feed. Enhances aim and awareness.",
         ItemType::Accessory, EquipSlot::Face, '&',
         {2, 0, 0, 3, 0}, 0,
         // 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens + 2 Copper Wire
         { {30, 1}, {48, 1}, {7003, 1}, {7021, 1}, {7010, 2} }},

        {"Edge Material", "Grip Assembly", "Dual-Edge",
         "A twin-bladed weapon with perfect balance. Strikes twice as fast.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {6, 0, 0, 0, 2}, 50,
         // 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Nano-Fiber
         { {30, 3}, {48, 1}, {7004, 2}, {7001, 1} }},

        {"Padding Weave", "Storage Frame", "Reinforced Pack",
         "A heavily padded cargo pack. Protects both you and your gear.",
         ItemType::Accessory, EquipSlot::Back, '\\',
         {0, 2, 3, 0, 0}, 0,
         // 2 Scrap + 2 Nano-Fiber + 1 Polymer Strip + 1 Adhesive Resin (no Circuitry — leather-y)
         { {30, 2}, {7001, 2}, {7011, 1}, {7013, 1} }},

        {"Power Conduit", "Thruster Core", "Overcharged Engine",
         "A hyperspace engine component running at dangerous output levels.",
         ItemType::ShipComponent, EquipSlot::Back, '#',
         {0, 0, 0, 0, 5}, 0,
         // 2 Scrap + 1 Circuitry + 2 Power Core + 1 Coolant Vial + 1 Plasma Cartridge
         { {30, 2}, {48, 1}, {7002, 2}, {7014, 1}, {7023, 1} }},

        {"Plating Alloy", "Joint Mechanism", "Articulated Armor",
         "Segmented armor that moves with you. Full protection, zero penalty.",
         ItemType::Armor, EquipSlot::Body, ']',
         {0, 5, 0, 0, 1}, 100,
         // 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo + 1 Nano Lattice
         { {30, 3}, {48, 1}, {7004, 2}, {7022, 1}, {7020, 1} }},

        {"Plasma Emitter", "Optic Module", "Guided Blaster",
         "A plasma weapon with auto-tracking optics. Rarely misses.",
         ItemType::RangedWeapon, EquipSlot::Missile, ')',
         {6, 0, 0, 1, 0}, 50,
         // 2 Scrap + 1 Circuitry + 1 Power Core + 1 Polished Lens + 1 Plasma Cartridge
         { {30, 2}, {48, 1}, {7002, 1}, {7021, 1}, {7023, 1} }},

        {"Blade Housing", "Joint Mechanism", "Combat Gauntlet",
         "An armored fist with embedded blades. Strike and defend as one.",
         ItemType::Armor, EquipSlot::LeftHand, '}',
         {3, 2, 0, 0, 0}, 70,
         // 2 Scrap + 1 Circuitry + 1 Alloy Ingot + 1 Nano-Fiber + 1 Micro-Servo
         { {30, 2}, {48, 1}, {7004, 1}, {7001, 1}, {7022, 1} }},

        {"Edge Material", "Plating Alloy", "Armored Blade",
         "A thick, heavy blade reinforced with armor plating. Hits like a wall.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {5, 3, 0, 0, 0}, 90,
         // 4 Scrap + 2 Alloy Ingot + 1 Nano-Fiber (no Circuitry — purely mechanical)
         { {30, 4}, {7004, 2}, {7001, 1} }},

        // Energy mod recipes — produce tinkering materials directly via custom builders.
        // Equipment-only fields (modifiers, durability, slot, glyph) are unused for these.
        {"Plating Alloy", "Storage Frame", "Reinforced Casing",
         "Energy mod. Adds +10 capacity to the host cell.",
         ItemType::CraftingMaterial, EquipSlot::Back, '*',
         {}, 0,
         // 2 Scrap + 1 Alloy Ingot + 1 Polymer Strip
         { {30, 2}, {7004, 1}, {7011, 1} },
         &build_reinforced_casing},

        {"Optic Module", "Power Conduit", "Receptor Plate",
         "Energy mod. +10% to incoming charge rate.",
         ItemType::CraftingMaterial, EquipSlot::Back, '*',
         {}, 0,
         // 1 Scrap + 1 Circuitry + 1 Copper Wire + 1 Polished Lens
         { {30, 1}, {48, 1}, {7010, 1}, {7021, 1} },
         &build_receptor_plate},

        {"Power Conduit", "Plating Alloy", "Brass Conduit",
         "Energy mod. +1 free unit per 10 transferred.",
         ItemType::CraftingMaterial, EquipSlot::Back, '*',
         {}, 0,
         // 1 Scrap + 1 Copper Wire + 1 Power Core
         { {30, 1}, {7010, 1}, {7002, 1} },
         &build_brass_conduit},

        // Accessory module recipes — produce behavioral modules via custom builders.
        {"Optic Module", "Joint Mechanism", "AI Module",
         "Adaptive control circuit. Slotted into an item to automate any manual trigger.",
         ItemType::CraftingMaterial, EquipSlot::Face, '*',
         {}, 0,
         // 2 Scrap + 1 Circuitry + 1 Circuit Board + 1 Nano Lattice + 1 QRC
         { {30, 2}, {48, 1}, {7003, 1}, {7020, 1}, {7030, 1} },
         &build_ai_module},

        {"Optic Module", "Padding Weave", "Light Sensor",
         "Photodiode array. Slotted into an item to auto-toggle anything that depends on ambient light.",
         ItemType::CraftingMaterial, EquipSlot::Face, '*',
         {}, 0,
         // 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens
         { {30, 1}, {48, 1}, {7003, 1}, {7021, 1} },
         &build_light_sensor},
    };
    return recipes;
}

const SynthesisRecipe* find_recipe(const std::string& bp1, const std::string& bp2) {
    for (const auto& r : synthesis_recipes()) {
        if ((bp1 == r.blueprint_1 && bp2 == r.blueprint_2) ||
            (bp1 == r.blueprint_2 && bp2 == r.blueprint_1))
            return &r;
    }
    return nullptr;
}

// Material matcher — recipe material_id can be either Item::id (T2 convention,
// e.g. 7001 for Nano-Fiber) or Item::item_def_id (junk reagents like Scrap
// Metal where item_def_id=30 but Item::id=6001). Match either form so all
// recipe shapes work consistently.
static bool item_matches_material(const Item& it, uint32_t material_id) {
    return it.id == material_id || it.item_def_id == material_id;
}

TinkerResult synthesize_item(const std::string& bp1, const std::string& bp2,
                              Player& player, std::mt19937& rng) {
    if (!player_has_skill(player, SkillId::Cat_Tinkering))
        return {false, false, "Requires Tinkering skill unlocked."};

    const auto* recipe = find_recipe(bp1, bp2);
    if (!recipe)
        return {false, false, "No known recipe for this combination."};

    // Check material costs
    for (const auto& req : recipe->material_costs) {
        int have = 0;
        for (const auto& it : player.inventory.items) {
            if (item_matches_material(it, req.material_id)) have += it.stack_count;
        }
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    // Consume materials
    for (const auto& req : recipe->material_costs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end() && needed > 0; ) {
            if (item_matches_material(*it, req.material_id)) {
                if (it->stack_count > needed) {
                    it->stack_count -= needed;
                    needed = 0;
                } else {
                    needed -= it->stack_count;
                    it = player.inventory.items.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    // Create result item.
    Item item;
    if (recipe->custom_builder) {
        // Material recipes (energy mods, etc.) — predefined builder fully populates the item.
        item = recipe->custom_builder();
    } else {
        // Equipment recipes — synthesize from recipe fields, scaled and rarity-rolled.
        item.id = 9000 + static_cast<uint32_t>(&*recipe - &synthesis_recipes()[0]);
        item.item_def_id = ITEM_SYNTH_PLASMA_EDGE + static_cast<uint16_t>(&*recipe - &synthesis_recipes()[0]);
        item.name = recipe->result_name;
        item.description = recipe->result_desc;
        item.type = recipe->result_type;
        if (recipe->result_slot != EquipSlot::Back || recipe->result_type != ItemType::ShipComponent)
            item.slot = recipe->result_slot;
        else
            item.slot = std::nullopt; // ship components have no equip slot
        item.modifiers = recipe->base_modifiers;
        item.max_durability = recipe->base_durability;
        item.durability = recipe->base_durability;
        item.weight = 3;

        scale_item_to_level(item, player.level);

        int luck_bonus = std::max(0, (player.attributes.luck - 10)) * 2;
        std::uniform_int_distribution<int> dist(0, 99);
        int roll = dist(rng) + luck_bonus;
        if (roll >= 99) item.rarity = Rarity::Legendary;
        else if (roll >= 95) item.rarity = Rarity::Epic;
        else if (roll >= 80) item.rarity = Rarity::Rare;
        else if (roll >= 50) item.rarity = Rarity::Uncommon;
        else item.rarity = Rarity::Common;

        init_enhancement_slots(item);

        int rarity_mult = 1 + static_cast<int>(item.rarity);
        item.buy_value = 100 * rarity_mult;
        item.sell_value = item.buy_value / 3;
    }

    std::string result_name = item.name;
    player.inventory.items.push_back(std::move(item));

    return {true, false, "Synthesized: " + result_name + "!"};
}

// ---------------------------------------------------------------------------
// Refinement recipes (junk -> T2 material)
// ---------------------------------------------------------------------------

const std::vector<RefinementRecipe>& refinement_recipes() {
    using R = RefinementRecipe;
    static const std::vector<R> recipes = {
        { "Smelt Alloy Ingot",          { {30, 3} },                        7004, ITEM_ALLOY_INGOT,      1 },
        { "Recover Circuit Board",      { {31, 2}, {7010, 1} },             7003, ITEM_CIRCUIT_BOARD,    1 },
        { "Spin Nano-Fiber",            { {32, 4}, {7013, 1} },             7001, ITEM_NANO_FIBER,       1 },
        { "Assemble Power Core",        { {47, 2}, {7010, 1} },             7002, ITEM_POWER_CORE,       1 },
        { "Build Circuitry",            { {31, 1}, {47, 1}, {7010, 1} },    48,   ITEM_CIRCUITRY,        1 },
        { "Polish Lens",                { {7012, 2}, {7011, 1} },           7021, ITEM_POLISHED_LENS,    1 },
        { "Tune Micro-Servo",           { {47, 2}, {7014, 1} },             7022, ITEM_MICRO_SERVO,      1 },
        { "Weave Nano Lattice",         { {7001, 3}, {7011, 1} },           7020, ITEM_NANO_LATTICE,     1 },
        { "Pressurize Plasma Cartridge",{ {7002, 2}, {7014, 1} },           7023, ITEM_PLASMA_CARTRIDGE, 1 },
    };
    return recipes;
}

TinkerResult refine_item(const RefinementRecipe& recipe, Player& player) {
    if (!player_has_skill(player, SkillId::BasicRepair))
        return {false, false, "Requires Basic Repair skill."};

    // Cost check
    for (const auto& req : recipe.inputs) {
        int have = 0;
        for (const auto& it : player.inventory.items)
            if (item_matches_material(it, req.material_id)) have += it.stack_count;
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    // Consume inputs
    for (const auto& req : recipe.inputs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end() && needed > 0; ) {
            if (item_matches_material(*it, req.material_id)) {
                if (it->stack_count > needed) { it->stack_count -= needed; needed = 0; }
                else { needed -= it->stack_count; it = player.inventory.items.erase(it); continue; }
            }
            ++it;
        }
    }

    // Produce output(s) - merge into existing stack if possible
    for (int i = 0; i < recipe.output_count; ++i) {
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == recipe.output_id) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            Item out = build_by_def_id(recipe.output_def_id);
            player.inventory.items.push_back(std::move(out));
        }
    }

    return {true, false, std::string("Refined: ") + recipe.name + "."};
}

// ---------------------------------------------------------------------------
// Schematic recipes (consumable crafting)
// ---------------------------------------------------------------------------

const std::vector<SchematicRecipe>& schematic_recipes() {
    static const std::vector<SchematicRecipe> recipes = {
        // Stims
        {  1, 7200, ITEM_HEALING_STIM,    "Healing Stim",    "Auto-injector. Restores HP.",
           { {30, 1}, {32, 1}, {7001, 1}, {7012, 1} }, 1 },
        {  2, 2003, ITEM_COMBAT_STIM,     "Adrenaline Stim", "Adrenaline injection.",
           { {30, 1}, {32, 1}, {7002, 1}, {7014, 1} }, 1 },
        {  3, 7201, ITEM_ENDURE_STIM,     "Endure Stim",     "Hardens you against damage.",
           { {30, 1}, {32, 1}, {7001, 1}, {7013, 1} }, 1 },
        {  4, 7202, ITEM_FOCUS_STIM,      "Focus Stim",      "Sharpens senses.",
           { {30, 1}, {32, 1}, {7021, 1}, {7014, 1} }, 1 },
        {  5, 7203, ITEM_BERSERKER_STIM,  "Berserker Stim",  "Risky combat surge.",
           { {30, 2}, {32, 1}, {7002, 1}, {7023, 1} }, 1 },
        {  6, 7204, ITEM_MEDKIT,          "Medkit",          "Multi-charge healing.",
           { {30, 2}, {7011, 1}, {7001, 2}, {7013, 1} }, 1 },
        // Grenades
        {  7, 5001, ITEM_FRAG_GRENADE,        "Frag Grenade",        "Physical AoE.",
           { {30, 2}, {32, 1}, {7002, 1}, {7013, 1} }, 1 },
        {  8, 5002, ITEM_EMP_GRENADE,         "EMP Grenade",         "Disables electronics.",
           { {30, 1}, {32, 1}, {48, 1}, {7003, 1}, {7010, 1} }, 1 },
        {  9, 7205, ITEM_INCENDIARY_GRENADE,  "Incendiary Grenade",  "Fire AoE + lingering burn.",
           { {30, 2}, {32, 1}, {7014, 1}, {7023, 1} }, 1 },
        { 10, 7206, ITEM_SMOKE_GRENADE,       "Smoke Grenade",       "Vision-blocking cloud.",
           { {30, 1}, {32, 1}, {7011, 1}, {7013, 1} }, 1 },
        { 11, 7207, ITEM_FLASHBANG,           "Flashbang",           "Stun in radius.",
           { {30, 1}, {32, 1}, {48, 1}, {7012, 1}, {7002, 1} }, 1 },
        // Mines
        { 12, 7208, ITEM_PROXIMITY_MINE,  "Proximity Mine",  "Trigger-on-step physical AoE.",
           { {30, 2}, {32, 1}, {48, 1}, {7002, 1}, {47, 1} }, 1 },
        { 13, 7209, ITEM_EMP_MINE,        "EMP Mine",        "Trigger-on-step EMP.",
           { {30, 1}, {32, 1}, {48, 1}, {7003, 1}, {47, 1} }, 1 },
        { 14, 7210, ITEM_INCENDIARY_MINE, "Incendiary Mine", "Trigger-on-step fire AoE.",
           { {30, 2}, {32, 1}, {48, 1}, {7023, 1}, {47, 1} }, 1 },
        { 15, 7211, ITEM_DECOY_MINE,      "Decoy Mine",      "Emits noise.",
           { {30, 1}, {32, 1}, {48, 1}, {7010, 1}, {47, 1} }, 1 },
        { 16, 7212, ITEM_CALTROPS,        "Caltrops",        "Cheap area denial.",
           { {30, 3}, {7013, 1} }, 1 },
        // Turrets
        { 17, 7213, ITEM_AUTO_TURRET,   "Auto-Turret",   "Stationary kinetic turret.",
           { {30, 3}, {32, 1}, {47, 1}, {7002, 1}, {7004, 1} }, 1 },
        { 18, 7214, ITEM_FLAME_TURRET,  "Flame Turret",  "Stationary cone-burst flamer.",
           { {30, 3}, {48, 1}, {47, 1}, {7014, 1}, {7023, 1} }, 1 },
        { 19, 7215, ITEM_ARC_TURRET,    "Arc Turret",    "Stationary tesla coil.",
           { {30, 2}, {48, 1}, {7010, 2}, {7002, 1}, {7021, 1} }, 1 },
        { 20, 7216, ITEM_SENTRY_DRONE,  "Sentry Drone",  "Mobile autonomous drone.",
           { {30, 3}, {48, 1}, {7022, 1}, {7020, 1}, {7002, 1}, {7023, 1} }, 1 },
    };
    return recipes;
}

const SchematicRecipe* find_schematic_recipe(uint16_t schematic_id) {
    for (const auto& r : schematic_recipes())
        if (r.schematic_id == schematic_id) return &r;
    return nullptr;
}

TinkerResult learn_schematic(Player& player, uint16_t schematic_id,
                             const char* name, const char* description) {
    for (const auto& ls : player.learned_schematics) {
        if (ls.schematic_id == schematic_id)
            return {false, false, std::string("You already know ") + name + "."};
    }
    player.learned_schematics.push_back({ schematic_id, name, description ? description : "" });
    return {true, true, std::string("Learned schematic: ") + name + "."};
}

TinkerResult craft_schematic(uint16_t schematic_id, Player& player) {
    if (!player_has_skill(player, SkillId::Cat_Tinkering))
        return {false, false, "Requires Tinkering skill unlocked."};

    bool known = false;
    for (const auto& ls : player.learned_schematics)
        if (ls.schematic_id == schematic_id) { known = true; break; }
    if (!known)
        return {false, false, "Schematic not learned."};

    const SchematicRecipe* recipe = find_schematic_recipe(schematic_id);
    if (!recipe)
        return {false, false, "Unknown schematic recipe."};

    // Cost check
    for (const auto& req : recipe->material_costs) {
        int have = 0;
        for (const auto& it : player.inventory.items)
            if (item_matches_material(it, req.material_id)) have += it.stack_count;
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    // Consume inputs
    for (const auto& req : recipe->material_costs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end() && needed > 0; ) {
            if (item_matches_material(*it, req.material_id)) {
                if (it->stack_count > needed) { it->stack_count -= needed; needed = 0; }
                else { needed -= it->stack_count; it = player.inventory.items.erase(it); continue; }
            }
            ++it;
        }
    }

    // Produce output(s)
    for (int i = 0; i < recipe->output_count; ++i) {
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == recipe->output_id) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            Item out = build_by_def_id(recipe->output_def_id);
            player.inventory.items.push_back(std::move(out));
        }
    }

    return {true, false, std::string("Crafted: ") + recipe->output_name + "."};
}

} // namespace astra
