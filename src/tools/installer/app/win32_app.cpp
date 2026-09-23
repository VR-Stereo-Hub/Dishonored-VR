// tools/installer/app/win32_app.cpp - see win32_app.h.
#include "app/win32_app.h"
#include "app/offscreen.h"
#include "ui/screens.h"
#include "model/view_state.h"
#include "sys/fs.h"
#include "sys/process.h"
#include "sys/resources.h"
#include "sys/steam.h"
#include "payload_ids.h"
#include "core/ui/ovl_ui.h"
#include "core/util/log.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <shobjidl.h>
#include <shellscalingapi.h>
#include <shellapi.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <stdio.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace dvr::setup::app {

namespace {

struct App {
    HWND hwnd = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* swap = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    float scale = 1.0f;
    int framesPending = 3;
    bool quit = false;

    Env env;
    ViewState view;

    // the worker
    std::thread worker;
    std::mutex mu;
    bool workerDone = false;
    Report workerReport;
    std::string workerOp;
    Detection workerDet;
    bool workerHasDet = false;
    std::string workerNotice;
    DWORD lastPoll = 0;
    DWORD lastProcessPoll = 0;
};
App* g_app = nullptr;
void dispatch(App& a, UiAction action);

void create_rtv(App& a)
{
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(a.swap->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
        a.dev->CreateRenderTargetView(back, nullptr, &a.rtv);
        back->Release();
    }
}
void release_rtv(App& a) { if (a.rtv) { a.rtv->Release(); a.rtv = nullptr; } }

void apply_scale(App& a, float scale)
{
    a.scale = scale;
    // ScaleAllSizes is cumulative, so start from the theme's own metrics each time.
    dvr::ovl::apply_theme();
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleDpi = scale;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    App* a = g_app;
    switch (msg) {
    case WM_GETMINMAXINFO:
        if (a) {
            auto* bounds = reinterpret_cast<MINMAXINFO*>(lp);
            bounds->ptMinTrackSize.x = (LONG)(760 * a->scale);
            bounds->ptMinTrackSize.y = (LONG)(640 * a->scale);
        }
        return 0;
    case WM_SIZE:
        if (a && a->swap && wp != SIZE_MINIMIZED) {
            release_rtv(*a);
            a->swap->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            create_rtv(*a);
            a->framesPending = 3;
        }
        return 0;
    case WM_DPICHANGED:
        if (a) {
            const RECT* r = (const RECT*)lp;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            apply_scale(*a, (float)HIWORD(wp) / 96.0f);
            a->framesPending = 3;
        }
        return 0;
    case WM_CLOSE:
        if (a) a->quit = true;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool create_window(App& a, HINSTANCE hinst)
{
    WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc; wc.hInstance = hinst; wc.lpszClassName = L"DishonoredVRLauncher";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);
    const DWORD style = WS_OVERLAPPEDWINDOW;
    POINT cursor; GetCursorPos(&cursor);
    HMONITOR mon = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(mon, &mi);
    UINT dpi = 96;
    { UINT dx = 96, dy = 96; if (SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy))) dpi = dx; }
    a.scale = dpi / 96.0f;
    RECT r = { 0, 0, (LONG)(kLogicalWidth * a.scale + 0.5f), (LONG)(kLogicalHeight * a.scale + 0.5f) };
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    const int w = r.right - r.left, h = r.bottom - r.top;
    const int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    const int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
    std::wstring title = L"Dishonored VR Launcher " + fs::widen(a.view.det.version);
    if (a.view.det.config != "RelWithDebInfo") title += L" [" + fs::widen(a.view.det.config) + L"]";
    a.hwnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(), style, x, y, w, h, nullptr, nullptr, hinst, nullptr);
    if (!a.hwnd) return false;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = a.hwnd;
    sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &a.swap, &a.dev, &fl, &a.ctx);
    if (FAILED(hr)) {
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; sd.BufferCount = 1;
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &a.swap, &a.dev, &fl, &a.ctx);
    }
    if (FAILED(hr)) hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &a.swap, &a.dev, &fl, &a.ctx);
    if (FAILED(hr)) { DVR_ERROR("setup: no D3D11 device for the window (hr 0x%08lx)", (unsigned long)hr); return false; }
    create_rtv(a);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    dvr::ovl::load_fonts();
    dvr::ovl::load_art(a.dev);
    ui::load_guide(a.dev);
    apply_scale(a, a.scale);
    ImGui_ImplWin32_Init(a.hwnd);
    ImGui_ImplDX11_Init(a.dev, a.ctx);
    ShowWindow(a.hwnd, SW_SHOWNORMAL);
    UpdateWindow(a.hwnd);
    return true;
}

void render_frame(App& a)
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const UiAction action = ui::draw(a.view);
    ImGui::Render();
    const float clear[4] = { 0x12 / 255.0f, 0x14 / 255.0f, 0x17 / 255.0f, 1 };
    a.ctx->OMSetRenderTargets(1, &a.rtv, nullptr);
    a.ctx->ClearRenderTargetView(a.rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    a.swap->Present(1, 0);
    if (action != UiAction::None) a.framesPending = 3;
    // the action is handled after Present so a click never delays the frame
    dispatch(a, action);
}

// ---- the operations -------------------------------------------------------------

std::wstring child_args(const Detection& det, const char* op, const Choices& c, bool deleteIni, const std::wstring& resultFile)
{
    std::wstring s = L"--elevated-apply --op " + fs::widen(op);
    s += L" --game-dir " + process::quote_arg(det.gameDir);
    if (!det.configDir.empty()) s += L" --config-dir " + process::quote_arg(det.configDir);
    s += L" --runtime " + fs::widen(runtime_token(c.runtime));
    s += L" --quality " + fs::widen(quality_token(c.quality));
    s += fs::wformat(L" --percent %.2f", c.pixelPercent);
    if (c.exact.w && c.exact.h) s += fs::wformat(L" --size %ux%u", c.exact.w, c.exact.h);
    const std::wstring json = c.vdxrJson.empty() ? det.vdxrJson : c.vdxrJson;
    if (!json.empty()) s += L" --vdxr-json " + process::quote_arg(json);
    for (int i = 0; i < PreferenceCount; ++i) {
        if (c.preferences[i] < 0) continue;
        s += L" " + std::wstring(kPreferences[i].flag) + L" ";
        s += i == Modifier ? std::to_wstring(c.preferences[i])
            : ((kPreferences[i].inverted ? !c.preferences[i] : c.preferences[i]) ? L"on" : L"off");
    }
    if (deleteIni) s += L" --delete-ini";
    s += L" --result " + process::quote_arg(resultFile);
    return s;
}

Report run_op(const Env& env, const Detection& det, const std::string& op, const Choices& c, bool deleteIni)
{
    if (op == "install") return do_install(env, det, c);
    if (op == "update") return do_update(env, det);
    if (op == "change") return do_change(env, det, c);
    if (op == "baseline") return do_baseline(env, det);
    if (op == "disable") return do_disable(env, det, true);
    if (op == "enable") return do_disable(env, det, false);
    if (op == "uninstall") return do_uninstall(env, det, deleteIni);
    Report r; r.add(StepStatus::Failed, "Unknown operation", op); return r;
}

// Runs `op` on the worker: in this process when it can write, else in an
// elevated headless copy of this exe. Re-detects afterwards so the screen is
// current.
void start_op(App& a, const std::string& op, const char* busyText)
{
    a.view.busy = true; a.view.busyText = busyText; a.view.notice.clear();
    a.view.lastOp = op;
    const Env env = a.env;
    const Detection det = a.view.det;
    const Choices choices = a.view.choices;
    const bool deleteIni = a.view.deleteIni;
    const bool elevate = det.needsElevation && (op != "baseline" || !det.configWritable);
    if (a.worker.joinable()) a.worker.join();
    a.worker = std::thread([&a, env, det, op, choices, deleteIni, elevate]() {
        Report report;
        std::string notice;
        if (elevate) {
            const std::wstring resultFile = fs::join(fs::temp_dir(), L"DishonoredVR-Launcher-result-" + fs::timestamp_local() + L".txt");
            DWORD code = 0, err = 0;
            DVR_INFO("setup: elevating for %s", op.c_str());
            if (!process::run_self_elevated_wait(child_args(det, op.c_str(), choices, deleteIni, resultFile), &code, &err)) {
                if (err == ERROR_CANCELLED) notice = "Administrator rights were not granted, so nothing was changed.";
                else notice = "Could not start the elevated installer: " + fs::narrow(fs::win_error_text(err));
            } else {
                std::vector<uint8_t> bytes;
                if (fs::read_file(resultFile, &bytes, nullptr) && report_from_text(std::string((const char*)bytes.data(), bytes.size()), &report)) {
                    fs::delete_file(resultFile, nullptr);
                } else {
                    report.add(StepStatus::Failed, "The elevated installer left no result", fs::format("It exited with code %lu.", (unsigned long)code));
                }
            }
        } else {
            report = run_op(env, det, op, choices, deleteIni);
        }
        Detection fresh = detect(env);
        std::lock_guard<std::mutex> lock(a.mu);
        a.workerReport = report; a.workerOp = op; a.workerDet = fresh; a.workerHasDet = true; a.workerNotice = notice;
        a.workerDone = true;
    });
}

void browse(App& a)
{
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || !dlg) return;
    DWORD opts = 0; dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(L"Choose the folder holding Dishonored.exe");
    if (SUCCEEDED(dlg->Show(a.hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                const std::wstring chosen = path;
                CoTaskMemFree(path);
                const std::wstring dir = steam::normalise_game_dir(chosen);
                a.env.gameDirOverride = dir.empty() ? chosen : dir;
                a.view.det = detect(a.env);
                if (!a.view.changingSettings) a.view.choices = a.view.det.suggested;
                a.view.notice.clear();
            }
            item->Release();
        }
    }
    dlg->Release();
}

void collect_support(App& a)
{
    a.view.busy = true; a.view.busyText = "Collecting the support bundle..."; a.view.notice.clear();
    const std::wstring gameDir = a.view.det.gameDir;
    if (a.worker.joinable()) a.worker.join();
    a.worker = std::thread([&a, gameDir]() {
        std::string notice;
        const resources::Blob script = resources::rcdata(IDR_COLLECT_SUPPORT);
        const std::wstring dir = fs::join(fs::temp_dir(), L"DishonoredVR-Launcher");
        const std::wstring ps1 = fs::join(dir, L"collect-support.ps1");
        const std::wstring out = fs::join(dir, L"collect-support.out.txt");
        DWORD err = 0;
        fs::make_dir(dir, &err);
        if (!script.ok() || !fs::write_file_atomic(ps1, script.data, script.size, &err)) {
            notice = "Could not unpack the support collector: " + fs::narrow(fs::win_error_text(err));
        } else {
            const std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " + process::quote_arg(ps1) + L" -GameDir " + process::quote_arg(gameDir);
            DWORD code = 0;
            if (!process::run_wait(cmd, &code, &err, 120000, out)) {
                notice = "Could not run PowerShell: " + fs::narrow(fs::win_error_text(err));
            } else {
                std::vector<uint8_t> bytes; fs::read_file(out, &bytes, nullptr);
                std::string text((const char*)bytes.data(), bytes.size());
                const size_t at = text.find("Support ZIP: ");
                if (code == 0 && at != std::string::npos) {
                    size_t end = text.find_first_of("\r\n", at);
                    notice = "Support bundle written to " + text.substr(at + 13, end == std::string::npos ? std::string::npos : end - at - 13) + " (a folder opened on it).";
                } else {
                    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
                    const size_t tail = text.find_last_of('\n');
                    notice = fs::format("The support collector failed (exit %lu): %s", (unsigned long)code, text.substr(tail == std::string::npos ? 0 : tail + 1).c_str());
                }
            }
        }
        DVR_INFO("setup: support bundle: %s", notice.c_str());
        std::lock_guard<std::mutex> lock(a.mu);
        a.workerOp = "support"; a.workerNotice = notice; a.workerHasDet = false;
        a.workerDone = true;
    });
}

void create_shortcut(App& a, bool desktop)
{
    // Keep shortcuts independent of Downloads, archives and temporary build folders.
    const std::wstring base = fs::known_folder(FOLDERID_LocalAppData);
    const std::wstring menu = fs::known_folder(desktop ? FOLDERID_Desktop : FOLDERID_Programs);
    if (base.empty() || menu.empty()) {
        a.view.notice = "Windows could not locate your user shortcut folders."; return;
    }
    const std::wstring dir = fs::join(base, L"DishonoredVR\\Launcher");
    const std::wstring target = fs::join(dir, L"DishonoredVR-Launcher.exe");
    const std::wstring source = fs::module_path();
    DWORD err = 0;
    if (!fs::make_dir(dir, &err)) {
        a.view.notice = "Could not create the launcher folder: " + fs::narrow(fs::win_error_text(err)); return;
    }
    if (!fs::iequals(source, target)) {
        std::vector<uint8_t> bytes;
        if (!fs::read_file(source, &bytes, &err) || !fs::write_file_atomic(target, bytes.data(), bytes.size(), &err)) {
            a.view.notice = "Could not save the launcher copy: " + fs::narrow(fs::win_error_text(err)); return;
        }
    }
    const std::wstring link = fs::join(menu, L"Dishonored VR Launcher.lnk");
    const std::wstring args = a.view.det.gameDir.empty() ? L"" : L"--game-dir " + process::quote_arg(a.view.det.gameDir);
    if (process::write_shortcut(link, target, args, &err))
        a.view.notice = desktop ? "Desktop shortcut created." : "Start menu shortcut created.";
    else a.view.notice = "Could not create the shortcut: " + fs::narrow(fs::win_error_text(err));
    DVR_INFO("launcher: shortcut %s: %s", fs::narrow(link).c_str(), a.view.notice.c_str());
}

void finish_worker(App& a)
{
    std::lock_guard<std::mutex> lock(a.mu);
    if (!a.workerDone) return;
    a.workerDone = false;
    a.view.busy = false;
    if (a.workerHasDet) a.view.det = a.workerDet;
    a.view.notice = a.workerNotice;
    if (a.workerOp == "support") { a.framesPending = 3; return; }
    if (a.workerOp == "baseline") {
        // the Done screen's own follow-up: fold the result into the list
        for (const auto& s : a.workerReport.steps) a.view.report.steps.push_back(s);
        a.view.report.baselinePending = a.workerReport.baselinePending && !a.workerReport.baselineApplied;
        if (a.workerReport.baselineApplied) {
            a.view.report.baselineApplied = true; a.view.report.baselinePending = false;
        }
        a.framesPending = 3;
        return;
    }
    if (a.workerReport.steps.empty() && !a.workerNotice.empty()) {   // UAC declined: stay where we were
        a.framesPending = 3;
        return;
    }
    a.view.report = a.workerReport;
    if (a.workerOp == "disable" || a.workerOp == "enable") {
        a.view.notice = a.workerReport.steps.empty() ? "" : a.workerReport.steps.back().title + ". " + a.workerReport.steps.back().detail;
        a.view.screen = Screen::Manage;
    } else {
        a.view.screen = Screen::Done;
        a.view.changingSettings = false;
        a.view.confirmUninstall = false;
    }
    a.framesPending = 3;
}

void dispatch(App& a, UiAction action)
{
    if (action == UiAction::None) return;
    ViewState& v = a.view;
    switch (action) {
    case UiAction::Install:
        start_op(a, v.changingSettings ? "change" : "install", v.changingSettings ? "Writing the settings..." : "Installing...");
        break;
    case UiAction::Browse: browse(a); break;
    case UiAction::Rescan:
        v.det = detect(a.env); v.notice.clear();
        if (v.screen == Screen::Setup && !v.changingSettings) v.choices = v.det.suggested;
        break;
    case UiAction::Launch:
        if (process::is_running(kGameExe) != process::Running::No) {
            v.notice = "Dishonored is already running, or its process could not be checked."; break;
        }
        DVR_INFO("launcher: Steam launch requested (app 205100)");
        if (!process::open_unelevated(kSteamLaunchUrl)) v.notice = "Could not reach Steam from here; launch Dishonored from your Steam library.";
        else v.notice = "Asked Steam to launch Dishonored. Put the headset on.";
        break;
    case UiAction::DesktopShortcut: create_shortcut(a, true); break;
    case UiAction::StartShortcut: create_shortcut(a, false); break;
    case UiAction::ShowGuide: v.guideReturn = v.screen; v.screen = Screen::Guide; break;
    case UiAction::BackFromGuide: v.screen = v.guideReturn; break;
    case UiAction::Close: a.quit = true; break;
    case UiAction::Update: start_op(a, "update", "Updating the mod..."); break;
    case UiAction::ChangeSettings:
        v.changingSettings = true; v.screen = Screen::Setup; v.choices = v.det.suggested; v.notice.clear(); break;
    case UiAction::CancelChange:
        v.changingSettings = false; v.screen = Screen::Manage; v.notice.clear(); break;
    case UiAction::ToggleDisable: start_op(a, v.det.disabled ? "enable" : "disable", v.det.disabled ? "Enabling VR..." : "Disabling VR..."); break;
    case UiAction::CollectSupport: collect_support(a); break;
    case UiAction::Uninstall: v.confirmUninstall = true; v.deleteIni = false; break;
    case UiAction::CancelUninstall: v.confirmUninstall = false; break;
    case UiAction::ConfirmUninstall: start_op(a, "uninstall", "Removing the mod..."); break;
    case UiAction::ApplyBaseline: start_op(a, "baseline", "Applying the game settings..."); break;
    case UiAction::OpenReleases: process::open_unelevated(kReleasesUrl); break;
    case UiAction::OpenGameFolder: if (v.det.gameFound) process::open_unelevated(v.det.gameDir); break;
    case UiAction::OpenLog: process::open_unelevated(fs::widen(v.logPath)); break;
    default: break;
    }
}

} // namespace

int run_gui(HINSTANCE hinst, const Env& env)
{
    App a;
    g_app = &a;
    a.env = env;
    a.view.det = detect(env);
    a.view.choices = a.view.det.suggested;
    a.view.screen = a.view.det.modInstalled ? Screen::Manage : Screen::Setup;
    a.view.logPath = dvr::log::path();
    if (!create_window(a, hinst)) {
        MessageBoxW(nullptr, L"Direct3D 11 could not be started, so the launcher cannot draw its window.\nThe mod itself would not run either; check the graphics driver.", L"Dishonored VR Launcher", MB_ICONERROR);
        return 1;
    }
    while (!a.quit) {
        const DWORD timeout = (a.view.busy || a.framesPending > 0) ? 16 : (a.view.report.baselinePending && a.view.screen == Screen::Done ? 500 : 1000);
        MsgWaitForMultipleObjectsEx(0, nullptr, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { a.quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            a.framesPending = 3;
        }
        if (a.quit) break;
        finish_worker(a);
        // Process changes must wake disabled Play/Update buttons after the game
        // exits. This cheap read does not re-detect or reset unsaved UI choices.
        const DWORD processNow = GetTickCount();
        if (!a.view.busy && processNow - a.lastProcessPoll >= 1000) {
            a.lastProcessPoll = processNow;
            const auto running = process::is_running(kGameExe);
            if (running != a.view.det.running) {
                a.view.det.running = running;
                a.framesPending = 3;
            }
        }
        // the Done screen waits for the game's first run
        if (a.view.screen == Screen::Done && a.view.report.baselinePending && !a.view.busy) {
            const DWORD now = GetTickCount();
            if (now - a.lastPoll > 2000) {
                a.lastPoll = now;
                const bool exists = fs::is_dir(a.view.det.configDir);
                if (exists != a.view.det.configExists) { a.view.det.configExists = exists; a.framesPending = 3; }
                if (exists && process::is_running(kGameExe) == process::Running::No) {
                    a.view.det = detect(a.env);
                    start_op(a, "baseline", "Applying the game settings...");
                }
            }
        }
        if (a.framesPending > 0 || a.view.busy) {
            render_frame(a);
            if (a.framesPending > 0) --a.framesPending;
        }
    }
    if (a.worker.joinable()) a.worker.join();
    ui::release_guide();
    dvr::ovl::release_art();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    release_rtv(a);
    if (a.swap) a.swap->Release();
    if (a.ctx) a.ctx->Release();
    if (a.dev) a.dev->Release();
    DestroyWindow(a.hwnd);
    g_app = nullptr;
    return 0;
}

int run_headless(const Env& env, const HeadlessArgs& args)
{
    const Detection det = detect(env);
    const Report r = run_op(env, det, args.op, args.choices, args.deleteIni);
    const std::string text = report_to_text(r);
    if (!args.resultFile.empty()) {
        DWORD err = 0;
        fs::write_file_atomic(args.resultFile, text.data(), text.size(), &err);
    } else {
        for (const auto& s : r.steps) {
            const char* tag = s.status == StepStatus::Ok ? "ok  " : s.status == StepStatus::Skipped ? "skip" : s.status == StepStatus::Warn ? "warn" : "FAIL";
            printf("%s  %s\n", tag, s.title.c_str());
            if (!s.detail.empty()) printf("      %s\n", s.detail.c_str());
        }
        printf("%s\n", r.ok ? "done" : "FAILED");
        fflush(stdout);
    }
    return r.ok ? 0 : (r.accessDenied ? 3 : 2);
}

} // namespace dvr::setup::app
