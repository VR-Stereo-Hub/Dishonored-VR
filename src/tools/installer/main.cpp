// tools/installer/main.cpp - DishonoredVR-Launcher-v1.0.0.exe (VR-198).
//
//   DishonoredVR-Launcher-v1.0.0.exe                          the window
//   DishonoredVR-Launcher-v1.0.0.exe --game-dir <dir>         ... against that game folder
//   DishonoredVR-Launcher-v1.0.0.exe --apply --op install --game-dir <dir> [--config-dir <dir>]
//        [--runtime vdxr|steamvr|auto] [--quality performance|balanced|quality|custom]
//        [--percent <n>] [--vdxr-json <path>] [--delete-ini]      unattended; prints the steps
//        [--mirror on|off] [--physical-crouch on|off] [--hide-rain-overlay on|off]
//        [--overwrite-settings] [--dpad-modifier 0|1|2|4]
//        [--dpad-flip on|off] [--pause-chord on|off] [--snap-turn on|off]  omitted preferences stay
//   DishonoredVR-Launcher-v1.0.0.exe --render <state>|all <out.bmp>|<dir> [--scale <f>]
//                                                   draw a screen headless (tools/installer-render.ps1)
//   --elevated-apply ... --result <file>            what the window runs under UAC; not for hand use
//
// The log is %LOCALAPPDATA%\DishonoredVR\dishonored_vr_launcher.log (previous run
// in .prev.log), the same folder the mod keeps its harness files in.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include "app/win32_app.h"
#include "app/offscreen.h"
#include "model/fake_states.h"
#include "sys/fs.h"
#include "sys/process.h"
#include "sys/support.h"
#include "sys/updates.h"
#include "core/util/log.h"
#include "dvr_version.h"

using namespace dvr::setup;

namespace {
struct Args {
    std::vector<std::wstring> v;
    bool has(const wchar_t* flag) const { for (const auto& a : v) if (fs::iequals(a, flag)) return true; return false; }
    std::wstring value(const wchar_t* flag, const wchar_t* def = L"") const
    {
        for (size_t i = 0; i + 1 < v.size(); ++i) if (fs::iequals(v[i], flag)) return v[i + 1];
        return def;
    }
};

void init_log()
{
    std::wstring dir = fs::env(L"DVR_DATA_DIR");
    if (dir.empty()) dir = fs::join(fs::known_folder(FOLDERID_LocalAppData), L"DishonoredVR");
    DWORD err = 0;
    fs::make_dir(dir, &err);
    dvr::log::init(fs::narrow_acp(dir).c_str(), "dishonored_vr_launcher");
    DVR_INFO("Dishonored VR Launcher %s (%s, %s) elevated=%d cmdline=%s", DVR_VERSION, DVR_BUILD_ID, DVR_BUILD_CONFIG,
             process::is_elevated(), fs::narrow(GetCommandLineW()).c_str());
}

bool d3dcompiler_present()
{
    return fs::is_file(fs::join(fs::system_dir(), L"d3dcompiler_47.dll"));
}

int render_mode(const Args& args)
{
    process::attach_parent_console();
    const std::wstring what = args.value(L"--render");
    std::wstring out;
    for (size_t i = 0; i + 2 < args.v.size(); ++i) if (fs::iequals(args.v[i], L"--render")) out = args.v[i + 2];
    const float scale = (float)_wtof(args.value(L"--scale", L"1").c_str());
    if (what.empty() || out.empty()) { printf("usage: --render <state>|all <out.bmp>|<dir> [--scale f]\n"); return 1; }
    std::vector<std::string> names;
    if (fs::iequals(what, L"all")) names = fake_state_names(); else names.push_back(fs::narrow(what));
    int rc = 0;
    for (const auto& name : names) {
        ViewState v;
        if (!fake_state(name, &v)) { printf("unknown state: %s\n", name.c_str()); rc = 1; continue; }
        const std::wstring path = names.size() > 1 || fs::is_dir(out) ? fs::join(out, fs::widen(name) + L".bmp") : out;
        if (names.size() > 1 || fs::is_dir(out)) { DWORD e = 0; fs::make_dir(out, &e); }
        std::string why;
        if (app::render_offscreen(v, scale > 0.1f ? scale : 1.0f, path, &why)) printf("wrote %s\n", fs::narrow(path).c_str());
        else { printf("FAILED %s: %s\n", name.c_str(), why.c_str()); rc = 1; }
    }
    fflush(stdout);
    return rc;
}

int headless_mode(const Args& args, Env env)
{
    app::HeadlessArgs h;
    h.op = fs::narrow(args.value(L"--op", L"install"));
    parse_runtime(args.value(L"--runtime", L"auto"), &h.choices.runtime);
    parse_quality(args.value(L"--quality", L"balanced"), &h.choices.quality);
    const std::wstring pct = args.value(L"--percent");
    if (!pct.empty()) h.choices.pixelPercent = (float)_wtof(pct.c_str());
    if (h.choices.quality != Quality::Custom) h.choices.pixelPercent = percent_for_quality(h.choices.quality, h.choices.pixelPercent);
    const std::wstring size = args.value(L"--size");   // WxH, an alternative to --percent
    if (!size.empty()) {
        unsigned w = 0, hh = 0;
        if (swscanf_s(size.c_str(), L"%ux%u", &w, &hh) == 2 && w && hh) h.choices.keep({ w, hh });
    }
    for (int i = 0; i < PreferenceCount; ++i) {
        const std::wstring value = args.value(kPreferences[i].flag);
        if (!args.has(kPreferences[i].flag)) continue;
        if (i == Modifier) {
            if (value != L"0" && value != L"1" && value != L"2" && value != L"4") {
                DVR_ERROR("launcher: --dpad-modifier requires 0, 1, 2 or 4"); return 1;
            }
            h.choices.preferences[i] = _wtoi(value.c_str());
        } else {
            if (value != L"on" && value != L"off") {
                DVR_ERROR("launcher: %s requires on or off", fs::narrow(kPreferences[i].flag).c_str()); return 1;
            }
            const bool on = value == L"on";
            h.choices.preferences[i] = kPreferences[i].inverted ? !on : on;
        }
    }
    h.choices.vdxrJson = args.value(L"--vdxr-json");
    h.choices.overwriteSettings = !args.has(L"--keep-settings");
    h.deleteIni = args.has(L"--delete-ini");
    h.resultFile = args.value(L"--result");
    env.elevated = args.has(L"--elevated-apply");
    if (h.resultFile.empty()) process::attach_parent_console();
    return app::run_headless(env, h);
}
int complete_update(const Args& args) {
    const auto source=fs::module_path(),target=args.value(L"--complete-update");
    std::string error; DWORD systemError=0;
    const bool replaced=updates::replace_launcher(source,target,fs::narrow(args.value(L"--sha256")),
                                                  wcstoul(args.value(L"--parent-pid",L"0").c_str(),nullptr,10),&error,&systemError);
    if(!replaced) {
        // A launcher saved in Program Files can need UAC independently of the game.
        if(systemError==ERROR_ACCESS_DENIED && !process::is_elevated() && !args.has(L"--no-restart")) {
            DWORD code=0,err=0;std::wstring retry;
            for(const auto& arg:args.v)retry+=L" "+process::quote_arg(arg);
            if(process::run_self_elevated_wait(retry,&code,&err))return (int)code;
        }
        const auto result=args.value(L"--result");
        if(!result.empty())fs::write_file_atomic(result,error.data(),error.size(),nullptr);
        if(!args.has(L"--no-restart"))MessageBoxW(nullptr,fs::widen(error).c_str(),L"Launcher update could not finish",MB_ICONERROR);
        return 2;
    }
    if(args.has(L"--no-restart"))return 0; // host replacement test, no GUI or game
    std::wstring resume=L"--resume-update";
    for(const wchar_t* key:{L"--game-dir",L"--config-dir"})if(!args.value(key).empty())resume+=L" "+std::wstring(key)+L" "+process::quote_arg(args.value(key));
    if(args.has(L"--keep-settings"))resume+=L" --keep-settings";
    // Existing shortcuts migrate to a stable path; future versions replace this file.
    const auto stableDir=fs::join(fs::known_folder(FOLDERID_LocalAppData),L"DishonoredVR\\Launcher");
    fs::make_dir(stableDir,nullptr);
    const auto stable=fs::join(stableDir,L"DishonoredVR-Launcher.exe");
    bool stableReady=fs::iequals(stable,target);
    if(!stableReady) {
        std::vector<uint8_t> bytes;
        stableReady=fs::read_file(source,&bytes,nullptr) && fs::write_file_atomic(stable,bytes.data(),bytes.size(),nullptr);
    }
    if(stableReady)for(const auto* folder:{&FOLDERID_Desktop,&FOLDERID_Programs}) {
        const auto link=fs::join(fs::known_folder(*folder),L"Dishonored VR Launcher.lnk");
        if(fs::is_file(link))process::write_shortcut(link,stable,L"",nullptr);
    }
    if(!process::open_unelevated(target,resume)) {
        MessageBoxW(nullptr,L"The launcher was updated, but Windows could not reopen it. Open the launcher again to install the mod update.",L"Launcher updated",MB_ICONINFORMATION);
        return 3;
    }
    return 0;
}

}

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    Args args;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; ++i) args.v.push_back(argv[i]);
        LocalFree(argv);
    }
    init_log();
    Env env;
    env.gameDirOverride = args.value(L"--game-dir");
    env.configDirOverride = args.value(L"--config-dir");
    env.vdxrJsonOverride = args.value(L"--vdxr-json");

    env.updateOnStart=args.has(L"--resume-update");
    env.keepSettings=args.has(L"--keep-settings");
    int rc = 0;
    if(args.has(L"--complete-update")) {
        rc=complete_update(args);
    } else if (args.has(L"--collect-logs")) {
        process::attach_parent_console();
        std::string notice;
        rc = support::collect(env.gameDirOverride, args.value(L"--support-out"), false, &notice) ? 0 : 1;
        const auto result = args.value(L"--result");
        if (!result.empty()) fs::write_file_atomic(result, notice.data(), notice.size(), nullptr);
        printf("%s\n", notice.c_str()); fflush(stdout);
    } else if (args.has(L"--render")) {
        rc = render_mode(args);
    } else if (args.has(L"--apply") || args.has(L"--elevated-apply")) {
        rc = headless_mode(args, env);
    } else {
        if (!d3dcompiler_present()) {
            // imgui_impl_dx11 needs it to build its shaders; the DLL is delay-loaded so
            // this message can be shown instead of a loader error box.
            MessageBoxW(nullptr,
                L"d3dcompiler_47.dll is missing from Windows (the SysWOW64 folder).\n\n"
                L"Both this launcher and the mod need it. It comes with Windows 10 and 11 and with the DirectX End-User Runtime; run Windows Update or install that runtime, then start the launcher again.",
                L"Dishonored VR Launcher", MB_ICONERROR);
            rc = 4;
        } else {
            rc = app::run_gui(hinst, env);
        }
    }
    DVR_INFO("setup: exit %d", rc);
    dvr::log::shutdown();
    CoUninitialize();
    return rc;
}
