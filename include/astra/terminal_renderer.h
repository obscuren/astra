#pragma once

#include "astra/renderer.h"
#include <memory>
#include <string>
#include <vector>

namespace astra {

class TerminalRenderer : public Renderer {
public:
    TerminalRenderer();
    ~TerminalRenderer() override;

    void init() override;
    void shutdown() override;
    void clear() override;
    void present() override;

    void draw_char(int x, int y, char ch) override;
    void draw_char(int x, int y, char ch, Color fg) override;
    void draw_glyph(int x, int y, const char* utf8, Color fg) override;
    void draw_string(int x, int y, const std::string& text) override;

    int get_width() const override;
    int get_height() const override;

    int poll_input() override;
    int wait_input() override;
    int wait_input_timeout(int timeout_ms) override;

private:
    void check_resize();
    void rebuild_buffer();

    struct Cell {
        char ch[5] = {' ', '\0', '\0', '\0', '\0'};
        Color fg = Color::Default;
    };

    int width_ = 80;
    int height_ = 24;
    std::vector<std::vector<Cell>> buffer_;
    std::string out_buf_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astra
