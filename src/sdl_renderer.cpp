#include "astra/sdl_renderer.h"
#include "astra/render_descriptor.h"
#include "astra/ui_types.h"

#include <cstring>
#include <stdexcept>

namespace astra {

// ---------------------------------------------------------------------------
// xterm-256 color palette → RGB
// ---------------------------------------------------------------------------

static SDL_Color xterm256_to_rgb(uint8_t idx) {
    // Standard 16 colors (0-15)
    static constexpr SDL_Color basic16[] = {
        {  0,   0,   0, 255}, // 0  black
        {205,   0,   0, 255}, // 1  red
        {  0, 205,   0, 255}, // 2  green
        {205, 205,   0, 255}, // 3  yellow
        {  0,   0, 238, 255}, // 4  blue
        {205,   0, 205, 255}, // 5  magenta
        {  0, 205, 205, 255}, // 6  cyan
        {229, 229, 229, 255}, // 7  white
        {127, 127, 127, 255}, // 8  bright black (dark gray)
        {255,   0,   0, 255}, // 9  bright red
        {  0, 255,   0, 255}, // 10 bright green
        {255, 255,   0, 255}, // 11 bright yellow
        { 92,  92, 255, 255}, // 12 bright blue
        {255,   0, 255, 255}, // 13 bright magenta
        {  0, 255, 255, 255}, // 14 bright cyan
        {255, 255, 255, 255}, // 15 bright white
    };

    if (idx < 16) return basic16[idx];

    // 216-color cube (16-231): r,g,b each 0-5
    if (idx < 232) {
        int i = idx - 16;
        int b = i % 6;
        int g = (i / 6) % 6;
        int r = i / 36;
        auto to8 = [](int v) -> uint8_t { return v == 0 ? 0 : static_cast<uint8_t>(55 + v * 40); };
        return {to8(r), to8(g), to8(b), 255};
    }

    // Grayscale ramp (232-255): 24 shades
    uint8_t v = static_cast<uint8_t>(8 + (idx - 232) * 10);
    return {v, v, v, 255};
}

// Build a static lookup table once.
static const SDL_Color* get_palette() {
    static SDL_Color palette[256];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < 256; ++i)
            palette[i] = xterm256_to_rgb(static_cast<uint8_t>(i));
        built = true;
    }
    return palette;
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

SdlRenderer::SdlRenderer(int cols, int rows, float font_size)
    : cols_(cols), rows_(rows), font_size_(font_size) {}

SdlRenderer::~SdlRenderer() {
    shutdown();
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

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

    // Size window to fit the requested grid, but cap to 80% of display
    int win_w = cols_ * cell_w_;
    int win_h = rows_ * cell_h_;

    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    if (mode) {
        int max_w = static_cast<int>(mode->w * 0.8f);
        int max_h = static_cast<int>(mode->h * 0.8f);
        if (win_w > max_w || win_h > max_h) {
            // Shrink cols/rows to fit
            cols_ = max_w / cell_w_;
            rows_ = max_h / cell_h_;
            win_w = cols_ * cell_w_;
            win_h = rows_ * cell_h_;
        }
    }

    window_ = SDL_CreateWindow("ASTRA", win_w, win_h, SDL_WINDOW_RESIZABLE);
    if (!window_) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    sdl_renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!sdl_renderer_) {
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }

    SDL_SetRenderVSync(sdl_renderer_, 1);

    // Build palette
    get_palette();

    // Initialize cell buffer
    buffer_.assign(rows_, std::vector<Cell>(cols_));
}

void SdlRenderer::shutdown() {
    clear_glyph_cache();
    if (font_) { TTF_CloseFont(font_); font_ = nullptr; }
    if (sdl_renderer_) { SDL_DestroyRenderer(sdl_renderer_); sdl_renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    TTF_Quit();
    SDL_Quit();
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

SDL_Color SdlRenderer::color_to_sdl(Color c) const {
    const auto* palette = get_palette();
    if (c == Color::Default) return palette[7]; // white
    return palette[static_cast<uint8_t>(c)];
}

// ---------------------------------------------------------------------------
// Glyph texture cache
// ---------------------------------------------------------------------------

// Simple hash: UTF-8 codepoint xor'd with color index.
static uint32_t glyph_cache_key(const char* utf8, Color fg) {
    uint32_t cp = 0;
    auto c = static_cast<unsigned char>(utf8[0]);
    if (c < 0x80) {
        cp = c;
    } else if ((c & 0xE0) == 0xC0) {
        cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(utf8[1]) & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(utf8[1]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(utf8[2]) & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(utf8[1]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(utf8[2]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(utf8[3]) & 0x3F);
    }
    return (cp << 8) | static_cast<uint8_t>(fg);
}

SDL_Texture* SdlRenderer::get_glyph_texture(const char* utf8, Color fg,
                                              int& out_w, int& out_h) {
    uint32_t key = glyph_cache_key(utf8, fg);
    auto it = glyph_cache_.find(key);
    if (it != glyph_cache_.end()) {
        out_w = it->second.w;
        out_h = it->second.h;
        return it->second.texture;
    }

    SDL_Color sdl_fg = color_to_sdl(fg);
    size_t len = std::strlen(utf8);
    SDL_Surface* surface = TTF_RenderText_Blended(font_, utf8, len, sdl_fg);
    if (!surface) {
        out_w = out_h = 0;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdl_renderer_, surface);
    out_w = surface->w;
    out_h = surface->h;
    SDL_DestroySurface(surface);

    if (texture) {
        glyph_cache_[key] = {texture, out_w, out_h};
    }
    return texture;
}

void SdlRenderer::clear_glyph_cache() {
    for (auto& [k, entry] : glyph_cache_) {
        if (entry.texture) SDL_DestroyTexture(entry.texture);
    }
    glyph_cache_.clear();
}

// ---------------------------------------------------------------------------
// Clear / Present
// ---------------------------------------------------------------------------

void SdlRenderer::clear() {
    for (auto& row : buffer_) {
        for (auto& cell : row) {
            cell.ch[0] = ' '; cell.ch[1] = '\0';
            cell.fg = Color::Default;
            cell.bg = Color::Default;
        }
    }
}

void SdlRenderer::present() {
    SDL_SetRenderDrawColor(sdl_renderer_, 0, 0, 0, 255);
    SDL_RenderClear(sdl_renderer_);

    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < cols_; ++x) {
            render_cell(x, y, buffer_[y][x]);
        }
    }

    SDL_RenderPresent(sdl_renderer_);
}

void SdlRenderer::render_cell(int x, int y, const Cell& cell) {
    float px = static_cast<float>(x * cell_w_);
    float py = static_cast<float>(y * cell_h_);

    // Draw background if not default (black)
    if (cell.bg != Color::Default) {
        SDL_Color bg = color_to_sdl(cell.bg);
        SDL_SetRenderDrawColor(sdl_renderer_, bg.r, bg.g, bg.b, bg.a);
        SDL_FRect bg_rect = {px, py,
                             static_cast<float>(cell_w_),
                             static_cast<float>(cell_h_)};
        SDL_RenderFillRect(sdl_renderer_, &bg_rect);
    }

    // Draw glyph (skip spaces)
    if (cell.ch[0] == ' ' && cell.ch[1] == '\0') return;
    if (cell.ch[0] == '\0') return;

    int tw, th;
    SDL_Texture* tex = get_glyph_texture(cell.ch, cell.fg, tw, th);
    if (!tex) return;

    SDL_FRect dst = {px, py,
                     static_cast<float>(tw),
                     static_cast<float>(cell_h_)};
    SDL_RenderTexture(sdl_renderer_, tex, nullptr, &dst);
}

// ---------------------------------------------------------------------------
// Draw primitives
// ---------------------------------------------------------------------------

void SdlRenderer::draw_char(int x, int y, char ch) {
    draw_char(x, y, ch, Color::Default, Color::Default);
}

void SdlRenderer::draw_char(int x, int y, char ch, Color fg) {
    draw_char(x, y, ch, fg, Color::Default);
}

void SdlRenderer::draw_char(int x, int y, char ch, Color fg, Color bg) {
    if (x >= 0 && x < cols_ && y >= 0 && y < rows_) {
        auto& cell = buffer_[y][x];
        cell.ch[0] = ch;
        cell.ch[1] = '\0';
        cell.fg = fg;
        cell.bg = bg;
    }
}

void SdlRenderer::draw_glyph(int x, int y, const char* utf8, Color fg) {
    draw_glyph(x, y, utf8, fg, Color::Default);
}

void SdlRenderer::draw_glyph(int x, int y, const char* utf8, Color fg, Color bg) {
    if (x >= 0 && x < cols_ && y >= 0 && y < rows_ && utf8) {
        auto& cell = buffer_[y][x];
        int i = 0;
        while (i < 4 && utf8[i]) {
            cell.ch[i] = utf8[i];
            ++i;
        }
        cell.ch[i] = '\0';
        cell.fg = fg;
        cell.bg = bg;
    }
}

void SdlRenderer::draw_string(int x, int y, const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        draw_char(x + static_cast<int>(i), y, text[i]);
    }
}

bool SdlRenderer::read_cell(int x, int y, char* glyph_out, Color& fg_out) const {
    if (x < 0 || x >= cols_ || y < 0 || y >= rows_) return false;
    const auto& cell = buffer_[y][x];
    for (int i = 0; i < 5; ++i) glyph_out[i] = cell.ch[i];
    fg_out = cell.fg;
    return true;
}

bool SdlRenderer::consume_quit_request() {
    bool was = quit_requested_;
    quit_requested_ = false;
    return was;
}

int SdlRenderer::get_width() const { return cols_; }
int SdlRenderer::get_height() const { return rows_; }

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

int SdlRenderer::poll_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            quit_requested_ = true;
            return 'q';
        }
        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            int new_cols = event.window.data1 / cell_w_;
            int new_rows = event.window.data2 / cell_h_;
            if (new_cols > 0 && new_rows > 0 &&
                (new_cols != cols_ || new_rows != rows_)) {
                cols_ = new_cols;
                rows_ = new_rows;
                buffer_.assign(rows_, std::vector<Cell>(cols_));
            }
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            auto key = event.key.key;
            auto mod = event.key.mod;

            // Arrow keys
            switch (key) {
                case SDLK_UP:      return KEY_UP;
                case SDLK_DOWN:    return KEY_DOWN;
                case SDLK_LEFT:    return KEY_LEFT;
                case SDLK_RIGHT:   return KEY_RIGHT;
                case SDLK_PAGEUP:  return KEY_PAGE_UP;
                case SDLK_PAGEDOWN:return KEY_PAGE_DOWN;
                case SDLK_DELETE:  return KEY_DELETE;
                case SDLK_TAB:
                    return (mod & SDL_KMOD_SHIFT) ? KEY_SHIFT_TAB : '\t';
                case SDLK_RETURN:  return '\n';
                case SDLK_ESCAPE:  return 27;
                case SDLK_SPACE:   return ' ';
                case SDLK_BACKSPACE: return 127;
                case SDLK_F1:      return KEY_F1;
                case SDLK_F2:      return KEY_F2;
                case SDLK_F3:      return KEY_F3;
                default: break;
            }

            // Letters
            if (key >= SDLK_A && key <= SDLK_Z) {
                char base = 'a' + static_cast<char>(key - SDLK_A);
                if (mod & SDL_KMOD_SHIFT) base = static_cast<char>(base - 32);
                return base;
            }

            // Digits
            if (key >= SDLK_0 && key <= SDLK_9) {
                return '0' + static_cast<char>(key - SDLK_0);
            }

            // Punctuation / symbols
            switch (key) {
                case SDLK_MINUS:       return (mod & SDL_KMOD_SHIFT) ? '_' : '-';
                case SDLK_EQUALS:      return (mod & SDL_KMOD_SHIFT) ? '+' : '=';
                case SDLK_LEFTBRACKET: return (mod & SDL_KMOD_SHIFT) ? '{' : '[';
                case SDLK_RIGHTBRACKET:return (mod & SDL_KMOD_SHIFT) ? '}' : ']';
                case SDLK_BACKSLASH:   return (mod & SDL_KMOD_SHIFT) ? '|' : '\\';
                case SDLK_SEMICOLON:   return (mod & SDL_KMOD_SHIFT) ? ':' : ';';
                case SDLK_APOSTROPHE:  return (mod & SDL_KMOD_SHIFT) ? '"' : '\'';
                case SDLK_COMMA:       return (mod & SDL_KMOD_SHIFT) ? '<' : ',';
                case SDLK_PERIOD:      return (mod & SDL_KMOD_SHIFT) ? '>' : '.';
                case SDLK_SLASH:       return (mod & SDL_KMOD_SHIFT) ? '?' : '/';
                case SDLK_GRAVE:       return (mod & SDL_KMOD_SHIFT) ? '~' : '`';
                default: break;
            }
        }
    }
    return -1;
}

int SdlRenderer::wait_input() {
    for (;;) {
        int key = poll_input();
        if (key != -1) return key;
        SDL_Delay(10);
    }
}

int SdlRenderer::wait_input_timeout(int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        int key = poll_input();
        if (key != -1) return key;
        SDL_Delay(10);
        elapsed += 10;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Semantic rendering — stubs (will be implemented incrementally)
// ---------------------------------------------------------------------------

void SdlRenderer::draw_entity(int x, int y, const RenderDescriptor& desc) {
    (void)desc;
    draw_char(x, y, '?', Color::Magenta);
}

void SdlRenderer::draw_animation(int x, int y, AnimationType type, int frame_index) {
    (void)type; (void)frame_index;
    draw_char(x, y, '*', Color::Yellow);
}

Rect SdlRenderer::draw_panel(const Rect& bounds, const PanelDesc& /*desc*/) {
    return bounds;
}

void SdlRenderer::draw_progress_bar(int /*x*/, int /*y*/, const ProgressBarDesc& /*desc*/) {}
void SdlRenderer::draw_ui_text(int /*x*/, int /*y*/, const TextDesc& /*desc*/) {}
void SdlRenderer::draw_styled_text(int /*x*/, int /*y*/, const StyledTextDesc& /*desc*/) {}
void SdlRenderer::draw_list(const Rect& /*bounds*/, const ListDesc& /*desc*/) {}
void SdlRenderer::draw_tab_bar(const Rect& /*bounds*/, const TabBarDesc& /*desc*/) {}
void SdlRenderer::draw_widget_bar(const Rect& /*bounds*/, const WidgetBarDesc& /*desc*/) {}
void SdlRenderer::draw_separator(const Rect& /*bounds*/, const SeparatorDesc& /*desc*/) {}
void SdlRenderer::draw_label_value(int /*x*/, int /*y*/, const LabelValueDesc& /*desc*/) {}
void SdlRenderer::draw_galaxy_map(const Rect& /*bounds*/, const GalaxyMapDesc& /*desc*/) {}

} // namespace astra
