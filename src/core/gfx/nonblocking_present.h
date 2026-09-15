// Desktop-only submission policy; no capture, pose or headset fence changes.
#pragma once
#include <d3d9.h>
#include <stdint.h>
namespace dvr::desktop {
struct NonblockingPresent {
    bool refused = false;
    uint32_t attempts = 0, accepted = 0, busy = 0, fallback = 0, errors = 0;
    template<class TryPresent, class NormalPresent>
    HRESULT present(bool eligible, TryPresent tryPresent, NormalPresent normalPresent) {
        if (!eligible || refused) { ++fallback; return normalPresent(); }
        ++attempts;
        const HRESULT hr = tryPresent();
        if (hr == D3DERR_WASSTILLDRAWING) { ++busy; return D3D_OK; }
        if (hr == D3DERR_INVALIDCALL) {
            refused = true;
            ++fallback;
            return normalPresent();
        }
        if (FAILED(hr)) ++errors;
        else ++accepted;
        return hr; // Device loss and other real failures must reach the game.
    }
};
} // namespace dvr::desktop
