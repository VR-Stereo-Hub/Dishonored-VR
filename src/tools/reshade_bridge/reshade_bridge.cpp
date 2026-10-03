// DishonoredVR_ReShade.addon32
// Minimal ReShade 6.8 (API 20) bridge. ABI checked against upstream v6.8.0:
// https://github.com/crosire/reshade/blob/v6.8.0/include/reshade.hpp
// https://github.com/crosire/reshade/blob/v6.8.0/include/reshade_events.hpp
//
// Loaded by ReShade from the game directory. It registers only the
// addon_event::reshade_present callback (event id 75) and forwards that event to
// exports in Dishonored VR's primary d3d9.dll.
//
// This deliberately treats ReShade's effect_runtime pointer as opaque, so the
// add-on does not need to vendor the entire ReShade SDK header tree.

#include <Windows.h>
#include <stdint.h>

extern "C" __declspec(dllexport) const char* NAME =
    "Dishonored VR post-ReShade XR bridge";
extern "C" __declspec(dllexport) const char* DESCRIPTION =
    "Runs Dishonored VR's XR capture after ReShade effects/overlay and before native D3D9 Present.";

namespace {

constexpr uint32_t kReShadeApiVersion = 20;      // ReShade 6.8
constexpr uint32_t kEventReShadePresent = 75;   // addon_event::reshade_present

using PFN_RegisterAddon   = bool (__cdecl*)(void*, uint32_t);
using PFN_UnregisterAddon = void (__cdecl*)(void*);
using PFN_RegisterEvent   = void (__cdecl*)(uint32_t, void*);
using PFN_UnregisterEvent = void (__cdecl*)(uint32_t, void*);

using PFN_SetActive = void (__cdecl*)(int);
using PFN_PresentCallback = void (__cdecl*)();

HMODULE g_self = nullptr;
HMODULE g_reshade = nullptr;
PFN_UnregisterAddon g_unregisterAddon = nullptr;
PFN_UnregisterEvent g_unregisterEvent = nullptr;
PFN_SetActive g_setActive = nullptr;
PFN_PresentCallback g_presentCallback = nullptr;

void __cdecl on_reshade_present(void* /*effect_runtime*/)
{
    if (g_presentCallback)
        g_presentCallback();
}

HMODULE find_reshade()
{
    // RESH-HOOK2 loads the renamed x86 module under this exact name.
    HMODULE h = GetModuleHandleW(L"ReShade32.dll");
    if (h && GetProcAddress(h, "ReShadeRegisterAddon"))
        return h;

    // Fallback for a conventional ReShade D3D9 proxy name, useful while testing.
    h = GetModuleHandleW(L"d3d9.dll");
    if (h && GetProcAddress(h, "ReShadeRegisterAddon"))
        return h;

    return nullptr;
}

bool attach_bridge()
{
    g_reshade = find_reshade();
    if (!g_reshade)
        return false;

    auto registerAddon =
        reinterpret_cast<PFN_RegisterAddon>(GetProcAddress(g_reshade, "ReShadeRegisterAddon"));
    auto registerEvent =
        reinterpret_cast<PFN_RegisterEvent>(GetProcAddress(g_reshade, "ReShadeRegisterEvent"));
    g_unregisterAddon =
        reinterpret_cast<PFN_UnregisterAddon>(GetProcAddress(g_reshade, "ReShadeUnregisterAddon"));
    g_unregisterEvent =
        reinterpret_cast<PFN_UnregisterEvent>(GetProcAddress(g_reshade, "ReShadeUnregisterEvent"));

    if (!registerAddon || !registerEvent || !g_unregisterAddon || !g_unregisterEvent)
        return false;

    if (!registerAddon(g_self, kReShadeApiVersion))
        return false;

    HMODULE dvr = GetModuleHandleW(L"d3d9.dll");
    if (!dvr) {
        g_unregisterAddon(g_self);
        return false;
    }

    g_setActive = reinterpret_cast<PFN_SetActive>(
        GetProcAddress(dvr, "DvrReShadeAddonSetActive"));
    g_presentCallback = reinterpret_cast<PFN_PresentCallback>(
        GetProcAddress(dvr, "DvrReShadePresentCallback"));

    if (!g_setActive || !g_presentCallback) {
        g_unregisterAddon(g_self);
        g_setActive = nullptr;
        g_presentCallback = nullptr;
        return false;
    }

    registerEvent(kEventReShadePresent, reinterpret_cast<void*>(&on_reshade_present));
    g_setActive(1);
    return true;
}

void detach_bridge()
{
    if (g_setActive)
        g_setActive(0);

    if (g_unregisterEvent)
        g_unregisterEvent(kEventReShadePresent, reinterpret_cast<void*>(&on_reshade_present));

    if (g_unregisterAddon)
        g_unregisterAddon(g_self);

    g_presentCallback = nullptr;
    g_setActive = nullptr;
    g_unregisterEvent = nullptr;
    g_unregisterAddon = nullptr;
    g_reshade = nullptr;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_self = hinst;
        DisableThreadLibraryCalls(hinst);
        if (!attach_bridge())
            return FALSE; // ReShade treats a failed add-on load as non-fatal to the game.
        break;

    case DLL_PROCESS_DETACH:
        detach_bridge();
        break;
    }
    return TRUE;
}
