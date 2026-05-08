#include "astra/pda_screen.h"

namespace astra {

void PdaScreen::draw_hacking(UIContext& ctx) {
    int cy = ctx.height() / 2 - 2;
    const char* title = "-- CYBERDECK --";
    const int title_cols = 15;
    ctx.text({.x = ctx.width() / 2 - title_cols / 2, .y = cy,
              .content = title, .tag = UITag::TextDim});

    const char* line = "Cyberdeck management is under construction.";
    int line_cols = 0;
    for (const char* p = line; *p; ++p) ++line_cols;
    ctx.text({.x = ctx.width() / 2 - line_cols / 2, .y = cy + 2,
              .content = line, .tag = UITag::TextDim});
}

void PdaScreen::draw_hacking_into(Renderer* renderer, Rect bounds) {
    if (!renderer || bounds.w <= 0 || bounds.h <= 0) return;
    UIContext ctx(renderer, bounds);
    for (int j = 0; j < ctx.height(); ++j) {
        for (int i = 0; i < ctx.width(); ++i) {
            ctx.put(i, j, ' ', Color::White, Color::Black);
        }
    }
    draw_hacking(ctx);
}

} // namespace astra
