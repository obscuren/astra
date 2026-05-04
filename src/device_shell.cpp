#include "astra/device_shell.h"

#include "astra/fixture_os_id.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hack_flavor.h"
#include "astra/hacking_system.h"
#include "astra/ip.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/ui.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <sstream>

namespace astra {

namespace {

// Approx chars-per-tick of streaming output. The host is at 60Hz; the spec
// asks for ~60 chars/sec which is one char per tick. Keeping it at 1 means
// the ritual streams in real-time without depending on world ticks.
constexpr int kRitualCharsPerTick = 1;

// Convenience — substitution for banner template strings.
std::string subst_banner_(std::string_view tpl,
                          std::string_view faction,
                          std::string_view fixture_name,
                          std::string_view version) {
    std::string out;
    out.reserve(tpl.size() + 32);
    for (size_t i = 0; i < tpl.size(); ) {
        if (tpl[i] == '%') {
            if (tpl.compare(i, 9, "%FACTION%") == 0)        { out.append(faction);      i += 9; continue; }
            if (tpl.compare(i, 14, "%FIXTURE_NAME%") == 0)  { out.append(fixture_name); i += 14; continue; }
            if (tpl.compare(i, 9, "%VERSION%") == 0)        { out.append(version);      i += 9; continue; }
        }
        out.push_back(tpl[i++]);
    }
    return out;
}

uint64_t shell_seed_(const Hackable* h) {
    if (!h) return 0xC0DEC0DEu;
    return static_cast<uint64_t>(h->ip) * 1000003u + static_cast<uint64_t>(h->jack_in_node_id);
}

const char* pick_(std::span<const char* const> pool, uint64_t seed, uint32_t salt) {
    if (pool.empty()) return "";
    std::mt19937_64 rng(seed ^ (salt * 0x9E3779B97F4A7C15ull));
    return pool[rng() % pool.size()];
}

} // namespace

void DeviceShell::open(Game& game, Hackable* target, ShellTier tier,
                       ShellVia via, const std::string& requested_user) {
    open_ = true;
    target_ = target;
    tier_ = tier;
    via_ = via;
    requested_user_ = requested_user;
    lines_.clear();
    input_.clear();
    cursor_ = 0;
    history_cursor_ = -1;
    ritual_pending_.clear();
    ritual_remaining_chars_ = 0;
    ritual_tick_acc_ = 0;
    ritual_done_ = false;

    // Queue the ritual + banner. Streamed out by tick().
    char ip_str[32];
    if (target_) {
        std::snprintf(ip_str, sizeof ip_str, "%s", format_ip(target_->ip).c_str());
    } else {
        std::snprintf(ip_str, sizeof ip_str, "0.0.0.0");
    }
    queue_ritual_line_(std::string("Connecting to ") + ip_str + ".... [SUCCESS]");
    queue_ritual_line_("Negotiating cipher... [SUCCESS]");
    {
        std::string welcome = "Authentication accepted. Welcome ";
        welcome += (tier_ == ShellTier::Root ? "root." : "Guest.");
        queue_ritual_line_(welcome);
    }
    queue_ritual_line_("");

    // Build the per-device filesystem before banner emit (banner doesn't
    // touch fs but commands might run during ritual).
    rebuild_fs_view();

    // Banner line(s).
    emit_banner_(game);
}

void DeviceShell::rebuild_fs_view() {
    if (target_) fs_view_.build(*target_, faction_);
}

void DeviceShell::close(Game& game) {
    open_ = false;
    target_ = nullptr;
    channel_ = HackChannel{};
    lines_.clear();
    input_.clear();
    cursor_ = 0;

    // Real-world doorway: yank cable.
    if (via_ == ShellVia::RealWorld && game.player().is_jacked_into >= 0) {
        game.player().is_jacked_into = -1;
        game.log("You yank the cable. Body free.");
    }
}

void DeviceShell::emit(const std::string& line, UITag tag) {
    lines_.push_back({line, tag});
    while (lines_.size() > 200) lines_.pop_front();
}

void DeviceShell::clear_scroll() {
    lines_.clear();
}

std::string DeviceShell::host_label_() const {
    if (!target_) return "device";
    const FixtureOsId& os = os_id_for(target_->source_type);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s-%s", os.os_name, os.version);
    return buf;
}

std::string DeviceShell::prompt_() const {
    std::string p = host_label_();
    p += ":";
    if (tier_ == ShellTier::Root) {
        p += "root# ";
    } else {
        p += "guest$ ";
    }
    return p;
}

void DeviceShell::queue_ritual_line_(const std::string& line) {
    ritual_pending_.push_back(line);
    if (ritual_remaining_chars_ == 0 && !ritual_pending_.empty()) {
        ritual_remaining_chars_ = static_cast<int>(ritual_pending_.front().size());
    }
}

void DeviceShell::emit_banner_(Game& game) {
    if (!target_) return;
    const FixtureOsId& os = os_id_for(target_->source_type);
    const HackFlavorPack& fp = flavor_for(faction_);

    // Banner chrome — pick deterministically from the seed so repeated opens
    // look consistent.
    uint64_t seed = shell_seed_(target_);
    const char* chrome_tpl = pick_(fp.banner_chrome, seed, 1);
    std::string banner = subst_banner_(chrome_tpl, fp.faction_name, os.prompt_host, os.version);
    queue_ritual_line_(banner);

    // MOTD.
    queue_ritual_line_(std::string("# ") + pick_(fp.motd_lines, seed, 2));
    queue_ritual_line_("");

    (void)game;
}

void DeviceShell::tick_frame(Game& game) {
    if (!open_) return;
    (void)game;

    // Ritual streaming: char-by-char.
    if (!ritual_done_) {
        ritual_tick_acc_ += kRitualCharsPerTick;
        while (!ritual_pending_.empty() && ritual_tick_acc_ > 0) {
            std::string& head = ritual_pending_.front();
            int reveal = std::min<int>(ritual_tick_acc_,
                                       std::max(0, ritual_remaining_chars_));
            ritual_tick_acc_ -= reveal;
            ritual_remaining_chars_ -= reveal;
            if (ritual_remaining_chars_ <= 0) {
                emit(head, UITag::TextDim);
                ritual_pending_.erase(ritual_pending_.begin());
                if (!ritual_pending_.empty()) {
                    ritual_remaining_chars_ = static_cast<int>(ritual_pending_.front().size());
                } else {
                    ritual_remaining_chars_ = 0;
                }
            } else {
                break; // not enough budget to finish current line
            }
        }
        if (ritual_pending_.empty() && ritual_remaining_chars_ == 0) {
            ritual_done_ = true;
        }
    }
}

void DeviceShell::tick_world(Game& game) {
    if (!open_) return;

    // Active long-channel: advance progress per world tick.
    if (channel_.active()) {
        channel_.progress_ticks++;
        if (channel_.progress_ticks >= channel_.scaled_turns) {
            const HackCommand* cmd = channel_.cmd;
            ParsedArgs args = channel_.args;
            // Sentinel flag so commands can disambiguate "completion" from
            // "start". The runtime calls execute() once on submit and once
            // again here on completion; the first call typically fires
            // shell.start_channel(...).
            args.argv.push_back("--__done");
            channel_ = HackChannel{};
            if (cmd && cmd->execute && target_) {
                HackCommandResult r = cmd->execute(args, *target_, *this, game);
                if (!r.message.empty()) emit(r.message, UITag::TextDefault);
            }
        }
    }
}

bool DeviceShell::handle_input(int key, Game& game) {
    if (!open_) return false;

    // While a long-channel is active, only Esc and ENTER (no-op) are accepted.
    if (channel_.active()) {
        if (key == 27) {
            abort_channel(game, "user");
            return true;
        }
        return true;
    }

    // Suppress input editing during ritual streaming — Esc still aborts shell.
    if (!ritual_done_) {
        if (key == 27) {
            close(game);
            return true;
        }
        return true;
    }

    switch (key) {
        case 27: // Esc — close / yank
            close(game);
            return true;
        case '\n': case '\r':
            submit_input(game);
            return true;
        case 127: case 8: // Backspace
            if (cursor_ > 0) {
                input_.erase(cursor_ - 1, 1);
                --cursor_;
            }
            return true;
        case KEY_DELETE:
            if (cursor_ < static_cast<int>(input_.size())) {
                input_.erase(cursor_, 1);
            }
            return true;
        case KEY_LEFT:
            if (cursor_ > 0) --cursor_;
            return true;
        case KEY_RIGHT:
            if (cursor_ < static_cast<int>(input_.size())) ++cursor_;
            return true;
        case KEY_UP: {
            int sz = static_cast<int>(history_.size());
            if (sz > 0) {
                if (history_cursor_ < 0) history_cursor_ = sz;
                if (history_cursor_ > 0) {
                    --history_cursor_;
                    input_ = history_[history_cursor_];
                    cursor_ = static_cast<int>(input_.size());
                }
            }
            return true;
        }
        case KEY_DOWN: {
            int sz = static_cast<int>(history_.size());
            if (history_cursor_ >= 0) {
                ++history_cursor_;
                if (history_cursor_ >= sz) {
                    history_cursor_ = -1;
                    input_.clear();
                } else {
                    input_ = history_[history_cursor_];
                }
                cursor_ = static_cast<int>(input_.size());
            }
            return true;
        }
        default:
            if (key >= 32 && key < 127) {
                if (input_.size() < 256) {
                    input_.insert(cursor_, 1, static_cast<char>(key));
                    ++cursor_;
                }
            }
            return true;
    }
}

void DeviceShell::submit_input(Game& game) {
    std::string line = input_;
    input_.clear();
    cursor_ = 0;
    history_cursor_ = -1;

    // Echo the prompt + line into the scroll.
    emit(prompt_() + line, UITag::TextDefault);

    // Trim leading/trailing whitespace.
    auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos) return;
    line = line.substr(first);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    if (line.empty()) return;

    history_.push_back(line);
    if (history_.size() > 64) history_.erase(history_.begin());

    dispatch_command_(line, game);
}

void DeviceShell::dispatch_command_(const std::string& line, Game& game) {
    ParsedArgs args = parse_command_line(line);
    if (args.argv.empty()) return;
    const std::string& name = args.argv[0];

    auto& reg = HackCommandRegistry::get();
    const HackCommand* cmd = reg.find(name);
    if (!cmd) {
        emit(name + ": command not found. Try 'help'.", UITag::TextDim);
        return;
    }

    // Tag check — does this device expose this command?
    if (cmd->required_tag != HackTag::None) {
        if (!target_ || !has_tag(target_->tags, cmd->required_tag)) {
            emit(name + ": not available on this device.", UITag::TextDim);
            return;
        }
    }
    // Root check.
    bool tier_root = (tier_ == ShellTier::Root);
    if (cmd->requires_root && !is_player_root(*target_, tier_root)) {
        emit(name + ": permission denied (root required). Try `hashcat --fast`.", UITag::TextDim);
        return;
    }

    if (args.wants_help) {
        render_help_for_(*cmd, game);
        return;
    }

    // Execute. Long-channel commands call shell.start_channel() inside.
    if (cmd->execute) {
        HackCommandResult r = cmd->execute(args, *target_, *this, game);
        if (!r.message.empty()) emit(r.message, UITag::TextDefault);
    }
}

void DeviceShell::render_help_for_(const HackCommand& cmd, Game& game) {
    ScaledCost cost = scaled_cost(cmd, game.player());
    emit(std::string(cmd.name) + " - " + cmd.description, UITag::TextDim);
    emit(std::string("USAGE:  ") + cmd.synopsis, UITag::TextDim);
    if (cmd.base_turns > 0 || cmd.base_heat > 0 || cmd.base_detection > 0) {
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "COST (for you, post-skill): %d turns | %d Heat | +%d Detection",
                      cost.turns, cost.heat, cost.detection);
        emit(buf, UITag::TextDim);
    } else {
        emit("COST: instant, free.", UITag::TextDim);
    }
}

bool DeviceShell::start_channel(const HackCommand& cmd, const ParsedArgs& args, Game& game) {
    if (channel_.active()) {
        emit(std::string(cmd.name) + ": a command is already in progress (Esc to abort).",
             UITag::TextDim);
        return false;
    }
    ScaledCost cost = scaled_cost(cmd, game.player());
    channel_.cmd = &cmd;
    channel_.args = args;
    channel_.scaled_turns = cost.turns;
    channel_.scaled_heat = cost.heat;
    channel_.scaled_detection = cost.detection;
    channel_.progress_ticks = 0;
    channel_.started_at_tick = game.world().world_tick();
    channel_.allow_partial = cmd.allow_partial;

    // Pay heat / detection up front (Plan 7 §7).
    if (cost.heat > 0) {
        // (Phase B will route heat through the equipped deck. Phase A logs
        //  the cost and lets HackingSystem.detection accumulate.)
    }
    if (cost.detection > 0 && via_ == ShellVia::RealWorld) {
        game.hacking().add_detection(cost.detection);
    }

    char buf[160];
    std::snprintf(buf, sizeof buf, "[*] %s... (channel %d turns)", cmd.name, cost.turns);
    emit(buf, UITag::TextDim);
    return true;
}

void DeviceShell::abort_channel(Game& game, const char* reason) {
    if (!channel_.active()) return;
    const HackCommand* cmd = channel_.cmd;
    int pct = channel_.percent();
    bool partial_ok = channel_.allow_partial && cmd && cmd->execute && target_;
    ParsedArgs args = channel_.args;

    char buf[160];
    std::snprintf(buf, sizeof buf, "[!] %s aborted (%s) at %d%%.",
                  cmd ? cmd->name : "channel",
                  reason ? reason : "interrupt",
                  pct);
    emit(buf, UITag::TextDim);

    channel_ = HackChannel{};

    if (partial_ok) {
        // Inject a synthetic flag with the percent so the command can
        // accumulate progress (cracked_digits / dumped_bytes / etc.).
        char pf[32];
        std::snprintf(pf, sizeof pf, "--__partial=%d", pct);
        args.argv.push_back(pf);
        HackCommandResult r = cmd->execute(args, *target_, *this, game);
        if (!r.message.empty()) emit(r.message, UITag::TextDim);
    }
}

void DeviceShell::render(Renderer* renderer, int screen_w, int screen_h, const Game& game) const {
    if (!open_) return;
    if (!renderer) return;

    // Centred ~80x24 panel. Bound to a minimum so it doesn't break on tiny
    // terminals.
    int w = std::min(80, screen_w - 4);
    int h = std::min(24, screen_h - 4);
    if (w < 40) w = std::min(screen_w, 40);
    if (h < 12) h = std::min(screen_h, 12);
    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;
    Rect bounds{x, y, w, h};
    UIContext outer(renderer, bounds);

    auto ctx = outer.panel({
        .title = " Device Shell ",
        .footer = " [Esc] yank cable / close shell  [Enter] run command ",
    });

    int content_h = ctx.height();
    int content_w = ctx.width();
    if (content_h <= 0 || content_w <= 0) return;
    int input_row = content_h - 1;

    // Output scroll — render most-recent lines into the rows above the prompt.
    int rows_for_output = input_row;
    int total = static_cast<int>(lines_.size());
    int start = std::max(0, total - rows_for_output);
    int row = 0;
    for (int i = start; i < total && row < rows_for_output; ++i, ++row) {
        const auto& l = lines_[i];
        std::string line = l.text;
        if (static_cast<int>(line.size()) > content_w) line.resize(content_w);
        ctx.text({.x = 0, .y = row, .content = line, .tag = l.tag});
    }

    // Bottom row: prompt + cursor or active-channel progress.
    if (channel_.active()) {
        // Inline progress bar  [▓▓▓░░░░░] N%
        std::string bar = "[*] ";
        bar += channel_.cmd ? channel_.cmd->name : "channel";
        bar += "... [";
        constexpr int kBarCells = 10;
        int filled = (channel_.percent() * kBarCells) / 100;
        if (filled > kBarCells) filled = kBarCells;
        for (int i = 0; i < kBarCells; ++i) {
            bar += (i < filled) ? '#' : '-';
        }
        char tail[16];
        std::snprintf(tail, sizeof tail, "] %d%%", channel_.percent());
        bar += tail;
        if (static_cast<int>(bar.size()) > content_w) bar.resize(content_w);
        ctx.text({.x = 0, .y = input_row, .content = bar, .tag = UITag::TextWarning});
    } else if (!ritual_done_) {
        // During ritual streaming, no prompt line — output rendering already
        // shows the streamed lines. Show a blinking dot for vibe.
        ctx.text({.x = 0, .y = input_row, .content = std::string("..."),
                  .tag = UITag::TextDim});
    } else {
        // Prompt.
        std::string p = prompt_();
        std::string display = input_;
        if (cursor_ >= static_cast<int>(display.size())) display += ' ';
        // Render prompt + buffer
        ctx.text({.x = 0, .y = input_row, .content = p + display, .tag = UITag::TextBright});
        // Inverted cursor block — mirror dev_console pattern.
        int cur_x = static_cast<int>(p.size()) + cursor_;
        char ch = (cursor_ < static_cast<int>(input_.size())) ? input_[cursor_] : ' ';
        if (cur_x >= 0 && cur_x < content_w) {
            ctx.put(cur_x, input_row, ch, Color::Black, Color::White);
        }
    }
    (void)game;
}

} // namespace astra
