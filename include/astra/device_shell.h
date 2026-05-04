#pragma once

#include "astra/device_fs.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/ui_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

class Game;

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

// Render-side abstraction used by DeviceShell + cmd_*.cpp. The PdaScreen
// implements this — the unified terminal (cyberdeck shell + ssh session) is
// a single scroll/input/prompt that morphs by session presence. While a shell
// is open, all output (ritual, command output, channel completion lines, the
// `logout` line) appends to the same scroll. While a long channel is active,
// the bar overwrites a single transient line in place; on commit/abort the
// final state is pushed into the scroll.
class ShellOutputSink {
public:
    virtual ~ShellOutputSink() = default;
    // Append a finished line to the scroll.
    virtual void shell_emit_line(const std::string& text, UITag tag) = 0;
    // Wipe the scroll. cmd_clear uses this.
    virtual void shell_clear_scroll() = 0;
    // Update the in-place progress line (overwritten each tick). Empty
    // string clears the transient slot without committing it.
    virtual void shell_set_progress_line(const std::string& text, UITag tag) = 0;
    // Push the current transient progress line into the scroll (used on
    // channel complete / abort). No-op when no transient line is set.
    virtual void shell_commit_progress_line() = 0;
};

class DeviceShell {
public:
    DeviceShell() = default;

    // Bind the output sink (PdaScreen). Must be set before open().
    void bind_sink(ShellOutputSink* sink) { sink_ = sink; }
    ShellOutputSink* sink() { return sink_; }

    // Open a shell session. `target` is the wired/adjacent Hackable.
    // `via` chooses the surrounding-context (real-world body wired or in-Grid
    // avatar). The connection ritual is queued through the sink.
    void open(Game& game, Hackable* target, ShellTier tier, ShellVia via,
              const std::string& requested_user);
    void close(Game& game);

    bool is_open() const { return open_; }
    Hackable* target() { return target_; }
    const Hackable* target() const { return target_; }
    ShellTier tier() const { return tier_; }
    ShellVia  via()  const { return via_; }

    // World tick: advances the active long-channel by one tick. Called from
    // HackingSystem::tick (which fires from Game::advance_world).
    void tick_world(Game& game);

    // ── Output API used by cmd_*.cpp ──
    // emit() forwards to the sink. Safe to call before open() / after close()
    // (becomes a no-op if sink is null).
    void emit(const std::string& line, UITag tag = UITag::TextDefault);
    void clear_scroll();

    // Submit a line typed at the device prompt — runs the matching HackCommand
    // through the registry, with tier + tag filters. Echo + tag-reject lines
    // are emitted via the sink. PdaScreen calls this when the user submits a
    // command while the session is active.
    void submit_command(const std::string& line, Game& game);

    // Prompt string for the current session (e.g. "TURRET-OS-2.7:guest$ ").
    std::string prompt() const { return prompt_(); }

    // Start a long channel for the given parsed command. Returns false if
    // a channel is already active (caller should print the in-progress error).
    bool start_channel(const HackCommand& cmd, const ParsedArgs& args, Game& game);

    // Abort the active channel (Esc or external interrupt). Calls on_partial
    // hook for allow_partial commands.
    void abort_channel(Game& game, const char* reason);

    // Read-only channel access (renderers + tests).
    const HackChannel& channel() const { return channel_; }
    bool channel_active() const { return channel_.active(); }

    // Session-only history (typed inside this device session). Used by the
    // universal `history` command to print just this session's lines.
    const std::vector<std::string>& history() const { return history_; }
    void push_history(const std::string& cmd) {
        history_.push_back(cmd);
        if (history_.size() > 64) history_.erase(history_.begin());
    }

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
    ShellOutputSink* sink_ = nullptr;

    bool open_ = false;
    Hackable* target_ = nullptr;
    ShellTier tier_ = ShellTier::Guest;
    ShellVia  via_ = ShellVia::RealWorld;
    std::string requested_user_;

    // Session-only command history (used by `history` cmd).
    std::vector<std::string> history_;

    HackChannel channel_;
    DeviceFsView fs_view_;
    std::string faction_;

    // Build host string used in prompt + banner.
    std::string host_label_() const;
    // Build the prompt string, e.g. "TURRET-OS-2.7:guest$ " or "DOR-OS:root# ".
    std::string prompt_() const;
    // Stream the connection ritual (banner + MOTD) through the sink.
    void emit_banner_(Game& game);

    // Render the per-cmd `--help` output.
    void render_help_for_(const HackCommand& cmd, Game& game);
};

} // namespace astra
