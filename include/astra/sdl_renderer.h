#pragma once

#include "astra/renderer.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>

namespace astra {

class SdlRenderer : public Renderer {
public:
    SdlRenderer(int cols = 80, int rows = 24, float font_size = 18.0f);

    void init() override;
    void shutdown() override;
    void clear() override;
    void present() override;

    void draw_char(int x, int y, char ch) override;
    void draw_string(int x, int y, const std::string& text) override;

    int get_width() const override;
    int get_height() const override;

    int poll_input() override;

private:
    int cols_;
    int rows_;
    float font_size_;
    int cell_w_ = 0;
    int cell_h_ = 0;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    TTF_Font* font_ = nullptr;

    std::vector<std::vector<char>> buffer_;

    void render_cell(int x, int y, char ch);
};

} // namespace astra
