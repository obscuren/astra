#include "astra/grid_input.h"

#include "astra/game.h"
#include "astra/grid_session.h"
#include "astra/hacking_system.h"
#include "astra/renderer.h"

#include <string>

namespace astra::grid_input {

namespace {

bool try_move(GridSession& s, int dx, int dy) {
    int nx = s.avatar_x + dx;
    int ny = s.avatar_y + dy;
    if (!s.sector.passable(nx, ny)) return false;
    s.avatar_x = nx;
    s.avatar_y = ny;
    return true;
}

void on_step(Game& game, GridSession& s) {
    GridTile here = s.sector.at(s.avatar_x, s.avatar_y);
    switch (here) {
        case GridTile::ExitNode:
            game.log("Disconnect channel...");
            game.hacking().jack_out(game, JackOutKind::Voluntary);
            return;
        case GridTile::DataNode: {
            int credits = 5 + 5 * s.trace_tick_per_turn;
            s.loot.credits += credits;
            s.sector.set(s.avatar_x, s.avatar_y, GridTile::Floor);
            game.log("Data node ripped: +" + std::to_string(credits) + " credits.");
            return;
        }
        case GridTile::EncryptedFile:
            game.log("Encrypted file. Run decrypt.exe to read.");
            return;
        case GridTile::Gateway:
            game.log("Gateway. (Traversal lands in a later task.)");
            return;
        default:
            return;
    }
}

void show_help(Game& game) {
    game.log("Grid: hjkl/arrows move, '.' wait, 'f' fire program,");
    game.log("walk to ⊙ for safe disconnect, Shift+Q hard jack-out.");
}

void open_program_picker_stub(Game& game, GridSession& s) {
    (void)s;
    game.log("No programs loaded. Slot a .exe into your cyberdeck first.");
}

} // namespace

bool handle(Game& game, int key) {
    auto* sess = game.hacking().session();
    if (!sess) return false;
    auto& s = *sess;

    auto move_with_step = [&](int dx, int dy) -> bool {
        bool moved = try_move(s, dx, dy);
        if (moved) on_step(game, s);
        return moved;
    };

    switch (key) {
        case KEY_UP:    case 'k': return move_with_step( 0, -1);
        case KEY_DOWN:  case 'j': return move_with_step( 0,  1);
        case KEY_LEFT:  case 'h': return move_with_step(-1,  0);
        case KEY_RIGHT: case 'l': return move_with_step( 1,  0);
        case '.':                 return true;
        case 'f': case 'F':       open_program_picker_stub(game, s); return false;
        case 'Q':
            game.hacking().jack_out(game, JackOutKind::HardJackOut);
            return false;
        case 'q':
            if (s.sector.at(s.avatar_x, s.avatar_y) == GridTile::ExitNode) {
                game.log("You are on the exit node. Walk onto it to disconnect.");
            } else {
                game.log("(Shift+Q for hard jack-out; walk to ⊙ for safe exit.)");
            }
            return false;
        case '?':
            show_help(game);
            return false;
    }
    return false;
}

} // namespace astra::grid_input
