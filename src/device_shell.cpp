#include "astra/device_shell.h"

#include "astra/fixture_os_id.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hack_flavor.h"
#include "astra/hacking_system.h"
#include "astra/ip.h"
#include "astra/player.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <sstream>

namespace astra {

namespace {

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
    history_.clear();
    channel_ = HackChannel{};

    // Build the per-device filesystem before banner emit (banner doesn't
    // touch fs but commands might run during ritual).
    rebuild_fs_view();

    if (!sink_) return;

    // Single "Connected" line + banner + MOTD. No multi-step auth ritual.
    char ip_str[32];
    if (target_) {
        std::snprintf(ip_str, sizeof ip_str, "%s", format_ip(target_->ip).c_str());
    } else {
        std::snprintf(ip_str, sizeof ip_str, "0.0.0.0");
    }
    sink_->shell_emit_line(std::string("Connected to ") + ip_str + ".",
                           UITag::TextDim);
    sink_->shell_emit_line("", UITag::TextDim);

    emit_banner_(game);
}

void DeviceShell::rebuild_fs_view() {
    if (target_) fs_view_.build(*target_, faction_);
}

void DeviceShell::close(Game& game) {
    if (!open_) return;
    open_ = false;
    Hackable* prev_target = target_;
    target_ = nullptr;
    channel_ = HackChannel{};

    // Print the standard ssh "logout / Connection closed." pair into the
    // shared scroll so it stays visible above the reverted prompt.
    if (sink_) {
        sink_->shell_emit_line("logout", UITag::TextDim);
        std::string ip_str = "device";
        if (prev_target) ip_str = format_ip(prev_target->ip);
        sink_->shell_emit_line("Connection to " + ip_str + " closed.", UITag::TextDim);
    }

    // Real-world doorway: yank cable.
    if (via_ == ShellVia::RealWorld && game.player().is_jacked_into >= 0) {
        game.player().is_jacked_into = -1;
        game.log("You yank the cable. Body free.");
    }
}

void DeviceShell::emit(const std::string& line, UITag tag) {
    if (sink_) sink_->shell_emit_line(line, tag);
}

void DeviceShell::clear_scroll() {
    if (sink_) sink_->shell_clear_scroll();
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

void DeviceShell::emit_banner_(Game& game) {
    if (!target_ || !sink_) return;
    const FixtureOsId& os = os_id_for(target_->source_type);
    const HackFlavorPack& fp = flavor_for(faction_);

    // Banner chrome — pick deterministically from the seed so repeated opens
    // look consistent. Templates contain literal newlines for box-drawing
    // rows; emit each line as its own scroll entry so the renderer doesn't
    // squash multi-row ASCII art into one cell.
    uint64_t seed = shell_seed_(target_);
    const char* chrome_tpl = pick_(fp.banner_chrome, seed, 1);
    std::string banner = subst_banner_(chrome_tpl, fp.faction_name, os.prompt_host, os.version);
    {
        std::size_t start = 0;
        while (start <= banner.size()) {
            std::size_t nl = banner.find('\n', start);
            std::string line = banner.substr(start,
                nl == std::string::npos ? std::string::npos : nl - start);
            sink_->shell_emit_line(line, UITag::TextDim);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }

    sink_->shell_emit_line(std::string("# ") + pick_(fp.motd_lines, seed, 2),
                           UITag::TextDim);
    sink_->shell_emit_line("", UITag::TextDim);

    (void)game;
}

void DeviceShell::tick_world(Game& game) {
    if (!open_) return;

    // Active long-channel: advance progress per world tick.
    if (channel_.active()) {
        channel_.progress_ticks++;

        // Update the transient progress line in the shared scroll. Format
        // is `[▓▓▓       ] 30%` — bare bar, no command-name prefix; the
        // typed command line is already echoed in scroll just above. We
        // commit on completion so the final 100% line stays in scrollback.
        if (sink_) {
            constexpr int kBarCells = 10;
            std::string bar = "[";
            int filled = (channel_.percent() * kBarCells) / 100;
            if (filled > kBarCells) filled = kBarCells;
            // U+2593 ▓ filled cell, ASCII space empty.
            for (int i = 0; i < kBarCells; ++i) {
                bar += (i < filled) ? "\xe2\x96\x93" : " ";
            }
            char tail[16];
            std::snprintf(tail, sizeof tail, "] %d%%", channel_.percent());
            bar += tail;
            sink_->shell_set_progress_line(bar, UITag::TextWarning);
        }

        if (channel_.progress_ticks >= channel_.scaled_turns) {
            const HackCommand* cmd = channel_.cmd;
            ParsedArgs args = channel_.args;
            // Commit the bar at 100% into scroll, clear the transient slot,
            // then run the cmd's completion path.
            if (sink_) {
                sink_->shell_commit_progress_line();
                sink_->shell_set_progress_line("", UITag::TextWarning);
            }
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

void DeviceShell::submit_command(const std::string& raw_line, Game& game) {
    // Trim whitespace.
    std::string line = raw_line;
    auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos) return;
    line = line.substr(first);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    if (line.empty()) return;

    push_history(line);

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
    if (cmd->requires_root && target_ && !is_player_root(*target_, tier_root)) {
        emit(name + ": permission denied (root required). Try `hashcat --fast`.",
             UITag::TextDim);
        return;
    }

    if (args.wants_help) {
        render_help_for_(*cmd, game);
        return;
    }

    if (cmd->execute && target_) {
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

    // No generic start banner — each command emits its own intent line
    // before calling start_channel(). The progress bar appears on the
    // first real-time tick.
    return true;
}

void DeviceShell::abort_channel(Game& game, const char* reason) {
    if (!channel_.active()) return;
    const HackCommand* cmd = channel_.cmd;
    int pct = channel_.percent();
    bool partial_ok = channel_.allow_partial && cmd && cmd->execute && target_;
    ParsedArgs args = channel_.args;

    // Commit the in-place progress line into scroll at its abort %, then
    // clear the transient slot.
    if (sink_) {
        sink_->shell_commit_progress_line();
        sink_->shell_set_progress_line("", UITag::TextWarning);
    }

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

} // namespace astra
