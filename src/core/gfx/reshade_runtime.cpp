#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/reshade_runtime.h"
#include "core/util/log.h"
#include <memory>
#include <string>
#include <psapi.h>
#include <vector>
#include "core/gfx/reshade_ini.h"
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
// [ReShade] LoadAllEffects: 0 (default) = ReShade loads only the effects the selected preset has
// switched on, so F10 lists those and nothing else, effects load faster and a 32-bit game keeps
// its memory. 1 = every effect in the shader folders is compiled and listed.
bool loadAll = false;
// ReShade reads its configuration ONCE, when a runtime object is constructed, and writes its
// own copy back when that object is destroyed (source/runtime.cpp, 6.8.0: load_config in the
// constructor, save_config in the destructor). So a value handed to ReShadeSetConfigValue on a
// live runtime changes nothing this session and is overwritten at exit. A change is applied by
// destroying the runtime, editing ReShade.ini, and creating a new one; `recreate` asks for that.
bool recreate = false, verify = false;
char liveNote[200] = {};
bool read_text(const std::wstring& path, std::string* out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {}; bool ok = GetFileSizeEx(f, &size) && size.QuadPart <= 4 * 1024 * 1024;
    if (ok) { out->resize((size_t)size.QuadPart); DWORD got = 0; ok = out->empty() || (ReadFile(f, out->data(), (DWORD)out->size(), &got, nullptr) && got == out->size()); }
    CloseHandle(f); return ok;
}
bool write_text(const std::wstring& path, const std::string& text) {
    const std::wstring staged = path + L".dvr-new";
    HANDLE f = CreateFileW(staged.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0; const bool ok = WriteFile(f, text.data(), (DWORD)text.size(), &put, nullptr) && put == text.size();
    CloseHandle(f);
    if (!ok || !MoveFileExW(staged.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(staged.c_str()); return false; }
    return true;
}
bool is_dir(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
// Make ReShade.ini say what dishonored_vr.ini asks for. Called before ReShade32.dll is loaded
// (nothing of ReShade has the file cached yet) and between two runtimes. Only these keys are
// touched; an existing ReShade.ini keeps everything else, and its own search paths stay first.
void prepare_config(const char* when) {
    const std::wstring ini = gameDirectory + L"ReShade.ini";
    std::string text;
    if (!read_text(ini, &text)) { DVR_INFO("reshade: ReShade.ini not readable (%s); nothing prepared", when); return; }
    const std::string before = text;
    loadAll = GetPrivateProfileIntW(L"ReShade", L"LoadAllEffects", 0, vrIni.c_str()) != 0;
    const int performance = (int)GetPrivateProfileIntW(L"ReShade", L"PerformanceMode", -1, vrIni.c_str());
    const bool skipChanged = reshade_ini::set(text, "GENERAL", "SkipLoadingDisabledEffects", loadAll ? "0" : "1");
    const bool perfChanged = (performance == 0 || performance == 1) && reshade_ini::set(text, "GENERAL", "PerformanceMode", performance ? "1" : "0");
    int effects = 0, textures = 0;
    const std::wstring root = gameDirectory + L"dvr-reshade-shaders\\";
    if (is_dir(root)) {
        // The folders the launcher installs, and the player's own. A package that is not on disk
        // is not added; the custom folders are created so an imported shader is always found.
        CreateDirectoryW((root + L"custom").c_str(), nullptr);
        CreateDirectoryW((root + L"custom\\Shaders").c_str(), nullptr);
        CreateDirectoryW((root + L"custom\\Textures").c_str(), nullptr);
        struct Pack { const wchar_t* dir; const char* path; };
        const Pack fx[] = { { L"standard\\Shaders", ".\\dvr-reshade-shaders\\standard\\Shaders" }, { L"sweetfx\\Shaders\\SweetFX", ".\\dvr-reshade-shaders\\sweetfx\\Shaders\\SweetFX" },
                            { L"prod80\\Shaders", ".\\dvr-reshade-shaders\\prod80\\Shaders" }, { L"custom\\Shaders", ".\\dvr-reshade-shaders\\custom\\Shaders\\**" } };
        const Pack tex[] = { { L"standard\\Textures", ".\\dvr-reshade-shaders\\standard\\Textures" }, { L"sweetfx\\Textures\\SweetFX", ".\\dvr-reshade-shaders\\sweetfx\\Textures\\SweetFX" },
                             { L"prod80\\Textures", ".\\dvr-reshade-shaders\\prod80\\Textures" }, { L"custom\\Textures", ".\\dvr-reshade-shaders\\custom\\Textures\\**" } };
        std::vector<std::string> wantFx, wantTex;
        for (const auto& p : fx) if (is_dir(root + p.dir)) wantFx.push_back(p.path);
        for (const auto& p : tex) if (is_dir(root + p.dir)) wantTex.push_back(p.path);
        effects = reshade_ini::add_list_items(text, "GENERAL", "EffectSearchPaths", wantFx);
        textures = reshade_ini::add_list_items(text, "GENERAL", "TextureSearchPaths", wantTex);
    }
    if (text == before) {
        DVR_INFO("reshade: ReShade.ini already as asked (%s): %s, search paths complete", when,
                 loadAll ? "every installed effect is loaded" : "only the selected preset's effects are loaded");
        return;
    }
    // One backup of the file as it was before this mod first changed it.
    const std::wstring backup = ini + L".before-dvr.bak";
    if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES) CopyFileW(ini.c_str(), backup.c_str(), TRUE);
    const bool ok = write_text(ini, text);
    DVR_LOG(DVR_CAT, ok ? ::dvr::log::Level::Info : ::dvr::log::Level::Warn,
        "reshade: ReShade.ini %s (%s): SkipLoadingDisabledEffects=%d%s ([ReShade] LoadAllEffects=%d)%s; search paths added: %d effect, %d texture. "
        "Nothing else in the file was touched; the original is ReShade.ini.before-dvr.bak",
        ok ? "updated" : "COULD NOT BE WRITTEN", when, loadAll ? 0 : 1, skipChanged ? " (changed)" : "", (int)loadAll,
        perfChanged ? (performance ? ", PerformanceMode=1 (changed)" : ", PerformanceMode=0 (changed)") : "", effects, textures);
}
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
// What the RUNNING runtime read at its construction, not what was last asked for.
static bool running_flag(const char* key) {
    char value[16] = {}; size_t size = sizeof(value);
    return getConfig && runtime && getConfig(nullptr,runtime,"GENERAL",key,value,&size) && value[0] == '1';
}
bool performance_mode() { return running_flag("PerformanceMode"); }
bool load_all_effects() { return runtime ? !running_flag("SkipLoadingDisabledEffects") : loadAll; }
const char* live_note() { return liveNote[0] ? liveNote : nullptr; }
// Both save the request in dishonored_vr.ini (applied to ReShade.ini before ReShade loads on
// every launch, so a restart always honours it) and ask for the runtime to be rebuilt now.
static bool request(const wchar_t* key, bool on, const char* what) {
    if (vrIni.empty() || !WritePrivateProfileStringW(L"ReShade", key, on ? L"1" : L"0", vrIni.c_str())) {
        DVR_WARN("reshade: could not save %s (dishonored_vr.ini not writable, error %lu)", what, GetLastError()); return false;
    }
    if (api()) api()->save_current_preset();
    recreate = runtime != nullptr; liveNote[0] = 0;
    DVR_INFO("reshade: %s %s (F10, saved); %s", what, on ? "on" : "off",
             recreate ? "the effect runtime is rebuilt on the next frame so ReShade reads it" : "applied when ReShade next starts");
    return true;
}
bool set_performance_mode(bool enabled) { return request(L"PerformanceMode", enabled, "performance mode"); }
bool set_load_all_effects(bool enabled) { return request(L"LoadAllEffects", enabled, "load every installed effect"); }
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
    loadAll = GetPrivateProfileIntW(L"ReShade", L"LoadAllEffects", 0, vrIni.c_str()) != 0;
    if (!enabled_next_start()) { DVR_INFO("reshade: installed but disabled; enable it in F10 > ReShade and restart"); return; }
    prepare_config("before ReShade loads");
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
    if (recreate && runtime) {
        // The old runtime writes its own settings to ReShade.ini as it goes; ours go in after.
        void* old = runtime; runtime = nullptr;
        destroyRuntime(old); deviceHooks.restore();
        prepare_config("F10 change, between two runtimes");
        verify = true;
    }
    recreate = false;
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
        if (verify) {
            // A verified write is not an honoured one: ask the NEW runtime what it read.
            verify = false;
            const int performance = (int)GetPrivateProfileIntW(L"ReShade", L"PerformanceMode", -1, vrIni.c_str());
            const bool skipOk = running_flag("SkipLoadingDisabledEffects") == !loadAll;
            const bool perfOk = performance < 0 || running_flag("PerformanceMode") == (performance == 1);
            if (skipOk && perfOk) liveNote[0] = 0;
            else _snprintf_s(liveNote, _TRUNCATE, "ReShade kept its old setting for now. It applies the next time Dishonored starts.");
            DVR_LOG(DVR_CAT, skipOk && perfOk ? ::dvr::log::Level::Info : ::dvr::log::Level::Warn,
                "reshade: rebuilt runtime reads SkipLoadingDisabledEffects=%d (asked %d) PerformanceMode=%d (asked %d; -1 = not asked) - %s",
                (int)running_flag("SkipLoadingDisabledEffects"), loadAll ? 0 : 1, (int)running_flag("PerformanceMode"), performance,
                skipOk && perfOk ? "HONOURED" : "NOT honoured live; ReShade.ini holds the request and the next launch applies it before ReShade loads");
        }
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
