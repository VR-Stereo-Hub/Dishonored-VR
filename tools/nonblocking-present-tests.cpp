#include "core/gfx/nonblocking_present.h"
#include <stdio.h>
#include <assert.h>
using dvr::desktop::NonblockingPresent;
int main() {
    NonblockingPresent p;
    int normal = 0, tried = 0;
    HRESULT response = S_OK;
    auto attempt = [&]() { ++tried; return response; };
    auto fallback = [&]() { ++normal; return D3D_OK; };
    assert(p.present(false, attempt, fallback) == S_OK && tried == 0 && normal == 1);
    response = D3DERR_WASSTILLDRAWING;
    assert(p.present(true, attempt, fallback) == S_OK && p.busy == 1 && normal == 1);
    response = D3DERR_DEVICELOST;
    assert(p.present(true, attempt, fallback) == response && p.errors == 1 && normal == 1);
    response = D3DERR_DEVICEHUNG;
    assert(p.present(true, attempt, fallback) == response && normal == 1);
    response = S_PRESENT_OCCLUDED;
    assert(p.present(true, attempt, fallback) == response && p.accepted == 1);
    response = D3DERR_INVALIDCALL;
    assert(p.present(true, attempt, fallback) == S_OK && p.refused && normal == 2);
    int before = tried;
    assert(p.present(true, attempt, fallback) == S_OK && tried == before && normal == 3);
    p = {};
    response = S_OK;
    assert(p.present(true, attempt, fallback) == S_OK && p.accepted == 1 && !p.refused);
    puts("Policy gates, busy, real failures, refusal latch and reset passed.");
    // Load native D3D9 explicitly, never the mod and never the game.
    wchar_t path[MAX_PATH]; GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\d3d9.dll");
    HMODULE lib = LoadLibraryW(path); assert(lib);
    auto create = reinterpret_cast<HRESULT(WINAPI*)(UINT, IDirect3D9Ex**)>(GetProcAddress(lib, "Direct3DCreate9Ex"));
    assert(create);
    IDirect3D9Ex* d3d = nullptr; HRESULT hr = create(D3D_SDK_VERSION, &d3d); assert(SUCCEEDED(hr));
    HWND wnd = CreateWindowExW(0, L"STATIC", L"DVR native presentation validation", WS_OVERLAPPEDWINDOW,
        0, 0, 128, 128, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(wnd);
    D3DPRESENT_PARAMETERS pp = {}; pp.Windowed = TRUE; pp.hDeviceWindow = wnd;
    pp.BackBufferWidth = 128; pp.BackBufferHeight = 128; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9Ex* dev = nullptr;
    hr = d3d->CreateDeviceEx(0, D3DDEVTYPE_HAL, wnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &pp, nullptr, &dev);
    printf("Native CreateDeviceEx: %08lx\n", (unsigned long)hr); assert(SUCCEEDED(hr));
    for (int i = 0; i < 200; ++i) {
        assert(SUCCEEDED(dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff203040, 1, 0)));
        hr = p.present(true,
            [&]() { return dev->PresentEx(nullptr, nullptr, nullptr, nullptr, D3DPRESENT_DONOTWAIT); },
            [&]() { return dev->Present(nullptr, nullptr, nullptr, nullptr); });
        assert(SUCCEEDED(hr));
    }
    printf("Native PresentEx: accepted=%u busy=%u refused=%d errors=%u\n", p.accepted, p.busy, p.refused, p.errors);
    assert(!p.refused && !p.errors);
    dev->Release(); d3d->Release(); DestroyWindow(wnd); FreeLibrary(lib);
    return 0;
}
