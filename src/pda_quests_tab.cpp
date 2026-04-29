#include "astra/pda_screen.h"
#include "astra/quest.h"
#include "astra/quest_graph.h"
#include "astra/display_name.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace astra {

namespace {

bool quest_is_bounty(const Quest& q) {
    if (q.objectives.empty()) return false;
    for (const auto& o : q.objectives) {
        if (o.type != ObjectiveType::KillNpc) return false;
    }
    return true;
}

enum class QuestCategory { Main, Contracts, Bounties };

QuestCategory classify_quest(const Quest& q) {
    if (q.is_story) return QuestCategory::Main;
    if (quest_is_bounty(q)) return QuestCategory::Bounties;
    return QuestCategory::Contracts;
}

const char* quest_cat_name(int idx) {
    switch (idx) {
        case 0: return "Main Missions";
        case 1: return "Contracts";
        case 2: return "Bounties";
        case 3: return "Completed";
    }
    return "";
}

} // namespace

std::vector<PdaScreen::QuestVisItem>
PdaScreen::build_quest_vis() const {
    std::vector<QuestVisItem> vis;
    if (!quests_) return vis;

    // Partition active + available
    std::vector<const Quest*> main_pool;   // active story + available story
    std::vector<const Quest*> contracts;
    std::vector<const Quest*> bounties;
    for (const auto& q : quests_->active_quests()) {
        switch (classify_quest(q)) {
            case QuestCategory::Main:      main_pool.push_back(&q); break;
            case QuestCategory::Contracts: contracts.push_back(&q); break;
            case QuestCategory::Bounties:  bounties.push_back(&q); break;
        }
    }
    for (const auto& q : quests_->available_quests()) {
        if (q.is_story) main_pool.push_back(&q);
    }

    const auto& completed = quests_->completed_quests();

    auto cat_row = [&](int idx) {
        QuestVisItem it;
        it.kind = QuestVisItem::Kind::Category;
        it.cat_idx = idx;
        vis.push_back(std::move(it));
    };
    auto expanded = [&](int idx) {
        return idx < static_cast<int>(quest_cat_expanded_.size())
            && quest_cat_expanded_[idx];
    };

    // Category 0: Main Missions
    if (!main_pool.empty()) {
        cat_row(0);
        if (expanded(0)) {
            // Gather arcs from main_pool preserving order, then standalones
            std::vector<std::string> arc_order;
            std::vector<const Quest*> standalones;
            for (const Quest* q : main_pool) {
                if (q->arc_id.empty()) { standalones.push_back(q); continue; }
                if (std::find(arc_order.begin(), arc_order.end(), q->arc_id) == arc_order.end())
                    arc_order.push_back(q->arc_id);
            }
            for (const std::string& arc : arc_order) {
                QuestVisItem hdr;
                hdr.kind = QuestVisItem::Kind::ArcHeader;
                hdr.cat_idx = 0;
                hdr.arc_id = arc;
                vis.push_back(std::move(hdr));

                // Skip arc members when the arc is collapsed.
                if (quest_arcs_collapsed_.count(arc)) continue;

                auto members = quest_graph().arc_members(arc);
                for (const std::string& mid : members) {
                    QuestVisItem it;
                    it.kind = QuestVisItem::Kind::Quest;
                    it.cat_idx = 0;
                    it.arc_id = arc;
                    it.quest_id = mid;
                    switch (quests_->status_of(mid)) {
                        case QuestStatus::Active:    it.qstate = QuestVisItem::QState::Active;    break;
                        case QuestStatus::Completed:
                        case QuestStatus::Failed:    it.qstate = QuestVisItem::QState::Completed; break;
                        case QuestStatus::Available: it.qstate = QuestVisItem::QState::Available; break;
                        default:                     it.qstate = QuestVisItem::QState::Locked;    break;
                    }
                    vis.push_back(std::move(it));
                }
            }
            for (const Quest* q : standalones) {
                QuestVisItem it;
                it.kind = QuestVisItem::Kind::Quest;
                it.cat_idx = 0;
                it.quest_id = q->id;
                it.qstate = (q->status == QuestStatus::Available)
                                ? QuestVisItem::QState::Available
                                : QuestVisItem::QState::Active;
                vis.push_back(std::move(it));
            }
        }
    }

    // Category 1: Contracts
    if (!contracts.empty()) {
        cat_row(1);
        if (expanded(1)) {
            for (const Quest* q : contracts) {
                QuestVisItem it;
                it.kind = QuestVisItem::Kind::Quest;
                it.cat_idx = 1;
                it.quest_id = q->id;
                it.qstate = QuestVisItem::QState::Active;
                vis.push_back(std::move(it));
            }
        }
    }

    // Category 2: Bounties
    if (!bounties.empty()) {
        cat_row(2);
        if (expanded(2)) {
            for (const Quest* q : bounties) {
                QuestVisItem it;
                it.kind = QuestVisItem::Kind::Quest;
                it.cat_idx = 2;
                it.quest_id = q->id;
                it.qstate = QuestVisItem::QState::Active;
                vis.push_back(std::move(it));
            }
        }
    }

    // Category 3: Completed
    //
    // A completed arc stage appears here only when the WHOLE arc is
    // completed — in-progress arcs keep their completed stages shown
    // under the Main-Missions arc header (Category 0), avoiding the
    // double-listing that makes a partly-done arc look doubled up.
    if (!completed.empty()) {
        cat_row(3);
        if (expanded(3)) {
            auto arc_fully_complete = [&](const std::string& arc) {
                for (const auto& mid : quest_graph().arc_members(arc)) {
                    QuestStatus st = quests_->status_of(mid);
                    if (st != QuestStatus::Completed && st != QuestStatus::Failed)
                        return false;
                }
                return true;
            };

            std::unordered_set<std::string> shown_arcs;
            for (const auto& q : completed) {
                if (!q.arc_id.empty()) {
                    if (shown_arcs.count(q.arc_id)) continue;
                    if (!arc_fully_complete(q.arc_id)) continue;

                    shown_arcs.insert(q.arc_id);
                    QuestVisItem hdr;
                    hdr.kind = QuestVisItem::Kind::ArcHeader;
                    hdr.cat_idx = 3;
                    hdr.arc_id = q.arc_id;
                    vis.push_back(std::move(hdr));

                    if (quest_arcs_collapsed_.count(q.arc_id)) continue;

                    for (const auto& mid : quest_graph().arc_members(q.arc_id)) {
                        QuestVisItem it;
                        it.kind = QuestVisItem::Kind::Quest;
                        it.cat_idx = 3;
                        it.arc_id = q.arc_id;
                        it.quest_id = mid;
                        it.qstate = QuestVisItem::QState::Completed;
                        vis.push_back(std::move(it));
                    }
                } else {
                    QuestVisItem it;
                    it.kind = QuestVisItem::Kind::Quest;
                    it.cat_idx = 3;
                    it.quest_id = q.id;
                    it.qstate = QuestVisItem::QState::Completed;
                    vis.push_back(std::move(it));
                }
            }
        }
    }

    return vis;
}

void PdaScreen::draw_quests(UIContext& ctx) {
    if (quest_cat_expanded_.size() < 4) quest_cat_expanded_.assign(4, true);

    if (!quests_ ||
        (quests_->active_quests().empty() &&
         quests_->available_quests().empty() &&
         quests_->completed_quests().empty())) {
        draw_stub(ctx, "No active quests.");
        return;
    }

    int w = ctx.width();
    int half = w / 2;

    auto vis = build_quest_vis();
    int n = static_cast<int>(vis.size());
    if (quest_cursor_ >= n) quest_cursor_ = n - 1;
    if (quest_cursor_ < 0) quest_cursor_ = 0;

    // Scroll so cursor is visible
    int list_h = ctx.height() - 1;
    if (quest_cursor_ < quest_scroll_) quest_scroll_ = quest_cursor_;
    if (quest_cursor_ >= quest_scroll_ + list_h) quest_scroll_ = quest_cursor_ - list_h + 1;
    if (quest_scroll_ < 0) quest_scroll_ = 0;

    // ── Left pane: list ──────────────────────────────────
    int y = 0;
    for (int i = quest_scroll_; i < n && y < list_h; ++i) {
        const auto& it = vis[i];
        bool selected = (quest_cursor_ == i);

        if (it.kind == QuestVisItem::Kind::Category) {
            // Category bar with background + triangle
            Color bar_bg = selected ? static_cast<Color>(235) : static_cast<Color>(233);
            for (int fx = 0; fx < half; ++fx)
                ctx.put(fx, y, ' ', bar_bg, bar_bg);

            bool exp = quest_cat_expanded_[it.cat_idx];
            const char* tri = exp ? "\xe2\x96\xbe" : "\xe2\x96\xb8";
            ctx.put(1, y, tri, Color::DarkGray);
            ctx.put(3, y, BoxDraw::V, Color::Black);

            Color name_fg = selected ? Color::Yellow : Color::White;
            const char* name = quest_cat_name(it.cat_idx);
            int lx = 5;
            for (const char* p = name; *p; ++p) ctx.put(lx++, y, *p, name_fg, bar_bg);

            // Count right-aligned
            int count = 0;
            for (const auto& v : vis)
                if (v.kind != QuestVisItem::Kind::Category
                    && v.kind != QuestVisItem::Kind::ArcHeader
                    && v.cat_idx == it.cat_idx)
                    count++;
            // If category is collapsed, still compute count by rebuilding minimal
            if (!exp) {
                count = 0;
                if (it.cat_idx == 0) {
                    for (const auto& q : quests_->active_quests())
                        if (classify_quest(q) == QuestCategory::Main) count++;
                    for (const auto& q : quests_->available_quests())
                        if (q.is_story) count++;
                } else if (it.cat_idx == 1) {
                    for (const auto& q : quests_->active_quests())
                        if (classify_quest(q) == QuestCategory::Contracts) count++;
                } else if (it.cat_idx == 2) {
                    for (const auto& q : quests_->active_quests())
                        if (classify_quest(q) == QuestCategory::Bounties) count++;
                } else if (it.cat_idx == 3) {
                    count = static_cast<int>(quests_->completed_quests().size());
                }
            }
            std::string cs = std::to_string(count);
            int cx_pos = half - 2 - static_cast<int>(cs.size());
            for (int ci = 0; ci < static_cast<int>(cs.size()); ++ci)
                ctx.put(cx_pos + ci, y, cs[ci], Color::Yellow, bar_bg);
        } else if (it.kind == QuestVisItem::Kind::ArcHeader) {
            std::string arc_name = it.arc_id;
            for (const auto& sq : story_quest_catalog()) {
                if (sq->arc_id() == it.arc_id && !sq->arc_title().empty()) {
                    arc_name = sq->arc_title(); break;
                }
            }
            if (selected) ctx.put(2, y, '>', Color::Yellow);
            ctx.text({.x = 4, .y = y, .content = "└─ " + arc_name,
                      .tag = selected ? UITag::TextBright : UITag::TextAccent});
        } else { // Quest
            if (selected) ctx.put(2, y, '>', Color::Yellow);
            std::string title;
            const char* glyph = "●";
            UITag tag = UITag::TextBright;
            StoryQuest* sq = find_story_quest(it.quest_id);
            RevealPolicy rev = sq ? sq->reveal_policy() : RevealPolicy::Full;

            switch (it.qstate) {
                case QuestVisItem::QState::Active: {
                    const Quest* q = quests_->find_active(it.quest_id);
                    title = q ? q->title : it.quest_id;
                    glyph = "\xe2\x97\x8f"; // ●
                    tag = selected ? UITag::TextBright : UITag::TextDefault;
                    break;
                }
                case QuestVisItem::QState::Available: {
                    const Quest* q = quests_->find_quest(it.quest_id).quest;
                    title = q ? q->title : it.quest_id;
                    glyph = "\xe2\x97\x8f"; // ●
                    tag = UITag::TextDim;
                    break;
                }
                case QuestVisItem::QState::Completed: {
                    const Quest* q = quests_->find_quest(it.quest_id).quest;
                    title = q ? q->title : it.quest_id;
                    glyph = "\xe2\x9c\x93"; // ✓
                    tag = UITag::TextSuccess;
                    break;
                }
                case QuestVisItem::QState::Locked: {
                    if (rev == RevealPolicy::Hidden) {
                        title = "??? — ???";
                        glyph = "?";
                    } else {
                        const Quest* q = quests_->find_quest(it.quest_id).quest;
                        title = q ? q->title : it.quest_id;
                        glyph = "\xe2\x97\x8b"; // ○
                    }
                    tag = UITag::TextDim;
                    break;
                }
            }

            // Indent: arc members deeper than flat entries
            int indent = it.arc_id.empty() ? 4 : 6;
#ifdef ASTRA_DEV_MODE
            // Dev overlay: append the quest id so 'quest finish <id>' is one
            // copy-paste away from the panel the designer is looking at.
            std::string dev_suffix = "  [" + it.quest_id + "]";
            ctx.styled_text({.x = indent, .y = y, .segments = {
                {std::string(glyph) + " ", tag},
                {title, tag},
                {dev_suffix, UITag::TextDim},
            }});
#else
            ctx.styled_text({.x = indent, .y = y, .segments = {
                {std::string(glyph) + " ", tag},
                {title, tag},
            }});
#endif
        }
        y++;
    }

    // Scroll indicators
    if (quest_scroll_ > 0) ctx.put(half - 1, 0, '^', Color::DarkGray);
    if (quest_scroll_ + list_h < n) ctx.put(half - 1, list_h - 1, 'v', Color::DarkGray);

    // ── Right pane: detail ──────────────────────────────
    int rx = half + 2;
    int rw = w - half - 3;
    int ry = 1;

    auto wrap = [&](int& dy, const std::string& s, UITag tag) {
        int line_x = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\n') { dy++; line_x = 0; continue; }
            if (s[i] == ' ' && line_x >= rw) { dy++; line_x = 0; continue; }
            ctx.text({.x = rx + line_x, .y = dy, .content = std::string(1, s[i]), .tag = tag});
            line_x++;
            if (line_x >= rw) { dy++; line_x = 0; }
        }
        dy++;
    };

    if (quest_cursor_ >= 0 && quest_cursor_ < n) {
        const auto& sel = vis[quest_cursor_];

        if (sel.kind == QuestVisItem::Kind::Category) {
            ctx.text({.x = rx, .y = ry, .content = quest_cat_name(sel.cat_idx),
                      .tag = UITag::TextBright});
            ry += 2;
            const char* blurb = "";
            switch (sel.cat_idx) {
                case 0: blurb = "Story-critical missions driving the main narrative."; break;
                case 1: blurb = "Side work: fetch, deliver, and scouting contracts."; break;
                case 2: blurb = "Kill contracts. Paid per confirmed target."; break;
                case 3: blurb = "Finished business — successes and failures."; break;
            }
            wrap(ry, blurb, UITag::TextDim);
        } else if (sel.kind == QuestVisItem::Kind::ArcHeader) {
            std::string arc_name = sel.arc_id;
            for (const auto& sq : story_quest_catalog()) {
                if (sq->arc_id() == sel.arc_id && !sq->arc_title().empty()) {
                    arc_name = sq->arc_title(); break;
                }
            }
            ctx.text({.x = rx, .y = ry, .content = arc_name, .tag = UITag::TextBright});
            ry += 2;
            auto members = quest_graph().arc_members(sel.arc_id);
            int active_n = 0, done_n = 0;
            for (const auto& mid : members) {
                switch (quests_->status_of(mid)) {
                    case QuestStatus::Active:    ++active_n; break;
                    case QuestStatus::Completed: ++done_n;   break;
                    default: break;
                }
            }
            std::string s = "Chain — " + std::to_string(done_n) + "/" +
                            std::to_string(members.size()) + " complete, " +
                            std::to_string(active_n) + " active.";
            ctx.text({.x = rx, .y = ry, .content = s, .tag = UITag::TextDim});
            ry += 2;
        } else { // Quest
            StoryQuest* sq = find_story_quest(sel.quest_id);
            RevealPolicy rev = sq ? sq->reveal_policy() : RevealPolicy::Full;
            const Quest* q = quests_->find_quest(sel.quest_id).quest;

            // Title
            std::string title = (sel.qstate == QuestVisItem::QState::Locked &&
                                 rev == RevealPolicy::Hidden)
                                ? "??? — ???"
                                : (q ? q->title : sel.quest_id);
            UITag title_tag = (sel.qstate == QuestVisItem::QState::Completed)
                                ? UITag::TextSuccess : UITag::TextBright;
            ctx.text({.x = rx, .y = ry, .content = title, .tag = title_tag});
            ry += 1;

            // Status badge
            const char* badge = "";
            UITag badge_tag = UITag::TextDim;
            switch (sel.qstate) {
                case QuestVisItem::QState::Active:    badge = "[Active]";    badge_tag = UITag::TextWarning; break;
                case QuestVisItem::QState::Available: badge = "[Available]"; badge_tag = UITag::TextAccent;  break;
                case QuestVisItem::QState::Completed: badge = "[Completed]"; badge_tag = UITag::TextSuccess; break;
                case QuestVisItem::QState::Locked:    badge = "[Locked]";    badge_tag = UITag::TextDim;     break;
            }
            ctx.text({.x = rx, .y = ry, .content = badge, .tag = badge_tag});
            ry += 2;

            // Description — only reveal once the quest has been accepted
            // (Active) or resolved (Completed). Available and Locked entries
            // show title + status only; players learn details from the giver.
            bool show_desc = (sel.qstate == QuestVisItem::QState::Active ||
                              sel.qstate == QuestVisItem::QState::Completed);
            if (show_desc && q && !q->description.empty()) {
                wrap(ry, q->description, UITag::TextDim);
                ry++;
            }

            // Available: speak-to hint
            if (sel.qstate == QuestVisItem::QState::Available && sq) {
                std::string giver = sq->offer_giver_role();
                std::string hint = giver.empty() ? std::string("Seek it out.")
                                                 : "Speak to " + giver + ".";
                ctx.text({.x = rx, .y = ry, .content = hint, .tag = UITag::TextAccent});
                ry += 2;
            }

            // Objectives
            if (q && !q->objectives.empty() &&
                (sel.qstate == QuestVisItem::QState::Active ||
                 sel.qstate == QuestVisItem::QState::Completed)) {
                ctx.text({.x = rx, .y = ry, .content = "Objectives:", .tag = UITag::TextDim});
                ry++;
                for (const auto& obj : q->objectives) {
                    if (ry >= ctx.height() - 1) break;
                    std::string status = obj.complete() ? "[x] " : "[ ] ";
                    std::string progress = " (" + std::to_string(obj.current_count) + "/" +
                                           std::to_string(obj.target_count) + ")";
                    UITag ot = obj.complete() ? UITag::TextSuccess : UITag::TextBright;
                    ctx.text({.x = rx + 1, .y = ry, .content = status + obj.description + progress,
                              .tag = ot});
                    ry++;
                }
                ry++;
            }

            // Reward
            if (q && (sel.qstate == QuestVisItem::QState::Active ||
                      sel.qstate == QuestVisItem::QState::Available)) {
                const auto& rw = q->reward;
                bool has_any = rw.xp > 0 || rw.credits > 0 || rw.skill_points > 0
                            || !rw.items.empty() || !rw.factions.empty();
                if (has_any && ry < ctx.height() - 1) {
                    std::string hdr = "Rewards:";
                    if (!rw.items.empty()) hdr += "   [Tab] inspect";
                    ctx.text({.x = rx, .y = ry, .content = hdr, .tag = UITag::TextDim});
                    ry++;
                    for (size_t i = 0; i < rw.items.size(); ++i) {
                        if (ry >= ctx.height() - 1) break;
                        bool focused = (quest_focus_ == QuestFocus::Right &&
                                        static_cast<int>(i) == quest_reward_cursor_);
                        if (focused) ctx.put(rx, ry, '>', Color::Yellow);
                        ctx.text_rich(rx + 2, ry, display_name(rw.items[i]),
                                      Color::Default);
                        ry++;
                    }
                    if (rw.xp > 0 && ry < ctx.height() - 1) {
                        ctx.text({.x = rx + 2, .y = ry,
                                  .content = std::to_string(rw.xp) + " XP",
                                  .tag = UITag::TextBright});
                        ry++;
                    }
                    if (rw.credits > 0 && ry < ctx.height() - 1) {
                        ctx.text({.x = rx + 2, .y = ry,
                                  .content = std::to_string(rw.credits) + "$",
                                  .tag = UITag::TextWarning});
                        ry++;
                    }
                    if (rw.skill_points > 0 && ry < ctx.height() - 1) {
                        ctx.text({.x = rx + 2, .y = ry,
                                  .content = std::to_string(rw.skill_points) + " SP",
                                  .tag = UITag::TextBright});
                        ry++;
                    }
                    for (const auto& fr : rw.factions) {
                        if (ry >= ctx.height() - 1) break;
                        if (fr.faction_name.empty() || fr.reputation_change == 0) continue;
                        std::string sign = fr.reputation_change > 0 ? "+" : "";
                        std::string line = sign + std::to_string(fr.reputation_change)
                                         + " reputation with " + fr.faction_name;
                        ctx.text({.x = rx + 2, .y = ry, .content = line,
                                  .tag = UITag::TextSuccess});
                        ry++;
                    }
                }
            }

            // Locked prereqs hint
            if (sel.qstate == QuestVisItem::QState::Locked && rev != RevealPolicy::Hidden) {
                ctx.text({.x = rx, .y = ry,
                          .content = "Prerequisites not yet met.",
                          .tag = UITag::TextDim});
                ry++;
            }
        }
    }
}

} // namespace astra
