// tools/installer/model/view_state.h - everything a screen draws. The screens
// mutate only `choices` and the small UI flags and return a UiAction; the
// controller (app/win32_app.cpp) runs the operation and fills `report`.
#pragma once
#include <string>
#include "model/installer.h"
#include "sys/updates.h"

namespace dvr::setup {

enum class Screen { Setup, Done, Manage, Guide, About };

enum class UiAction {
    None, Install, Browse, SelectGame, Rescan, Launch, Close, CheckUpdates, DownloadUpdate,
    Update, ChangeSettings, CancelChange, ToggleDisable, CollectSupport,
    Uninstall, ConfirmUninstall, CancelUninstall, ApplyBaseline,
    ShowAbout, OpenKofi, CreditPizza, CreditVoid, CreditGingas, SaveUpdatePreference, OpenReleases, OpenGameFolder, OpenLog, ShowGuide, BackFromGuide, DesktopShortcut, StartShortcut,
    SaveHeadset
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
    int selectedGame=-1;
    bool updateChecking=false, updateDownloading=false, updatePopup=false;
    std::vector<updates::Release> releases;
    std::string updateMessage;
    std::string logPath;
    // VR-223: the headset the player reported (launcher.ini [Headset] Model), ""
    // when none is recorded yet. While it is empty the picker is a modal that
    // cannot be dismissed. `headsetPick` / `headsetOther` are the picker's draft;
    // SaveHeadset moves the draft into `headset` and writes it.
    std::string headset;
    int headsetPick = -1;
    char headsetOther[64] = {};
    bool headsetPicking = false;  // the picker is open (required, or to change a recorded one)
    std::string headsetPending;   // what SaveHeadset records
};

} // namespace dvr::setup
