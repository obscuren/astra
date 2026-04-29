#include "astra/pda_screen.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <string>

namespace astra {

// ─────────────────────────────────────────────────────────────────
// Journal tab
// ─────────────────────────────────────────────────────────────────

namespace {

// returns "" if quest has no arc
std::string lookup_arc_title(const std::string& quest_id) {
    if (quest_id.empty()) return "";
    StoryQuest* sq = find_story_quest(quest_id);
    if (!sq) return "";
    std::string title = sq->arc_title();
    if (!title.empty()) return title;
    return sq->arc_id();
}

} // namespace

void PdaScreen::draw_journal(UIContext& ctx) {
    int w = ctx.width();
    int half = w / 2;

    if (player_->journal.empty()) {
        draw_stub(ctx, "No entries yet.");
        return;
    }

    // Clamp cursor
    int count = static_cast<int>(player_->journal.size());
    if (journal_cursor_ >= count) journal_cursor_ = count - 1;
    if (journal_cursor_ < 0) journal_cursor_ = 0;

    // Left panel: entry list (newest first)
    int y = 1;
    for (int i = count - 1; i >= 0; --i) {
        if (y >= ctx.height() - 1) break;
        const auto& entry = player_->journal[i];
        bool selected = (journal_cursor_ == i);

        if (selected) ctx.put(1, y, '>', Color::Yellow);

        // Category icon
        UITag cat_tag;
        char cat_icon;
        switch (entry.category) {
            case JournalCategory::Blueprint:  cat_icon = '+'; cat_tag = UITag::TextAccent; break;
            case JournalCategory::Discovery:  cat_icon = '*'; cat_tag = UITag::TextSuccess; break;
            case JournalCategory::Encounter:  cat_icon = '!'; cat_tag = UITag::TextDanger; break;
            case JournalCategory::Event:      cat_icon = '.'; cat_tag = UITag::TextBright; break;
            case JournalCategory::Quest:      cat_icon = '?'; cat_tag = UITag::TextWarning; break;
        }
        ctx.styled_text({.x = 2, .y = y, .segments = {
            {std::string(1, cat_icon), cat_tag},
        }});

        // Title (truncated to fit left half)
        std::string title = entry.title;
        std::string arc_title = lookup_arc_title(entry.quest_id);
        if (!arc_title.empty()) {
            title = "[" + arc_title + "] " + title;
        }
        int max_title = half - 5;
        if (static_cast<int>(title.size()) > max_title)
            title = title.substr(0, max_title);
        ctx.text({.x = 4, .y = y, .content = title,
                  .tag = selected ? UITag::TextBright : UITag::TextDim});

        y++;

        // Timestamp below title
        if (y < ctx.height() - 1) {
            ctx.text({.x = 4, .y = y, .content = entry.timestamp, .tag = UITag::TextDim});
            y += 2; // extra gap between entries
        }
    }

    // Right panel: selected entry detail
    if (journal_cursor_ >= 0 && journal_cursor_ < count) {
        const auto& entry = player_->journal[journal_cursor_];
        int rx = half + 3;
        int rw = w - half - 4;
        int ry = 1;

        // Timestamp
        ctx.text({.x = rx, .y = ry, .content = entry.timestamp, .tag = UITag::TextDim});
        ry += 2;

        // Title (with arc prefix if applicable)
        std::string detail_title = entry.title;
        std::string detail_arc = lookup_arc_title(entry.quest_id);
        if (!detail_arc.empty()) {
            detail_title = "[" + detail_arc + "] " + detail_title;
        }
        ctx.text({.x = rx, .y = ry, .content = detail_title, .tag = UITag::TextBright});
        ry += 2;

        // Technical section
        if (!entry.technical.empty()) {
            // Word-wrap the technical text
            int line_x = 0;
            for (size_t i = 0; i < entry.technical.size() && ry < ctx.height() - 2; ++i) {
                if (entry.technical[i] == '\n') {
                    ry++;
                    line_x = 0;
                    continue;
                }
                if (entry.technical[i] == ' ' && line_x >= rw) {
                    ry++;
                    line_x = 0;
                    continue;
                }
                ctx.put(rx + line_x, ry, entry.technical[i], Color::Cyan);
                line_x++;
                if (line_x >= rw) {
                    ry++;
                    line_x = 0;
                }
            }
            ry += 2;
        }

        // Separator
        if (ry < ctx.height() - 4) {
            ctx.text({.x = rx, .y = ry, .content = "--- Commander's Notes ---", .tag = UITag::TextDim});
            ry += 2;
        }

        // Personal notes
        if (!entry.personal.empty() && ry < ctx.height() - 2) {
            int line_x = 0;
            for (size_t i = 0; i < entry.personal.size() && ry < ctx.height() - 1; ++i) {
                if (entry.personal[i] == '\n') {
                    ry++;
                    line_x = 0;
                    continue;
                }
                if (entry.personal[i] == ' ' && line_x >= rw) {
                    ry++;
                    line_x = 0;
                    continue;
                }
                ctx.put(rx + line_x, ry, entry.personal[i], Color::Default);
                line_x++;
                if (line_x >= rw) {
                    ry++;
                    line_x = 0;
                }
            }
        }

        // Live map preview for Discovery entries.
        if (entry.category == JournalCategory::Discovery &&
            entry.has_discovery_location && world_ != nullptr) {
            const int pw = 11;
            const int ph = 7;
            int py = ctx.height() - ph - 2;

            if (py > ry + 1) {  // enough room below the notes
                const auto& nav = world_->navigation();
                bool same_body = (nav.current_system_id == static_cast<uint32_t>(entry.discovery_system_id) &&
                                  nav.current_body_index == entry.discovery_body_index &&
                                  nav.current_moon_index == entry.discovery_moon_index);

                // Label line above the preview.
                if (!entry.discovery_location_name.empty()) {
                    ctx.text({.x = rx, .y = py - 1,
                              .content = entry.discovery_location_name,
                              .tag = UITag::TextDim});
                }

                if (same_body) {
                    const TileMap& owm = world_->map();
                    int cx = entry.discovery_overworld_x;
                    int cy = entry.discovery_overworld_y;
                    for (int dy = 0; dy < ph; ++dy) {
                        for (int dx = 0; dx < pw; ++dx) {
                            int mx = cx - pw / 2 + dx;
                            int my = cy - ph / 2 + dy;
                            if (mx < 0 || mx >= owm.width() ||
                                my < 0 || my >= owm.height()) {
                                ctx.put(rx + dx, py + dy, ' ', Color::Default);
                                continue;
                            }
                            Tile t = owm.get(mx, my);
                            // Apply hidden-POI render substitution for consistency
                            // with the main overworld view.
                            if (const auto* hidden = owm.find_hidden_poi(mx, my)) {
                                t = hidden->underlying_tile;
                            }
                            const char* g = overworld_glyph(t, mx, my);
                            bool is_center = (dx == pw / 2 && dy == ph / 2);
                            Color c = is_center ? Color::Yellow : Color::Default;
                            ctx.put(rx + dx, py + dy, g, c);
                        }
                    }
                } else {
                    ctx.text({.x = rx, .y = py,
                              .content = "(not in current system)",
                              .tag = UITag::TextDim});
                }
            }
        }
    }
}

} // namespace astra
