#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/reshade_runtime.h"
#include "core/util/log.h"
#include <string>
namespace dvr::reshade_runtime {
namespace {
using Create = bool (__cdecl*)(int, void*, void*, void*, const char*, void**);
using Update = void (__cdecl*)(void*);
Create createRuntime = nullptr;
Update updateRuntime = nullptr, destroyRuntime = nullptr;
void* runtime = nullptr;
bool manualMode = false, attempted = false, failed = false;
std::string config;
struct Scope { bool prior = inside; Scope() { inside = true; } ~Scope() { inside = prior; } };
}
bool manual() { return manualMode; }
void load_optional() {
    if (attempted) return;
    attempted = true;
    wchar_t exe[32768] = {};
    const DWORD n = GetModuleFileNameW(nullptr, exe, _countof(exe));
    if (!n || n >= _countof(exe)) return;
    std::wstring dir(exe); const auto slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return;
    dir.resize(slash + 1);
    manualMode = GetPrivateProfileIntW(L"ReShade", L"ManualRuntime", 0, (dir + L"dishonored_vr.ini").c_str()) != 0;
    const auto dll = dir + L"ReShade32.dll";
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        DVR_INFO("reshade: optional ReShade32.dll absent"); return;
    }
    // Official ReShade 6.8 API: include/reshade.hpp and source/addon.cpp.
    // Disable graphics interception before loading; otherwise it wraps both the
    // game's D3D9 and the VR compositor's D3D11 devices and forces native Present.
    constexpr auto env = L"RESHADE_DISABLE_GRAPHICS_HOOK";
    const DWORD size = GetEnvironmentVariableW(env, nullptr, 0);
    std::wstring previous(size, L'\0');
    if (size) GetEnvironmentVariableW(env, previous.data(), size);
    if (manualMode && !SetEnvironmentVariableW(env, L"1")) {
        DVR_WARN("reshade: cannot disable graphics hooks; optional effects not loaded"); return;
    }
    HMODULE module = LoadLibraryW(dll.c_str());
    const DWORD error = GetLastError();
    if (manualMode) SetEnvironmentVariableW(env, size ? previous.c_str() : nullptr);
    if (!module) { DVR_WARN("reshade: load failed error=%lu", error); return; }
    DVR_INFO("reshade: loaded mode=%s", manualMode ? "manual (desktop mirror policy preserved)" : "hook (legacy native Present)");
    if (!manualMode) return;
    createRuntime = reinterpret_cast<Create>(GetProcAddress(module,"ReShadeCreateEffectRuntime"));
    updateRuntime = reinterpret_cast<Update>(GetProcAddress(module,"ReShadeUpdateAndPresentEffectRuntime"));
    destroyRuntime = reinterpret_cast<Update>(GetProcAddress(module,"ReShadeDestroyEffectRuntime"));
    if (!createRuntime || !updateRuntime || !destroyRuntime) {
        failed = true; DVR_WARN("reshade: manual API unavailable; effects disabled"); return;
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
    if (old) destroyRuntime(old);
    failed = false;
}
void render(IDirect3DDevice9* device) {
    if (!manualMode || failed || !createRuntime) return;
    Scope scope;
    if (!runtime) {
        IDirect3DSwapChain9* swap = nullptr;
        if (FAILED(device->GetSwapChain(0, &swap))) return;
        // device_api::d3d9 = 0x9000 in ReShade 6.8 reshade_api_device.hpp.
        const bool ok = createRuntime(0x9000, device, nullptr, swap, config.c_str(), &runtime);
        swap->Release();
        if (!ok) { failed = true; DVR_WARN("reshade: manual runtime creation failed; effects disabled until device reset"); return; }
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
