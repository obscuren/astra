#include "astra/input_manager.h"

namespace astra {

void InputManager::begin_look(int px, int py) {
    looking_ = true;
    look_x_ = px;
    look_y_ = py;
    look_blink_ = 0;
}

void InputManager::begin_look_at(int mx, int my) {
    looking_ = true;
    look_x_ = mx;
    look_y_ = my;
    look_blink_ = 0;
}

void InputManager::handle_look_input(int key, int map_w, int map_h) {
    switch (key) {
        case 'k': case KEY_UP:    look_y_--; break;
        case 'j': case KEY_DOWN:  look_y_++; break;
        case 'h': case KEY_LEFT:  look_x_--; break;
        case 'l': case KEY_RIGHT: look_x_++; break;
        case 27:
            looking_ = false;
            return;
        default:
            return;
    }
    if (look_x_ < 0) look_x_ = 0;
    if (look_y_ < 0) look_y_ = 0;
    if (look_x_ >= map_w) look_x_ = map_w - 1;
    if (look_y_ >= map_h) look_y_ = map_h - 1;
}

} // namespace astra
