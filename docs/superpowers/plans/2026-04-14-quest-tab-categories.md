# Quest Tab Categorization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure the Quest tab into four categories (Main Missions, Contracts, Bounties, Completed), render story arcs under Main with active/locked sequencing, and remove the "Story Threads" section from the Journal tab.

**Architecture:** All changes land in `draw_quests` inside `src/character_screen.cpp`. Quest categorization is a pure function over existing `Quest` state and the `QuestGraph` arc lookup. The journal's `Story Threads` block is deleted; `lookup_arc_title` helper stays (still used for journal entry tagging). No header/API changes to `quest.h` or `quest_graph.h`.

**Tech Stack:** C++20, existing `UIContext` rendering, `QuestManager`, `QuestGraph`.

---

## File Structure

- Modify: `src/character_screen.cpp`
  - Replace the `case CharTab::Quests` body (lines 984–1030) with a categorized renderer.
  - Delete the "Story Threads" block in `draw_journal` (lines 2255–2301).
- No new files. Categorization logic lives as static helpers at the top of `character_screen.cpp`.

## Design Notes

**Category assignment** (active + available quests):
- `is_story == true` → **Main Missions**
- `is_story == false`:
  - All objectives have `type == ObjectiveType::KillNpc` → **Bounties**
  - Otherwise → **Contracts**
- Any `completed_quests()` entry → **Completed** (regardless of original category)

**Main Missions layout**:
- Group by `arc_id`. Empty `arc_id` → render as a standalone entry (no arc header).
- An arc appears only if at least one of its members is active, available, or completed.
- Within an arc, iterate members in `quest_graph().arc_members(arc_id)` topological order.
- Per-member glyph:
  - Active → `●` white, show description + objectives + reward
  - Available (NpcOffer, unlocked but not accepted) → `●` dim + hint "Speak to <role>"
  - Locked, `RevealPolicy::Full` or `TitleOnly` → `○` dim, show title in grey, "— locked"
  - Locked, `RevealPolicy::Hidden` → `?` dim, show "??? — ???" in grey
  - Completed members within an active arc → `✓` green, title only

**Contracts / Bounties layout**: Flat bullet list of active quests, same objective detail as current Main entries.

**Completed layout**: Flat list, title + optional arc tag `[Arc Name]` prefix.

**Arc title lookup**: reuse existing `lookup_arc_title(quest_id)` helper (keep its definition in place).

---

## Task 1: Extract category classification helpers

**Files:**
- Modify: `src/character_screen.cpp` (add static helpers above `draw_journal`, near existing `lookup_arc_title`)

- [ ] **Step 1: Add classifier helpers**

Add near the top of the anonymous namespace / static section in `character_screen.cpp`:

```cpp
static bool quest_is_bounty(const Quest& q) {
    if (q.objectives.empty()) return false;
    for (const auto& o : q.objectives) {
        if (o.type != ObjectiveType::KillNpc) return false;
    }
    return true;
}

enum class QuestCategory { Main, Contracts, Bounties };

static QuestCategory classify_quest(const Quest& q) {
    if (q.is_story) return QuestCategory::Main;
    if (quest_is_bounty(q)) return QuestCategory::Bounties;
    return QuestCategory::Contracts;
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: success, no warnings.

- [ ] **Step 3: Commit**

```bash
git add src/character_screen.cpp
git commit -m "refactor(quests): add category classifier helpers"
```

---

## Task 2: Replace Quest tab body with categorized renderer

**Files:**
- Modify: `src/character_screen.cpp` lines 984–1030 (the `case CharTab::Quests` block)

- [ ] **Step 1: Replace the case body**

Replace the entire block from `case CharTab::Quests: {` to the matching `break;` with the following:

```cpp
case CharTab::Quests: {
    if (!quests_ ||
        (quests_->active_quests().empty() &&
         quests_->available_quests().empty() &&
         quests_->completed_quests().empty())) {
        draw_stub(content, "No active quests.");
        break;
    }

    int y = 0;
    const int maxy = content.height() - 1;

    // Partition active + available by category
    std::vector<const Quest*> main_active;
    std::vector<const Quest*> contracts;
    std::vector<const Quest*> bounties;
    for (const auto& q : quests_->active_quests()) {
        switch (classify_quest(q)) {
            case QuestCategory::Main:      main_active.push_back(&q); break;
            case QuestCategory::Contracts: contracts.push_back(&q); break;
            case QuestCategory::Bounties:  bounties.push_back(&q); break;
        }
    }
    for (const auto& q : quests_->available_quests()) {
        // Available story quests only surface under Main Missions arcs
        if (q.is_story) main_active.push_back(&q);
    }

    auto draw_header = [&](const char* label, int n) {
        if (y >= maxy) return;
        std::string s = std::string(label) + " (" + std::to_string(n) + ")";
        content.text({.x = 1, .y = y, .content = s, .tag = UITag::TextWarning});
        y++;
    };

    auto draw_objectives_and_reward = [&](const Quest& q) {
        content.text({.x = 4, .y = y, .content = q.description, .tag = UITag::TextDim});
        y++;
        for (const auto& obj : q.objectives) {
            if (y >= maxy) return;
            std::string status = obj.complete() ? "[x] " : "[ ] ";
            std::string progress = " (" + std::to_string(obj.current_count) + "/" +
                                   std::to_string(obj.target_count) + ")";
            UITag obj_tag = obj.complete() ? UITag::TextSuccess : UITag::TextBright;
            content.text({.x = 5, .y = y, .content = status + obj.description + progress,
                          .tag = obj_tag});
            y++;
        }
        if (y < maxy) {
            std::string rew = "  Reward:";
            if (q.reward.xp > 0) rew += " " + std::to_string(q.reward.xp) + " XP";
            if (q.reward.credits > 0) rew += " " + std::to_string(q.reward.credits) + "$";
            if (q.reward.skill_points > 0) rew += " " + std::to_string(q.reward.skill_points) + " SP";
            content.text({.x = 4, .y = y, .content = rew, .tag = UITag::TextAccent});
            y++;
        }
        y++;
    };

    // ── Main Missions ─────────────────────────────────────
    if (!main_active.empty()) {
        draw_header("Main Missions", static_cast<int>(main_active.size()));

        // Gather arc_ids referenced by main_active (preserving first-seen order)
        std::vector<std::string> arc_order;
        std::vector<const Quest*> standalones;
        for (const Quest* q : main_active) {
            if (q->arc_id.empty()) { standalones.push_back(q); continue; }
            if (std::find(arc_order.begin(), arc_order.end(), q->arc_id) == arc_order.end()) {
                arc_order.push_back(q->arc_id);
            }
        }

        // Render each arc
        for (const std::string& arc : arc_order) {
            if (y >= maxy) break;
            // Arc title: use catalog's arc_title() via any member
            std::string arc_name = arc;
            for (const auto& sq : story_quest_catalog()) {
                if (sq->arc_id() == arc && !sq->arc_title().empty()) {
                    arc_name = sq->arc_title(); break;
                }
            }
            content.text({.x = 2, .y = y, .content = "└─ " + arc_name,
                          .tag = UITag::TextBright});
            y++;

            auto members = quest_graph().arc_members(arc);
            for (const std::string& mid : members) {
                if (y >= maxy) break;
                StoryQuest* sq = find_story_quest(mid);
                if (!sq) continue;

                // Classify member state
                const Quest* act = quests_->find_active(mid);
                bool is_completed = false;
                for (const auto& c : quests_->completed_quests()) {
                    if (c.id == mid) { is_completed = true; break; }
                }
                bool is_available = false;
                for (const auto& a : quests_->available_quests()) {
                    if (a.id == mid) { is_available = true; break; }
                }
                bool is_locked = false;
                for (const auto& l : quests_->locked_quests()) {
                    if (l.id == mid) { is_locked = true; break; }
                }

                if (act) {
                    content.styled_text({.x = 3, .y = y, .segments = {
                        {"● ", UITag::TextBright},
                        {act->title, UITag::TextBright},
                    }});
                    y++;
                    if (y < maxy) draw_objectives_and_reward(*act);
                } else if (is_completed) {
                    std::string title;
                    for (const auto& c : quests_->completed_quests()) {
                        if (c.id == mid) { title = c.title; break; }
                    }
                    content.styled_text({.x = 3, .y = y, .segments = {
                        {"✓ ", UITag::TextSuccess},
                        {title, UITag::TextSuccess},
                    }});
                    y++;
                } else if (is_available) {
                    std::string title;
                    for (const auto& a : quests_->available_quests()) {
                        if (a.id == mid) { title = a.title; break; }
                    }
                    std::string giver = sq->offer_giver_role();
                    std::string hint = giver.empty() ? std::string("Available")
                                                     : "Speak to " + giver;
                    content.styled_text({.x = 3, .y = y, .segments = {
                        {"● ", UITag::TextDim},
                        {title + "  — " + hint, UITag::TextDim},
                    }});
                    y++;
                } else if (is_locked) {
                    RevealPolicy rev = sq->reveal_policy();
                    if (rev == RevealPolicy::Hidden) {
                        content.styled_text({.x = 3, .y = y, .segments = {
                            {"? ", UITag::TextDim},
                            {"??? — ???", UITag::TextDim},
                        }});
                    } else {
                        // Title visible, description hidden unless Full
                        std::string title;
                        for (const auto& l : quests_->locked_quests()) {
                            if (l.id == mid) { title = l.title; break; }
                        }
                        content.styled_text({.x = 3, .y = y, .segments = {
                            {"○ ", UITag::TextDim},
                            {title + "  — locked", UITag::TextDim},
                        }});
                    }
                    y++;
                }
            }
            y++; // blank between arcs
        }

        // Standalone story quests (no arc_id)
        for (const Quest* q : standalones) {
            if (y >= maxy) break;
            content.styled_text({.x = 3, .y = y, .segments = {
                {"● ", UITag::TextBright},
                {q->title, UITag::TextBright},
            }});
            y++;
            if (y < maxy) draw_objectives_and_reward(*q);
        }
    }

    // ── Contracts ─────────────────────────────────────────
    if (!contracts.empty() && y < maxy) {
        draw_header("Contracts", static_cast<int>(contracts.size()));
        for (const Quest* q : contracts) {
            if (y >= maxy) break;
            content.styled_text({.x = 3, .y = y, .segments = {
                {"● ", UITag::TextBright},
                {q->title, UITag::TextBright},
            }});
            y++;
            if (y < maxy) draw_objectives_and_reward(*q);
        }
    }

    // ── Bounties ──────────────────────────────────────────
    if (!bounties.empty() && y < maxy) {
        draw_header("Bounties", static_cast<int>(bounties.size()));
        for (const Quest* q : bounties) {
            if (y >= maxy) break;
            content.styled_text({.x = 3, .y = y, .segments = {
                {"● ", UITag::TextDanger},
                {q->title, UITag::TextDanger},
            }});
            y++;
            if (y < maxy) draw_objectives_and_reward(*q);
        }
    }

    // ── Completed ─────────────────────────────────────────
    if (!quests_->completed_quests().empty() && y < maxy) {
        draw_header("Completed", static_cast<int>(quests_->completed_quests().size()));
        for (const auto& q : quests_->completed_quests()) {
            if (y >= maxy) break;
            std::string arc = lookup_arc_title(q.id);
            std::string label = q.title;
            if (!arc.empty()) label = "[" + arc + "] " + label;
            UITag tag = (q.status == QuestStatus::Completed) ? UITag::TextSuccess
                                                             : UITag::TextDanger;
            content.styled_text({.x = 3, .y = y, .segments = {
                {"✓ ", tag},
                {label, tag},
            }});
            y++;
        }
    }
    break;
}
```

- [ ] **Step 2: Add `<algorithm>` include if missing**

Check top of file. `<algorithm>` is already included (line 14).

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: success.

- [ ] **Step 4: Run and visually verify**

Run: `./build/astra`
- Start a new game through the tutorial "Getting Airborne" — should appear under Main Missions as a standalone entry (no arc header).
- Accept "The Missing Hauler" from the Station Keeper.
- Press `c` → Quest tab. Expect Main Missions → `└─ The Hauler Arc` with `●` active for Missing Hauler, then `○`/`?` locked followers per reveal policies.
- Accept a random contract (fetch/deliver/scout) → should appear under Contracts.
- Accept a kill quest → should appear under Bounties.
- Complete one → should move to Completed.

- [ ] **Step 5: Commit**

```bash
git add src/character_screen.cpp
git commit -m "feat(ui): categorize Quest tab with Main/Contracts/Bounties/Completed"
```

---

## Task 3: Remove Story Threads from Journal

**Files:**
- Modify: `src/character_screen.cpp` lines 2255–2301 (the `// Pending story quests (ghosts …)` block inside `draw_journal`).

- [ ] **Step 1: Delete the Story Threads block**

Remove the entire block starting at `// Pending story quests (ghosts — not selectable, not in journal entries)` and ending at the closing `}` before `// Right panel: selected entry detail`. Keep `lookup_arc_title` usage in journal entry rendering (still needed for the `[Arc] Title` prefix on journal entries).

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: success.

- [ ] **Step 3: Verify**

Run: `./build/astra` → open Journal tab → confirm "-- Story Threads --" section is gone; journal entries still show `[Arc]` prefix when applicable.

- [ ] **Step 4: Commit**

```bash
git add src/character_screen.cpp
git commit -m "refactor(ui): remove Story Threads from Journal (moved to Quest tab)"
```

---

## Task 4: Roadmap update

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Add an entry under UI/UX**

Insert after the "Message log scrollback" line:

```markdown
- [x] **Quest tab categorization** — Main Missions / Contracts / Bounties / Completed; arcs rendered under Main with active, locked (title/hidden), and completed steps inline
```

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs(roadmap): check off quest tab categorization"
```

---

## Self-Review Checklist (reader sanity)

- Quest classification covers every active/available quest: story → Main, kill-only → Bounty, else → Contract. ✓
- Completed always reported in its own section regardless of origin category. ✓
- Arc rendering uses `quest_graph().arc_members(arc)` for topological order (already deterministic from T4 of the prior plan). ✓
- Hidden reveal gives `??? — ???`; TitleOnly shows real title. ✓
- `lookup_arc_title` stays; only the ghost-list section in Journal is deleted. ✓
- No new headers/APIs introduced — this is a pure renderer refactor. ✓
