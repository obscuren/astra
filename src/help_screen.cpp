#include "astra/help_screen.h"

namespace astra {

void HelpScreen::open() {
    open_ = true;
    tab_ = 0;
    scroll_ = 0;
}

bool HelpScreen::handle_input(int key) {
    if (!open_) return false;

    switch (key) {
        case 27: case '?':
            open_ = false;
            return true;
        case 'q':
            tab_ = (tab_ - 1 + tab_count_) % tab_count_;
            scroll_ = 0;
            return true;
        case 'e':
            tab_ = (tab_ + 1) % tab_count_;
            scroll_ = 0;
            return true;
        case KEY_UP: case 'k':
            if (scroll_ > 0) --scroll_;
            return true;
        case KEY_DOWN: case 'j':
            ++scroll_;
            return true;
    }
    return true; // consume all input while open
}

void HelpScreen::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_) return;

    struct HelpLine { const char* key; const char* desc; };

    static const HelpLine tab_controls[] = {
        {"", "GENERAL"},
        {"Space", "Use / interact with nearby object"},
        {"l", "Look / examine (free cursor)"},
        {"c", "Character screen"},
        {"?", "This help screen"},
        {"Esc", "Pause menu"},
        {"", ""},
        {"", "MOVEMENT"},
        {"Arrow keys", "Move in four directions"},
        {"> / <", "Enter / exit stairs or dungeon"},
        {".", "Wait one turn"},
        {"", ""},
        {"", "ITEMS"},
        {"g", "Pick up item from ground"},
        {"i", "Inspect item / switch to inventory"},
        {"d", "Drop selected item"},
        {"Tab", "Cycle side panel tabs"},
        {"Shift+Tab", "Cycle tabs (reverse)"},
        {"Ctrl+H", "Toggle side panel"},
        {"", ""},
        {"", "PANEL TABS"},
        {"+/=", "Move cursor down in panel"},
        {"-", "Move cursor up in panel"},
        {"Enter", "Confirm action in panel"},
        {nullptr, nullptr},
    };

    static const HelpLine tab_movement[] = {
        {"", "OVERWORLD"},
        {"Arrow keys", "Move across the overworld map"},
        {">", "Enter detail map at current tile"},
        {"Space", "Enter detail map at landing pad"},
        {"l", "Look at terrain features"},
        {"", ""},
        {"", "DETAIL MAPS"},
        {"Arrow keys", "Walk through settlements, ruins, outposts"},
        {">", "Enter dungeon at portal tiles"},
        {"<", "Return to overworld"},
        {"", "Edge walk auto-transitions to adjacent tiles"},
        {"", ""},
        {"", "DUNGEONS"},
        {"Arrow keys", "Explore rooms and corridors"},
        {"<", "Exit dungeon to detail map"},
        {"", "Step on portal tiles to auto-exit"},
        {"", ""},
        {"", "STARSHIP"},
        {"Space", "Board ship at ship terminal"},
        {"<", "Disembark from ship"},
        {nullptr, nullptr},
    };

    static const HelpLine tab_combat[] = {
        {"", "MELEE"},
        {"Arrow keys", "Walk into hostile NPC to attack"},
        {"", "Damage = Attack - Target Defense"},
        {"", ""},
        {"", "RANGED"},
        {"t", "Enter targeting mode"},
        {"s", "Shoot at current target"},
        {"r", "Reload weapon (consumes battery)"},
        {"", ""},
        {"", "TARGETING MODE"},
        {"Arrow keys", "Move targeting cursor"},
        {"Enter", "Confirm target"},
        {"Esc", "Cancel targeting"},
        {"", "Green line = in range, Red = out of range"},
        {"", ""},
        {"", "FRIENDLY NPCS"},
        {"Arrow keys", "Walk into friendly NPC to swap positions"},
        {"", "NPC returns to their spot next turn"},
        {nullptr, nullptr},
    };

    static const HelpLine tab_systems[] = {
        {"", "NPC INTERACTION"},
        {"Space", "Interact with adjacent NPC"},
        {"Tab", "Quick trade (in NPC dialog)"},
        {"l", "Look at NPC (in dialog)"},
        {"", ""},
        {"", "CHARACTER SCREEN (c)"},
        {"q / e", "Previous / next tab"},
        {"", "Tabs: Skills, Attributes, Equipment,"},
        {"", "Tinkering, Journal, Quests, Reputation, Ship"},
        {"", ""},
        {"", "TINKERING"},
        {"r", "Repair item on workbench"},
        {"a", "Analyze item (learn blueprint)"},
        {"s", "Salvage item (disassemble)"},
        {"f", "Assemble (commit enhancements)"},
        {"y", "Synthesize (combine blueprints)"},
        {"", ""},
        {"", "TRADING"},
        {"Space", "Buy or sell selected item"},
        {"Tab", "Switch between merchant and inventory"},
        {"", ""},
        {"", "EFFECTS"},
        {"", "Active effects shown in bottom bar"},
        {"", "Effects tick down each turn"},
        {"", "Damage effects modify incoming damage"},
        {nullptr, nullptr},
    };

    static const HelpLine* tabs[] = { tab_controls, tab_movement, tab_combat, tab_systems };
    static const char* tab_names[] = { "Controls", "Movement", "Combat", "Systems" };

    int margin = 2;
    Rect bounds{margin, margin, screen_w - margin * 2, screen_h - margin * 2};
    Panel help(renderer, bounds, "Help");
    help.set_footer("[ESC] Close  [q/e] Prev/Next Tab  [Up/Down] Scroll");
    help.draw();
    DrawContext ctx = help.content();

    // Tab bar
    int tx = 1;
    for (int i = 0; i < tab_count_; ++i) {
        Color c = (i == tab_) ? Color::Yellow : Color::DarkGray;
        std::string label = (i == tab_)
            ? std::string("[") + tab_names[i] + "]"
            : std::string(" ") + tab_names[i] + " ";
        ctx.text(tx, 0, label, c);
        tx += static_cast<int>(label.size()) + 1;
    }

    // Separator
    for (int x = 0; x < ctx.width(); ++x)
        ctx.put(x, 1, BoxDraw::H, Color::DarkGray);

    // Content
    const HelpLine* lines = tabs[tab_];
    int content_y = 3;
    int visible_h = ctx.height() - content_y;

    int total_lines = 0;
    for (const HelpLine* l = lines; l->key != nullptr || l->desc != nullptr; ++l)
        ++total_lines;

    int max_scroll = total_lines - visible_h;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_ > max_scroll) scroll_ = max_scroll;

    int row = 0;
    int drawn = 0;
    for (const HelpLine* l = lines; l->key != nullptr || l->desc != nullptr; ++l) {
        if (row < scroll_) { ++row; continue; }
        if (drawn >= visible_h) break;

        int y = content_y + drawn;
        if (l->key == nullptr) {
            ctx.text(2, y, l->desc, Color::DarkGray);
        } else if (l->key[0] == '\0' && l->desc[0] != '\0') {
            ctx.text(1, y, l->desc, Color::White);
        } else if (l->key[0] == '\0') {
            // Blank line
        } else {
            ctx.text(3, y, l->key, Color::Yellow);
            ctx.text(19, y, l->desc, Color::DarkGray);
        }
        ++row;
        ++drawn;
    }
}

} // namespace astra
