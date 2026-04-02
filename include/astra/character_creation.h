#pragma once

#include "astra/character.h"
#include "astra/race.h"
#include "astra/renderer.h"
#include "astra/ui.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

enum class CreationStep : uint8_t {
    CharacterType,
    Race,
    Class,
    Attributes,
    Name,
    Location,
    Summary,
};

static constexpr int creation_step_count = 7;

struct RaceTemplate {
    Race race;
    const char* name;
    char glyph;
    const char* tagline;
    int attr_mods[6];           // STR, AGI, TOU, INT, WIL, LUC
    Resistances resist_mods;
    const char* bullets[4];     // nullptr-terminated
};

const RaceTemplate& race_template(Race r);
const std::vector<RaceTemplate>& all_race_templates();
const std::vector<RaceTemplate>& playable_race_templates();

struct CreationResult {
    Race race = Race::Human;
    PlayerClass player_class = PlayerClass::Voidwalker;
    PrimaryAttributes attributes;
    Resistances resistances;
    std::string name;
    bool complete = false;
};

class CharacterCreation {
public:
    CharacterCreation() = default;

    bool is_open() const;
    void open(Renderer* renderer);
    void close();

    bool handle_input(int key);
    void draw(int screen_w, int screen_h);

    bool is_complete() const;
    CreationResult consume_result();

private:
    Renderer* renderer_ = nullptr;
    std::mt19937 rng_{std::random_device{}()};
    bool open_ = false;
    CreationStep step_ = CreationStep::CharacterType;
    CreationResult result_;

    // Step 0: Character Type
    int type_cursor_ = 0;           // 0 = Presets, 1 = New

    // Step 1: Race
    int race_cursor_ = 0;

    // Step 2: Class
    int class_cursor_ = 0;

    // Step 3: Attributes
    int attr_cursor_ = 0;
    int attr_points_remaining_ = 10;
    int attr_alloc_[6] = {};

    // Step 4: Name
    std::string name_buffer_;

    // Step 5: Location
    int location_cursor_ = 0;
    static constexpr int max_name_len_ = 20;

    // Navigation
    void advance_step();
    void retreat_step();

    // Rendering
    void draw_breadcrumbs(UIContext& ctx);
    void draw_title(UIContext& ctx, const char* subtitle);
    void draw_footer(UIContext& ctx, const char* extra = nullptr);
    void draw_type_step(UIContext& ctx);
    void draw_race_step(UIContext& ctx);
    void draw_class_step(UIContext& ctx);
    void draw_attributes_step(UIContext& ctx);
    void draw_name_step(UIContext& ctx);
    void draw_location_step(UIContext& ctx);
    void draw_summary_step(UIContext& ctx);

    void draw_card(UIContext& ctx, int x, int y, int w, int h,
                   char glyph, const char* name, bool selected);

    PrimaryAttributes compute_final_attributes() const;
    Resistances compute_final_resistances() const;
    std::string generate_random_name();
};

} // namespace astra
