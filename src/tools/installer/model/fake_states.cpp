// tools/installer/model/fake_states.cpp - see fake_states.h. Representative, not
// live: paths and numbers are invented so the layout can be judged.
#include "model/fake_states.h"

namespace dvr::setup {

namespace {
Detection base_detection()
{
    Detection d;
    d.version = "41.0.0"; d.buildId = "702-g1a2b3c4d"; d.config = "RelWithDebInfo";
    d.payloadOk = true; d.embeddedIniVersion = 15; d.embeddedSha = "0a3c57f6e1d2c3b4a5968778695a4b3c2d1e0f9a8b7c6d5e4f3a2b1c0d9e8f7a";
    d.gameDir = L"D:\\SteamLibrary\\steamapps\\common\\Dishonored\\Binaries\\Win32";
    d.gameFound = true; d.gameNote = "Found in your Steam library";
    d.running = process::Running::No;
    d.gameWritable = true;
    d.configDir = L"C:\\Users\\player\\Documents\\My Games\\Dishonored\\DishonoredGame\\Config";
    d.configExists = true; d.configWritable = true;
    d.d3dcompiler = true;
    d.vdxrPresent = true; d.vdxrJson = L"C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr-32.json";
    d.steamvrPresent = true;
    d.activeRuntime = L"virtualdesktop-openxr-32.json";
    d.gpu.name = L"NVIDIA GeForce RTX 4070 Ti SUPER"; d.gpu.budgetBytes = 14ull << 30;
    d.suggested.runtime = Runtime::Vdxr; d.suggested.quality = Quality::Balanced; d.suggested.pixelPercent = 100.0f;
    return d;
}
Detection installed_detection()
{
    Detection d = base_detection();
    d.d3d9Present = true; d.modInstalled = true; d.iniExists = true; d.iniVersion = 15;
    d.installedSha = "5e1f0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d8e7f6a5b4c3d2e1f";
    d.record.valid = true; d.record.version = "41.0.0"; d.record.buildId = "686-ga351bfc31"; d.record.config = "RelWithDebInfo";
    d.record.installedUtc = "2026-09-22T18:40:11Z"; d.record.runtime = "vdxr"; d.record.quality = "balanced";
    d.record.width = 2750; d.record.height = 2850;
    d.iniRuntime = Runtime::Vdxr; d.iniSize = { 2750, 2850 };
    return d;
}
Report install_report(bool baselinePending, bool failed)
{
    Report r;
    r.add(StepStatus::Ok, "Installed d3d9.dll", "2540 KB");
    r.add(StepStatus::Ok, "Installed dvr_steamvr32.dll", "310 KB");
    r.add(StepStatus::Ok, "Installed openvr_api.dll", "412 KB");
    r.add(StepStatus::Ok, "Wrote dishonored_vr.ini", "A byte copy of the settings this build was tuned and tested with; only the choices below differ.");
    r.add(StepStatus::Ok, "Headset: Quest via Virtual Desktop", "[VR] Runtime=native, XrRuntimeJson=C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr-32.json");
    r.add(StepStatus::Ok, "Render size: Balanced (tested), 2750x2850 per eye", "100% of the tested 2750x2850. Set the headset to 90 Hz: this size was judged there, and ghosts at 120.");
    r.add(StepStatus::Ok, "Data folder: %LOCALAPPDATA%\\DishonoredVR", "[Paths] DataDir= (empty)");
    if (failed) {
        r.add(StepStatus::Failed, "Could not write DishonoredEngine.ini", "Access is denied. (5)\nControlled folder access in Windows Security may be protecting Documents; allow DishonoredVR-Launcher.exe there, or apply the four values with setup-game-ini.ps1 -VRBaseline from the zip.");
    } else if (baselinePending) {
        r.add(StepStatus::Skipped, "Game settings: waiting for the game's first run", "The game writes its own settings folder the first time it runs. Launch Dishonored once from Steam, quit to the desktop, and this window applies the last four settings by itself.");
        r.baselinePending = true;
    } else {
        r.add(StepStatus::Ok, "DishonoredEngine.ini: VR baseline applied", "[Engine.Engine] bSmoothFrameRate: TRUE -> FALSE\n[SystemSettings] DepthOfField: True -> False\n[SystemSettings] UseVsync: True -> False\nBacked up to DishonoredEngine.ini.20260923-101500.dvr-backup.");
        r.add(StepStatus::Ok, "DishonoredInput.ini: VR baseline applied", "[Engine.PlayerInput] bEnableMouseSmoothing: TRUE -> FALSE\nBacked up to DishonoredInput.ini.20260923-101500.dvr-backup.");
        r.baselineApplied = true;
    }
    if (!failed) r.add(StepStatus::Ok, "Recorded the install: 41.0.0 (702-g1a2b3c4d, RelWithDebInfo)", "dishonored_vr_install.json beside the game.");
    return r;
}
}

std::vector<std::string> fake_state_names()
{
    return { "guide", "guide-zoom", "setup-found", "setup-steamvr", "setup-controls", "setup-notfound", "setup-running", "setup-elevate", "setup-advanced", "setup-change",
             "done", "done-waiting", "done-failed", "manage", "manage-disabled", "manage-update", "manage-uninstall", "busy" };
}

bool fake_state(const std::string& name, ViewState* v)
{
    *v = ViewState();
    v->logPath = "C:\\Users\\player\\AppData\\Local\\DishonoredVR\\dishonored_vr_launcher.log";
    if (name == "guide" || name == "guide-zoom") {
        v->det = base_detection(); v->choices = v->det.suggested;
        v->screen = Screen::Guide;
        if (name == "guide-zoom") v->guideZoom = 2.0f;
        return true;
    }
    if (name == "setup-found") {
        v->det = base_detection(); v->choices = v->det.suggested; return true;
    }
    if (name == "setup-steamvr" || name == "setup-controls") {
        v->det = base_detection(); v->choices = v->det.suggested;
        v->choices.runtime = Runtime::SteamVr;
        v->controlsOpen = name == "setup-controls";
        return true;
    }
    if (name == "setup-notfound") {
        v->det = base_detection();
        v->det.gameFound = false; v->det.gameDir.clear();
        v->det.gameNote = "Dishonored (Steam app 205100) is not in any of the 2 Steam libraries. Use Change to point at the folder holding Dishonored.exe (the Steam build; GOG is a different exe and is not supported).";
        v->det.vdxrPresent = false; v->det.activeRuntime = L"steamxr_win32.json";
        v->det.gpu.name = L"NVIDIA GeForce RTX 4060"; v->det.gpu.budgetBytes = 7ull << 30;
        v->det.suggested.runtime = Runtime::SteamVr; v->det.suggested.quality = Quality::Performance; v->det.suggested.pixelPercent = 75.0f;
        v->choices = v->det.suggested; return true;
    }
    if (name == "setup-running") {
        v->det = base_detection(); v->det.running = process::Running::Yes; v->choices = v->det.suggested; return true;
    }
    if (name == "setup-elevate") {
        v->det = base_detection(); v->det.gameWritable = false; v->det.gameWriteErr = ERROR_ACCESS_DENIED; v->det.needsElevation = true;
        v->det.gameDir = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dishonored\\Binaries\\Win32";
        v->choices = v->det.suggested; return true;
    }
    if (name == "setup-advanced") {
        v->det = base_detection(); v->choices = v->det.suggested; v->advancedOpen = true;
        v->choices.quality = Quality::Custom; v->choices.pixelPercent = 110.0f; return true;
    }
    if (name == "setup-change") {
        v->det = installed_detection(); v->choices = v->det.suggested; v->changingSettings = true; return true;
    }
    if (name == "done") {
        v->det = installed_detection(); v->screen = Screen::Done; v->lastOp = "install"; v->report = install_report(false, false); return true;
    }
    if (name == "done-waiting") {
        v->det = installed_detection(); v->det.configExists = false; v->screen = Screen::Done; v->lastOp = "install"; v->report = install_report(true, false); return true;
    }
    if (name == "done-failed") {
        v->det = installed_detection(); v->screen = Screen::Done; v->lastOp = "install"; v->report = install_report(false, true); return true;
    }
    if (name == "manage") {
        v->det = installed_detection(); v->screen = Screen::Manage; return true;
    }
    if (name == "manage-disabled") {
        v->det = installed_detection(); v->det.disabled = true; v->screen = Screen::Manage;
        v->notice = "Support bundle written to C:\\Users\\player\\Desktop\\DishonoredVR Support\\support-20260923-101500-123.zip"; return true;
    }
    if (name == "manage-update") {
        v->det = installed_detection(); v->det.installedSha = v->det.embeddedSha; v->det.record.buildId = "702-g1a2b3c4d"; v->screen = Screen::Manage; return true;
    }
    if (name == "manage-uninstall") {
        v->det = installed_detection(); v->screen = Screen::Manage; v->confirmUninstall = true; return true;
    }
    if (name == "busy") {
        v->det = base_detection(); v->choices = v->det.suggested; v->busy = true; v->busyText = "Installing..."; return true;
    }
    return false;
}

} // namespace dvr::setup
