#pragma once

#include "astra/device_fs.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/rect.h"
#include "astra/ui.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace astra {

class Game;
class Renderer;

enum class ShellTier : uint8_t { Guest, Root };
enum class ShellVia  : uint8_t { RealWorld, Grid };

// Long-channel state machine. Owned by DeviceShell; only one active at a time.
struct HackChannel {
    const HackCommand* cmd = nullptr;
    ParsedArgs         args;
    int                scaled_turns = 0;
    int                scaled_heat = 0;
    int                scaled_detection = 0;
    int                progress_ticks = 0;   // increments per world tick
    int                started_at_tick = 0;
    bool               allow_partial = false;

    bool active() const { return cmd != nullptr; }
    int  percent() const {
        if (scaled_turns <= 0) return 0;
        int p = (progress_ticks * 100) / scaled_turns;
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        return p;
    }
};

// One line in the shell output scroll. Tagged for color / vibe.
struct ShellLine {
    std::string text;
    UITag       tag = UITag::TextDefault;
};

class DeviceShell {
public:
    DeviceShell() = default;

    // Open a shell session. `target` is the wired/adjacent Hackable.
    // `via` chooses the surrounding-context (real-world body wired or in-Grid
    // avatar). `manual_ssh` toggles the strict ssh-reject behaviour for
    // root@locked.
    void open(Game& game, Hackable* target, ShellTier tier, ShellVia via,
              const std::string& requested_user);
    void close(Game& game);

    bool is_open() const { return open_; }
    Hackable* target() { return target_; }
    const Hackable* target() const { return target_; }
    ShellTier tier() const { return tier_; }
    ShellVia  via()  const { return via_; }

    // Idle/frame tick: drives the connection-ritual char streaming. Called
    // once per renderer frame from the main loop's idle branch.
    void tick_frame(Game& game);

    // World tick: advances the active long-channel by one tick. Called from
    // HackingSystem::tick (which fires from Game::advance_world).
    void tick_world(Game& game);

    // Backwards-compat shim — calls both for callers that don't differentiate.
    void tick(Game& game) { tick_frame(game); tick_world(game); }

    // Input — returns true if the key was consumed. Handles arrow keys,
    // Home/End, Backspace/Delete, mid-line insert, Enter, Esc.
    bool handle_input(int key, Game& game);

    // Render a full-screen panel. The default path picks a centred ~80x24
    // panel (real-world doorway: shell over the world). Plan 7 §3b lets
    // the in-Grid doorway render into a bounded rect — the Tron window
    // playfield rect — so HUD chrome (Trace/Heat panes, log pane) stays
    // visible. Pass a non-empty `bounds` to use that rect instead.
    void render(Renderer* renderer, int screen_w, int screen_h, const Game& game) const;
    void render_into(Renderer* renderer, Rect bounds, const Game& game) const;

    // Append an output line. Use this from cmd_*.cpp.
    void emit(const std::string& line, UITag tag = UITag::TextDefault);

    // Submit current input buffer as a command.
    void submit_input(Game& game);

    // Start a long channel for the given parsed command. Returns false if
    // a channel is already active (caller should print the in-progress error).
    bool start_channel(const HackCommand& cmd, const ParsedArgs& args, Game& game);

    // Abort the active channel (Esc or external interrupt). Calls on_partial
    // hook for allow_partial commands.
    void abort_channel(Game& game, const char* reason);

    // Read-only channel access (renderers + tests).
    const HackChannel& channel() const { return channel_; }
    bool channel_active() const { return channel_.active(); }

    // Clear scroll. Called by `cmd_clear`.
    void clear_scroll();

    // Ringed history (most-recent first).
    const std::vector<std::string>& history() const { return history_; }

    // Procedural per-device filesystem (Plan 7 §11). Built on open(); rebuilt
    // when wiped_paths changes (cmd_wipe rebuilds in-place).
    DeviceFsView& fs_view() { return fs_view_; }
    const DeviceFsView& fs_view() const { return fs_view_; }
    void rebuild_fs_view();
    // Faction string used to pick the flavor pack. Resolved by HackingSystem
    // at open(); commands consult it via shell.faction().
    const std::string& faction() const { return faction_; }
    void set_faction(std::string s) { faction_ = std::move(s); }

private:
    bool open_ = false;
    Hackable* target_ = nullptr;
    ShellTier tier_ = ShellTier::Guest;
    ShellVia  via_ = ShellVia::RealWorld;
    std::string requested_user_;

    // Output scroll.
    std::deque<ShellLine> lines_;

    // Input buffer + cursor.
    std::string input_;
    int         cursor_ = 0;
    int         history_cursor_ = -1;
    std::vector<std::string> history_;

    // Connection ritual streaming. While ritual_remaining_ > 0, we're
    // playing the connect/banner sequence and the prompt is suppressed.
    std::vector<std::string> ritual_pending_;  // queued lines to stream
    int  ritual_remaining_chars_ = 0;          // chars of current line still to reveal
    int  ritual_tick_acc_ = 0;
    bool ritual_done_ = false;

    HackChannel channel_;
    DeviceFsView fs_view_;
    std::string faction_;

    // Build host string used in prompt + banner.
    std::string host_label_() const;

    // Build the prompt string, e.g. "TURRET-OS-2.7:guest$ " or "DOR-OS:root# ".
    std::string prompt_() const;

    // Stream-render: emit a banner block + MOTD + prompt.
    void emit_banner_(Game& game);

    // Streaming helper — split text into lines and queue for char-by-char.
    void queue_ritual_line_(const std::string& line);

    // Handle an `<cmd>` or `<cmd> --help`.
    void dispatch_command_(const std::string& line, Game& game);

    void render_help_for_(const HackCommand& cmd, Game& game);
};

} // namespace astra
