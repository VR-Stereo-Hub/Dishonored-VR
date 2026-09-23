// tools/installer/model/view_state.h - everything a screen draws. The screens
// mutate only `choices` and the small UI flags and return a UiAction; the
// controller (app/win32_app.cpp) runs the operation and fills `report`.
#pragma once
#include <string>
#include "model/installer.h"

namespace dvr::setup {

enum class Screen { Setup, Done, Manage, Guide };

enum class UiAction {
    None, Install, Browse, Rescan, Launch, Close,
    Update, ChangeSettings, CancelChange, ToggleDisable, CollectSupport,
    Uninstall, ConfirmUninstall, CancelUninstall, ApplyBaseline,
    OpenReleases, OpenGameFolder, OpenLog, ShowGuide, BackFromGuide, DesktopShortcut, StartShortcut
};

struct ViewState {
    Screen screen = Screen::Setup;
    Screen guideReturn = Screen::Setup;
    float guideZoom = 1.0f;
    Detection det;
    Choices choices;
    Report report;
    std::string lastOp;           // install | update | change | uninstall | disable | enable | baseline
    bool busy = false;
    std::string busyText;
    std::string notice;           // one line under the footer: a declined UAC prompt, a support bundle path
    bool changingSettings = false;   // Manage -> Set up, for the choices only
    bool confirmUninstall = false;
    bool deleteIni = false;
    bool advancedOpen = false;
    bool controlsOpen = false;
    std::string logPath;
};

} // namespace dvr::setup
