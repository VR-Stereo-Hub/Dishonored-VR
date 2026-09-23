// tools/installer/model/installer.h - what the installer knows and what it does.
// No ImGui in here: detect() reads the machine into a Detection, the do_*
// functions carry out one operation and return a Report of steps with human
// text, and the screens only draw those. The elevated child and the
// unattended --apply mode run the same functions, so the three paths cannot
// drift.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "model/choices.h"
#include "sys/install_record.h"
#include "sys/process.h"
#include "sys/gpu.h"

namespace dvr::setup {

struct Env {
    std::wstring gameDirOverride;    // --game-dir, or the folder the player browsed to
    std::wstring configDirOverride;  // --config-dir (tests, and the elevated child)
    std::wstring vdxrJsonOverride;   // --vdxr-json
    bool elevated = false;           // this process holds an elevated token
};

struct Detection {
    // the game
    std::wstring gameDir;            // the folder holding Dishonored.exe, or ""
    bool gameFound = false;
    std::string gameNote;            // how it was found, or why not (UTF-8, for the screen)
    process::Running running = process::Running::Unknown;
    bool gameWritable = false; DWORD gameWriteErr = 0;
    std::wstring configDir;          // Documents\My Games\Dishonored\DishonoredGame\Config
    bool configExists = false;
    bool configWritable = true; DWORD configWriteErr = 0;
    bool needsElevation = false;     // a write probe was denied and we are not elevated yet
    bool d3dcompiler = false;        // d3dcompiler_47.dll in the system folder

    // the runtimes on this machine
    bool vdxrPresent = false; std::wstring vdxrJson;
    bool steamvrPresent = false;
    std::wstring activeRuntime;      // the 32-bit ActiveRuntime manifest's file name, or ""
    gpu::Info gpu;

    // what is installed already
    bool d3d9Present = false;
    bool modInstalled = false;       // our d3d9.dll: an install record, or the ini/log/dxvk trace beside it
    bool foreignD3d9 = false;        // a d3d9.dll that is not ours and has no backup yet
    bool backupPresent = false;      // d3d9.dll.dvr-backup
    std::string installedSha;        // sha256 of the game folder's d3d9.dll, "" when absent
    InstallRecord record;
    bool disabled = false;           // disable_vr.txt
    bool iniExists = false; int iniVersion = 0;
    Runtime iniRuntime = Runtime::Auto; std::wstring iniJson; Size iniSize; std::wstring iniDataDir;

    // the payload in this exe
    bool payloadOk = false;
    int embeddedIniVersion = 0;      // [Meta] Version of the embedded ini = the mod's kConfigVersion
    std::string embeddedSha;
    bool embeddedLegacy = false;     // the d3d9.dll carries src/legacy (VR-180): never a play build
    std::string version, buildId, config;
    bool installedIsEmbedded() const { return d3d9Present && installedSha == embeddedSha; }

    Choices suggested;               // preselection for the Set up screen
};

enum class StepStatus { Ok, Skipped, Warn, Failed };
struct StepResult { std::string title; std::string detail; StepStatus status = StepStatus::Ok; };
struct Report {
    std::vector<StepResult> steps;
    bool ok = true;                  // no Failed step
    bool accessDenied = false;       // a Failed step was ERROR_ACCESS_DENIED (elevation may help)
    bool baselineApplied = false;    // the four game-ini values are in place
    bool baselinePending = false;    // the game has never run: no config folder yet
    void add(StepStatus s, std::string title, std::string detail = "");
    void fail(std::string title, DWORD err, std::string prefix = "");
};

Detection detect(const Env& env);
Report do_install(const Env& env, const Detection& det, const Choices& choices);
Report do_update(const Env& env, const Detection& det);          // the DLLs only; ini untouched
Report do_change(const Env& env, const Detection& det, const Choices& choices);   // the five keys only
Report do_baseline(const Env& env, const Detection& det);        // the four game-ini values
Report do_disable(const Env& env, const Detection& det, bool disabled);
Report do_uninstall(const Env& env, const Detection& det, bool deleteIni);

// For the elevated child: one line per step, tab-separated, and a header.
std::string report_to_text(const Report& r);
bool report_from_text(const std::string& text, Report* out);

std::wstring default_config_dir();
constexpr const wchar_t* kGameExe = L"Dishonored.exe";
constexpr const wchar_t* kSteamLaunchUrl = L"steam://rungameid/205100";
constexpr const wchar_t* kReleasesUrl = L"https://github.com/VR-Stereo-Hub/Dishonored-VR/releases";

} // namespace dvr::setup
