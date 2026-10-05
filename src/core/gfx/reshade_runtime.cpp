#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/reshade_runtime.h"
#include "core/util/log.h"
#include <memory>
#include <string>
#include <psapi.h>
#include "../../../third_party/reshade/reshade_api.hpp"
namespace dvr::reshade_runtime {
namespace {
using Create = bool (__cdecl*)(int, void*, void*, void*, const char*, void**);
using Update = void (__cdecl*)(void*);
Create createRuntime = nullptr;
Update updateRuntime = nullptr, destroyRuntime = nullptr;
void* runtime = nullptr;
bool manualMode = false, attempted = false, failed = false;
std::string config;
std::wstring gameDirectory, vrIni;
bool available = false, controlsCompatible = false;
char loadFailure[512] = {};
// Scoped process environment value: set for one call, previous value put back after.
struct ScopedEnv {
    const wchar_t* name; std::wstring previous; bool had = false, ok = false;
    ScopedEnv(const wchar_t* n, const wchar_t* value) : name(n) {
        const DWORD size = GetEnvironmentVariableW(n, nullptr, 0);
        had = size != 0;
        if (had) { previous.assign(size, L'\0'); GetEnvironmentVariableW(n, previous.data(), size); }
        ok = SetEnvironmentVariableW(n, value) != FALSE;
    }
    ~ScopedEnv() { SetEnvironmentVariableW(name, had ? previous.c_str() : nullptr); }
};
// A module already in the process that exports ReShadeVersion: ReShade refuses to load a
// second copy of itself (source/dll_main.cpp), so this names the one in the way.
std::wstring other_reshade() {
    HMODULE mods[1024]; DWORD need = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need)) return {};
    for (DWORD i = 0; i < need / sizeof(HMODULE) && i < _countof(mods); ++i) {
        if (!GetProcAddress(mods[i], "ReShadeVersion")) continue;
        wchar_t path[MAX_PATH] = {}; GetModuleFileNameW(mods[i], path, MAX_PATH);
        return path;
    }
    return {};
}
using GetConfig = bool (__cdecl*)(void*, void*, const char*, const char*, char*, size_t*);
using SetConfig = void (__cdecl*)(void*, void*, const char*, const char*, const char*);
GetConfig getConfig = nullptr;
SetConfig setConfig = nullptr;
// The native PURE D3D9 device rewrites its dispatch table during BeginStateBlock.
// ReShade uses that API during setup. Retain only this module's detours, leaving
// the native runtime's other dispatch changes intact. D3D9 base has 119 methods.
struct DeviceHooks {
    IDirect3DDevice9* device = nullptr;
    void* entries[119] = {};
    void capture(IDirect3DDevice9* dev) {
        device = dev;
        MEMORY_BASIC_INFORMATION own = {};
        VirtualQuery(reinterpret_cast<const void*>(&render), &own, sizeof(own));
        void** table = *reinterpret_cast<void***>(dev);
        for (unsigned i = 0; i < 119; ++i) {
            MEMORY_BASIC_INFORMATION entry = {};
            entries[i] = VirtualQuery(table[i], &entry, sizeof(entry)) &&
                own.AllocationBase && entry.AllocationBase == own.AllocationBase ? table[i] : nullptr;
        }
    }
    void restore() const {
        if (!device) return;
        void** table = *reinterpret_cast<void***>(device);
        unsigned restored = 0;
        for (unsigned i = 0; i < 119; ++i) {
            if (!entries[i] || table[i] == entries[i]) continue;
            DWORD previous = 0;
            if (!VirtualProtect(&table[i], sizeof(void*), PAGE_EXECUTE_READWRITE, &previous)) {
                DVR_WARN("reshade: cannot restore device hook slot=%u error=%lu", i, GetLastError());
                continue;
            }
            table[i] = entries[i];
            DWORD ignored = 0; VirtualProtect(&table[i], sizeof(void*), previous, &ignored);
            ++restored;
        }
        if (restored) DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 4,
            "reshade: restored %u device hooks after native state-block table rewrite", restored);
    }
} deviceHooks;
struct RestoreHooks { ~RestoreHooks() { deviceHooks.restore(); } };
struct Scope { bool prior = inside; Scope() { inside = true; } ~Scope() { inside = prior; } };
}
bool manual() { return manualMode; }
bool installed() { return available; }
bool controls_supported() { return controlsCompatible; }
const char* load_failure() { return loadFailure[0] ? loadFailure : nullptr; }
const wchar_t* directory() { return gameDirectory.c_str(); }
reshade::api::effect_runtime* api() { return manualMode && controlsCompatible ? static_cast<reshade::api::effect_runtime*>(runtime) : nullptr; }
bool enabled_next_start() { return !vrIni.empty() && GetPrivateProfileIntW(L"ReShade", L"Enabled", 0, vrIni.c_str()) != 0; }
bool set_enabled_next_start(bool enabled) {
    if (vrIni.empty()) return false;
    // The supported VR integration retains mirror suppression. Legacy remains an explicit INI option.
    if (enabled && !WritePrivateProfileStringW(L"ReShade",L"ManualRuntime",L"1",vrIni.c_str())) return false;
    const bool ok = WritePrivateProfileStringW(L"ReShade",L"Enabled",enabled ? L"1" : L"0",vrIni.c_str()) != FALSE;
    if (ok) DVR_INFO("reshade: next launch %s (F10; restart required)", enabled ? "enabled" : "disabled");
    else DVR_WARN("reshade: could not save next-launch setting error=%lu", GetLastError());
    return ok;
}
bool performance_mode() {
    char value[16] = {}; size_t size = sizeof(value);
    return getConfig && runtime && getConfig(nullptr,runtime,"GENERAL","PerformanceMode",value,&size) && value[0] == '1';
}
bool set_performance_mode(bool enabled) {
    if (!api() || !setConfig) return false;
    api()->save_current_preset();
    setConfig(nullptr,runtime,"GENERAL","PerformanceMode",enabled ? "1" : "0");
    api()->reload_effect_next_frame(nullptr);
    DVR_INFO("reshade: performance mode %s; effect reload queued (F10)",enabled ? "on" : "off");
    return true;
}
// Pre-release audit: effects on/off for an A/B plan row (`reshade effects on|off`). Session only: the
// preset is not saved, so the next launch has the player's own state. Present thread.
int set_effects(int on) {
    auto* rt = api();
    if (!rt) return -1;
    if (on >= 0) rt->set_effects_state(on != 0);
    return rt->get_effects_state() ? 1 : 0;
}
void load_optional() {
    if (attempted) return;
    attempted = true;
    wchar_t exe[32768] = {};
    const DWORD n = GetModuleFileNameW(nullptr, exe, _countof(exe));
    if (!n || n >= _countof(exe)) return;
    std::wstring dir(exe); const auto slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return;
    dir.resize(slash + 1);
    gameDirectory = dir; vrIni = dir + L"dishonored_vr.ini";
    manualMode = GetPrivateProfileIntW(L"ReShade", L"ManualRuntime", 1, vrIni.c_str()) != 0;
    const auto dll = dir + L"ReShade32.dll";
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        DVR_INFO("reshade: optional ReShade32.dll absent"); return;
    }
    available = true;
    if (!enabled_next_start()) { DVR_INFO("reshade: installed but disabled; enable it in F10 > ReShade and restart"); return; }
    // What ReShade's own DllMain will check, read BEFORE the load so a refusal can be
    // explained: it refuses (LoadLibrary 1114) when no ReShade.ini exists for the exe and
    // it is not loaded under a proxy name, and when another ReShade is already loaded.
    const bool iniHere = GetFileAttributesW((dir + L"ReShade.ini").c_str()) != INVALID_FILE_ATTRIBUTES;
    const std::wstring other = other_reshade();
    HMODULE module = nullptr;
    DWORD error = 0;
    {
        // Official ReShade 6.8 API: include/reshade.hpp and source/addon.cpp.
        // Disable graphics interception before loading; otherwise it wraps both the
        // game's D3D9 and the VR compositor's D3D11 devices and forces native Present.
        std::unique_ptr<ScopedEnv> hook;
        if (manualMode) {
            hook.reset(new ScopedEnv(L"RESHADE_DISABLE_GRAPHICS_HOOK", L"1"));
            if (!hook->ok) { DVR_WARN("reshade: cannot disable graphics hooks; optional effects not loaded"); return; }
        }
        // [ReShade] Enabled=1 is the explicit opt-in that check exists to require, so skip
        // it: a ReShade.ini that is missing or deleted must not silently disable ReShade.
        ScopedEnv loadingCheck(L"RESHADE_DISABLE_LOADING_CHECK", L"1");
        module = LoadLibraryW(dll.c_str());
        error = GetLastError();
    }
    if (!module) {
        char why[256];
        if (!other.empty())
            _snprintf_s(why, _TRUNCATE, "another ReShade is already loaded from %ls, and ReShade refuses a second copy", other.c_str());
        else if (error == ERROR_DLL_INIT_FAILED)
            _snprintf_s(why, _TRUNCATE, "ReShade refused to start (ReShade.ini beside the game: %s)", iniHere ? "present" : "MISSING");
        else if (error == ERROR_MOD_NOT_FOUND || error == ERROR_FILE_NOT_FOUND)
            _snprintf_s(why, _TRUNCATE, "a file ReShade32.dll needs is missing");
        else if (error == ERROR_BAD_EXE_FORMAT)
            _snprintf_s(why, _TRUNCATE, "ReShade32.dll is not a 32-bit DLL");
        else
            _snprintf_s(why, _TRUNCATE, "Windows could not load ReShade32.dll");
        _snprintf_s(loadFailure, _TRUNCATE, "error %lu: %s", error, why);
        DVR_WARN("reshade: load failed error=%lu - %s. ReShade stays off this launch; restarting will not change it. "
                 "Run Install ReShade in the launcher again to repair the ReShade files.", error, why);
        return;
    }
    DVR_INFO("reshade: loaded mode=%s", manualMode ? "manual (desktop mirror policy preserved)" : "hook (legacy native Present)");
    getConfig = reinterpret_cast<GetConfig>(GetProcAddress(module,"ReShadeGetConfigValue"));
    setConfig = reinterpret_cast<SetConfig>(GetProcAddress(module,"ReShadeSetConfigValue"));
    if (!manualMode) return;
    // Negotiate the public C++ API before exposing the pinned API-20 vtable to F10.
    using RegisterAddon = bool (__cdecl*)(void*, uint32_t);
    auto registerAddon = reinterpret_cast<RegisterAddon>(GetProcAddress(module,"ReShadeRegisterAddon"));
    HMODULE owner = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&render), &owner);
    controlsCompatible = owner && registerAddon && registerAddon(owner,20);
    if (!controlsCompatible) DVR_WARN("reshade: public API 20 unavailable; F10 shader controls disabled");
    createRuntime = reinterpret_cast<Create>(GetProcAddress(module,"ReShadeCreateEffectRuntime"));
    updateRuntime = reinterpret_cast<Update>(GetProcAddress(module,"ReShadeUpdateAndPresentEffectRuntime"));
    destroyRuntime = reinterpret_cast<Update>(GetProcAddress(module,"ReShadeDestroyEffectRuntime"));
    if (!createRuntime || !updateRuntime || !destroyRuntime) {
        failed = true; _snprintf_s(loadFailure, _TRUNCATE, "this ReShade32.dll lacks the effect-runtime API (ReShade 6.8 add-on build required)");
        DVR_WARN("reshade: manual API unavailable; effects disabled"); return;
    }
    const auto ini = dir + L"ReShade.ini";
    const int bytes = WideCharToMultiByte(CP_UTF8,0,ini.c_str(),-1,nullptr,0,nullptr,nullptr);
    config.resize(bytes);
    WideCharToMultiByte(CP_UTF8,0,ini.c_str(),-1,config.data(),bytes,nullptr,nullptr);
}
void reset() {
    Scope scope;
    // Clear before destruction: releasing runtime resources can reenter device Release.
    void* old = runtime; runtime = nullptr;
    if (old) { destroyRuntime(old); deviceHooks.restore(); }
    deviceHooks = {};
    failed = false;
}
void render(IDirect3DDevice9* device) {
    if (!manualMode || failed || !createRuntime) return;
    Scope scope;
    if (!deviceHooks.device) deviceHooks.capture(device);
    RestoreHooks restoreHooks;
    if (!runtime) {
        IDirect3DSwapChain9* swap = nullptr;
        if (FAILED(device->GetSwapChain(0, &swap))) return;
        // device_api::d3d9 = 0x9000 in ReShade 6.8 reshade_api_device.hpp.
        const bool ok = createRuntime(0x9000, device, nullptr, swap, config.c_str(), &runtime);
        swap->Release();
        if (!ok) {
            failed = true; _snprintf_s(loadFailure, _TRUNCATE, "ReShade loaded but could not create its effect runtime (see ReShade.log beside the game)");
            DVR_WARN("reshade: manual runtime creation failed; effects disabled until device reset"); return;
        }
        loadFailure[0] = 0;
        DVR_INFO("reshade: manual runtime ready; effects before VR capture, no native Present required");
    }
    // ReShade's hooked D3D9 path uses the same BeginScene/on_present/EndScene order.
    const HRESULT hr = device->BeginScene();
    if (FAILED(hr)) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000, "reshade: BeginScene failed 0x%08lx; skipping effects", (unsigned long)hr);
        return;
    }
    updateRuntime(runtime);
    device->EndScene();
}
}
