#include "astra/character_creation.h"

#include <cstring>

namespace astra {

// ── Race templates ──────────────────────────────────────────────────

static const std::vector<RaceTemplate> s_race_templates = {
    {Race::Human, "Human", '@',
     "Adaptable and resourceful — the galaxy's generalists.",
     {0, 0, 0, 0, 0, 2}, {0, 0, 0, 0},
     {"Bonus to Luck (+2)", "No attribute penalties", "Balanced and versatile", nullptr}},

    {Race::Veldrani, "Veldrani", 'V',
     "Tall, blue-skinned diplomats. Intelligent and perceptive.",
     {-1, 1, -1, 2, 1, 0}, {0, 0, 3, 0},
     {"Intelligent (+2 INT) and willful (+1 WIL)", "Natural cold resistance (+3)",
      "Slightly frail (-1 STR, -1 TOU)", nullptr}},

    {Race::Kreth, "Kreth", 'K',
     "Stocky, mineral-skinned engineers. Immensely strong.",
     {3, -2, 2, 0, 0, -1}, {5, 0, 0, 3},
     {"Extremely strong (+3 STR) and tough (+2 TOU)", "Acid resistance (+5), heat resistance (+3)",
      "Slow (-2 AGI) and unlucky (-1 LUC)", nullptr}},

    {Race::Sylphari, "Sylphari", 'S',
     "Wispy, luminescent wanderers. Graceful and willful.",
     {-2, 3, -2, 1, 2, 0}, {0, 5, 0, 0},
     {"Very agile (+3 AGI) and strong-willed (+2 WIL)", "Electrical resistance (+5)",
      "Physically fragile (-2 STR, -2 TOU)", nullptr}},

    {Race::Xytomorph, "Xytomorph", 'X',
     "Chitinous predators. Physically superior, intellectually stunted.",
     {2, 2, 1, -2, -2, 1}, {3, 0, 0, 0},
     {"Strong (+2 STR), agile (+2 AGI), lucky (+1 LUC)", "Acid resistance (+3)",
      "Dim (-2 INT) and weak-willed (-2 WIL)", nullptr}},

    {Race::Stellari, "Stellari", '*',
     "Luminous stellar engineers. Ancient minds, frail bodies.",
     {-1, -1, 0, 3, 2, -1}, {0, 3, 0, 5},
     {"Brilliant (+3 INT) and wise (+2 WIL)", "Heat resistance (+5), electrical resistance (+3)",
      "Physically weak (-1 STR, -1 AGI, -1 LUC)", nullptr}},
};

const RaceTemplate& race_template(Race r) {
    for (const auto& rt : s_race_templates) {
        if (rt.race == r) return rt;
    }
    return s_race_templates[0];
}

const std::vector<RaceTemplate>& all_race_templates() {
    return s_race_templates;
}

const std::vector<RaceTemplate>& playable_race_templates() {
    static const std::vector<RaceTemplate> playable = [] {
        std::vector<RaceTemplate> v;
        for (const auto& rt : s_race_templates) {
            if (rt.race != Race::Xytomorph) v.push_back(rt);
        }
        return v;
    }();
    return playable;
}

// ── Attribute helpers ───────────────────────────────────────────────

static const char* attr_names[6] = {"STR", "AGI", "TOU", "INT", "WIL", "LUC"};
static const char* attr_full_names[6] = {
    "Strength", "Agility", "Toughness", "Intelligence", "Willpower", "Luck"
};

// ── CharacterCreation ───────────────────────────────────────────────

bool CharacterCreation::is_open() const { return open_; }
bool CharacterCreation::is_complete() const { return result_.complete; }

void CharacterCreation::open(Renderer* renderer) {
    renderer_ = renderer;
    open_ = true;
    step_ = CreationStep::CharacterType;
    type_cursor_ = 0;
    result_ = CreationResult{};
    race_cursor_ = 0;
    class_cursor_ = 0;
    attr_cursor_ = 0;
    attr_points_remaining_ = 10;
    for (int i = 0; i < 6; ++i) attr_alloc_[i] = 0;
    name_buffer_.clear();
}

void CharacterCreation::close() {
    open_ = false;
    renderer_ = nullptr;
}

CreationResult CharacterCreation::consume_result() {
    CreationResult r = std::move(result_);
    close();
    return r;
}

// ── Input ───────────────────────────────────────────────────────────

bool CharacterCreation::handle_input(int key) {
    if (!open_) return false;

    const auto& races = playable_race_templates();
    const auto& classes = gameplay_classes();

    switch (step_) {
    case CreationStep::CharacterType:
        switch (key) {
        case KEY_LEFT: case 'a': case 'h':
            type_cursor_ = (type_cursor_ - 1 + 3) % 3;
            return true;
        case KEY_RIGHT: case 'd': case 'l':
            type_cursor_ = (type_cursor_ + 1) % 3;
            return true;
        case '\n': case '\r': case ' ': case '9':
            if (type_cursor_ == 0) { // Presets
                advance_step();
            } else if (type_cursor_ == 2) { // Random
                // Randomize everything and jump to Summary
                race_cursor_ = rng_() % races.size();
                result_.race = races[race_cursor_].race;
                class_cursor_ = rng_() % classes.size();
                result_.player_class = classes[class_cursor_];
                for (int i = 0; i < 6; ++i) attr_alloc_[i] = 0;
                int pts = 10;
                while (pts > 0) {
                    int idx = rng_() % 6;
                    if (attr_alloc_[idx] < 8) { attr_alloc_[idx]++; pts--; }
                }
                attr_points_remaining_ = 0;
                result_.attributes = compute_final_attributes();
                result_.resistances = compute_final_resistances();
                name_buffer_ = generate_random_name();
                result_.name = name_buffer_;
                step_ = CreationStep::Summary;
            }
            // type_cursor_ == 1 (New) — not implemented yet, ignore
            return true;
        case 27:
            close();
            return true;
        }
        break;

    case CreationStep::Race:
        switch (key) {
        case KEY_LEFT: case 'a': case 'h':
            race_cursor_ = (race_cursor_ - 1 + (int)races.size()) % (int)races.size();
            return true;
        case KEY_RIGHT: case 'd': case 'l':
            race_cursor_ = (race_cursor_ + 1) % (int)races.size();
            return true;
        case '\n': case '\r': case ' ': case '9':
            result_.race = races[race_cursor_].race;
            advance_step();
            return true;
        case 27:
            retreat_step();
            return true;
        case 'r': case 'R':
            race_cursor_ = rng_() % races.size();
            return true;
        }
        break;

    case CreationStep::Class:
        switch (key) {
        case KEY_LEFT: case 'a': case 'h':
            class_cursor_ = (class_cursor_ - 1 + (int)classes.size()) % (int)classes.size();
            return true;
        case KEY_RIGHT: case 'd': case 'l':
            class_cursor_ = (class_cursor_ + 1) % (int)classes.size();
            return true;
        case '\n': case '\r': case ' ': case '9':
            result_.player_class = classes[class_cursor_];
            advance_step();
            return true;
        case 27:
            retreat_step();
            return true;
        case 'r': case 'R':
            class_cursor_ = rng_() % classes.size();
            return true;
        }
        break;

    case CreationStep::Attributes:
        switch (key) {
        case KEY_LEFT: case 'a': case 'h':
            attr_cursor_ = (attr_cursor_ - 1 + 6) % 6;
            return true;
        case KEY_RIGHT: case 'd': case 'l':
            attr_cursor_ = (attr_cursor_ + 1) % 6;
            return true;
        case KEY_UP: case 'w': case 'k':
            if (attr_points_remaining_ > 0 && attr_alloc_[attr_cursor_] < 8) {
                attr_alloc_[attr_cursor_]++;
                attr_points_remaining_--;
            }
            return true;
        case KEY_DOWN: case 's': case 'j':
            if (attr_alloc_[attr_cursor_] > 0) {
                attr_alloc_[attr_cursor_]--;
                attr_points_remaining_++;
            }
            return true;
        case '\n': case '\r': case ' ': case '9':
            result_.attributes = compute_final_attributes();
            result_.resistances = compute_final_resistances();
            advance_step();
            return true;
        case 27:
            retreat_step();
            return true;
        case 'r': case 'R': {
            // Random allocation
            for (int i = 0; i < 6; ++i) attr_alloc_[i] = 0;
            int pts = 10;
            while (pts > 0) {
                int idx = rng_() % 6;
                if (attr_alloc_[idx] < 8) {
                    attr_alloc_[idx]++;
                    pts--;
                }
            }
            attr_points_remaining_ = 0;
            return true;
        }
        }
        break;

    case CreationStep::Name:
        switch (key) {
        case '\n': case '\r':
            if (name_buffer_.empty()) {
                name_buffer_ = generate_random_name();
            }
            result_.name = name_buffer_;
            advance_step();
            return true;
        case 27:
            retreat_step();
            return true;
        case 127: case 8: // Backspace / Delete
            if (!name_buffer_.empty()) {
                name_buffer_.pop_back();
            }
            return true;
        case 'r': case 'R':
            if (name_buffer_.empty()) {
                name_buffer_ = generate_random_name();
                return true;
            }
            // If buffer non-empty, fall through to add the character
            [[fallthrough]];
        default:
            if (key >= 32 && key < 127 && (int)name_buffer_.size() < max_name_len_) {
                name_buffer_ += static_cast<char>(key);
            }
            return true;
        }
        break;

    case CreationStep::Location:
        switch (key) {
        case '\n': case '\r': case ' ': case '9':
            advance_step();
            return true;
        case 27:
            retreat_step();
            return true;
        // Left/Right for future locations
        case KEY_LEFT: case 'a': case 'h':
            location_cursor_ = (location_cursor_ - 1 + 1) % 1; // only 1 option for now
            return true;
        case KEY_RIGHT: case 'd': case 'l':
            location_cursor_ = (location_cursor_ + 1) % 1;
            return true;
        }
        break;

    case CreationStep::Summary:
        switch (key) {
        case '\n': case '\r': case ' ':
            result_.complete = true;
            return true;
        case 27:
            retreat_step();
            return true;
        case 'r': case 'R':
            // Randomize everything
            race_cursor_ = rng_() % races.size();
            result_.race = races[race_cursor_].race;
            class_cursor_ = rng_() % classes.size();
            result_.player_class = classes[class_cursor_];
            for (int i = 0; i < 6; ++i) attr_alloc_[i] = 0;
            int pts = 10;
            while (pts > 0) {
                int idx = rng_() % 6;
                if (attr_alloc_[idx] < 8) { attr_alloc_[idx]++; pts--; }
            }
            attr_points_remaining_ = 0;
            result_.attributes = compute_final_attributes();
            result_.resistances = compute_final_resistances();
            name_buffer_ = generate_random_name();
            result_.name = name_buffer_;
            return true;
        }
        break;
    }
    return true;
}

void CharacterCreation::advance_step() {
    int s = static_cast<int>(step_);
    if (s < creation_step_count - 1) {
        step_ = static_cast<CreationStep>(s + 1);
    }
}

void CharacterCreation::retreat_step() {
    int s = static_cast<int>(step_);
    if (s > 0) {
        step_ = static_cast<CreationStep>(s - 1);
    } else {
        close();
    }
}

// ── Rendering ───────────────────────────────────────────────────────

static int content_height_for_step(CreationStep step) {
    // breadcrumbs(1) + gap(1) + title(1) + subtitle(1) + gap(1) + body + gap(1) + footer(1)
    // = 7 + body height
    switch (step) {
    case CreationStep::CharacterType: return 7 + 13;
    case CreationStep::Race:       return 7 + 17;
    case CreationStep::Class:      return 7 + 17;
    case CreationStep::Attributes: return 7 + 17;
    case CreationStep::Name:       return 7 + 10;
    case CreationStep::Location:   return 7 + 13;
    case CreationStep::Summary:    return 7 + 17;
    }
    return 24;
}

void CharacterCreation::draw(int screen_w, int screen_h) {
    if (!renderer_) return;

    // Compute centered content rect
    int content_h = content_height_for_step(step_);
    int oy = std::max(0, (screen_h - content_h) / 2);

    Rect content_rect{0, oy, screen_w, content_h};
    DrawContext ctx(renderer_, content_rect);

    // Header: breadcrumbs + title
    draw_breadcrumbs(ctx);

    const char* subtitles[] = {
        ":choose character type:", ":choose your race:", ":choose your class:",
        ":allocate attributes:", ":name your character:", ":choose starting location:",
        ":build summary:"
    };
    draw_title(ctx, subtitles[static_cast<int>(step_)]);

    // Step content
    switch (step_) {
    case CreationStep::CharacterType: draw_type_step(ctx);     break;
    case CreationStep::Race:       draw_race_step(ctx);       break;
    case CreationStep::Class:      draw_class_step(ctx);      break;
    case CreationStep::Attributes: draw_attributes_step(ctx); break;
    case CreationStep::Name:       draw_name_step(ctx);       break;
    case CreationStep::Location:   draw_location_step(ctx);   break;
    case CreationStep::Summary:    draw_summary_step(ctx);    break;
    }

    // Footer at bottom of content block
    switch (step_) {
    case CreationStep::CharacterType:
        draw_footer(ctx, "[Enter] Select");
        break;
    case CreationStep::Summary:
        draw_footer(ctx, "[Enter] Begin Journey");
        break;
    case CreationStep::Name:
        draw_footer(ctx, "[Enter] Confirm");
        break;
    default:
        draw_footer(ctx, "[Enter] Next");
        break;
    }
}

void CharacterCreation::draw_breadcrumbs(DrawContext& ctx) {
    static const char* step_names[] = {"Type", "Race", "Class", "Attributes", "Name", "Location", "Summary"};
    int current = static_cast<int>(step_);
    int w = ctx.width();

    int total_w = 0;
    for (int i = 0; i < creation_step_count; ++i) {
        total_w += static_cast<int>(std::strlen(step_names[i]));
        if (i < creation_step_count - 1) total_w += 3;
    }
    int x = (w - total_w) / 2;

    for (int i = 0; i < creation_step_count; ++i) {
        UITag tag = (i < current)  ? UITag::TextSuccess
                  : (i == current) ? UITag::TextWarning
                                   : UITag::TextDim;
        ctx.text({.x = x, .y = 0, .content = std::string(step_names[i]), .tag = tag});
        x += static_cast<int>(std::strlen(step_names[i]));
        if (i < creation_step_count - 1) {
            ctx.text({.x = x, .y = 0, .content = " > ", .tag = UITag::TextDim});
            x += 3;
        }
    }
}

void CharacterCreation::draw_title(DrawContext& ctx, const char* subtitle) {
    int w = ctx.width();
    std::string title = "CHARACTER CREATION";
    ctx.text({.x = (w - (int)title.size()) / 2, .y = 2, .content = title, .tag = UITag::TextAccent});
    std::string sub(subtitle);
    ctx.text({.x = (w - (int)sub.size()) / 2, .y = 3, .content = sub, .tag = UITag::TextDim});
}

void CharacterCreation::draw_footer(DrawContext& ctx, const char* extra) {
    int y = ctx.height() - 1;
    std::vector<TextSegment> segs;
    auto append_key_action = [&](const std::string& key, const std::string& action) {
        auto ka = key_action_segments(key, action);
        segs.insert(segs.end(), ka.begin(), ka.end());
    };
    append_key_action("Esc", "Back");
    segs.push_back({"    ", UITag::TextDim});
    append_key_action("R", "Randomize");
    if (extra) {
        // Parse extra string for [Key] Action pairs
        std::string ex(extra);
        size_t pos = 0;
        while (pos < ex.size()) {
            size_t bracket = ex.find('[', pos);
            if (bracket == std::string::npos) break;
            size_t close = ex.find(']', bracket + 1);
            if (close == std::string::npos) break;
            segs.push_back({"    ", UITag::TextDim});
            std::string key = ex.substr(bracket + 1, close - bracket - 1);
            // Find action text after "] "
            size_t action_start = close + 1;
            if (action_start < ex.size() && ex[action_start] == ' ') action_start++;
            size_t action_end = ex.find('[', action_start);
            if (action_end == std::string::npos) action_end = ex.size();
            // Trim trailing spaces from action
            while (action_end > action_start && ex[action_end - 1] == ' ') action_end--;
            std::string action = ex.substr(action_start, action_end - action_start);
            append_key_action(key, action);
            pos = (ex.find('[', close + 1) != std::string::npos) ? ex.find('[', close + 1) : ex.size();
        }
    }
    int w = ctx.width();
    ctx.styled_text({.x = w / 2, .y = y, .segments = segs, .align = TextAlign::Center});
}

void CharacterCreation::draw_card(DrawContext& ctx, int x, int y, int w, int h,
                                   char glyph, const char* name, bool selected) {
    using namespace BoxDraw;
    Color bc = selected ? Color::Yellow : Color::DarkGray;
    Color accent_c = selected ? Color::Yellow : Color::DarkGray;
    Color name_c = selected ? Color::White : Color::DarkGray;

    auto draw_ornament_row = [&](int ry, const char* left_corner, const char* right_corner) {
        ctx.put(x, ry, left_corner, bc);
        ctx.put(x + 1, ry, H, bc);
        ctx.put(x + 2, ry, DR, accent_c);
        ctx.put(x + 3, ry, DH, accent_c);
        ctx.put(x + 4, ry, DV, accent_c);
        for (int i = 5; i < w - 5; ++i) ctx.put(x + i, ry, ' ');
        ctx.put(x + w - 5, ry, DV, accent_c);
        ctx.put(x + w - 4, ry, DH, accent_c);
        ctx.put(x + w - 3, ry, DL, accent_c);
        ctx.put(x + w - 2, ry, H, bc);
        ctx.put(x + w - 1, ry, right_corner, bc);
    };

    // Top ornament border
    draw_ornament_row(y, TL, TR);

    // Middle rows
    for (int row = 1; row < h - 1; ++row) {
        ctx.put(x, y + row, V, bc);
        for (int i = 1; i < w - 1; ++i) ctx.put(x + i, y + row, ' ');
        ctx.put(x + w - 1, y + row, V, bc);
    }

    // Bottom ornament border
    draw_ornament_row(y + h - 1, BL, BR);

    // Glyph and name with equal spacing:
    // row 0: top border, row 1: blank, row 2: glyph, row 3: blank, row 4: name, row 5: blank, row 6: bottom border
    int glyph_x = x + w / 2;
    int glyph_y = y + 2;
    ctx.put(glyph_x, glyph_y, glyph, selected ? Color::White : Color::DarkGray);

    int name_len = static_cast<int>(std::strlen(name));
    int name_x = x + (w - name_len) / 2;
    UITag name_tag = selected ? UITag::TextBright : UITag::TextDim;
    ctx.text({.x = name_x, .y = y + 4, .content = std::string(name), .tag = name_tag});
}

// ── Step: Race ──────────────────────────────────────────────────────

void CharacterCreation::draw_type_step(DrawContext& ctx) {
    int card_w = 16;
    int card_h = 7;
    int gap = 3;
    int total_w = 3 * card_w + 2 * gap;
    int start_x = (ctx.width() - total_w) / 2;
    int card_y = 5;

    // Presets card
    draw_card(ctx, start_x, card_y, card_w, card_h,
              'P', "Presets", type_cursor_ == 0);

    // New card (always grayed out — not implemented)
    int new_x = start_x + card_w + gap;
    draw_card(ctx, new_x, card_y, card_w, card_h,
              '+', "New", false);

    // Random card
    int rand_x = new_x + card_w + gap;
    draw_card(ctx, rand_x, card_y, card_w, card_h,
              '?', "Random", type_cursor_ == 2);

    // Description
    int desc_y = card_y + card_h + 2;
    int cw = ctx.width();
    auto center_text = [&](int cy, const std::string& s, UITag tag) {
        ctx.text({.x = (cw - (int)s.size()) / 2, .y = cy, .content = s, .tag = tag});
    };
    if (type_cursor_ == 0) {
        center_text(desc_y, "Pick from several preset characters.", UITag::TextBright);
        center_text(desc_y + 1, "Race, class, and attributes are pre-configured.", UITag::TextDim);
    } else if (type_cursor_ == 1) {
        center_text(desc_y, "Build a character from scratch.", UITag::TextDim);
        center_text(desc_y + 1, "(Coming soon)", UITag::TextDim);
    } else {
        center_text(desc_y, "Generate a completely random character.", UITag::TextBright);
        center_text(desc_y + 1, "Race, class, attributes, and name — all randomized.", UITag::TextDim);
    }
}

void CharacterCreation::draw_race_step(DrawContext& ctx) {
    const auto& races = playable_race_templates();
    int count = static_cast<int>(races.size());
    int card_w = 14;
    int card_h = 7;
    int gap = 2;
    int total_w = count * card_w + (count - 1) * gap;
    int start_x = (ctx.width() - total_w) / 2;
    int card_y = 5;

    for (int i = 0; i < count; ++i) {
        int x = start_x + i * (card_w + gap);
        draw_card(ctx, x, card_y, card_w, card_h,
                  races[i].glyph, races[i].name, i == race_cursor_);
    }

    // Description area below cards
    const auto& sel = races[race_cursor_];
    int desc_y = card_y + card_h + 2;
    int rw = ctx.width();

    std::string tagline(sel.tagline);
    ctx.text({.x = (rw - (int)tagline.size()) / 2, .y = desc_y, .content = tagline, .tag = UITag::TextBright});
    desc_y += 2;

    for (int i = 0; i < 4 && sel.bullets[i]; ++i) {
        std::string line = std::string("  * ") + sel.bullets[i];
        ctx.text({.x = (rw - (int)line.size()) / 2, .y = desc_y + i, .content = line, .tag = UITag::TextDim});
    }

    // Stat modifiers
    desc_y += 5;
    std::string mods;
    for (int i = 0; i < 6; ++i) {
        if (!mods.empty()) mods += "  ";
        mods += attr_names[i];
        mods += (sel.attr_mods[i] >= 0) ? " +" : " ";
        mods += std::to_string(sel.attr_mods[i]);
    }
    ctx.text({.x = (rw - (int)mods.size()) / 2, .y = desc_y, .content = mods, .tag = UITag::TextAccent});
}

// ── Step: Class ─────────────────────────────────────────────────────

void CharacterCreation::draw_class_step(DrawContext& ctx) {
    const auto& classes = gameplay_classes();
    int count = static_cast<int>(classes.size());
    int card_w = 16;
    int card_h = 7;
    int gap = 2;
    int total_w = count * card_w + (count - 1) * gap;
    int start_x = (ctx.width() - total_w) / 2;
    int card_y = 5;

    // Glyph per class
    static const char class_glyphs[] = {'V', 'G', 'T', 'O', 'M'};

    for (int i = 0; i < count; ++i) {
        int x = start_x + i * (card_w + gap);
        draw_card(ctx, x, card_y, card_w, card_h,
                  class_glyphs[i], class_name(classes[i]), i == class_cursor_);
    }

    // Description
    const auto& tmpl = class_template(classes[class_cursor_]);
    int desc_y = card_y + card_h + 2;
    int clw = ctx.width();

    std::string desc(tmpl.description);
    ctx.text({.x = (clw - (int)desc.size()) / 2, .y = desc_y, .content = desc, .tag = UITag::TextBright});
    desc_y += 3;

    // Base attributes
    std::string stats;
    const auto& a = tmpl.attributes;
    int vals[6] = {a.strength, a.agility, a.toughness, a.intelligence, a.willpower, a.luck};
    for (int i = 0; i < 6; ++i) {
        if (!stats.empty()) stats += "  ";
        stats += attr_names[i];
        stats += " ";
        stats += std::to_string(vals[i]);
    }
    ctx.text({.x = (clw - (int)stats.size()) / 2, .y = desc_y, .content = stats, .tag = UITag::TextAccent});
    desc_y += 2;

    // Starting skills
    std::string skills = "Skills: ";
    for (size_t i = 0; i < tmpl.starting_skills.size(); ++i) {
        const auto* sk = find_skill(tmpl.starting_skills[i]);
        if (sk) {
            if (skills.size() > 8) skills += ", ";
            skills += sk->name;
        }
    }
    // Truncate if too long
    if ((int)skills.size() > clw - 4) {
        skills = skills.substr(0, clw - 7) + "...";
    }
    ctx.text({.x = (clw - (int)skills.size()) / 2, .y = desc_y, .content = skills, .tag = UITag::TextDim});
    desc_y += 1;

    std::string extras = "SP: " + std::to_string(tmpl.starting_sp)
                       + "  Credits: " + std::to_string(tmpl.starting_money)
                       + "  Bonus HP: +" + std::to_string(tmpl.bonus_hp);
    ctx.text({.x = (clw - (int)extras.size()) / 2, .y = desc_y, .content = extras, .tag = UITag::TextDim});
}

// ── Step: Attributes ────────────────────────────────────────────────

void CharacterCreation::draw_attributes_step(DrawContext& ctx) {
    int y = 5;

    // Points remaining
    std::string pts = "Points remaining: " + std::to_string(attr_points_remaining_);
    UITag pts_tag = (attr_points_remaining_ > 0) ? UITag::TextWarning : UITag::TextSuccess;
    int aw = ctx.width();
    ctx.text({.x = (aw - (int)pts.size()) / 2, .y = y, .content = pts, .tag = pts_tag});
    y += 2;

    // Get race and class data
    const auto& rt = race_template(result_.race);
    const auto& ct = class_template(result_.player_class);
    int class_vals[6] = {ct.attributes.strength, ct.attributes.agility,
                         ct.attributes.toughness, ct.attributes.intelligence,
                         ct.attributes.willpower, ct.attributes.luck};

    // Two rows of three attribute boxes
    int box_w = 10;
    int box_h = 4;
    int gap = 2;
    int row_w = 3 * box_w + 2 * gap;
    int start_x = (ctx.width() - row_w) / 2;

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            int idx = row * 3 + col;
            int bx = start_x + col * (box_w + gap);
            int by = y + row * (box_h + 1);
            bool sel = (idx == attr_cursor_);
            Color border_c = sel ? Color::Cyan : Color::DarkGray;
            Color val_c = sel ? Color::Yellow : Color::White;

            // Box border (UTF-8 box drawing)
            using namespace BoxDraw;
            ctx.put(bx, by, TL, border_c);
            for (int i = 1; i < box_w - 1; ++i) ctx.put(bx + i, by, H, border_c);
            ctx.put(bx + box_w - 1, by, TR, border_c);

            for (int r = 1; r < box_h - 1; ++r) {
                ctx.put(bx, by + r, V, border_c);
                for (int i = 1; i < box_w - 1; ++i) ctx.put(bx + i, by + r, ' ');
                ctx.put(bx + box_w - 1, by + r, V, border_c);
            }

            ctx.put(bx, by + box_h - 1, BL, border_c);
            for (int i = 1; i < box_w - 1; ++i) ctx.put(bx + i, by + box_h - 1, H, border_c);
            ctx.put(bx + box_w - 1, by + box_h - 1, BR, border_c);

            // Label
            int label_len = static_cast<int>(std::strlen(attr_names[idx]));
            UITag label_tag = sel ? UITag::TextAccent : UITag::TextDim;
            ctx.text({.x = bx + (box_w - label_len) / 2, .y = by,
                      .content = std::string(attr_names[idx]), .tag = label_tag});

            // Final value
            int final_val = class_vals[idx] + rt.attr_mods[idx] + attr_alloc_[idx];
            std::string val_str = std::to_string(final_val);
            UITag val_tag = sel ? UITag::TextWarning : UITag::TextBright;
            ctx.text({.x = bx + (box_w - (int)val_str.size()) / 2, .y = by + 1,
                      .content = val_str, .tag = val_tag});

            // Allocation indicator
            if (attr_alloc_[idx] > 0) {
                std::string alloc = "(+" + std::to_string(attr_alloc_[idx]) + ")";
                ctx.text({.x = bx + (box_w - (int)alloc.size()) / 2, .y = by + 2,
                          .content = alloc, .tag = UITag::TextSuccess});
            }

            // Selection arrows
            if (sel) {
                ctx.put(bx + box_w / 2 - 1, by + box_h, '^', Color::Cyan);
            }
        }
    }

    // Breakdown for selected attribute
    int breakdown_y = y + 2 * (box_h + 1) + 1;
    int idx = attr_cursor_;
    int base = class_vals[idx];
    int race_mod = rt.attr_mods[idx];
    int alloc = attr_alloc_[idx];
    int total = base + race_mod + alloc;

    std::string breakdown = std::string(attr_full_names[idx]) + ": "
        + "Class(" + std::to_string(base) + ")"
        + " + Race(" + (race_mod >= 0 ? "+" : "") + std::to_string(race_mod) + ")"
        + " + You(+" + std::to_string(alloc) + ")"
        + " = " + std::to_string(total);
    ctx.text({.x = (aw - (int)breakdown.size()) / 2, .y = breakdown_y,
              .content = breakdown, .tag = UITag::TextDim});

    // Navigation hint
    std::vector<TextSegment> nav_segs;
    auto ka1 = key_action_segments("Up/Down", "adjust");
    nav_segs.insert(nav_segs.end(), ka1.begin(), ka1.end());
    nav_segs.push_back({"    ", UITag::TextDim});
    auto ka2 = key_action_segments("Left/Right", "select attribute");
    nav_segs.insert(nav_segs.end(), ka2.begin(), ka2.end());
    ctx.styled_text({.x = aw / 2, .y = breakdown_y + 2, .segments = nav_segs, .align = TextAlign::Center});
}

// ── Step: Name ──────────────────────────────────────────────────────

void CharacterCreation::draw_name_step(DrawContext& ctx) {
    int y = 5;
    int nw = ctx.width();

    std::string prompt = "Enter your name, commander.";
    ctx.text({.x = (nw - (int)prompt.size()) / 2, .y = y, .content = prompt, .tag = UITag::TextBright});
    y += 2;

    // Input box
    int box_w = max_name_len_ + 4;
    int box_x = (ctx.width() - box_w) / 2;
    int box_y = y;

    using namespace BoxDraw;
    ctx.put(box_x, box_y, TL, Color::Cyan);
    for (int i = 1; i < box_w - 1; ++i) ctx.put(box_x + i, box_y, H, Color::Cyan);
    ctx.put(box_x + box_w - 1, box_y, TR, Color::Cyan);

    ctx.put(box_x, box_y + 1, V, Color::Cyan);
    for (int i = 1; i < box_w - 1; ++i) ctx.put(box_x + i, box_y + 1, ' ');
    ctx.put(box_x + box_w - 1, box_y + 1, V, Color::Cyan);

    ctx.put(box_x, box_y + 2, BL, Color::Cyan);
    for (int i = 1; i < box_w - 1; ++i) ctx.put(box_x + i, box_y + 2, H, Color::Cyan);
    ctx.put(box_x + box_w - 1, box_y + 2, BR, Color::Cyan);

    // Text + cursor
    std::string display = name_buffer_ + "_";
    ctx.text({.x = box_x + 2, .y = box_y + 1, .content = display, .tag = UITag::TextWarning});

    // Hints
    std::vector<TextSegment> hint_segs;
    auto h1 = key_action_segments("R", "Random name");
    hint_segs.insert(hint_segs.end(), h1.begin(), h1.end());
    hint_segs.push_back({"    ", UITag::TextDim});
    auto h2 = key_action_segments("Backspace", "Delete");
    hint_segs.insert(hint_segs.end(), h2.begin(), h2.end());
    hint_segs.push_back({"    ", UITag::TextDim});
    auto h3 = key_action_segments("Enter", "Confirm");
    hint_segs.insert(hint_segs.end(), h3.begin(), h3.end());
    ctx.styled_text({.x = nw / 2, .y = box_y + 4, .segments = hint_segs, .align = TextAlign::Center});

    if (name_buffer_.empty()) {
        std::string hint2 = "Press [R] for a random name, or just type.";
        ctx.text({.x = (nw - (int)hint2.size()) / 2, .y = box_y + 6, .content = hint2, .tag = UITag::TextDim});
    }
}

// ── Step: Summary ───────────────────────────────────────────────────

void CharacterCreation::draw_location_step(DrawContext& ctx) {
    int card_w = 20;
    int card_h = 7;
    int start_x = (ctx.width() - card_w) / 2;
    int card_y = 5;

    // The Heavens Above — only location for now
    draw_card(ctx, start_x, card_y, card_w, card_h,
              'H', "The Heavens Above", location_cursor_ == 0);

    int desc_y = card_y + card_h + 2;
    int lw = ctx.width();
    std::string loc1 = "Space station orbiting Jupiter.";
    std::string loc2 = "A bustling hub of trade and travelers.";
    std::string loc3 = "Recommended for new players.";
    ctx.text({.x = (lw - (int)loc1.size()) / 2, .y = desc_y, .content = loc1, .tag = UITag::TextBright});
    ctx.text({.x = (lw - (int)loc2.size()) / 2, .y = desc_y + 1, .content = loc2, .tag = UITag::TextDim});
    ctx.text({.x = (lw - (int)loc3.size()) / 2, .y = desc_y + 2, .content = loc3, .tag = UITag::TextSuccess});
}

void CharacterCreation::draw_summary_step(DrawContext& ctx) {
    using namespace BoxDraw;
    int w = ctx.width();
    int col_w = 22;
    int gap_w = 2;
    int total_w = col_w * 3 + gap_w * 2;
    int start_x = (w - total_w) / 2;
    int y = 5;
    int box_h = 16;  // height of each box

    auto final_attr = compute_final_attributes();
    auto final_resist = compute_final_resistances();
    const auto& ct = class_template(result_.player_class);

    // Helper: draw a titled box frame at (bx, by) with width bw and height bh
    auto draw_titled_box = [&](int bx, int by, int bw, int bh, const std::string& title) {
        // Top border with embedded title: ┌─ Title ─────┐
        ctx.put(bx, by, TL, Color::DarkGray);
        ctx.put(bx + 1, by, H, Color::DarkGray);
        ctx.text({.x = bx + 2, .y = by, .content = " " + title + " ", .tag = UITag::TextAccent});
        int title_end = bx + 2 + (int)title.size() + 2;
        for (int i = title_end; i < bx + bw - 1; ++i) ctx.put(i, by, H, Color::DarkGray);
        ctx.put(bx + bw - 1, by, TR, Color::DarkGray);

        // Side borders + clear interior
        for (int row = 1; row < bh - 1; ++row) {
            ctx.put(bx, by + row, V, Color::DarkGray);
            for (int i = 1; i < bw - 1; ++i) ctx.put(bx + i, by + row, ' ');
            ctx.put(bx + bw - 1, by + row, V, Color::DarkGray);
        }

        // Bottom border
        ctx.put(bx, by + bh - 1, BL, Color::DarkGray);
        for (int i = 1; i < bw - 1; ++i) ctx.put(bx + i, by + bh - 1, H, Color::DarkGray);
        ctx.put(bx + bw - 1, by + bh - 1, BR, Color::DarkGray);
    };

    // ── Left column: Attributes ──
    int lx = start_x;
    draw_titled_box(lx, y, col_w, box_h, "Attributes");

    ctx.text({.x = lx + 2, .y = y + 1, .content = std::string("STR  ") + std::to_string(final_attr.strength), .tag = UITag::TextBright});
    ctx.text({.x = lx + 2, .y = y + 2, .content = std::string("AGI  ") + std::to_string(final_attr.agility), .tag = UITag::TextBright});
    ctx.text({.x = lx + 2, .y = y + 3, .content = std::string("TOU  ") + std::to_string(final_attr.toughness), .tag = UITag::TextBright});
    ctx.text({.x = lx + 2, .y = y + 4, .content = std::string("INT  ") + std::to_string(final_attr.intelligence), .tag = UITag::TextBright});
    ctx.text({.x = lx + 2, .y = y + 5, .content = std::string("WIL  ") + std::to_string(final_attr.willpower), .tag = UITag::TextBright});
    ctx.text({.x = lx + 2, .y = y + 6, .content = std::string("LUC  ") + std::to_string(final_attr.luck), .tag = UITag::TextBright});

    ctx.text({.x = lx + 2, .y = y + 8, .content = "Resistances", .tag = UITag::TextAccent});
    int ry = y + 9;
    if (final_resist.acid > 0) {
        ctx.text({.x = lx + 3, .y = ry, .content = std::string("Acid  +") + std::to_string(final_resist.acid), .tag = UITag::TextSuccess});
        ry++;
    }
    if (final_resist.electrical > 0) {
        ctx.text({.x = lx + 3, .y = ry, .content = std::string("Elec  +") + std::to_string(final_resist.electrical), .tag = UITag::TextSuccess});
        ry++;
    }
    if (final_resist.cold > 0) {
        ctx.text({.x = lx + 3, .y = ry, .content = std::string("Cold  +") + std::to_string(final_resist.cold), .tag = UITag::TextSuccess});
        ry++;
    }
    if (final_resist.heat > 0) {
        ctx.text({.x = lx + 3, .y = ry, .content = std::string("Heat  +") + std::to_string(final_resist.heat), .tag = UITag::TextSuccess});
        ry++;
    }

    // ── Center column: Character identity ──
    int cx = start_x + col_w + gap_w;
    draw_titled_box(cx, y, col_w, box_h, "Character");

    // Name centered in the box
    int name_x = cx + (col_w - (int)result_.name.size()) / 2;
    ctx.text({.x = name_x, .y = y + 1, .content = result_.name, .tag = UITag::TextWarning});

    // Glyph centered
    ctx.put(cx + col_w / 2, y + 3, '@', Color::Yellow);

    // Race and class centered in box
    std::string race_str(race_name(result_.race));
    std::string class_str(class_name(result_.player_class));
    ctx.text({.x = cx + (col_w - (int)race_str.size()) / 2, .y = y + 5, .content = race_str, .tag = UITag::TextBright});
    ctx.text({.x = cx + (col_w - (int)class_str.size()) / 2, .y = y + 6, .content = class_str, .tag = UITag::TextAccent});

    // Derived stats
    int hp = 10 + ct.bonus_hp + (final_attr.toughness - 10) * 2;
    int dodge = 3 + (final_attr.agility - 10) / 3;
    int defense = 5 + (final_attr.toughness - 10) / 3;
    int attack = 1 + (final_attr.strength - 10) / 2;
    ctx.text({.x = cx + 2, .y = y + 8, .content = std::string("HP: ") + std::to_string(hp), .tag = UITag::TextBright});
    ctx.text({.x = cx + 2, .y = y + 9, .content = std::string("Dodge: ") + std::to_string(dodge), .tag = UITag::TextBright});
    ctx.text({.x = cx + 2, .y = y + 10, .content = std::string("Defense: ") + std::to_string(defense), .tag = UITag::TextBright});
    ctx.text({.x = cx + 2, .y = y + 11, .content = std::string("Attack: ") + std::to_string(attack), .tag = UITag::TextBright});

    // ── Right column: Class info ──
    int rx = start_x + 2 * (col_w + gap_w);
    draw_titled_box(rx, y, col_w, box_h, class_name(result_.player_class));

    ctx.text({.x = rx + 2, .y = y + 1, .content = "Starting skills:", .tag = UITag::TextDim});
    int sy = y + 2;
    for (const auto& sid : ct.starting_skills) {
        const auto* sk = find_skill(sid);
        if (sk) {
            ctx.text({.x = rx + 3, .y = sy, .content = std::string(sk->name), .tag = UITag::TextBright});
            sy++;
        }
    }
    sy++;
    ctx.text({.x = rx + 2, .y = sy, .content = std::string("SP: ") + std::to_string(ct.starting_sp), .tag = UITag::TextDim});
    ctx.text({.x = rx + 2, .y = sy + 1, .content = std::string("Credits: ") + std::to_string(ct.starting_money), .tag = UITag::TextDim});
    ctx.text({.x = rx + 2, .y = sy + 2, .content = std::string("Bonus HP: +") + std::to_string(ct.bonus_hp), .tag = UITag::TextDim});
}

// ── Helpers ─────────────────────────────────────────────────────────

PrimaryAttributes CharacterCreation::compute_final_attributes() const {
    const auto& rt = race_template(result_.race);
    const auto& ct = class_template(result_.player_class);
    PrimaryAttributes a;
    a.strength     = ct.attributes.strength     + rt.attr_mods[0] + attr_alloc_[0];
    a.agility      = ct.attributes.agility      + rt.attr_mods[1] + attr_alloc_[1];
    a.toughness    = ct.attributes.toughness    + rt.attr_mods[2] + attr_alloc_[2];
    a.intelligence = ct.attributes.intelligence + rt.attr_mods[3] + attr_alloc_[3];
    a.willpower    = ct.attributes.willpower    + rt.attr_mods[4] + attr_alloc_[4];
    a.luck         = ct.attributes.luck         + rt.attr_mods[5] + attr_alloc_[5];
    return a;
}

Resistances CharacterCreation::compute_final_resistances() const {
    const auto& rt = race_template(result_.race);
    const auto& ct = class_template(result_.player_class);
    Resistances r;
    r.acid       = ct.resistances.acid       + rt.resist_mods.acid;
    r.electrical = ct.resistances.electrical + rt.resist_mods.electrical;
    r.cold       = ct.resistances.cold       + rt.resist_mods.cold;
    r.heat       = ct.resistances.heat       + rt.resist_mods.heat;
    return r;
}

std::string CharacterCreation::generate_random_name() {
    static const char* prefixes[] = {
        "Zar", "Kae", "Rix", "Vel", "Tho", "Nym", "Ash", "Ori", "Dex", "Sol",
        "Jax", "Kor", "Zel", "Vyn", "Tal", "Nex", "Ira", "Rho", "Cyr", "Eos",
    };
    static const char* suffixes[] = {
        "on", "ia", "ex", "us", "ar", "en", "is", "ax", "ra", "id",
        "os", "el", "an", "ix", "or", "um", "as", "yn", "al", "ik",
    };
    int pi = rng_() % 20;
    int si = rng_() % 20;
    return std::string(prefixes[pi]) + suffixes[si];
}

} // namespace astra
