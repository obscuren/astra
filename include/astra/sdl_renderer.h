#pragma once

#include "astra/renderer.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace astra {

class SdlRenderer : public Renderer {
public:
    SdlRenderer(int cols = 200, int rows = 50, float font_size = 16.0f);
    ~SdlRenderer() override;

    void init() override;
    void shutdown() override;
    void clear() override;
    void present() override;

    void draw_char(int x, int y, char ch) override;
    void draw_char(int x, int y, char ch, Color fg) override;
    void draw_char(int x, int y, char ch, Color fg, Color bg) override;
    void draw_glyph(int x, int y, const char* utf8, Color fg) override;
    void draw_glyph(int x, int y, const char* utf8, Color fg, Color bg) override;
    void draw_string(int x, int y, const std::string& text) override;

    bool read_cell(int x, int y, char* glyph_out, Color& fg_out) const override;
    bool consume_quit_request() override;

    int get_width() const override;
    int get_height() const override;

    void draw_entity(int x, int y, const RenderDescriptor& desc) override;
    void draw_animation(int x, int y, AnimationType type, int frame_index) override;

    // Semantic UI
    Rect draw_panel(const Rect& bounds, const PanelDesc& desc) override;
    void draw_progress_bar(int x, int y, const ProgressBarDesc& desc) override;
    void draw_ui_text(int x, int y, const TextDesc& desc) override;
    void draw_styled_text(int x, int y, const StyledTextDesc& desc) override;
    void draw_list(const Rect& bounds, const ListDesc& desc) override;
    void draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) override;
    void draw_widget_bar(const Rect& bounds, const WidgetBarDesc& desc) override;
    void draw_separator(const Rect& bounds, const SeparatorDesc& desc) override;
    void draw_label_value(int x, int y, const LabelValueDesc& desc) override;
    void draw_galaxy_map(const Rect& bounds, const GalaxyMapDesc& desc) override;

    int poll_input() override;
    int wait_input() override;
    int wait_input_timeout(int timeout_ms) override;

private:
    struct Cell {
        char ch[5] = {' ', '\0', '\0', '\0', '\0'};
        Color fg = Color::Default;
        Color bg = Color::Default;
    };

    int cols_;
    int rows_;
    float font_size_;
    int cell_w_ = 0;
    int cell_h_ = 0;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    TTF_Font* font_ = nullptr;

    std::vector<std::vector<Cell>> buffer_;
    bool quit_requested_ = false;

    // Glyph texture cache: key = (codepoint << 16 | fg_color_index)
    struct GlyphCacheEntry {
        SDL_Texture* texture = nullptr;
        int w = 0;
        int h = 0;
    };
    std::unordered_map<uint32_t, GlyphCacheEntry> glyph_cache_;

    SDL_Color color_to_sdl(Color c) const;
    void render_cell(int x, int y, const Cell& cell);
    SDL_Texture* get_glyph_texture(const char* utf8, Color fg, int& out_w, int& out_h);
    void clear_glyph_cache();
};

} // namespace astra
