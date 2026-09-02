// core/util/paths.h - where the mod keeps its files.
//
//   game_dir()  the folder holding d3d9.dll and Dishonored.exe. User-facing
//               files live here because users and the README depend on it:
//               dishonored_vr.ini, dishonored_vr.log, dishonored_vr_crash.txt,
//               disable_vr.txt.
//   data_dir()  %LOCALAPPDATA%\DishonoredVR (override: DVR_DATA_DIR). Harness
//               and bulk files: command.txt, ack.txt, status.json, dumps\,
//               xrsim\. Never inside the game folder, which may be read-only
//               and which a user might zip up in a bug report.
#pragma once
#include <windows.h>

namespace dvr::paths {
void        init(HINSTANCE self);      // from DllMain; kernel32 only
const char* game_dir();
const char* data_dir();                // created on first call
const char* dumps_dir();               // <data_dir>\dumps, created on first call
// <dir>\<name> into out (size MAX_PATH); returns out
const char* in_game_dir(char* out, const char* name);
const char* in_data_dir(char* out, const char* name);
} // namespace dvr::paths
