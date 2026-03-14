#pragma once

#include "crawler/renderer.h"
#include <vector>
#include <termios.h>

namespace crawler {

class TerminalRenderer : public Renderer {
public:
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
    int width_ = 80;
    int height_ = 24;
    std::vector<std::vector<char>> buffer_;
    struct termios orig_termios_;
};

} // namespace crawler
