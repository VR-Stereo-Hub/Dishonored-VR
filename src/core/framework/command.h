// core/framework/command.h - the command seam: a text file an agent (or the
// player) writes and the mod polls, so the running game can be driven and
// inspected without a rebuild or a headset.
//
//   <data_dir>\command.txt   one command per line; consumed (truncated) once read
//   <data_dir>\ack.txt       "seq N" + the lines applied, written after each batch
//
// The poll runs on the PRESENT thread (tools\game-cmd.ps1 foregrounds the game
// first because the game may pause its render loop unfocused). Commands that
// must land on the game's script thread set a request the script lane picks
// up (see game/dishonored/commands.cpp). Game commands are tried first so the
// game layer can shadow a core word; then the core vocabulary:
//   status                     write status.json now
//   log level <lvl>            error|warn|info|debug|trace for every category
//   log cat <cat> <lvl>        one category
//   log flush
//   cmd                        poll counters and paths
//   skip <subsystem>           what DVR_SKIP disabled
// Everything else: one "cmd: unknown" line.
#pragma once
#include <stdint.h>

namespace dvr::command {
typedef bool (*GameHandler)(const char* cmd, const char* args);
void set_game_handler(GameHandler h);
void poll(double nowMs);              // call every frame; checks the file at 1 Hz
void dispatch_line(const char* line); // one line, game handler first
bool core_command(const char* cmd, const char* args);
uint32_t sequence();                  // batches applied so far
} // namespace dvr::command
