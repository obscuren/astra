#include "astra/sdl_renderer.h"

#include <stdexcept>

namespace astra {

SdlRenderer::SdlRenderer(int cols, int rows, float font_size)
    : cols_(cols), rows_(rows), font_size_(font_size) {}

void SdlRenderer::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    if (!TTF_Init()) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + SDL_GetError());
    }

    const char* font_paths[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/SFMono-Regular.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
    };

    for (const auto* path : font_paths) {
        font_ = TTF_OpenFont(path, font_size_);
        if (font_) break;
    }

    if (!font_) {
        throw std::runtime_error("Could not find a monospace font.");
    }

    TTF_GetStringSize(font_, "W", 1, &cell_w_, &cell_h_);

    int win_w = cols_ * cell_w_;
    int win_h = rows_ * cell_h_;

    window_ = SDL_CreateWindow("Astra", win_w, win_h, 0);
    if (!window_) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    sdl_renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!sdl_renderer_) {
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }

    SDL_SetRenderVSync(sdl_renderer_, 1);

    buffer_.assign(rows_, std::vector<char>(cols_, ' '));
}

void SdlRenderer::shutdown() {
    if (font_) { TTF_CloseFont(font_); font_ = nullptr; }
    if (sdl_renderer_) { SDL_DestroyRenderer(sdl_renderer_); sdl_renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    TTF_Quit();
    SDL_Quit();
}

void SdlRenderer::clear() {
    for (auto& row : buffer_) {
        std::fill(row.begin(), row.end(), ' ');
    }
}

void SdlRenderer::present() {
    SDL_SetRenderDrawColor(sdl_renderer_, 0, 0, 0, 255);
    SDL_RenderClear(sdl_renderer_);

    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < cols_; ++x) {
            if (buffer_[y][x] != ' ') {
                render_cell(x, y, buffer_[y][x]);
            }
        }
    }

    SDL_RenderPresent(sdl_renderer_);
}

void SdlRenderer::draw_char(int x, int y, char ch) {
    if (x >= 0 && x < cols_ && y >= 0 && y < rows_) {
        buffer_[y][x] = ch;
    }
}

void SdlRenderer::draw_string(int x, int y, const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        draw_char(x + static_cast<int>(i), y, text[i]);
    }
}

int SdlRenderer::get_width() const { return cols_; }
int SdlRenderer::get_height() const { return rows_; }

int SdlRenderer::poll_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return 'q';
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            auto key = event.key.key;
            if (key >= SDLK_A && key <= SDLK_Z) {
                return 'a' + (key - SDLK_A);
            }
            switch (key) {
                case SDLK_ESCAPE:  return 'q';
                case SDLK_RETURN:  return '\n';
                case SDLK_SPACE:   return ' ';
                case SDLK_UP:      return KEY_UP;
                case SDLK_DOWN:    return KEY_DOWN;
                case SDLK_LEFT:    return KEY_LEFT;
                case SDLK_RIGHT:   return KEY_RIGHT;
            }
        }
    }
    return -1;
}

void SdlRenderer::render_cell(int x, int y, char ch) {
    char text[2] = { ch, '\0' };
    SDL_Color fg = { 255, 255, 255, 255 };

    SDL_Surface* surface = TTF_RenderText_Blended(font_, text, 1, fg);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdl_renderer_, surface);
    SDL_DestroySurface(surface);
    if (!texture) return;

    SDL_FRect dst = {
        static_cast<float>(x * cell_w_),
        static_cast<float>(y * cell_h_),
        static_cast<float>(cell_w_),
        static_cast<float>(cell_h_)
    };
    SDL_RenderTexture(sdl_renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}

} // namespace astra
