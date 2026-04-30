#include "astra/dev_console.h"
#include "astra/animation.h"
#include "astra/aura.h"
#include "astra/consciousness_save.h"
#include "astra/biome_profile.h"
#include "astra/body_presets.h"
#include "astra/display_name.h"
#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/puzzles.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/grid_network.h"
#include "astra/hackable.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/loot_table.h"
#include "astra/lore_generator.h"
#include "astra/npc.h"
#include "astra/quest_fixture.h"
#include "astra/skill_defs.h"
#include "astra/skill_grant.h"
#include "astra/soul_mirror.h"
#include "astra/star_chart.h"
#include "astra/station_type.h"
#include "astra/tilemap.h"
#include "astra/tinkering.h"
#include "astra/trap.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <ctime>
#include <map>
#include <sstream>

namespace astra {

static std::optional<Rarity> parse_rarity_arg(std::string_view s) {
    auto eq_ci = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
            char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
            if (ca != cb) return false;
        }
        return true;
    };
    if (eq_ci(s, "c") || eq_ci(s, "common"))    return Rarity::Common;
    if (eq_ci(s, "u") || eq_ci(s, "uncommon"))  return Rarity::Uncommon;
    if (eq_ci(s, "r") || eq_ci(s, "rare"))      return Rarity::Rare;
    if (eq_ci(s, "e") || eq_ci(s, "epic"))      return Rarity::Epic;
    if (eq_ci(s, "l") || eq_ci(s, "legendary")) return Rarity::Legendary;
    return std::nullopt;
}

static std::string_view rarity_short_name(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "common";
        case Rarity::Uncommon:  return "uncommon";
        case Rarity::Rare:      return "rare";
        case Rarity::Epic:      return "epic";
        case Rarity::Legendary: return "legendary";
    }
    return "?";
}

void DevConsole::toggle() {
    open_ = !open_;
    input_.clear();
    cursor_ = 0;
}

void DevConsole::log(const std::string& msg) {
    output_.push_back(msg);
    if (output_.size() > max_output_) {
        output_.pop_front();
    }
}

bool DevConsole::handle_input(int key, Game& game) {
    if (!open_) return false;

    switch (key) {
        case 27: // Esc
            open_ = false;
            return true;
        case '\n': case '\r':
            if (!input_.empty()) {
                history_.push_back(input_);
                if (history_.size() > max_history_)
                    history_.pop_front();
                log("> " + input_);
                execute_command(input_, game);
                input_.clear();
                cursor_ = 0;
                scroll_ = 0;
                history_idx_ = -1;
            }
            return true;
        case 127: case 8:
            if (cursor_ > 0) {
                input_.erase(cursor_ - 1, 1);
                --cursor_;
            }
            return true;
        case KEY_DELETE:
            if (cursor_ < input_.size()) {
                input_.erase(cursor_, 1);
            }
            return true;
        case KEY_LEFT:
            if (cursor_ > 0) --cursor_;
            return true;
        case KEY_RIGHT:
            if (cursor_ < input_.size()) ++cursor_;
            return true;
        case KEY_UP: {
            int sz = static_cast<int>(history_.size());
            if (sz > 0) {
                if (history_idx_ < 0) history_idx_ = sz;
                if (history_idx_ > 0) {
                    --history_idx_;
                    input_ = history_[history_idx_];
                    cursor_ = input_.size();
                }
            }
            return true;
        }
        case KEY_DOWN: {
            int sz = static_cast<int>(history_.size());
            if (history_idx_ >= 0) {
                ++history_idx_;
                if (history_idx_ >= sz) {
                    history_idx_ = -1;
                    input_.clear();
                } else {
                    input_ = history_[history_idx_];
                }
                cursor_ = input_.size();
            }
            return true;
        }
        case KEY_PAGE_UP:
            scroll_++;
            return true;
        case KEY_PAGE_DOWN:
            if (scroll_ > 0) scroll_--;
            return true;
        default:
            if (key >= 32 && key < 127) {
                input_.insert(cursor_, 1, static_cast<char>(key));
                ++cursor_;
            }
            return true;
    }
}

void DevConsole::execute_command(const std::string& cmd, Game& game) {
    std::vector<std::string> args;
    std::string token;
    for (char c : cmd) {
        if (c == ' ') {
            if (!token.empty()) { args.push_back(token); token.clear(); }
        } else {
            token += c;
        }
    }
    if (!token.empty()) args.push_back(token);
    if (args.empty()) return;

    auto& player = game.player();
    const auto& verb = args[0];

    if (verb == "help") {
        log("Commands:");
        log("  warp random        - warp to random map");
        log("  warp stamp <type>  - POI stamp test (ruins, ship, outpost, cave, settlement, landing)");
        log("  budget             - dump current planet's PoiBudget");
        log("  discoveries        - list Discovery-category journal entries");
        log("  give hp <n>        - set HP");
        log("  give xp <n>        - set XP");
        log("  give money <n>     - set credits");
        log("  give sp <n>        - set skill points");
        log("  give ap <n>        - set attribute points");
        log("  give rep <faction> <n> - set faction reputation");
        log("  give ship <component>         - install ship component (engine [or engine_coil_mk1], hull_plate, shield_generator, navi_computer_mk2)");
        log("  give item                     - list all items (identifier, category, rarity range)");
        log("  give item <identifier>        - spawn item at Common, level 1");
        log("  give item <id> <rarity>       - spawn at given rarity, level 1");
        log("  give item <id> <rarity> <lvl> - fully specified (rarity: c/u/r/e/l)");
        log("  set invuln         - toggle invulnerability");
        log("  set level <n>      - set player level");
        log("  effect burn <dur>  - apply burn effect");
        log("  effect regen <dur> - apply regen effect");
        log("  effect clear       - remove all effects");
        log("  kill all           - kill all hostile NPCs");
        log("  quest kill         - random kill quest");
        log("  quest fetch        - random fetch quest");
        log("  quest deliver      - random deliver quest");
        log("  quest scout        - random scout quest");
        log("  quest story        - The Missing Hauler");
        log("  quest begin <id>   - force-start a story quest by id (bypass prereqs)");
        log("  quest complete <id> - fill objectives so quest is ready for hand-in");
        log("  quest finish <id>  - force-complete active quest by id (fires cascade)");
        log("  heal               - full heal");
        log("  reveal_traps       - toggle render of all hidden traps");
        log("  spawn-trap <prox|emp|incendiary|decoy|caltrops|dungeon> - spawn a dungeon trap at player feet");
        log("  learn-schem <name|all> - learn a schematic by output name (prox, emp, caltrops, healing_stim, frag_grenade, ...)");
        log("  bearings           - regain bearings if lost");
        log("  lore list           - list lore-annotated systems");
        log("  lore warp <feature> - warp to system (beacon/megastructure/terraformed/scarred/battle/weapon/plague/tier1-3)");
        log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock|neutron|derelict)");
        log("  chart reveal <name> - reveal system by name substring");
        log("  chart hide <name>   - hide system by name substring");
        log("  spawn <role> - spawn an enemy NPC adjacent to player");
        log("    roles: archon_remnant, void_reaver, archon_sentinel, conclave_sentry,");
        log("           heavy_conclave_sentry, rust_hound, sentry_drone, archon_automaton");
        log("  fixtures     - list quest fixtures (id, location key, tile)");
        log("  tp <x> <y>   - teleport to tile (x, y) on current map");
        log("  tp <fixture_id> - teleport to that quest fixture if it's on the current map");
        log("  history             - show world lore history");
        log("  biome_test <biome> [settlement [frontier|advanced|ruined]]");
        log("                     [ruins [monolithic|baroque|crystal|industrial] [connected]]");
        log("                     [outpost]");
        log("                     [ship [pod|freighter|corvette]]");
        log("                     [cave [natural|mine|excavation]]");
        log("    biomes: grassland forest jungle sandy rocky volcanic marsh ice");
        log("    fungal crystal corroded aquatic alien_crystalline alien_organic");
        log("    alien_geometric alien_void alien_light scarred_scorched scarred_glassed");
        log("    settlement styles: frontier, advanced, ruined (default: frontier)");
        log("    ruins: generates ruin POI; civ style optional; 'connected' sets all 4 edges");
        log("    outpost: fenced fort with main building, tents, campfires");
        log("    ship: crashed wreck; class optional (auto = lore-weighted)");
        log("    cave: dungeon entrance; variant optional (natural/mine/excavation)");
        log("  dungen <style> [civ] - generate a pipeline dungeon (style: simple_rooms)");
        log("  editor             - open map editor");
        log("  clear              - clear console");
        log("  give skill <id|name>          - learn a skill");
        log("  spawn-hackable <kind>         - place a hackable at adjacent tile");
        log("  spawn-implant <neural-backup> - add an implant to inventory");
        log("  unequip-implant <0|1>         - remove implant from slot, return to inventory");
        log("  detection <n>                 - set zone detection counter");
        log("  sync-soul                     - force Sync Soul on nearest Precursor console");
        log("  unlock-anchor                 - grant ConsciousnessAnchor + seed deep-Grid base");
        log("  rebirth                       - open Sgr A* rebirth modal");
        log("  rebirth-reset                 - delete consciousness.dat (clean slate)");
    }
    else if (verb == "clear") {
        clear();
    }
    else if (verb == "budget") {
        const TileMap& owm = game.world().map();
        if (owm.map_type() != MapType::Overworld) {
            output_.push_back("(not on an overworld — fly to a planet)");
        } else {
            const PoiBudget& b = owm.poi_budget();
            std::string report = format_poi_budget(b);
            size_t start = 0;
            while (start < report.size()) {
                size_t nl = report.find('\n', start);
                std::string line = report.substr(start, nl - start);
                if (!line.empty()) output_.push_back(line);
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            output_.push_back("Hidden POIs: " +
                std::to_string(owm.hidden_pois().size()));
            output_.push_back("Anchor hints: " +
                std::to_string(owm.anchor_hints().size()));
        }
    }
    else if (verb == "discoveries") {
        const auto& journal = game.player().journal;
        int count = 0;
        for (const auto& e : journal) {
            if (e.category == JournalCategory::Discovery) {
                output_.push_back(e.title);
                ++count;
            }
        }
        if (count == 0) output_.push_back("(no discoveries)");
    }
    else if (verb == "heal") {
        player.hp = player.effective_max_hp();
        log("HP restored to " + std::to_string(player.hp));
    }
    else if (verb == "reveal_traps") {
        game.toggle_reveal_traps();
        log(game.reveal_traps_debug() ? "Trap debug ON" : "Trap debug OFF");
    }
    else if (verb == "spawn-trap") {
        if (args.size() < 2) { log("usage: spawn-trap <kind>"); return; }
        const std::string& s = args[1];
        TrapKind k;
        if      (s == "prox")       k = TrapKind::ProximityMine;
        else if (s == "emp")        k = TrapKind::EmpMine;
        else if (s == "incendiary") k = TrapKind::IncendiaryMine;
        else if (s == "decoy")      k = TrapKind::DecoyMine;
        else if (s == "caltrops")   k = TrapKind::Caltrops;
        else if (s == "dungeon")    k = TrapKind::DungeonGeneric;
        else { log("unknown trap kind: " + s); return; }
        place_dungeon_trap(game.world(), game.player().x, game.player().y, k);
        log("Spawned " + display_name(k));
    }
    else if (verb == "learn-schem") {
        if (args.size() < 2) {
            log("usage: learn-schem <name|all>");
            log("  Try: prox emp incendiary decoy caltrops");
            log("       healing_stim adrenaline_stim endure_stim focus_stim berserker_stim medkit");
            log("       frag_grenade emp_grenade cryo_grenade incendiary_grenade smoke_grenade flashbang");
            return;
        }
        // Map common short names to schematic output_name substrings.
        auto match = [](const std::string& s, const SchematicRecipe& r) {
            std::string out = r.output_name ? r.output_name : "";
            // case-insensitive substring or short alias
            auto lower = [](std::string v) { for (auto& c : v) c = std::tolower(c); return v; };
            std::string sl = lower(s), nl = lower(out);
            if (s == "prox")       return out == std::string("Proximity Mine");
            if (s == "emp")        return out == std::string("EMP Mine");
            if (s == "incendiary") return out == std::string("Incendiary Mine");
            if (s == "decoy")      return out == std::string("Decoy Mine");
            if (s == "caltrops")   return out == std::string("Caltrops");
            // generic substring match: "frag_grenade" -> "Frag Grenade"
            std::string alias = sl;
            for (auto& c : alias) if (c == '_') c = ' ';
            return nl.find(alias) != std::string::npos;
        };

        const auto& recipes = schematic_recipes();
        int learned = 0;
        for (const auto& r : recipes) {
            if (args[1] == "all" || match(args[1], r)) {
                auto res = learn_schematic(player, r.schematic_id,
                                           r.output_name, r.output_desc);
                if (res.success) { ++learned; log(res.message); }
            }
        }
        if (learned == 0) log("No matching schematic for '" + args[1] + "'.");
    }
    else if (verb == "solve") {
        auto& map = game.world().map();
        int solved = 0;
        for (int i = 0; i < map.puzzle_count(); ++i) {
            auto& ps = map.puzzle_mut(i);
            if (ps.solved) continue;
            astra::dungeon::on_button_pressed(game, ps.id);
            ++solved;
        }
        log("Solved " + std::to_string(solved) + " puzzle(s).");
    }
    else if (verb == "flash") {
        game.animations().spawn_effect(anim_damage_flash, player.x, player.y);
        log("Spawned damage flash at player (" + std::to_string(player.x) + "," + std::to_string(player.y) +
            "). Active effects: " + std::to_string(game.animations().has_active_effects() ? 1 : 0));
    }
    else if (verb == "editor") {
        game.map_editor().open(game);
        if (game.map_editor().is_open()) {
            log("Map editor opened.");
        }
    }
    else if (verb == "bearings") {
        if (game.lost()) {
            game.set_lost(false);
            log("Bearings regained.");
        } else {
            log("You're not lost.");
        }
    }
    else if (verb == "warp" && args.size() >= 2) {
        if (args[1] == "random") {
            game.dev_command_warp_random();
            log("Warped to random map.");
        } else if (args[1] == "stamp" && args.size() >= 3) {
            static const std::pair<const char*, Tile> stamps[] = {
                {"ruins", Tile::OW_Ruins}, {"ship", Tile::OW_CrashedShip},
                {"outpost", Tile::OW_Outpost}, {"cave", Tile::OW_CaveEntrance},
                {"settlement", Tile::OW_Settlement},
            };
            bool found = false;
            for (const auto& [name, tile] : stamps) {
                if (args[2] == name) {
                    game.dev_command_warp_stamp(tile);
                    log("Warped to " + std::string(name) + " stamp.");
                    found = true;
                    break;
                }
            }
            if (!found) log("Unknown stamp: " + args[2]);
        } else {
            log("Usage: warp random | warp stamp <type>");
        }
    }
    else if (verb == "biome_test" && args.size() < 2) {
        log("Usage: biome_test <biome>");
        log("Type 'help' for biome list.");
    }
    else if (verb == "biome_test" && args.size() >= 2) {
        Biome biome;
        if (!parse_biome(args[1], biome)) {
            log("Unknown biome: " + args[1]);
            log("Options: grassland, forest, jungle, sandy, rocky, volcanic,");
            log("  aquatic, ice, fungal, crystal, corroded,");
            log("  alien_crystalline, alien_organic, alien_geometric,");
            log("  alien_void, alien_light, scarred_scorched, scarred_glassed");
            return;
        }
        int layer = 0;
        std::string poi_type;
        std::string poi_style;
        bool connected = false;
        float ruin_decay_override = -1.0f;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "settlement") {
                poi_type = "settlement";
            } else if (args[i] == "ruins") {
                poi_type = "ruins";
            } else if (args[i] == "outpost") {
                poi_type = "outpost";
            } else if (args[i] == "ship" || args[i] == "crashed_ship") {
                poi_type = "ship";
            } else if (args[i] == "pod" || args[i] == "freighter" || args[i] == "corvette") {
                if (poi_type.empty()) poi_type = "ship";
                poi_style = args[i];
            } else if (args[i] == "cave" || args[i] == "cave_entrance") {
                poi_type = "cave";
            } else if (args[i] == "natural" || args[i] == "mine" || args[i] == "excavation") {
                if (poi_type.empty()) poi_type = "cave";
                poi_style = args[i];
            } else if (args[i] == "connected") {
                connected = true;
            } else if (args[i] == "frontier") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "frontier";
            } else if (args[i] == "advanced") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "advanced";
            } else if (args[i] == "ruined") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "ruined";
            } else if (args[i] == "monolithic" || args[i] == "baroque" ||
                       args[i] == "crystal" || args[i] == "industrial") {
                if (poi_type.empty()) poi_type = "ruins";
                poi_style = args[i];
            } else {
                // Try as float first (for ruin decay), then int (for layer)
                try {
                    float f = std::stof(args[i]);
                    if (args[i].find('.') != std::string::npos) {
                        ruin_decay_override = f;
                    } else {
                        layer = static_cast<int>(f);
                    }
                } catch (...) {
                    log("Invalid arg: " + args[i]);
                    return;
                }
            }
        }
        std::string civ_name;
        if (poi_type == "ruins") civ_name = poi_style;
        game.dev_command_biome_test(biome, layer, poi_type,
                                    poi_type == "ruins" ? "" : poi_style,
                                    connected, civ_name, ruin_decay_override);
        std::string msg = "Biome test: " + args[1] + " (360x150)";
        if (poi_type == "settlement") {
            std::string style_display = poi_style.empty() ? "frontier" : poi_style;
            msg += " + settlement (" + style_display + ")";
        } else if (poi_type == "ruins") {
            msg += " + ruins";
            if (!civ_name.empty()) msg += " (" + civ_name + ")";
            if (ruin_decay_override >= 0.0f)
                msg += " decay=" + std::to_string(ruin_decay_override).substr(0, 4);
            if (connected) msg += " (connected)";
        } else if (poi_type == "outpost") {
            msg += " + outpost";
        } else if (poi_type == "ship") {
            msg += " + crashed ship";
            if (!poi_style.empty()) msg += " (" + poi_style + ")";
        } else if (poi_type == "cave") {
            msg += " + cave entrance";
            if (!poi_style.empty()) msg += " (" + poi_style + ")";
        }
        log(msg);
    }
    else if (verb == "dungen") {
        if (args.size() < 2) {
            log("usage: dungen <style_id> [civ_name]");
            log("  styles: simple_rooms");
            return;
        }

        dungeon::StyleId sid;
        if (!dungeon::parse_style_id(args[1], sid)) {
            log("unknown style: " + args[1]);
            return;
        }
        const std::string civ_name = args.size() >= 3 ? args[2] : "Natural";

        uint32_t seed = static_cast<uint32_t>(std::time(nullptr));
        game.dev_command_dungen(sid, civ_name, seed);

        log("generated dungeon: " + args[1] + " / " + civ_name);
    }
    else if (verb == "dumpmap") {
        std::string path = args.size() >= 2 ? args[1] : std::string{};
        std::string written = game.dev_command_dumpmap(path);
        if (written.empty()) log("dumpmap: failed to write file");
        else                 log("dumpmap: wrote " + written);
    }
    else if (verb == "give" && args.size() >= 3 && args[1] == "ship") {
        Item item;
        if (args[2] == "engine" || args[2] == "engine_coil_mk1") {
            item = build_engine_coil_mk1();
        } else {
            const LootEntry* entry = find_entry_by_identifier(args[2]);
            if (entry == nullptr || entry->category != Category::ShipComponent) {
                log("give ship: unknown component '" + args[2]
                    + "' (try: engine, hull_plate, shield_generator, navi_computer_mk2)");
                return;
            }
            item = build_by_def_id(entry->item_def_id);
        }
        log("Added " + item.name + " to ship cargo.");
        player.ship.cargo.push_back(std::move(item));
    }
    else if (verb == "give" && args.size() >= 2 && args[1] == "item") {
        // Subcommand: list mode (no identifier given)
        if (args.size() == 2) {
            const auto& entries = loot_table_all_entries();

            // Group by category for readability.
            std::map<Category, std::vector<const LootEntry*>> by_cat;
            for (const auto& e : entries) by_cat[e.category].push_back(&e);

            log("Loot table — " + std::to_string(entries.size()) + " items");
            for (const auto& [cat, list] : by_cat) {
                log("  [" + std::string(category_name(cat)) + "]");
                for (const auto* e : list) {
                    Item probe = build_by_def_id(e->item_def_id);
                    std::string line = "    " + e->identifier + "  " + probe.name
                                     + "  (" + std::string(rarity_short_name(e->min_rarity))
                                     + ".." + std::string(rarity_short_name(e->max_rarity))
                                     + ")";
                    log(line);
                }
            }
            return;
        }

        // Subcommand: spawn an item
        std::string_view identifier = args[2];
        Rarity rarity = Rarity::Common;
        int level = 1;

        if (args.size() >= 4) {
            auto parsed = parse_rarity_arg(args[3]);
            if (!parsed.has_value()) {
                log("give item: unknown rarity '" + args[3]
                    + "' (expected c/u/r/e/l or full name)");
                return;
            }
            rarity = *parsed;
        }
        if (args.size() >= 5) {
            try {
                level = std::stoi(args[4]);
            } catch (...) {
                log("give item: level must be an integer");
                return;
            }
            if (level < 1) level = 1;
        }

        const LootEntry* entry = find_entry_by_identifier(identifier);
        if (entry == nullptr) {
            log("give item: unknown identifier '" + std::string(identifier)
                + "' (try `give item` with no args to list)");
            return;
        }
        if (rarity < entry->min_rarity || rarity > entry->max_rarity) {
            log("give item: '" + entry->identifier + "' rarity out of range ("
                + std::string(rarity_short_name(entry->min_rarity)) + ".."
                + std::string(rarity_short_name(entry->max_rarity)) + ")");
            return;
        }

        Item item = build_by_def_id(entry->item_def_id);
        scale_item_to_rarity(item, rarity);
        scale_item_to_level(item, level);
        // No affixes — dev command stays deterministic.

        log("Added " + entry->identifier + " ("
            + std::string(rarity_short_name(rarity)) + ", lvl "
            + std::to_string(level) + ") to inventory.");
        add_to_inventory_stacked(player.inventory, std::move(item));
    }
    else if (verb == "give" && args.size() >= 3 && args[1] == "skill") {
        std::string name = args[2];
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        SkillId target = SkillId{};
        std::string display_name;
        bool found = false;
        for (const auto& cat : skill_catalog()) {
            std::string cn = cat.name;
            for (auto& c : cn) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string cn_snake = cn;
            for (auto& c : cn_snake) if (c == ' ' || c == '-') c = '_';
            if (cn == name || cn_snake == name ||
                ("cat_" + cn_snake) == name) {
                target = cat.unlock_id;
                display_name = "Cat_" + cat.name;
                found = true;
                break;
            }
            for (const auto& sk : cat.skills) {
                std::string sn = sk.name;
                for (auto& c : sn) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                std::string sn_snake = sn;
                for (auto& c : sn_snake) if (c == ' ' || c == '-') c = '_';
                if (sn == name || sn_snake == name) {
                    target = sk.id;
                    display_name = sk.name;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) { log("unknown skill: " + args[2]); return; }
        if (std::find(player.learned_skills.begin(), player.learned_skills.end(), target) ==
                player.learned_skills.end()) {
            player.learned_skills.push_back(target);
            log("Granted skill: " + display_name);
        } else {
            log("Already learned.");
        }
    }
    else if (verb == "give" && args.size() >= 3) {
        int val = 0;
        try { val = std::stoi(args[2]); } catch (...) {
            log("Invalid number: " + args[2]);
            return;
        }
        if (args[1] == "hp") {
            player.hp = val;
            if (player.hp > player.effective_max_hp()) player.hp = player.effective_max_hp();
            log("HP set to " + std::to_string(player.hp));
        } else if (args[1] == "xp") {
            player.xp = val;
            log("XP set to " + std::to_string(player.xp));
            game.dev_command_level_up();
        } else if (args[1] == "money") {
            player.money = val;
            log("Credits set to " + std::to_string(player.money));
        } else if (args[1] == "sp") {
            player.skill_points = val;
            log("SP set to " + std::to_string(player.skill_points));
        } else if (args[1] == "ap") {
            player.attribute_points = val;
            log("AP set to " + std::to_string(player.attribute_points));
        } else if (args[1] == "rep" && args.size() >= 4) {
            // give rep <faction words...> <value>
            // Last arg is the value, everything between is the faction name
            int rep_val = 0;
            try { rep_val = std::stoi(args.back()); } catch (...) {
                log("Invalid number: " + args.back());
                return;
            }
            std::string faction;
            for (size_t i = 2; i < args.size() - 1; ++i) {
                if (!faction.empty()) faction += " ";
                faction += args[i];
            }
            bool found = false;
            for (auto& fs : player.reputation) {
                if (fs.faction_name == faction) {
                    fs.reputation = rep_val;
                    found = true;
                    break;
                }
            }
            if (found)
                log("Reputation with " + faction + " set to " + std::to_string(rep_val));
            else
                log("Unknown faction: " + faction);
        } else {
            log("Unknown: give " + args[1]);
        }
    }
    else if (verb == "set" && args.size() >= 2) {
        if (args[1] == "invuln") {
            if (has_effect(player.effects, EffectId::Invulnerable)) {
                remove_effect(player.effects, EffectId::Invulnerable);
                log("Invulnerability OFF");
            } else {
                add_effect(player.effects, make_invulnerable_ge());
                log("Invulnerability ON");
            }
            rebuild_auras_from_sources(player);
        } else if (args[1] == "level" && args.size() >= 3) {
            int lvl = 0;
            try { lvl = std::stoi(args[2]); } catch (...) {
                log("Invalid number: " + args[2]);
                return;
            }
            while (player.level < lvl) {
                game.dev_command_level_up();
            }
            log("Level set to " + std::to_string(player.level));
        } else {
            log("Unknown: set " + args[1]);
        }
    }
    else if (verb == "effect" && args.size() >= 2) {
        if (args[1] == "clear") {
            player.effects.clear();
            log("All effects cleared.");
            rebuild_auras_from_sources(player);
        } else if (args[1] == "burn") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_burn_ge(dur, 1));
            rebuild_auras_from_sources(player);
            log("Burn applied for " + std::to_string(dur) + " ticks.");
        } else if (args[1] == "regen") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_regen_ge(dur, 1));
            rebuild_auras_from_sources(player);
            log("Regen applied for " + std::to_string(dur) + " ticks.");
        } else if (args[1] == "poison") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_poison_ge(dur, 1));
            rebuild_auras_from_sources(player);
            log("Poison applied for " + std::to_string(dur) + " ticks.");
        } else {
            log("Unknown effect: " + args[1]);
        }
    }
    else if (verb == "kill" && args.size() >= 2 && args[1] == "all") {
        int count = 0;
        for (auto& npc : game.npcs()) {
            if (npc.alive() && is_hostile_to_player(npc.faction, game.player())) {
                npc.hp = 0;
                ++count;
            }
        }
        game.dev_command_kill_hostiles();
        log("Killed " + std::to_string(count) + " hostile NPCs.");
    }
    else if (verb == "quest") {
        if (args.size() >= 2 && args[1] == "kill") {
            auto q = game.quests().generate_kill_quest(game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "fetch") {
            auto q = game.quests().generate_fetch_quest(game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "deliver") {
            auto q = game.quests().generate_deliver_quest("Merchant", game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "scout") {
            // Pick a random landable body from the current system
            auto& nav = game.world().navigation();
            std::string body;
            int body_idx = -1;
            uint32_t sys_id = 0;
            for (auto& sys : nav.systems) {
                if (sys.id == nav.current_system_id) {
                    generate_system_bodies(sys);
                    sys_id = sys.id;
                    std::vector<int> landable;
                    for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
                        if (sys.bodies[i].landable) landable.push_back(i);
                    }
                    if (!landable.empty()) {
                        int idx = std::uniform_int_distribution<int>(
                            0, static_cast<int>(landable.size()) - 1)(game.world().rng());
                        body_idx = landable[idx];
                        body = sys.bodies[body_idx].name;
                    }
                    break;
                }
            }
            if (body.empty()) body = "Unknown Body";
            auto q = game.quests().generate_scout_quest(body, game.world().rng());
            // Register map marker
            if (sys_id != 0 && body_idx >= 0) {
                q.target_system_id = sys_id;
                q.target_body_index = body_idx;
                LocationKey mk = {sys_id, body_idx, -1, false, -1, -1, 0};
                QuestLocationMeta meta;
                meta.quest_id = q.id;
                meta.quest_title = q.title;
                meta.target_system_id = sys_id;
                meta.target_body_index = body_idx;
                meta.remove_on_completion = true;
                game.world().quest_locations()[mk] = std::move(meta);
            }
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "story") {
            auto* sq = find_story_quest("story_missing_hauler");
            if (sq && !game.quests().has_active_quest("story_missing_hauler")) {
                auto q = sq->create_quest();
                q.arc_id = sq->arc_id();
                q.prerequisite_ids = sq->prerequisite_ids();
                q.reveal = sq->reveal_policy();
                log("Quest: " + q.title);
                log("  " + q.description);
                game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
                sq->on_accepted(game);
                log("Quest markers placed on star chart.");
            } else if (game.quests().has_active_quest("story_missing_hauler")) {
                log("Quest already active.");
            } else {
                log("Story quest not found.");
            }
        } else if (args.size() >= 2 && args[1] == "fixture") {
            // Register a debug def and plant it adjacent to the player.
            QuestFixtureDef def;
            def.id = "dev_smoke_fixture";
            def.glyph = '*';
            def.color = 135;
            def.prompt = "Play debug transmission";
            def.log_message = "You nudge the debug fixture. It beeps.";
            def.log_title = "DEV SMOKE TRANSMISSION";
            def.log_lines = {
                "This is line one of the debug transmission.",
                "",
                "Line two. The reveal should advance at thirty chars per second.",
                "Press Space to skip; press Esc to close; re-interact to replay.",
            };
            register_quest_fixture(def);

            FixtureData fd;
            fd.type = FixtureType::QuestFixture;
            fd.interactable = true;
            fd.passable = true;
            fd.quest_fixture_id = def.id;

            int fx = game.player().x + 1;
            int fy = game.player().y;
            if (fx >= 0 && fx < game.world().map().width() &&
                fy >= 0 && fy < game.world().map().height() &&
                game.world().map().passable(fx, fy) &&
                game.world().map().fixture_id(fx, fy) < 0) {
                game.world().map().add_fixture(fx, fy, fd);
                log("Planted dev_smoke_fixture at (" +
                    std::to_string(fx) + "," + std::to_string(fy) + ")");
            } else {
                log("No open tile adjacent to player for fixture.");
            }
        } else if (args.size() >= 3 && args[1] == "begin") {
            // Force-start a story quest by id, bypassing prereqs and dialog.
            const std::string& qid = args[2];
            if (game.quests().has_active_quest(qid)) {
                log("quest begin: already active");
                return;
            }
            auto* sq = find_story_quest(qid);
            if (!sq) {
                log("quest begin: no story quest with id '" + qid + "'");
                return;
            }
            auto q = sq->create_quest();
            // Mirror init_from_catalog: carry arc/prereq/reveal metadata so
            // journal rendering places the quest under its arc when it later
            // completes (instead of as a loose entry).
            q.arc_id = sq->arc_id();
            q.prerequisite_ids = sq->prerequisite_ids();
            q.reveal = sq->reveal_policy();
            game.quests().accept_quest(std::move(q), game.world().world_tick(),
                                       game.player());
            sq->on_accepted(game);
            log("Force-started quest: " + qid);
        } else if (args.size() >= 3 && args[1] == "complete") {
            // Tick every objective to its target so the quest is ready for
            // hand-in (e.g. talking to the giver NPC). Does NOT call
            // complete_quest — use "quest finish" for that.
            const std::string& qid = args[2];
            Quest* q = game.quests().find_active(qid);
            if (!q) {
                log("quest complete: '" + qid + "' is not active");
                return;
            }
            for (auto& obj : q->objectives) obj.current_count = obj.target_count;
            log("Objectives filled for: " + qid + " (hand in to complete)");
        } else if (args.size() >= 3 && args[1] == "finish") {
            // Force-complete an active quest by id (fires on_completed + DAG).
            const std::string& qid = args[2];
            if (!game.quests().has_active_quest(qid)) {
                log("quest finish: '" + qid + "' is not active");
                return;
            }
            // Tick every objective to its target so complete_quest sees it done
            // and its reward / journal paths run normally.
            if (Quest* q = game.quests().find_active(qid)) {
                for (auto& obj : q->objectives) obj.current_count = obj.target_count;
            }
            game.quests().complete_quest(qid, game, game.world().world_tick());
            log("Force-finished quest: " + qid);
        } else {
            log("Usage: quest kill|fetch|deliver|scout|story|fixture");
            log("       quest begin <id> | quest complete <id> | quest finish <id>");
        }
    }
    else if (verb == "history") {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        // Close console and open the lore viewer
        open_ = false;
        game.open_lore_viewer();
    }
    else if (verb == "lore" && args.size() >= 2) {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        auto& nav = game.world().navigation();

        // Match a lore feature name to a system predicate
        auto match_system = [](const std::string& feature, const LoreAnnotation& la) -> bool {
            if (feature == "beacon")         return la.beacon;
            if (feature == "megastructure")  return la.has_megastructure;
            if (feature == "terraformed")    return la.terraformed;
            if (feature == "scarred" || feature == "scar")
                return la.battle_site || la.weapon_test_site;
            if (feature == "battle")         return la.battle_site;
            if (feature == "weapon")         return la.weapon_test_site;
            if (feature == "plague")         return la.plague_origin;
            if (feature == "tier3")          return la.lore_tier >= 3;
            if (feature == "tier2")          return la.lore_tier >= 2;
            if (feature == "tier1")          return la.lore_tier >= 1;
            return false;
        };

        if (args[1] == "list") {
            // List all systems with lore features
            int count = 0;
            for (const auto& sys : nav.systems) {
                const auto& la = sys.lore;
                if (la.lore_tier == 0) continue;
                std::string flags;
                if (la.beacon)          flags += " [beacon]";
                if (la.has_megastructure) flags += " [mega]";
                if (la.terraformed)     flags += " [terraform]";
                if (la.battle_site)     flags += " [battle]";
                if (la.weapon_test_site) flags += " [weapon]";
                if (la.plague_origin)   flags += " [plague]";
                if (la.scar_count > 0)  flags += " [scars:" + std::to_string(la.scar_count) + "]";
                log("  #" + std::to_string(sys.id) + " " + sys.name +
                    " (tier " + std::to_string(la.lore_tier) + ")" + flags);
                if (++count >= 30) { log("  ... (truncated)"); break; }
            }
            if (count == 0) log("No lore-annotated systems found.");
            else log(std::to_string(count) + " systems shown.");
        }
        else if (args[1] == "warp" && args.size() >= 3) {
            const std::string& feature = args[2];
            // Find first matching system
            for (const auto& sys : nav.systems) {
                if (match_system(feature, sys.lore)) {
                    game.dev_command_warp_to_system(sys.id);

                    std::string flags;
                    if (sys.lore.beacon) flags += " beacon";
                    if (sys.lore.has_megastructure) flags += " mega";
                    if (sys.lore.terraformed) flags += " terraform";
                    if (sys.lore.battle_site) flags += " battle";
                    if (sys.lore.weapon_test_site) flags += " weapon";
                    if (sys.lore.scar_count > 0) flags += " scars:" + std::to_string(sys.lore.scar_count);
                    log("Warped to " + sys.name + " (#" + std::to_string(sys.id) +
                        ", tier " + std::to_string(sys.lore.lore_tier) + ")" + flags);
                    return;
                }
            }
            log("No system found with feature: " + feature);
        }
        else {
            log("Usage: lore list | lore warp <feature>");
            log("Features: beacon, megastructure, terraformed, scarred, battle, weapon, plague, tier1, tier2, tier3");
        }
    }
    else if (verb == "chart") {
        auto& nav = game.world().navigation();
        if (args.size() >= 2 && args[1] == "create") {
            std::string kind = "asteroid";
            std::string name = "Custom";
            if (args.size() == 3) {
                // Single extra arg: name only (back-compat, `chart create Foo`).
                name = args[2];
            } else if (args.size() >= 4) {
                // Two extra args: <kind> <name>. Kind must be known.
                std::string a2 = args[2];
                if (a2 != "asteroid" && a2 != "scar" &&
                    a2 != "rock" && a2 != "neutron" && a2 != "derelict") {
                    log("chart create: unknown kind '" + a2 +
                        "' (expected asteroid|scar|rock|neutron|derelict)");
                    return;
                }
                kind = a2;
                name = args[3];
            }

            auto coords = pick_coords_near(nav, nav.current_system_id,
                                           2.0f, 5.0f, game.world().rng());
            if (!coords) {
                log("chart create: couldn't find a spot near current system");
                return;
            }

            CustomSystemSpec spec;
            spec.name = name;
            spec.gx = coords->first;
            spec.gy = coords->second;
            spec.star_class = StarClass::ClassM;
            spec.discovered = true;

            if (kind == "asteroid") {
                spec.bodies = { make_landable_asteroid(name + " Rock") };
            } else if (kind == "scar") {
                spec.bodies = { make_scar_planet(name + " Prime") };
            } else if (kind == "neutron") {
                spec.star_class = StarClass::Neutron;
                spec.bodies = { make_landable_asteroid(name + " Fragment") };
            } else if (kind == "derelict") {
                spec.has_station = true;
                spec.station.type = StationType::Abandoned;
                spec.station.specialty = StationSpecialty::Generic;
                spec.station.name = name + " Outpost";
                spec.star_class = StarClass::ClassG;
                spec.bodies = {};
            } else { // "rock"
                CelestialBody b;
                b.name = name + " Rock";
                b.type = BodyType::Rocky;
                b.atmosphere = Atmosphere::None;
                b.temperature = Temperature::Cold;
                b.size = 2;
                b.landable = true;
                b.danger_level = 1;
                b.day_length = 200;
                spec.bodies = { std::move(b) };
            }

            uint32_t id = add_custom_system(nav, std::move(spec));
            log("Created custom " + kind + " system '" + name + "' id=" +
                std::to_string(id) + " at (" + std::to_string(coords->first) +
                ", " + std::to_string(coords->second) + ")");
        } else if (args.size() >= 2 && args[1] == "reveal") {
            if (args.size() < 3) { log("Usage: chart reveal <name-substring>"); return; }
            const std::string& needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (reveal_system(nav, s.id)) {
                        log("Revealed '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else if (args.size() >= 2 && args[1] == "hide") {
            if (args.size() < 3) { log("Usage: chart hide <name-substring>"); return; }
            const std::string& needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (hide_system(nav, s.id)) {
                        log("Hid '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else {
            log("Usage: chart create [kind] [name]|reveal <name>|hide <name>");
        }
    }
    else if (verb == "spawn") {
        if (args.size() < 2) {
            log("Usage: spawn <role>  (archon_remnant|void_reaver|archon_sentinel|conclave_sentry|heavy_conclave_sentry|rust_hound|sentry_drone|archon_automaton)");
            return;
        }
        std::string role_arg = args[1];
        std::string role_name;
        if      (role_arg == "archon_remnant")   role_name = "Archon Remnant";
        else if (role_arg == "void_reaver")      role_name = "Void Reaver";
        else if (role_arg == "archon_sentinel")  role_name = "Archon Sentinel";
        else if (role_arg == "conclave_sentry")  role_name = "Conclave Sentry";
        else if (role_arg == "heavy_conclave_sentry") role_name = "Heavy Conclave Sentry";
        else if (role_arg == "rust_hound")       role_name = "Rust Hound";
        else if (role_arg == "sentry_drone")     role_name = "Sentry Drone";
        else if (role_arg == "conclave_sentry_drone") role_name = "Conclave Sentry Drone";
        else if (role_arg == "archon_sentry_drone")   role_name = "Archon Sentry Drone";
        else if (role_arg == "archon_automaton") role_name = "Archon Automaton";
        else {
            log("spawn: unknown role '" + role_arg +
                "' (archon_remnant|void_reaver|archon_sentinel|conclave_sentry|heavy_conclave_sentry|rust_hound|sentry_drone|archon_automaton)");
            return;
        }

        Npc npc = create_npc_by_role(role_name, game.world().rng());
        // Walk the 8 neighbours until a passable empty tile is found.
        const int dx[] = {1, -1, 0, 0, 1, 1, -1, -1};
        const int dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
        bool placed = false;
        for (int i = 0; i < 8 && !placed; ++i) {
            int nx = game.player().x + dx[i];
            int ny = game.player().y + dy[i];
            if (nx < 0 || nx >= game.world().map().width()) continue;
            if (ny < 0 || ny >= game.world().map().height()) continue;
            if (!game.world().map().passable(nx, ny)) continue;
            bool occupied = false;
            for (const auto& other : game.world().npcs()) {
                if (other.alive() && other.x == nx && other.y == ny) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) continue;
            npc.x = nx;
            npc.y = ny;
            game.world().npcs().push_back(std::move(npc));
            log("Spawned " + role_name + " at (" + std::to_string(nx) +
                "," + std::to_string(ny) + ")");
            placed = true;
        }
        if (!placed) log("spawn: no adjacent passable tile");
    }
    else if (verb == "fixtures") {
        // Dump quest-fixture placements across all quest_locations PLUS
        // notable navigation fixtures (stairs, hatches, anything with a
        // quest_fixture_id) on the current map, so the dev can see
        // what's been stamped where.
        const auto& qlocs = game.world().quest_locations();
        int total = 0;
        if (!qlocs.empty()) {
            log("-- quest_locations metadata --");
            for (const auto& [key, meta] : qlocs) {
                auto [sys, b, m, stn, ow_x, ow_y, d] = key;
                for (const auto& p : meta.fixtures) {
                    ++total;
                    std::string loc = "sys=" + std::to_string(sys) +
                                      " body=" + std::to_string(b) +
                                      (m >= 0 ? " moon=" + std::to_string(m) : "") +
                                      (stn ? " [station]" : "") +
                                      (d > 0 ? " depth=" + std::to_string(d) : "");
                    if (p.x < 0 || p.y < 0) {
                        log(p.fixture_id + " — " + loc + " — (unplaced)");
                    } else {
                        log(p.fixture_id + " — " + loc + " — tile (" +
                            std::to_string(p.x) + "," + std::to_string(p.y) + ")");
                    }
                }
            }
        }

        // Current map scan for navigation fixtures and any quest-tagged
        // fixture that doesn't appear in quest_locations (e.g. the
        // Conclave Archive hatch, programmatically placed by poi_phase).
        const auto& m = game.world().map();
        int map_shown = 0;
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& f = m.fixture(fidx);
                const char* tname = nullptr;
                switch (f.type) {
                    case FixtureType::StairsUp:     tname = "stairs_up";     break;
                    case FixtureType::StairsDown:   tname = "stairs_down";   break;
                    case FixtureType::StairsDownPrecursor: tname = "stairs_down_precursor"; break;
                    case FixtureType::DungeonHatch: tname = "hatch";         break;
                    case FixtureType::QuestFixture: tname = "quest_fixture"; break;
                    default: break;
                }
                if (!tname) continue;
                if (map_shown == 0) log("-- current map fixtures --");
                std::string line = std::string(tname) + " (" +
                                   std::to_string(x) + "," +
                                   std::to_string(y) + ")";
                if (!f.quest_fixture_id.empty())
                    line += " id=" + f.quest_fixture_id;
                log(line);
                ++map_shown;
            }
        }

        if (total == 0 && map_shown == 0)
            log("No quest fixtures declared and no navigation fixtures on current map.");
    }
    else if (verb == "mapfix" && args.size() == 1) {
        // List every placed fixture on the current map.
        const auto& m = game.world().map();
        int shown = 0;
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& f = m.fixture(fidx);
                std::string line = "(" + std::to_string(x) + "," +
                                   std::to_string(y) + ") type=" +
                                   std::to_string(static_cast<int>(f.type));
                if (!f.quest_fixture_id.empty())
                    line += " id=" + f.quest_fixture_id;
                log(line);
                ++shown;
            }
        }
        if (shown == 0) log("No fixtures on this map.");
        else log("Total: " + std::to_string(shown));
    }
    else if (verb == "regen" && args.size() == 1) {
        // Purge the location cache so the current map and all dungeon
        // levels regenerate on next entry. Useful when generator logic
        // has changed and cached maps are stale. Does NOT affect the
        // current map you're standing on until you leave and come back.
        size_t n = game.world().location_cache().size();
        game.world().location_cache().clear();
        log("Purged " + std::to_string(n) + " cached locations. Leave and re-enter any map to regenerate.");
    }
    else if (verb == "tp" && args.size() >= 3) {
        // Teleport player to (x, y) on the current map.
        int tx = std::atoi(args[1].c_str());
        int ty = std::atoi(args[2].c_str());
        if (tx < 0 || tx >= game.world().map().width() ||
            ty < 0 || ty >= game.world().map().height()) {
            log("tp: out of bounds");
            return;
        }
        game.player().x = tx;
        game.player().y = ty;
        log("Teleported to (" + std::to_string(tx) + "," + std::to_string(ty) + ")");
    }
    else if (verb == "tp" && args.size() == 2) {
        // Teleport targets, in priority order:
        //   (a) short keyword for FixtureType — "stairs_down", "stairs_up",
        //       "hatch" — jumps to the first matching fixture on the map.
        //   (b) quest fixture id in QuestLocationMeta.fixtures.
        //   (c) quest_fixture_id set on an actual fixture on the current
        //       map (for programmatically placed fixtures).
        const std::string& fid = args[1];
        FixtureType ft_target = FixtureType::Table;
        bool use_type = true;
        if      (fid == "stairs_down") ft_target = FixtureType::StairsDown;
        else if (fid == "stairs_down_precursor") ft_target = FixtureType::StairsDownPrecursor;
        else if (fid == "stairs_up")   ft_target = FixtureType::StairsUp;
        else if (fid == "hatch")       ft_target = FixtureType::DungeonHatch;
        else use_type = false;

        if (use_type) {
            const auto& m = game.world().map();
            for (int y = 0; y < m.height(); ++y) {
                for (int x = 0; x < m.width(); ++x) {
                    int fidx = m.fixture_id(x, y);
                    if (fidx < 0) continue;
                    if (m.fixture(fidx).type != ft_target) continue;
                    game.player().x = x;
                    game.player().y = y;
                    log("Teleported to " + fid + " at (" +
                        std::to_string(x) + "," + std::to_string(y) + ")");
                    return;
                }
            }
            log("tp: no " + fid + " on this map");
            return;
        }
        for (const auto& [key, meta] : game.world().quest_locations()) {
            for (const auto& p : meta.fixtures) {
                if (p.fixture_id != fid) continue;
                if (p.x < 0 || p.y < 0) {
                    log("tp: fixture '" + fid + "' hasn't been placed yet (enter its map first)");
                    return;
                }
                if (p.x >= game.world().map().width() ||
                    p.y >= game.world().map().height()) {
                    log("tp: fixture '" + fid + "' is on a different map");
                    return;
                }
                game.player().x = p.x;
                game.player().y = p.y;
                log("Teleported to '" + fid + "' at (" +
                    std::to_string(p.x) + "," + std::to_string(p.y) + ")");
                return;
            }
        }
        // Pass 2: scan the current map's placed fixtures.
        const auto& m = game.world().map();
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& f = m.fixture(fidx);
                if (f.quest_fixture_id != fid) continue;
                game.player().x = x;
                game.player().y = y;
                log("Teleported to '" + fid + "' at (" +
                    std::to_string(x) + "," + std::to_string(y) + ")");
                return;
            }
        }
        log("tp: fixture '" + fid + "' not found");
    }
    else if (verb == "spawn-hackable") {
        if (args.size() < 2) {
            log("usage: spawn-hackable <turret|camera|door|conduit|console>");
            return;
        }
        DeviceKind dk;
        FixtureType ft;
        if      (args[1] == "turret")  { dk = DeviceKind::Turret;           ft = FixtureType::Console; }
        else if (args[1] == "camera")  { dk = DeviceKind::Camera;           ft = FixtureType::Console; }
        else if (args[1] == "door")    { dk = DeviceKind::Door;             ft = FixtureType::Door; }
        else if (args[1] == "conduit") { dk = DeviceKind::PowerConduit;     ft = FixtureType::Conduit; }
        else if (args[1] == "console") { dk = DeviceKind::PrecursorConsole; ft = FixtureType::Console; }
        else { log("unknown kind: " + args[1]); return; }

        auto& m = game.world().map();
        static const int dxs[] = {1, -1, 0, 0};
        static const int dys[] = {0, 0, 1, -1};
        bool placed = false;
        for (int i = 0; i < 4 && !placed; ++i) {
            int nx = player.x + dxs[i];
            int ny = player.y + dys[i];
            if (m.passable(nx, ny) && m.fixture_id(nx, ny) < 0) {
                FixtureData fd = make_fixture(ft);
                fd.interactable = true;
                fd.cyber = make_hackable(dk, 1);
                if (dk == DeviceKind::PrecursorConsole) {
                    auto& net = game.world().grid_network();
                    GridNodeId nid = register_precursor_console(net, "DevConsole.Spawn",
                                                                 game.world().seed(), 1);
                    fd.cyber->jack_in_node_id = static_cast<int>(nid.value);
                    // Populate lore fragments (1..4) using position-encoded archive ids.
                    auto& rng = game.world().rng();
                    int n_fragments = 1 + static_cast<int>(rng() % 4);
                    fd.cyber->lore_fragments.clear();
                    for (int fi = 0; fi < n_fragments; ++fi) {
                        LoreFragmentSeed f;
                        f.archive_id = "ARCH-" + std::to_string(nx) + "x"
                                     + std::to_string(ny) + "-" + std::to_string(fi);
                        fd.cyber->lore_fragments.push_back(std::move(f));
                    }
                }
                game.world().map().add_fixture(nx, ny, fd);
                log("Placed " + std::string(device_kind_name(dk)) + " at (" +
                    std::to_string(nx) + "," + std::to_string(ny) + ").");
                placed = true;
            }
        }
        if (!placed) log("no adjacent passable tile available.");
    }
    else if (verb == "detection") {
        if (args.size() < 2) {
            log("usage: detection <0..100>  (sets active zone Detection counter)");
            return;
        }
        int n = 0;
        try { n = std::stoi(args[1]); } catch (...) {
            log("invalid number");
            return;
        }
        int cur = game.hacking().detection();
        game.hacking().add_detection(n - cur);
        log("Detection = " + std::to_string(game.hacking().detection()));
    }
    else if (verb == "jack-out") {
        if (!game.hacking().jacked_in()) {
            log("Not jacked in.");
            return;
        }
        game.hacking().jack_out(game, JackOutKind::Voluntary);
        log("Jacked out.");
    }
    else if (verb == "jack") {
        if (args.size() < 2) {
            log("usage: jack <node-label>");
            return;
        }
        const auto& net = game.world().grid_network();
        const GridNode* match = nullptr;
        for (const auto& n : net.nodes()) {
            if (n.label == args[1]) { match = &n; break; }
        }
        if (!match) { log("Unknown node: " + args[1]); return; }
        game.hacking().jack_in(game, match->id);
    }
    else if (verb == "trace") {
        if (args.size() < 2) {
            log("usage: trace <0..100>  (sets Grid Trace counter)");
            return;
        }
        if (!game.hacking().jacked_in()) {
            log("Not jacked in.");
            return;
        }
        int n = 0;
        try { n = std::stoi(args[1]); } catch (...) { log("invalid number"); return; }
        n = std::clamp(n, 0, 100);
        game.hacking().session()->trace = n;
        log("Trace = " + std::to_string(n));
    }
    else if (verb == "spawn-ice") {
        if (args.size() < 2) {
            log("usage: spawn-ice <white|gray|black>");
            return;
        }
        if (!game.hacking().jacked_in()) {
            log("Not jacked in.");
            return;
        }
        IceColor color;
        int hp;
        if      (args[1] == "white") { color = IceColor::White; hp = 1; }
        else if (args[1] == "gray")  { color = IceColor::Gray;  hp = 2; }
        else if (args[1] == "black") { color = IceColor::Black; hp = 4; }
        else { log("unknown color: " + args[1]); return; }

        auto* sess = game.hacking().session();
        // Place adjacent to avatar in any passable, unoccupied tile.
        static const int dxs[4] = { 0, 0, -1, 1 };
        static const int dys[4] = { -1, 1, 0, 0 };
        for (int d = 0; d < 4; ++d) {
            int nx = sess->avatar_x + dxs[d];
            int ny = sess->avatar_y + dys[d];
            if (!sess->sector.passable(nx, ny)) continue;
            bool occupied = false;
            for (auto& i : sess->ice) if (i.x == nx && i.y == ny) { occupied = true; break; }
            if (occupied) continue;
            GridIce ice;
            ice.x = nx; ice.y = ny; ice.color = color; ice.hp = hp;
            sess->ice.push_back(ice);
            log("Spawned " + args[1] + " ICE.");
            return;
        }
        log("No adjacent passable tile to spawn ICE.");
    }
    else if (verb == "dump-precursor") {
        // Find the nearest Precursor console hackable on the current map.
        const auto& m  = game.world().map();
        const auto& px = game.player().x;
        const auto& py = game.player().y;
        const Hackable* nearest = nullptr;
        int best_dist = INT_MAX;
        int w = m.width();
        int h = m.height();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& fd = m.fixture(fidx);
                if (!fd.cyber) continue;
                if (fd.cyber->device_kind != DeviceKind::PrecursorConsole) continue;
                int dist = std::abs(x - px) + std::abs(y - py);
                if (dist < best_dist) { best_dist = dist; nearest = &*fd.cyber; }
            }
        }
        if (!nearest) {
            log("no Precursor console on this map");
            return;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "fragments: %zu / progress: %d",
                      nearest->lore_fragments.size(), nearest->soul_mirror_progress);
        log(buf);
        for (const auto& f : nearest->lore_fragments) {
            log("  - " + f.archive_id + (f.committed ? " [done]" : ""));
        }
    }
    else if (verb == "sync-soul") {
        // Force-start a manual Soul Mirror channel on the nearest Precursor console.
        const auto& m  = game.world().map();
        const int   px = game.player().x;
        const int   py = game.player().y;
        Hackable* console = nullptr;
        int best_dist = INT_MAX;
        int w = m.width();
        int h = m.height();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int fidx = m.fixture_id(x, y);
                if (fidx < 0) continue;
                const auto& fd = m.fixture(fidx);
                if (!fd.cyber) continue;
                if (fd.cyber->device_kind != DeviceKind::PrecursorConsole) continue;
                int dist = std::abs(x - px) + std::abs(y - py);
                if (dist < best_dist) {
                    best_dist = dist;
                    console = &*game.world().map().fixture_mut(fidx).cyber;
                }
            }
        }
        if (!console) { log("no Precursor console nearby"); return; }
        soul_mirror::begin_active(game, *console);
        log("Forced Sync Soul start.");
    }
    else if (verb == "spawn-implant") {
        if (args.size() < 2) {
            log("usage: spawn-implant <neural-backup>");
            return;
        }
        if (args[1] == "neural-backup") {
            add_to_inventory_stacked(game.player().inventory, build_neural_backup());
            log("Spawned Neural Backup in inventory.");
        } else {
            log("unknown implant name: " + args[1]);
        }
    }
    else if (verb == "unequip-implant") {
        if (args.size() != 1) { log("usage: unequip-implant <0|1>"); return; }
        int slot = std::atoi(args[0].c_str());
        if (slot < 0 || slot >= astra::Player::IMPLANT_SLOTS) {
            log("slot out of range");
            return;
        }
        auto& s = game.player().implants[slot];
        if (!s) { log("slot is empty"); return; }
        add_to_inventory_stacked(game.player().inventory, *s);
        s = std::nullopt;
        log("Unequipped implant from slot " + args[0]);
    }
    else if (verb == "unlock-anchor") {
        grant_skill(game.player(), SkillId::ConsciousnessAnchor);
        apply_skill_side_effects(game, SkillId::ConsciousnessAnchor);
        log("ConsciousnessAnchor unlocked. Base seeded.");
    }
    else if (verb == "rebirth") {
        game.rebirth().begin();
        log("Sgr A* rebirth modal opened.");
    }
    else if (verb == "rebirth-reset") {
        delete_consciousness();
        log("consciousness.dat cleared.");
    }
    else {
        log("Unknown command: " + verb + ". Type 'help' for commands.");
    }
}

void DevConsole::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_) return;

    int con_h = std::min(20, screen_h / 2);
    if (con_h < 10) con_h = 10;
    Rect bounds{0, screen_h - con_h, screen_w, con_h};
    UIContext outer(renderer, bounds);
    auto ctx = outer.panel({
        .title = "Console",
        .footer = "[Esc] Close  [Enter] Execute  [Up/Down] History  [PgUp/PgDn] Scroll",
    });

    int content_h = ctx.height();
    int input_row = content_h - 1;

    // Input prompt with in-place cursor
    std::string display = input_;
    if (cursor_ >= display.size()) display += ' ';
    ctx.styled_text({.x = 0, .y = input_row, .segments = {
        {"> ", UITag::TextAccent},
        {display, UITag::TextBright},
    }});
    char cursor_ch = (cursor_ < input_.size()) ? input_[cursor_] : ' ';
    ctx.put(2 + static_cast<int>(cursor_), input_row, cursor_ch,
            Color::Black, Color::White);

    // Scrollable output
    int out_rows = input_row;
    int total = static_cast<int>(output_.size());

    int max_scroll = total - out_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_ > max_scroll) scroll_ = max_scroll;

    int end = total - scroll_;
    int start = end - out_rows;
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < end && row < out_rows; ++i, ++row) {
        const auto& line = output_[i];
        // Echo lines (starting with >) show bright, output shows dim
        UITag tag = (line.size() >= 2 && line[0] == '>') ? UITag::TextBright : UITag::TextDim;
        ctx.text({.x = 0, .y = row, .content = line, .tag = tag});
    }
}

} // namespace astra
