# Kickoff Prompt — System Faction Ownership

Copy the fenced block below into a fresh Claude Code session in `/Users/jeffrey/dev/crawler` when you're ready to start implementation.

---

```
Implement the System Faction Ownership slice for Astra.

Spec:  docs/plans/2026-04-20-system-faction-ownership-spec.md
Plan:  docs/plans/2026-04-20-system-faction-ownership-plan.md

Read the plan first — tasks are bite-sized with complete code in every
step. The spec gives the full design rationale if a task is ambiguous.

Execution approach:
- Use the superpowers:subagent-driven-development skill.
- Dispatch one fresh subagent per task via the general-purpose agent type.
- Always use Opus for subagents; never Haiku or Sonnet.
- Review the subagent's report between tasks. Do NOT run a formal
  two-stage spec/quality review for every task — these are mechanical
  C++ edits against a detailed plan, and that level of ceremony is
  overkill. A quick inspection of the commit and status is enough
  unless something looks off.

Ground rules:
- Create and work on branch `feat/system-faction-ownership` before
  touching any code. Main stays clean.
- Astra has no test framework. Validation is `cmake --build build -j`
  plus in-game smoke tests per the plan's final checklist.
- Build flag preference: `-DDEV=ON` is assumed (existing build dir at
  ./build is already configured for dev mode).
- Commit at the end of each task with the exact commit messages the
  plan prescribes. Frequent commits = safe revert.
- Do NOT push — ever — unless I explicitly ask.
- Do NOT merge to main until I've run the smoke-test checklist myself.
- If clangd flags newly-added unused includes in files you touched,
  drop them and fold the cleanup into the same task's commit where
  possible; otherwise a tiny `chore:` follow-up is fine. Do NOT
  amend commits (create new ones).
- Pre-existing clangd warnings in files you didn't touch are out of
  scope.

When a task requires adapting plan code to the real Astra API (e.g.
renderer draw primitives, variable names in existing functions), do
that adaptation — the plan calls those moments out explicitly and the
subagent should use judgment there, not wait for clarification.

After all ten tasks complete and the branch is ready for smoke
testing, stop and summarize. Do not auto-merge. I'll run the
verification checklist at the end of the plan before merging.
```

---

## What to check before kicking off

- [ ] Current branch is `main` and clean (`git status`)
- [ ] Build directory is configured with `-DDEV=ON` (already the case per your usual workflow)
- [ ] Latest EventBus slice has been merged to main (yes — commit `8a637c0` and onward)
- [ ] You've skim-reviewed the spec and plan and are happy with the approach

## Expected outcome

After the session runs, you'll have:
- Branch `feat/system-faction-ownership` with ~10 commits
- Galaxy view rendering territorial bands (default on)
- `F` keybind to toggle bands off/on in galaxy view
- Sol reliably reads as Terran Federation
- Stage 4 ambushes gated to Conclave-controlled systems
- Stage 3 beacon reads as Unclaimed
- Save/load preserves the territorial map via deterministic regeneration

Then: run the plan's final verification checklist manually. If it passes, merge.

## If things go sideways

- **Palette too bright / bands fight stars:** adjust indexes in `faction_tint_color` (Task 8b) — tunable.
- **Generation not deterministic across runs:** check that `assign_system_factions(nav, world.seed())` uses `world.seed()` exactly (not a local `std::random_device` anywhere). The plan calls this out but worth verifying.
- **Capital placement fails / starvation:** relaxation loop in Task 3 already degrades gracefully to 5-6 capitals if 7 won't fit. If the result looks visually wrong, tune `kCapitalMinDist` or `kInfluenceRadius`.
- **Toggle key conflict:** plan says fall back to `V` — verify no other binding steps on it either.

## After merge

Next logical slice is **Stage 4 quest scaffolding** — a small `story_stellar_signal_gauntlet` quest that wraps the ambushes in narrative framing. That's the original "next step" we paused on; faction ownership is the prerequisite that's now in place.
