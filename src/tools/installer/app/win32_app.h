// tools/installer/app/win32_app.h - the window and the controller. One fixed
// 760x640 (logical) window, DPI-scaled, that idles at 0 % CPU and redraws on
// input; the operations run on a worker thread so the window keeps painting;
// a write the process may not make is handed to an elevated copy of this exe
// running headless (--elevated-apply), whose step results come back through a
// file. Steam and the browser are always launched from an unelevated token.
#pragma once
#include <windows.h>
#include "model/installer.h"

namespace dvr::setup::app {

// Runs the GUI to completion. Returns the process exit code.
int run_gui(HINSTANCE hinst, const Env& env);

// The headless worker: --apply and --elevated-apply. Writes the report to
// `resultFile` when given, else to the attached console. Exit code 0 = ok,
// 2 = a step failed, 3 = a step failed with access denied.
struct HeadlessArgs {
    std::string op = "install";       // install | update | change | baseline | disable | enable | uninstall
    Choices choices;
    bool deleteIni = false;
    std::wstring resultFile;
};
int run_headless(const Env& env, const HeadlessArgs& args);

} // namespace dvr::setup::app
