#include "astra/ability.h"
#include "astra/game.h"
#include "astra/map_renderer.h"
#include "terminal_theme.h"
#include "astra/overworld_stamps.h"
#include "astra/tile_props.h"
#include "astra/ui.h"

namespace astra {

Color Game::hp_color() const {
    int pct = (player_.max_hp > 0) ? (100 * player_.hp / player_.max_hp) : 0;
    if (pct < 30) return Color::Red;
    if (pct < 80) return Color::Yellow;
    return Color::Green;
}

Color Game::hunger_color() const {
    switch (player_.hunger) {
        case HungerState::Satiated: return Color::Green;
        case HungerState::Normal:   return Color::Default;
        case HungerState::Hungry:   return Color::Yellow;
        case HungerState::Starving: return Color::Red;
    }
    return Color::Default;
}

// Block-letter ASTRA logo using █ (U+2588 full block)
#define B "\xe2\x96\x88"  // █
static const char* title_letter_A[] = {
    "    " B B B "    ", "   " B B " " B B "   ", "  " B B "   " B B "  ",
    " " B B "     " B B " ", " " B B B B B B B B B " ", " " B B "     " B B " ", " " B B "     " B B " ",
};
static const char* title_letter_S[] = {
    "  " B B B B B B B "  ", " " B B "     " B B " ", " " B B "        ",
    "  " B B B B B B B "  ", "        " B B " ", " " B B "     " B B " ", "  " B B B B B B B "  ",
};
static const char* title_letter_T[] = {
    " " B B B B B B B B B " ", "    " B B B "    ", "    " B B B "    ",
    "    " B B B "    ", "    " B B B "    ", "    " B B B "    ", "    " B B B "    ",
};
static const char* title_letter_R[] = {
    " " B B B B B B B B "  ", " " B B "     " B B " ", " " B B "     " B B " ",
    " " B B B B B B B B "  ", " " B B "   " B B "   ", " " B B "    " B B "  ", " " B B "     " B B " ",
};
#undef B
static const char* const* title_letters[] = {
    title_letter_A, title_letter_S, title_letter_T, title_letter_R, title_letter_A,
};
static constexpr int title_letter_count = 5;
static constexpr int title_letter_height = 7;
static constexpr int title_letter_width = 11;
static constexpr int title_letter_gap = 2;

static const char* menu_items[] = {
#ifdef ASTRA_DEV_MODE
    "Developer Mode (new character)",
#endif
    "New Game",
    "Load Game",
    "Hall of Fame",
#ifdef ASTRA_DEV_MODE
    "Editor (WIP)",
#endif
    "Quit",
};

static const char* tab_names[] = {
    "Messages",
    "Equipment",
    "Ship",
    "Wait",
};

static const char* fixture_type_name(FixtureType type) {
    switch (type) {
        case FixtureType::Table:         return "Table";
        case FixtureType::Console:       return "Console";
        case FixtureType::Crate:         return "Crate";
        case FixtureType::Bunk:          return "Bunk";
        case FixtureType::Rack:          return "Rack";
        case FixtureType::Conduit:       return "Conduit";
        case FixtureType::ShuttleClamp:  return "Shuttle Clamp";
        case FixtureType::Shelf:         return "Shelf";
        case FixtureType::Viewport:      return "Viewport";
        case FixtureType::Door:          return "Door";
        case FixtureType::Window:        return "Window";
        case FixtureType::Torch:         return "Torch";
        case FixtureType::Stool:         return "Stool";
        case FixtureType::Debris:        return "Debris";
        case FixtureType::HealPod:       return "Healing Pod";
        case FixtureType::FoodTerminal:  return "Food Terminal";
        case FixtureType::WeaponDisplay: return "Weapon Display";
        case FixtureType::RepairBench:   return "Repair Bench";
        case FixtureType::SupplyLocker:  return "Supply Locker";
        case FixtureType::StarChart:     return "Star Chart";
        case FixtureType::RestPod:       return "Rest Pod";
        case FixtureType::ShipTerminal:  return "Ship Terminal";
        case FixtureType::CommandTerminal: return "Command Terminal";
        case FixtureType::DungeonHatch:    return "Floor Hatch";
        case FixtureType::StairsUp:        return "Stairs Up";
        case FixtureType::NaturalObstacle: return "Natural Obstacle";
        case FixtureType::SettlementProp:  return "Settlement Prop";
    }
    return "Unknown";
}

static const char* fixture_type_desc(FixtureType type) {
    switch (type) {
        case FixtureType::Table:         return "A sturdy surface bolted to the deck.";
        case FixtureType::Console:       return "A terminal display scrolls with data.";
        case FixtureType::Crate:         return "A sealed cargo crate. Heavy and immovable.";
        case FixtureType::Bunk:          return "A narrow sleeping rack with a thin mattress.";
        case FixtureType::Rack:          return "A wall-mounted storage rack.";
        case FixtureType::Conduit:       return "Exposed piping hums with energy.";
        case FixtureType::ShuttleClamp:  return "Magnetic clamps securing a docked vessel.";
        case FixtureType::Shelf:         return "Metal shelving lined with containers.";
        case FixtureType::Viewport:      return "A reinforced window looking out into space.";
        case FixtureType::Door:          return "A reinforced bulkhead door.";
        case FixtureType::Window:        return "A transparent panel set into the wall.";
        case FixtureType::Torch:         return "A flickering wall-mounted lamp.";
        case FixtureType::Stool:         return "A simple seat bolted to the floor.";
        case FixtureType::Debris:        return "Scattered wreckage and broken components.";
        case FixtureType::HealPod:       return "A medical pod that restores health using nanite technology.";
        case FixtureType::FoodTerminal:  return "An automated food dispensary serving synth-grub and rations.";
        case FixtureType::WeaponDisplay: return "Weapons gleam behind reinforced glass.";
        case FixtureType::RepairBench:   return "A workbench with tools for equipment maintenance.";
        case FixtureType::SupplyLocker:  return "A secure locker for storing personal equipment.";
        case FixtureType::StarChart:     return "A holographic star chart projector.";
        case FixtureType::RestPod:       return "A padded pod for deep restorative sleep.";
        case FixtureType::ShipTerminal:  return "A terminal for boarding your docked starship.";
        case FixtureType::CommandTerminal: return "ARIA — the ship's autonomous intelligence. Manages all onboard systems.";
        case FixtureType::DungeonHatch:    return "A heavy floor hatch with caution markings. Leads to the maintenance tunnels below.";
        case FixtureType::StairsUp:        return "Stairs leading back up to the surface.";
        case FixtureType::NaturalObstacle: return "A natural formation blocking the path.";
        case FixtureType::SettlementProp:  return "A piece of settlement infrastructure.";
    }
    return "";
}

std::string Game::look_tile_name(int mx, int my) const {
    // NPC
    for (const auto& npc : world_.npcs()) {
        if (npc.x == mx && npc.y == my) return npc.display_name();
    }
    // Player
    if (mx == player_.x && my == player_.y) return player_.name;
    // Ground item
    for (const auto& gi : world_.ground_items()) {
        if (gi.x == mx && gi.y == my) return gi.item.name;
    }
    // Fixture
    Tile t = world_.map().get(mx, my);
    if (t == Tile::Fixture) {
        int fid = world_.map().fixture_id(mx, my);
        if (fid >= 0) return fixture_type_name(world_.map().fixture(fid).type);
    }
    // Overworld tiles
    switch (t) {
        case Tile::OW_Plains:       return "Plains";
        case Tile::OW_Mountains:    return "Mountains";
        case Tile::OW_Crater:       return "Crater";
        case Tile::OW_IceField:     return "Ice Field";
        case Tile::OW_LavaFlow:     return "Lava Flow";
        case Tile::OW_Desert:       return "Desert";
        case Tile::OW_Fungal:       return "Fungal Growth";
        case Tile::OW_Forest:       return "Dense Forest";
        case Tile::OW_River:        return "River";
        case Tile::OW_Lake:         return "Lake";
        case Tile::OW_Swamp:        return "Swamp";
        case Tile::OW_CaveEntrance: return "Cave Entrance";
        case Tile::OW_Ruins:        return "Ruins";
        case Tile::OW_Settlement:   return "Settlement";
        case Tile::OW_CrashedShip:  return "Crashed Ship";
        case Tile::OW_Outpost:      return "Outpost";
        case Tile::OW_Landing:      return "Landing Pad";
        case Tile::Wall: case Tile::StructuralWall: return "Wall";
        case Tile::Floor:           return "";
        case Tile::IndoorFloor:     return "";
        case Tile::Portal:          return "Stairs";
        case Tile::Water:           return "Water";
        case Tile::Ice:             return "Ice";
        case Tile::Empty:           return "Void";
        default: return "";
    }
}

std::string Game::look_tile_desc(int mx, int my) const {
    // NPC
    for (const auto& npc : world_.npcs()) {
        if (npc.x == mx && npc.y == my) {
            std::string desc = std::string(race_name(npc.race));
            if (!npc.role.empty()) desc += " " + npc.role;
            desc += ".";
            if (npc.interactions.talk) {
                desc += " \"" + npc.interactions.talk->greeting + "\"";
            }
            return desc;
        }
    }
    // Player
    if (mx == player_.x && my == player_.y) {
        return std::string(race_name(player_.race)) + " " +
               class_name(player_.player_class) + ". Level " +
               std::to_string(player_.level) + ".";
    }
    // Ground item
    for (const auto& gi : world_.ground_items()) {
        if (gi.x == mx && gi.y == my) return gi.item.description;
    }
    // Fixture
    Tile t = world_.map().get(mx, my);
    if (t == Tile::Fixture) {
        int fid = world_.map().fixture_id(mx, my);
        if (fid >= 0) return fixture_type_desc(world_.map().fixture(fid).type);
    }
    // Overworld tiles
    switch (t) {
        case Tile::OW_Plains:       return "Open terrain stretches to the horizon.";
        case Tile::OW_Mountains:    return "Towering peaks of jagged rock pierce the atmosphere.";
        case Tile::OW_Crater:       return "A massive impact crater scarring the surface.";
        case Tile::OW_IceField:     return "A frozen expanse of crystalline ice.";
        case Tile::OW_LavaFlow:     return "Molten rock glows beneath a cracked surface.";
        case Tile::OW_Desert:       return "Barren dunes of fine dust stretch endlessly.";
        case Tile::OW_Fungal:       return "Strange luminescent fungi carpet the ground.";
        case Tile::OW_Forest:       return "Dense alien vegetation blocks the view.";
        case Tile::OW_River:        return "A rushing current of liquid cuts through the terrain.";
        case Tile::OW_Lake:         return "A vast body of liquid shimmers under the sky.";
        case Tile::OW_Swamp:        return "Murky pools and twisted growth make footing treacherous.";
        case Tile::OW_CaveEntrance: return "A dark opening leads underground. Press > to enter.";
        case Tile::OW_Ruins:        return "Crumbling remains of an ancient structure.";
        case Tile::OW_Settlement:   return "A small settlement. Signs of habitation are visible.";
        case Tile::OW_CrashedShip:  return "The twisted wreckage of a downed vessel.";
        case Tile::OW_Outpost:      return "A fortified outpost overlooks the terrain.";
        case Tile::OW_Landing:      return "A flat landing pad. Your ship is docked here.";
        case Tile::Wall: case Tile::StructuralWall:
            return "Solid construction blocks the way.";
        case Tile::Floor: case Tile::IndoorFloor:
            return "Smooth plating covers the deck.";
        case Tile::Portal:          return "Passage leading to another level.";
        case Tile::Water:           return "Murky liquid pools on the ground.";
        case Tile::Ice:             return "A slick frozen surface.";
        case Tile::Empty:           return "The void between the stars.";
        default: return "";
    }
}

void Game::render_look_popup() {
    if (!input_.looking()) return;

    Visibility v = world_.visibility().get(input_.look_x(), input_.look_y());
    if (v == Visibility::Unexplored) return;

    std::string name = look_tile_name(input_.look_x(), input_.look_y());
    if (name.empty()) return;

    std::string desc = look_tile_desc(input_.look_x(), input_.look_y());
    std::string glyph = std::string(input_.cached_glyph()); // cached from render_map (input_.look_x(), input_.look_y());
    Color glyph_color = input_.cached_color(); // cached from render_map (input_.look_x(), input_.look_y());

    // Word-wrap description to fit popup width
    int popup_w = std::max(36, std::min(static_cast<int>(name.size()) + 12, screen_w_ / 2));
    int inner_w = popup_w - 4; // padding

    std::vector<std::string> desc_lines;
    if (!desc.empty()) {
        std::string line;
        for (size_t i = 0; i < desc.size(); ++i) {
            if (desc[i] == ' ' && static_cast<int>(line.size()) >= inner_w) {
                desc_lines.push_back(line);
                line.clear();
            } else {
                line += desc[i];
                if (static_cast<int>(line.size()) >= inner_w + 5) {
                    desc_lines.push_back(line);
                    line.clear();
                }
            }
        }
        if (!line.empty()) desc_lines.push_back(line);
    }

    // Popup height: top(1) + glyph_row(1) + name_row(1) + sep(1) + desc_lines + bottom(1)
    int popup_h = 4 + static_cast<int>(desc_lines.size()) + (desc_lines.empty() ? 0 : 1);

    // Center popup on screen
    int px = (screen_w_ - popup_w) / 2;
    int py = (screen_h_ - popup_h) / 2;

    // Draw Panel-style popup
    Rect bounds{px, py, popup_w, popup_h};
    DrawContext ctx(renderer_.get(), bounds);
    ctx.fill(' ');

    Color border = Color::White;
    // Top: ▐▀▀▀▌
    ctx.put(0, 0, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < popup_w - 1; ++x)
        ctx.put(x, 0, BoxDraw::UPPER_HALF, border);
    ctx.put(popup_w - 1, 0, BoxDraw::LEFT_HALF, border);
    // Sides
    for (int y = 1; y < popup_h - 1; ++y) {
        ctx.put(0, y, BoxDraw::RIGHT_HALF, border);
        ctx.put(popup_w - 1, y, BoxDraw::LEFT_HALF, border);
    }
    // Bottom: ▐▄▄▄▌
    ctx.put(0, popup_h - 1, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < popup_w - 1; ++x)
        ctx.put(x, popup_h - 1, BoxDraw::LOWER_HALF, border);
    ctx.put(popup_w - 1, popup_h - 1, BoxDraw::LEFT_HALF, border);

    // Glyph centered
    int row = 1;
    ctx.put(popup_w / 2, row, glyph.c_str(), glyph_color);
    row++;

    // Name centered
    int name_x = (popup_w - static_cast<int>(name.size())) / 2;
    ctx.text(name_x, row, name, Color::White);
    row++;

    // Separator
    if (!desc_lines.empty()) {
        for (int x = 1; x < popup_w - 1; ++x)
            ctx.put(x, row, BoxDraw::H, Color::DarkGray);
        row++;

        // Description lines
        for (const auto& dl : desc_lines) {
            ctx.text(2, row, dl, Color::DarkGray);
            row++;
        }
    }
}

void Game::pickup_ground_item() {
    for (auto it = world_.ground_items().begin(); it != world_.ground_items().end(); ++it) {
        if (it->x == player_.x && it->y == player_.y) {
            std::string picked_name = it->item.name;
            if (it->item.type == ItemType::ShipComponent) {
                // Ship components go to ship cargo
                log("You pick up " + picked_name + " (stored in ship cargo).");
                player_.ship.cargo.push_back(std::move(it->item));
            } else {
                if (!player_.inventory.can_add(it->item)) {
                    log("Too heavy to pick up " + it->item.name + ".");
                    return;
                }
                log("You pick up " + picked_name + ".");
                player_.inventory.items.push_back(std::move(it->item));
            }
            world_.ground_items().erase(it);
            quest_manager_.on_item_picked_up(picked_name);
            advance_world(ActionCost::move);
            return;
        }
    }
    log("Nothing here to pick up.");
}

void Game::drop_item(int index) {
    auto& items = player_.inventory.items;
    if (index < 0 || index >= static_cast<int>(items.size())) return;

    Item item = std::move(items[index]);
    items.erase(items.begin() + index);

    log("You drop " + item.name + ".");
    world_.ground_items().push_back({player_.x, player_.y, std::move(item)});
}

void Game::use_item(int index) {
    auto& items = player_.inventory.items;
    if (index < 0 || index >= static_cast<int>(items.size())) return;

    auto& item = items[index];
    switch (item.type) {
        case ItemType::Food: {
            int heal = item.buy_value * 5; // scale heal with item value
            if (heal < 5) heal = 5;
            player_.hp = std::min(player_.hp + heal, player_.max_hp);
            if (player_.hunger > HungerState::Satiated)
                player_.hunger = static_cast<HungerState>(
                    static_cast<uint8_t>(player_.hunger) - 1);
            log("You eat the " + item.name + ". (+" +
                std::to_string(heal) + " HP)");
            break;
        }
        case ItemType::Stim: {
            int heal = 5;
            player_.hp = std::min(player_.hp + heal, player_.max_hp);
            log("You inject the " + item.name + ". (+5 HP)");
            break;
        }
        case ItemType::Battery: {
            auto& eq = player_.equipment.missile;
            if (!eq || !eq->ranged) {
                log("No ranged weapon equipped to recharge.");
                return;
            }
            auto& rd = *eq->ranged;
            if (rd.current_charge >= rd.charge_capacity) {
                log("Weapon is already fully charged.");
                return;
            }
            int added = std::min(5, rd.charge_capacity - rd.current_charge);
            rd.current_charge += added;
            log("You recharge " + eq->name + ". (+" + std::to_string(added) + " charge)");
            break;
        }
        default:
            log("You can't use " + item.name + ".");
            return;
    }

    // Consume the item
    if (item.stackable && item.stack_count > 1) {
        --item.stack_count;
    } else {
        items.erase(items.begin() + index);
    }
    advance_world(ActionCost::wait);
}

void Game::equip_item(int index) {
    auto& items = player_.inventory.items;
    if (index < 0 || index >= static_cast<int>(items.size())) return;

    auto& item = items[index];
    if (!item.slot.has_value()) {
        log("Can't equip " + item.name + ".");
        return;
    }

    auto& slot = player_.equipment.slot_ref(*item.slot);
    Item to_equip = std::move(item);
    items.erase(items.begin() + index);

    if (slot) {
        // Swap: old equipped item goes to inventory
        items.push_back(std::move(*slot));
        log("You unequip " + items.back().name + ".");
    }
    log("You equip " + to_equip.name + ".");
    slot = std::move(to_equip);
}

void Game::unequip_slot(int index) {
    auto slot = static_cast<EquipSlot>(index);
    auto& opt = player_.equipment.slot_ref(slot);
    if (!opt) {
        log("Nothing equipped in that slot.");
        return;
    }

    Item item = std::move(*opt);
    opt.reset();

    if (!player_.inventory.can_add(item)) {
        log("Inventory too heavy. " + item.name + " stays equipped.");
        opt = std::move(item);
        return;
    }
    log("You unequip " + item.name + ".");
    player_.inventory.items.push_back(std::move(item));
}



void Game::handle_gameover_input(int key) {
    switch (key) {
        case '\n': case '\r':
            state_ = GameState::MainMenu;
            menu_selection_ = 0;
            break;
        case 'q': case 'Q':
            running_ = false;
            break;
    }
}

void Game::handle_load_input(int key) {
    switch (key) {
        case '\033':
            state_ = prev_state_;
            if (state_ == GameState::MainMenu) menu_selection_ = 0;
            break;
        case 'w': case 'k': case KEY_UP:
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ - 1 + static_cast<int>(save_slots_.size()))
                                  % static_cast<int>(save_slots_.size());
            }
            break;
        case 's': case 'j': case KEY_DOWN:
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ + 1) % static_cast<int>(save_slots_.size());
            }
            break;
        case '\n': case '\r':
            if (!save_slots_.empty() &&
                load_selection_ >= 0 &&
                load_selection_ < static_cast<int>(save_slots_.size())) {
                if (save_system_.load(save_slots_[load_selection_].filename, *this)) {
                    log("Game loaded.");
                }
            }
            break;
    }
}

void Game::handle_hall_input(int key) {
    switch (key) {
        case '\033':
            state_ = GameState::MainMenu;
            menu_selection_ = 0;
            confirm_delete_ = false;
            break;
        case 'w': case 'k': case KEY_UP:
            confirm_delete_ = false;
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ - 1 + static_cast<int>(save_slots_.size()))
                                  % static_cast<int>(save_slots_.size());
            }
            break;
        case 's': case 'j': case KEY_DOWN:
            confirm_delete_ = false;
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ + 1) % static_cast<int>(save_slots_.size());
            }
            break;
        case 'd': case 'D':
            if (!save_slots_.empty()) {
                confirm_delete_ = !confirm_delete_;
            }
            break;
        case 'y': case 'Y':
            if (confirm_delete_ && !save_slots_.empty() &&
                load_selection_ >= 0 &&
                load_selection_ < static_cast<int>(save_slots_.size())) {
                delete_save(save_slots_[load_selection_].filename);
                save_slots_.erase(save_slots_.begin() + load_selection_);
                if (load_selection_ >= static_cast<int>(save_slots_.size())) {
                    load_selection_ = static_cast<int>(save_slots_.size()) - 1;
                }
                if (load_selection_ < 0) load_selection_ = 0;
                confirm_delete_ = false;
            }
            break;
        case 'n': case 'N':
            confirm_delete_ = false;
            break;
    }
}


void Game::update() {
    // Tick-based — world updates happen in response to player actions.
    // Sync quest journal entries with current objective progress
    if (state_ == GameState::Playing) {
        quest_manager_.update_quest_journals(player_);
    }
}

// --- Rendering ---

void Game::render() {
    renderer_->clear();

    switch (state_) {
        case GameState::MainMenu:   render_menu();          break;
        case GameState::Playing:    render_play();          break;
        case GameState::GameOver:   render_gameover();      break;
        case GameState::LoadMenu:   render_load_menu();     break;
        case GameState::HallOfFame: render_hall_of_fame();  break;
    }

    renderer_->present();
}

// Deterministic star at any world coordinate
static char star_at(int x, int y) {
    // Hash the coordinate to get a deterministic pseudo-random value
    unsigned h = static_cast<unsigned>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    h ^= h >> 16;
    if ((h % 100) >= 3) return '\0'; // ~3% chance of a star
    unsigned st = (h >> 8) % 10;
    if (st < 6) return '.';
    if (st < 9) return '*';
    return '+';
}

void Game::render_menu() {
    // Character creation overlay
    if (character_creation_.is_open()) {
        character_creation_.draw(screen_w_, screen_h_);
        return;
    }

    DrawContext ctx(renderer_.get(), screen_rect_);

    // Starfield backdrop
    for (int sy = 0; sy < screen_h_; ++sy) {
        for (int sx = 0; sx < screen_w_; ++sx) {
            char star = star_at(sx, sy);
            if (star) {
                Color c = (star == '+') ? Color::Yellow
                        : (star == '*') ? Color::White
                                        : Color::Cyan;
                ctx.put(sx, sy, star, c);
            }
        }
    }

    // Block-letter ASTRA logo
    int logo_w = title_letter_count * title_letter_width
               + (title_letter_count - 1) * title_letter_gap;
    int logo_x = (screen_w_ - logo_w) / 2;
    int art_start_y = screen_h_ / 2 - title_letter_height - 2;

    for (int row = 0; row < title_letter_height; ++row) {
        int lx = logo_x;
        for (int li = 0; li < title_letter_count; ++li) {
            ctx.text(lx, art_start_y + row, title_letters[li][row], Color::White);
            lx += title_letter_width + title_letter_gap;
        }
    }

    int menu_y = art_start_y + title_letter_height + 2;
    for (int i = 0; i < menu_item_count_; ++i) {
        if (i == menu_selection_) {
            // ·::|  Label  |::·
            std::string item = menu_items[i];
            std::string padded = "  " + item + "  ";
            std::string full = ".::|" + padded + "|::.";
            int x = (screen_w_ - static_cast<int>(full.size())) / 2;
            ctx.text(x, menu_y + i, ".::", Color::Red);
            ctx.put(x + 3, menu_y + i, BoxDraw::V, Color::Cyan);
            ctx.text(x + 4, menu_y + i, padded, Color::Yellow);
            ctx.put(x + 4 + static_cast<int>(padded.size()), menu_y + i, BoxDraw::V, Color::Cyan);
            ctx.text(x + 5 + static_cast<int>(padded.size()), menu_y + i, "::.", Color::Red);
        } else {
            std::string label = "     " + std::string(menu_items[i]) + "     ";
            ctx.text_center(menu_y + i, label, Color::DarkGray);
        }
    }

    // Quit confirm overlay on menu
    render_quit_confirm();
}

void Game::render_play() {
    // Map editor draws its own full-screen UI (except during play-test)
    if (map_editor_.is_open() && !map_editor_.playing()) {
        map_editor_.draw(screen_w_, screen_h_);
        return;
    }

    render_stats_bar();
    render_bars();

    render_tabs();

    UIContext sep_ctx(renderer_.get(), separator_rect_);
    sep_ctx.separator({.vertical = true});

    render_map({renderer_.get(), map_rect_, world_, player_, combat_, input_, camera_x_, camera_y_, &animations_});

    if (panel_visible_) {
        render_side_panel();
    }
    UIContext bottom_sep(renderer_.get(), bottom_sep_rect_);
    bottom_sep.separator({});

    render_effects_bar();
    render_abilities_bar();

    // Overlay windows
    if (inspecting_item_) render_item_inspect();
    render_look_popup();
    dialog_.draw(renderer_.get(), screen_w_, screen_h_);
    render_pause_menu();
    render_quit_confirm();
    console_.draw(renderer_.get(), screen_w_, screen_h_);
    help_screen_.draw(renderer_.get(), screen_w_, screen_h_);
    repair_bench_.draw(screen_w_, screen_h_);
    trade_window_.draw(screen_w_, screen_h_);
    character_screen_.draw(screen_w_, screen_h_);
    star_chart_viewer_.draw(screen_w_, screen_h_);
    lost_popup_.draw(renderer_.get(), screen_w_, screen_h_);

    // Welcome screen overlay
    if (show_welcome_) {
        render_welcome_screen();
    }
}

void Game::render_stats_bar() {
    DrawContext ctx(renderer_.get(), stats_bar_rect_);

    // Dev mode indicator
    int lx = 1;
    if (dev_mode_) {
        ctx.text(lx, 0, "[DEV]", Color::Red);
        lx += 6;
    }

    // Left side: level :: temp :: hunger :: money
    lx = ctx.label_value(lx, 0, "LVL:", Color::DarkGray,
        std::to_string(player_.level), Color::White);

    ctx.text(lx, 0, " :: ", Color::DarkGray); lx += 4;
    lx = ctx.label_value(lx, 0, "T:", Color::DarkGray,
        std::to_string(player_.temperature) + "~", Color::White);

    const char* hname = hunger_name(player_.hunger);
    if (hname[0] != '\0') {
        ctx.text(lx, 0, " :: ", Color::DarkGray); lx += 4;
        ctx.text(lx, 0, hname, hunger_color());
        lx += static_cast<int>(std::string_view(hname).size());
    }

    ctx.text(lx, 0, " :: ", Color::DarkGray); lx += 4;
    lx = ctx.label_value(lx, 0, "", Color::DarkGray,
        std::to_string(player_.money) + "$", Color::Yellow);

    // Build calendar string for width measurement: "C1 D3 [▓▓▓▒░░░░] ☀"
    // Progress bar is 8 chars + brackets = 10, icon = 1
    std::string cal = format_calendar(world_.world_tick());
    cal += " [--------] "; // placeholder for width calc (8-char bar + brackets + space)
    cal += phase_icon(world_.day_clock().phase());

    // Right side: stats, calendar, location — measure total width for right-alignment
    std::string right;
    int eff_qn = player_.quickness + player_.equipment.total_modifiers().quickness;
    right += " QN:";  right += std::to_string(eff_qn);
    right += " :: MS:"; right += std::to_string(player_.move_speed);
    right += " :: AV:"; right += std::to_string(player_.effective_attack());
    right += " :: DV:"; right += std::to_string(player_.effective_dodge());
    right += " :: ";    right += cal;
    right += " :: ";    right += world_.map().location_name();
    right += " ";

    int rx = ctx.width() - static_cast<int>(right.size());
    if (rx < lx + 2) rx = lx + 2;

    // Fill gap between left items and right items with repeating <<>>
    {
        const char pattern[] = "<<>>";
        int gap_start = lx + 1;
        int gap_end = rx - 1;
        for (int i = gap_start; i < gap_end; ++i) {
            ctx.put(i, 0, pattern[(i - gap_start) % 4], Color::Cyan);
        }
    }

    // Render right side with per-segment colors
    int x = rx;
    x = ctx.label_value(x, 0, "QN:", Color::DarkGray,
        std::to_string(player_.quickness + player_.equipment.total_modifiers().quickness), Color::White);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "MS:", Color::DarkGray,
        std::to_string(player_.move_speed), Color::White);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "AV:", Color::DarkGray,
        std::to_string(player_.effective_attack()), Color::Blue);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "DV:", Color::DarkGray,
        std::to_string(player_.effective_dodge()), Color::Blue);

    // Calendar + day progress bar + phase icon
    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    {
        std::string cal_text = format_calendar(world_.world_tick()) + " ";
        ctx.text(x, 0, cal_text, Color::DarkGray);
        x += static_cast<int>(cal_text.size());

        // Phase color
        Color phase_col;
        switch (world_.day_clock().phase()) {
            case TimePhase::Dawn: phase_col = Color::Yellow; break;
            case TimePhase::Day:  phase_col = Color::Yellow; break;
            case TimePhase::Dusk: phase_col = static_cast<Color>(130); break;
            case TimePhase::Night:phase_col = Color::Blue; break;
        }

        // Day progress bar: [▓▓▓▒░░░░]
        constexpr int bar_len = 8;
        float frac = world_.day_clock().day_fraction();
        int filled = static_cast<int>(frac * bar_len);
        if (filled > bar_len) filled = bar_len;

        ctx.put(x, 0, '[', Color::DarkGray); ++x;
        for (int i = 0; i < bar_len; ++i) {
            if (i < filled) {
                // ▓ filled
                ctx.text(x, 0, "\xe2\x96\x93", phase_col);
            } else if (i == filled) {
                // ▒ current position
                ctx.text(x, 0, "\xe2\x96\x92", phase_col);
            } else {
                // ░ empty
                ctx.text(x, 0, "\xe2\x96\x91", Color::DarkGray);
            }
            ++x;
        }
        ctx.put(x, 0, ']', Color::DarkGray); ++x;

        // Phase icon
        ctx.text(x, 0, " ", Color::Default); ++x;
        ctx.text(x, 0, phase_icon(world_.day_clock().phase()), phase_col);
        x += 1;
    }

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    ctx.text(x, 0, world_.map().location_name(), Color::White);
}

void Game::render_bars() {
    // Format value strings and find max width for alignment
    std::string hp_val = std::to_string(player_.hp) + "/" + std::to_string(player_.max_hp);
    std::string xp_val = std::to_string(player_.xp) + "/" + std::to_string(player_.max_xp);
    int val_w = static_cast<int>(std::max(hp_val.size(), xp_val.size()));

    // Right-justify values by padding with spaces
    while (static_cast<int>(hp_val.size()) < val_w) hp_val = " " + hp_val;
    while (static_cast<int>(xp_val.size()) < val_w) xp_val = " " + xp_val;

    // "HP: " and "XP: " are both 4 chars — labels already align
    // bar_start = 1 (margin) + 4 (label) + val_w + 1 (space)
    int bar_start = 1 + 4 + val_w + 1;

    // HP bar — hp_color() provides value-aware coloring (green/yellow/red)
    // but UITag::StatHealth is fixed green, so we keep hp_color() for the
    // value text via raw ctx.text() while using semantic progress_bar().
    {
        DrawContext ctx(renderer_.get(), hp_bar_rect_);
        ctx.text(1, 0, "HP:", Color::DarkGray);
        ctx.text(4, 0, hp_val, hp_color());
        int bar_w = ctx.width() - bar_start - 2;
        if (bar_w > 0) {
            ctx.progress_bar({.x=bar_start, .y=0, .width=bar_w,
                              .value=player_.hp, .max=player_.max_hp,
                              .tag=UITag::HealthBar});
        }
    }

    // XP bar
    {
        DrawContext ctx(renderer_.get(), xp_bar_rect_);
        ctx.label_value({.x=1, .y=0, .label="XP:", .label_tag=UITag::TextDim,
                         .value=xp_val, .value_tag=UITag::XpBar});
        int bar_w = ctx.width() - bar_start - 2;
        if (bar_w > 0) {
            ctx.progress_bar({.x=bar_start, .y=0, .width=bar_w,
                              .value=player_.xp, .max=player_.max_xp,
                              .tag=UITag::XpBar});
        }
    }
}

void Game::render_tabs() {
    UIContext ctx(renderer_.get(), tabs_rect_);

    // Build tab names vector from the static array
    std::vector<std::string> tabs;
    for (int i = 0; i < panel_tab_count; ++i)
        tabs.push_back(tab_names[i]);

    ctx.tab_bar({.tabs = tabs, .active = active_tab_});

    // Separator below tabs
    UIContext sep(renderer_.get(), {tabs_rect_.x, tabs_rect_.y + 1, tabs_rect_.w, 1});
    sep.separator({});
}


void Game::render_side_panel() {
    DrawContext ctx(renderer_.get(), side_panel_rect_);

    switch (static_cast<PanelTab>(active_tab_)) {
        case PanelTab::Messages: {
            int max_w = ctx.width();
            int visible = ctx.height();
            if (max_w <= 0 || visible <= 0) break;

            // Word-wrap all messages into display lines
            // Continuation lines are indented to align past the ":: " prefix
            static constexpr int indent = 3; // matches ":: " prefix
            // Count visible (non-marker) characters in a string
            auto visible_len = [](std::string_view s) {
                int len = 0;
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] == COLOR_BEGIN) { ++i; continue; } // skip color byte
                    if (s[i] == COLOR_END) continue;
                    ++len;
                }
                return len;
            };

            struct WrappedLine {
                std::string_view text;
                int x; // 0 for first line, indent for continuations
            };
            std::vector<WrappedLine> lines;

            for (const auto& msg : messages_) {
                std::string_view remaining = msg;
                bool first = true;
                while (!remaining.empty()) {
                    int line_w = first ? max_w : max_w - indent;
                    if (line_w <= 0) line_w = 1;
                    int x = first ? 0 : indent;
                    first = false;

                    if (visible_len(remaining) <= line_w) {
                        lines.push_back({remaining, x});
                        break;
                    }
                    // Find cut point at line_w visible chars, preferring word boundaries
                    int vis = 0;
                    int cut = 0;
                    int last_space = -1;
                    for (size_t i = 0; i < remaining.size() && vis < line_w; ++i) {
                        if (remaining[i] == COLOR_BEGIN) { ++i; continue; }
                        if (remaining[i] == COLOR_END) continue;
                        if (remaining[i] == ' ') last_space = static_cast<int>(i);
                        ++vis;
                        cut = static_cast<int>(i) + 1;
                    }
                    if (last_space > 0) cut = last_space;
                    if (cut <= 0) cut = static_cast<int>(remaining.size());
                    lines.push_back({remaining.substr(0, cut), x});
                    remaining = remaining.substr(cut);
                    if (!remaining.empty() && remaining[0] == ' ')
                        remaining = remaining.substr(1);
                }
            }

            // Scrollback: message_scroll_ = 0 means latest, >0 means scrolled up
            int total = static_cast<int>(lines.size());
            int max_scroll = (total > visible) ? total - visible : 0;
            if (message_scroll_ > max_scroll) message_scroll_ = max_scroll;
            int start = (total > visible) ? total - visible - message_scroll_ : 0;
            if (start < 0) start = 0;

            // Scroll indicator when scrolled back
            if (message_scroll_ > 0) {
                std::string indicator = "-- scrolled (" + std::to_string(message_scroll_) + ") [-/+] --";
                int ix = (max_w - static_cast<int>(indicator.size())) / 2;
                if (ix < 0) ix = 0;
                ctx.text(ix, 0, indicator, Color::Yellow);
                // Shift content down by 1 to make room for indicator
                start++;
                visible--;
            }

            int y = message_scroll_ > 0 ? 1 : 0;
            for (int i = start; i < start + visible && i < total; ++i, ++y) {
                // Render with inline color markers
                int px = lines[i].x;
                Color cur = Color::Default;
                for (size_t ci = 0; ci < lines[i].text.size(); ++ci) {
                    char ch = lines[i].text[ci];
                    if (ch == COLOR_BEGIN && ci + 1 < lines[i].text.size()) {
                        cur = static_cast<Color>(
                            static_cast<uint8_t>(lines[i].text[ci + 1]));
                        ++ci;
                        continue;
                    }
                    if (ch == COLOR_END) {
                        cur = Color::Default;
                        continue;
                    }
                    ctx.put(px, y, ch, cur);
                    ++px;
                }
            }
            break;
        }
        case PanelTab::Equipment: {
            const auto& eq = player_.equipment;
            int y = 0;
            auto draw_slot = [&](const char* label, const std::optional<Item>& slot) {
                if (y >= ctx.height() - 1) return;
                ctx.text(1, y, label, Color::DarkGray);
                int lx = 1 + static_cast<int>(std::string_view(label).size());
                if (slot) {
                    auto sv = item_visual(slot->item_def_id);
                    ctx.put(lx, y, sv.glyph, rarity_color(slot->rarity));
                    ctx.text(lx + 1, y, " " + slot->name, rarity_color(slot->rarity));
                } else {
                    ctx.text(lx, y, "---", Color::DarkGray);
                }
                ++y;
            };
            draw_slot("Face:    ", eq.face);
            draw_slot("Head:    ", eq.head);
            draw_slot("Body:    ", eq.body);
            draw_slot("L.Arm:   ", eq.left_arm);
            draw_slot("R.Arm:   ", eq.right_arm);
            draw_slot("L.Hand:  ", eq.left_hand);
            draw_slot("R.Hand:  ", eq.right_hand);
            draw_slot("Back:    ", eq.back);
            draw_slot("Feet:    ", eq.feet);
            draw_slot("Thrown:  ", eq.thrown);
            draw_slot("Missile: ", eq.missile);

            // Stat bonuses summary
            y++;
            if (y < ctx.height() - 1) {
                auto mods = eq.total_modifiers();
                ctx.text(1, y, "Bonuses:", Color::DarkGray);
                ++y;
                if (mods.attack && y < ctx.height() - 1) {
                    ctx.text(2, y, "ATK +" + std::to_string(mods.attack), Color::Red);
                    ++y;
                }
                if (mods.defense && y < ctx.height() - 1) {
                    ctx.text(2, y, "DEF +" + std::to_string(mods.defense), Color::Blue);
                    ++y;
                }
                if (mods.max_hp && y < ctx.height() - 1) {
                    ctx.text(2, y, "HP  +" + std::to_string(mods.max_hp), Color::Green);
                    ++y;
                }
                if (mods.view_radius && y < ctx.height() - 1) {
                    ctx.text(2, y, "VIS +" + std::to_string(mods.view_radius), Color::Cyan);
                    ++y;
                }
                if (mods.quickness && y < ctx.height() - 1) {
                    std::string sign = mods.quickness > 0 ? "+" : "";
                    ctx.text(2, y, "QCK " + sign + std::to_string(mods.quickness), Color::Yellow);
                    ++y;
                }
            }
            // Read-only — manage equipment via character screen (c)
            ctx.text(1, ctx.height() - 1, "[c] manage equipment", Color::DarkGray);
            break;
        }
        case PanelTab::Ship:
            ctx.text(1, 1, "Ship: Docked", Color::DarkGray);
            ctx.text(1, 2, "Hull: 100%", Color::DarkGray);
            break;
        case PanelTab::Wait: {
            // Time info header
            int y = 0;
            {
                Color phase_col;
                switch (world_.day_clock().phase()) {
                    case TimePhase::Dawn: phase_col = Color::Yellow; break;
                    case TimePhase::Day:  phase_col = Color::Yellow; break;
                    case TimePhase::Dusk: phase_col = static_cast<Color>(130); break;
                    case TimePhase::Night:phase_col = Color::Blue; break;
                }
                std::string time_info = std::string(phase_icon(world_.day_clock().phase()))
                    + " " + phase_name(world_.day_clock().phase());
                ctx.text(1, y, time_info, phase_col);
                ++y;
                ctx.text(1, y, format_calendar(world_.world_tick()), Color::DarkGray);
                y += 2;
            }

            // Wait options
            static const char* wait_options[] = {
                "Wait 1 turn",
                "Wait 10 turns",
                "Wait 50 turns",
                "Wait 100 turns",
                "Wait until healed",
                "Wait until morning",
            };
            static constexpr int wait_option_count = 6;
            for (int i = 0; i < wait_option_count && y < ctx.height() - 1; ++i) {
                bool selected = (i == wait_cursor_);
                std::string label = std::string("[") + std::to_string(i + 1) + "] " + wait_options[i];
                if (i == 5 && world_.day_clock().phase() == TimePhase::Day) {
                    // "Wait until morning" greyed out during day
                    ctx.text(1, y, label, Color::DarkGray);
                } else {
                    if (selected) ctx.text(0, y, ">", Color::Yellow);
                    ctx.text(1, y, label, selected ? Color::White : Color::Default);
                }
                ++y;
            }
            // Hints
            ctx.text(1, ctx.height() - 1, "+/- [Enter]wait [1-6]quick", Color::DarkGray);
            break;
        }
    }
}

void Game::render_item_inspect() {
    const auto& item = inspected_item_;

    int win_w = 44;
    int win_h = 18;
    if (win_w > screen_w_ - 4) win_w = screen_w_ - 4;
    if (win_h > screen_h_ - 4) win_h = screen_h_ - 4;

    Window win(renderer_.get(), screen_w_, screen_h_, win_w, win_h, item.name);
    win.set_footer("[any key] Close");
    win.draw();

    DrawContext ctx = win.content();
    draw_item_info(ctx, item);
}

void Game::render_effects_bar() {
    UIContext ctx(renderer_.get(), effects_rect_);

    // EFFECTS label + items — effects use per-effect Color, so we keep them
    // on the old text() API. Only the label and [none] use styled_text().
    std::vector<TextSegment> eff_segs;
    eff_segs.push_back({"EFFECTS: ", UITag::TextDim});

    bool any_effect = false;
    for (const auto& e : player_.effects) {
        if (!e.show_in_bar) continue;
        any_effect = true;
    }
    if (!any_effect) {
        eff_segs.push_back({"[none]", UITag::TextDim});
    }
    ctx.styled_text({.x = 1, .y = 0, .segments = eff_segs});

    // Per-effect items use raw Color (e.color varies per effect)
    if (any_effect) {
        int x = 1 + 9; // after "EFFECTS: "
        for (const auto& e : player_.effects) {
            if (!e.show_in_bar) continue;
            std::string label = e.name;
            if (e.remaining > 0) {
                label += "(" + std::to_string(e.remaining) + ")";
            }
            ctx.text(x, 0, label, e.color);
            x += static_cast<int>(label.size()) + 1;
        }
    }

    // TARGET section — uses NPC disposition color, stays on old API
    int mid = ctx.width() / 3;
    ctx.text(mid, 0, "TARGET:", Color::DarkGray);
    if (combat_.target_npc() && combat_.target_npc()->alive()) {
        std::string info = " " + combat_.target_npc()->display_name() +
            " (" + std::to_string(combat_.target_npc()->hp) + "/" +
            std::to_string(combat_.target_npc()->max_hp) + ")";
        Color tc = Color::DarkGray;
        switch (combat_.target_npc()->disposition) {
            case Disposition::Hostile:  tc = Color::Red; break;
            case Disposition::Neutral:  tc = Color::Yellow; break;
            case Disposition::Friendly: tc = Color::Green; break;
        }
        ctx.text(mid + 7, 0, info, tc);
    } else {
        ctx.text(mid + 7, 0, " [none]", Color::DarkGray);
    }

    // Ranged weapon hints (right-aligned) — uses dynamic charge color, stays on old API
    const auto& rw = player_.equipment.missile;
    if (rw && rw->ranged) {
        const auto& rd = *rw->ranged;
        std::string keys = "[t]arget [s]hoot [r]eload ";
        std::string charge = std::to_string(rd.current_charge) + "/" +
                             std::to_string(rd.charge_capacity);
        std::string full = keys + charge;
        int ix = ctx.width() - static_cast<int>(full.size()) - 1;
        Color charge_color = (rd.current_charge >= rd.charge_per_shot)
            ? Color::Cyan : Color::Red;
        ctx.text(ix, 0, keys, Color::DarkGray);
        ctx.text(ix + static_cast<int>(keys.size()), 0, charge, charge_color);
    }
}

void Game::render_abilities_bar() {
    UIContext ctx(renderer_.get(), abilities_rect_);

    std::vector<TextSegment> segs;
    segs.push_back({"ABILITIES: ", UITag::TextDim});

    auto bar = get_ability_bar(player_);
    if (bar.empty()) {
        segs.push_back({"[none]", UITag::TextDim});
    } else {
        for (int i = 0; i < 5 && i < static_cast<int>(bar.size()); ++i) {
            const auto* ab = find_ability(bar[i]);
            if (!ab) continue;
            bool on_cd = has_effect(player_.effects, ab->cooldown_effect);
            const auto* cd_eff = find_effect(player_.effects, ab->cooldown_effect);

            std::string label = "[" + std::to_string(i + 1) + "] " + ab->name;
            if (on_cd && cd_eff && cd_eff->remaining > 0) {
                label += "(" + std::to_string(cd_eff->remaining) + ")";
            }
            label += "  ";

            UITag tag = on_cd ? UITag::TextDim : UITag::TextWarning;
            segs.push_back({label, tag});
        }
    }

    ctx.styled_text({.x = 1, .y = 0, .segments = segs});
}

void Game::render_gameover() {
    DrawContext ctx(renderer_.get(), screen_rect_);

    int cy = screen_h_ / 2 - 4;
    ctx.text_center(cy,     "YOU HAVE DIED", Color::Red);
    cy += 2;
    if (!death_message_.empty()) {
        ctx.text_center(cy, death_message_);
        cy += 2;
    }
    ctx.text_center(cy,     "Survived " + std::to_string(world_.world_tick()) + " ticks");
    ctx.text_center(cy + 1, "Reached level " + std::to_string(player_.level));
    cy += 3;
    ctx.text_center(cy,     "[Enter] Main Menu    [Q] Quit", Color::DarkGray);
}

void Game::render_load_menu() {
    int win_w = std::min(screen_w_ - 4, 60);
    int win_h = std::min(screen_h_ - 4, static_cast<int>(save_slots_.size()) + 6);
    if (win_h < 8) win_h = 8;

    Window win(renderer_.get(), screen_w_, screen_h_, win_w, win_h, "Load Game");
    win.set_footer("[Enter] Load  [Esc] Back");
    win.draw();
    DrawContext ctx = win.content();

    if (save_slots_.empty()) {
        ctx.text_center(ctx.height() / 2, "No saved games found.", Color::DarkGray);
        return;
    }

    for (int i = 0; i < static_cast<int>(save_slots_.size()); ++i) {
        if (i >= ctx.height()) break;
        const auto& slot = save_slots_[i];
        bool selected = (i == load_selection_);

        std::string line;
        if (selected) line += "> "; else line += "  ";
        line += slot.location;
        line += "  LVL:" + std::to_string(slot.player_level);
        line += "  T:" + std::to_string(slot.world_tick);

        Color fg = selected ? Color::Yellow : Color::Default;
        ctx.text(0, i, line, fg);
    }
}

void Game::render_hall_of_fame() {
    int win_w = std::min(screen_w_ - 4, 70);
    int win_h = std::min(screen_h_ - 4, static_cast<int>(save_slots_.size()) + 6);
    if (win_h < 8) win_h = 8;

    Window win(renderer_.get(), screen_w_, screen_h_, win_w, win_h, "Hall of Fame");
    if (confirm_delete_) {
        win.set_footer("Delete this entry? [Y] Yes  [N] No", Color::Red);
    } else {
        win.set_footer("[D] Delete  [Esc] Back");
    }
    win.draw();
    DrawContext ctx = win.content();

    if (save_slots_.empty()) {
        ctx.text_center(ctx.height() / 2, "No fallen heroes yet.", Color::DarkGray);
        return;
    }

    for (int i = 0; i < static_cast<int>(save_slots_.size()); ++i) {
        if (i >= ctx.height()) break;
        const auto& slot = save_slots_[i];
        bool selected = (i == load_selection_);

        std::string line;
        if (selected) line += "> "; else line += "  ";

        if (!slot.death_message.empty()) {
            line += slot.death_message;
        } else {
            line += "Unknown cause";
        }
        line += "  LVL:" + std::to_string(slot.player_level);
        line += "  T:" + std::to_string(slot.world_tick);
        line += "  XP:" + std::to_string(slot.xp);
        line += "  $" + std::to_string(slot.money);
        line += "  K:" + std::to_string(slot.kills);

        Color fg = selected ? Color::Yellow : Color::DarkGray;
        ctx.text(0, i, line, fg);
    }
}


// ---------------------------------------------------------------------------
// Welcome screen — semantic UI
// ---------------------------------------------------------------------------

void Game::render_welcome_screen() {
    int margin = 4;
    int win_w = 60;
    if (win_w > screen_w_ - margin * 2) win_w = screen_w_ - margin * 2;

    // Content: greeting(1) + blank(1) + lore(3) + blank(1) + location(2) + blank(2) +
    //          controls heading(1) + blank(1) + 7 key rows = 19
    int content_h = 19;
    int chrome_h = 2 + 2 + 1; // border(2) + title+sep(2) + footer(1)
    int win_h = content_h + chrome_h;
    if (win_h > screen_h_ - margin * 2) win_h = screen_h_ - margin * 2;

    int wx = (screen_w_ - win_w) / 2;
    int wy = (screen_h_ - win_h) / 2;

    UIContext full(renderer_.get(), Rect{wx, wy, win_w, win_h});
    auto ctx = full.panel({.title = "A S T R A", .footer = "[Space] Continue"});
    int cw = ctx.width();
    int y = 0;

    // Greeting
    std::string greeting = "Welcome, " + player_.name + ".";
    int gx = (cw - static_cast<int>(greeting.size())) / 2;
    if (gx < 1) gx = 1;
    ctx.text({.x = gx, .y = y, .content = greeting, .tag = UITag::TextBright});
    y += 2;

    // Lore text
    auto center_dim = [&](const std::string& s) {
        int tx = (cw - static_cast<int>(s.size())) / 2;
        if (tx < 1) tx = 1;
        ctx.text({.x = tx, .y = y, .content = s, .tag = UITag::TextDim});
        y++;
    };
    center_dim("Your journey to the center of the galaxy begins.");
    center_dim("The supermassive black hole Sagittarius A* awaits.");
    center_dim("But first, you must survive.");
    y++;

    // Location
    auto center_accent = [&](const std::string& s) {
        int tx = (cw - static_cast<int>(s.size())) / 2;
        if (tx < 1) tx = 1;
        ctx.text({.x = tx, .y = y, .content = s, .tag = UITag::TextAccent});
        y++;
    };
    center_accent("You are docked at The Heavens Above,");
    center_accent("a space station orbiting Jupiter.");
    y += 2;

    // Controls heading
    int kx = 5;
    ctx.text({.x = kx, .y = y, .content = "CONTROLS", .tag = UITag::TextBright});
    y += 2;

    // Key bindings
    auto key_row = [&](const std::string& key, const std::string& action) {
        ctx.styled_text({.x = kx, .y = y, .segments = {
            {key, UITag::KeyLabel},
        }});
        ctx.text({.x = kx + 22, .y = y, .content = action, .tag = UITag::TextDim});
        y++;
    };
    key_row("Arrow keys", "Move");
    key_row("Space",      "Use / interact");
    key_row("l",          "Look / examine");
    key_row("c",          "Character screen");
    key_row("t / s",      "Target / shoot");
    key_row("> / <",      "Enter / exit");
    key_row("ESC",        "Pause menu");
}

// ---------------------------------------------------------------------------
// Pause menu — semantic UI
// ---------------------------------------------------------------------------

void Game::render_pause_menu() {
    if (!pause_menu_.is_open()) return;

    // Build option list from the PopupMenu state
    // We need to reconstruct options — read them by trying each known key
    struct PauseOpt { char key; std::string label; };
    std::vector<PauseOpt> opts;

    // The pause menu options are set up in handle_play_input with fixed keys
    // We'll re-read from the PopupMenu by matching the selection index
    // Since PopupMenu doesn't expose options, render using the known set
    opts.push_back({'r', "Return to game"});
    opts.push_back({'o', "Options"});
    opts.push_back({'h', "Help"});
    opts.push_back({'s', "Save game"});
    opts.push_back({'l', "Load game"});
    if (!dev_mode_) {
        opts.push_back({'q', "Save and quit"});
    }
    opts.push_back({'x', "Quit without saving"});

    int margin = 4;
    int content_w = 0;
    for (const auto& o : opts) {
        int w = 6 + static_cast<int>(o.label.size()); // "[X] " + label
        if (w > content_w) content_w = w;
    }
    int win_w = content_w + 6; // padding
    if (win_w < 30) win_w = 30;

    int content_h = 1; // blank before options
    content_h += static_cast<int>(opts.size()) * 2 - 1; // options with spacing
    content_h += 1; // padding after
    int chrome_h = 2 + 2 + 1; // border + title+sep + footer
    int win_h = content_h + chrome_h;

    int wx = (screen_w_ - win_w) / 2;
    int wy = (screen_h_ - win_h) / 2;

    UIContext full(renderer_.get(), Rect{wx, wy, win_w, win_h});
    auto ctx = full.panel({.title = "Game Menu", .footer = "[Esc] Close"});
    int cw = ctx.width();
    int y = 1;

    // Build list items
    std::vector<ListItem> items;
    int sel = pause_menu_.selected();
    for (int i = 0; i < static_cast<int>(opts.size()); ++i) {
        std::string label = "[" + std::string(1, opts[i].key) + "] " + opts[i].label;
        items.push_back({label, UITag::OptionNormal, i == sel});
    }

    int list_h = ctx.height() - y;
    if (list_h > 0) {
        int scroll = 0;
        if (sel >= list_h) scroll = sel - list_h + 1;
        auto list_area = ctx.sub(Rect{0, y, cw, list_h});
        list_area.list({.items = items, .scroll_offset = scroll,
                        .tag = UITag::ConversationOption, .selected_tag = UITag::OptionSelected});
    }
}

// ---------------------------------------------------------------------------
// Quit confirm — semantic UI
// ---------------------------------------------------------------------------

void Game::render_quit_confirm() {
    if (!quit_confirm_.is_open()) return;

    int win_w = 36;
    int content_h = 5; // blank + 2 options with spacing + blank
    int chrome_h = 2 + 2 + 1; // border + title+sep + footer
    int win_h = content_h + chrome_h;

    int wx = (screen_w_ - win_w) / 2;
    int wy = (screen_h_ - win_h) / 2;

    UIContext full(renderer_.get(), Rect{wx, wy, win_w, win_h});
    auto ctx = full.panel({.title = "Quit without saving?", .footer = "[Esc] Cancel"});
    int cw = ctx.width();
    int y = 1;

    int sel = quit_confirm_.selected();
    std::vector<ListItem> items;
    items.push_back({"[Y] Yes, quit", UITag::OptionNormal, sel == 0});
    items.push_back({"[N] No, keep playing", UITag::OptionNormal, sel == 1});

    int list_h = ctx.height() - y;
    if (list_h > 0) {
        auto list_area = ctx.sub(Rect{0, y, cw, list_h});
        list_area.list({.items = items, .tag = UITag::ConversationOption, .selected_tag = UITag::OptionSelected});
    }
}

} // namespace astra
