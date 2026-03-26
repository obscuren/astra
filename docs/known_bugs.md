# Known Bugs

## Load menu Esc returns to main menu instead of game

**Steps:**
1. During gameplay, press Esc (pause menu)
2. Select "Load game"
3. Press Esc to cancel

**Expected:** Returns to gameplay (Playing state)
**Actual:** Returns to main menu (title screen)

**Cause:** The load menu's Esc handler sets `state_ = GameState::MainMenu` unconditionally, rather than checking if the player came from the pause menu during an active game.

**Severity:** Medium — player loses unsaved progress if they accidentally enter the load screen.
