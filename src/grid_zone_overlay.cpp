#include "astra/grid_zone_overlay.h"

#include <string>

namespace astra::grid_zone_overlay {

namespace {

Color tier_color(int t) {
    switch (t) {
        case 1: return Color::Blue;
        case 2: return Color::Magenta;
        case 3: return Color::Red;
    }
    return Color::DarkGray;
}

// Draw a UTF-8 string one visual cell per codepoint, clipped to the playfield.
// Handles ASCII (1 byte) and multi-byte UTF-8 (2–4 bytes) correctly.
void draw_utf8_string(Renderer& r,
                      int x, int y, int pfx, int pfy, int pfw, int pfh,
                      const std::string& text, Color c) {
    int cursor = x;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
        int len = 1;
        if      ((b & 0xE0) == 0xC0) len = 2;
        else if ((b & 0xF0) == 0xE0) len = 3;
        else if ((b & 0xF8) == 0xF0) len = 4;

        if (cursor >= 0 && cursor < pfw && y >= 0 && y < pfh) {
            if (len == 1) {
                r.draw_char(pfx + cursor, pfy + y, text[i], c);
            } else {
                char buf[5] = {0};
                for (int k = 0; k < len && i + static_cast<size_t>(k) < text.size(); ++k)
                    buf[k] = text[i + static_cast<size_t>(k)];
                r.draw_glyph(pfx + cursor, pfy + y, buf, c);
            }
        }
        ++cursor;
        i += static_cast<size_t>(len);
    }
}

// Count the number of visible cells a UTF-8 string will occupy.
// Each codepoint (regardless of byte length) occupies exactly 1 cell.
int utf8_cell_width(const std::string& text) {
    int cells = 0;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char b = static_cast<unsigned char>(text[i]);
        int len = 1;
        if      ((b & 0xE0) == 0xC0) len = 2;
        else if ((b & 0xF0) == 0xE0) len = 3;
        else if ((b & 0xF8) == 0xF0) len = 4;
        ++cells;
        i += static_cast<size_t>(len);
    }
    return cells;
}

} // namespace

void draw(Renderer& r,
          const GridSector& sec,
          const GridCamera& cam,
          int pfx, int pfy, int pfw, int pfh) {
    if (sec.zone_boxes.empty()) return;
    if (sec.zone_boxes.size() < 2) return;  // single-zone LAN: no banner clutter

    // All banners sit at playfield row 0 regardless of each zone's bbox y,
    // so they are always visible even when the zone's top edge is at sector y=0.
    constexpr int kBannerY = 0;

    // Middle dot (U+00B7, · ) — smaller than bullet, denser stride.
    constexpr const char* kPerimGlyph = "\xc2\xb7";

    // Perimeter dots may only be drawn on Void cells. Any other tile
    // (Floor in a corridor between rooms, a Door bridge, a room wall, a
    // ⊕/⊙ landmark, etc.) takes precedence — the divider must NEVER
    // erase existing content.
    auto can_stamp = [&](int sector_x, int sector_y) {
        if (!sec.in_bounds(sector_x, sector_y)) return false;
        return sec.at(sector_x, sector_y) == GridTile::Void;
    };

    for (const auto& z : sec.zone_boxes) {
        Color c = tier_color(z.tier);

        // Top + bottom edges: dot every 2nd cell.
        for (int dx = 0; dx < z.w; ++dx) {
            if ((dx % 2) != 0) continue;
            int wx     = z.x + dx;
            int wy_top = z.y - 1;
            int wy_bot = z.y + z.h;
            int sx     = wx - cam.cam_x;
            int sy_top = wy_top - cam.cam_y;
            int sy_bot = wy_bot - cam.cam_y;
            if (sx < 0 || sx >= pfw) continue;
            if (sy_top >= 0 && sy_top < pfh && can_stamp(wx, wy_top))
                r.draw_glyph(pfx + sx, pfy + sy_top, kPerimGlyph, c);
            if (sy_bot >= 0 && sy_bot < pfh && can_stamp(wx, wy_bot))
                r.draw_glyph(pfx + sx, pfy + sy_bot, kPerimGlyph, c);
        }
        // Left + right edges: dot every row (terminal cells are ~2:1 tall,
        // so every-row on verticals matches every-2nd on horizontals).
        for (int dy = 0; dy < z.h; ++dy) {
            int wy   = z.y + dy;
            int wx_l = z.x - 1;
            int wx_r = z.x + z.w;
            int sy   = wy - cam.cam_y;
            int sx_l = wx_l - cam.cam_x;
            int sx_r = wx_r - cam.cam_x;
            if (sy < 0 || sy >= pfh) continue;
            if (sx_l >= 0 && sx_l < pfw && can_stamp(wx_l, wy))
                r.draw_glyph(pfx + sx_l, pfy + sy, kPerimGlyph, c);
            if (sx_r >= 0 && sx_r < pfw && can_stamp(wx_r, wy))
                r.draw_glyph(pfx + sx_r, pfy + sy, kPerimGlyph, c);
        }

        // Banner: "— BANNER —" centred horizontally over the zone's x-extent,
        // always at playfield row 0 (visible regardless of zone's sector y).
        // em-dash: U+2014 → UTF-8 \xe2\x80\x94
        std::string text = "\xe2\x80\x94 " + z.banner + " \xe2\x80\x94";
        int center_in_sector = z.x + z.w / 2;
        int bx = center_in_sector - cam.cam_x - utf8_cell_width(text) / 2;
        draw_utf8_string(r, bx, kBannerY, pfx, pfy, pfw, pfh, text, c);
    }
}

} // namespace astra::grid_zone_overlay
