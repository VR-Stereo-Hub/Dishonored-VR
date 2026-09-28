#pragma once
#include <d3d9.h>
#include <d3d11.h>

namespace dvr::capture::interop {
// D3D9 -> D3D11 interop uses a one-level DEFAULT render-target texture.
// Keep its owner until the D3D11 view and level-zero surface are released.
// https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource
struct Image {
    IDirect3DTexture9* owner = nullptr;
    IDirect3DSurface9* surface = nullptr;
    ID3D11Texture2D* texture = nullptr;
    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    ~Image() { reset(); }
    void reset() {
        if (texture) { texture->Release(); texture = nullptr; }
        if (surface) { surface->Release(); surface = nullptr; }
        if (owner) { owner->Release(); owner = nullptr; }
    }
};
inline D3DFORMAT preferred_format(D3DFORMAT backbuffer) {
    return backbuffer == D3DFMT_X8R8G8B8 ? D3DFMT_A8R8G8B8 : backbuffer;
}
struct Result { HRESULT hr; const char* step; };
template<class Device9, class Device11>
inline Result create(Device9* dev, Device11* dev11,
                     UINT w, UINT h, D3DFORMAT fmt, Image& out) {
    out.reset();
    HANDLE handle = nullptr;
    HRESULT hr = dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET,
                                   fmt, D3DPOOL_DEFAULT, &out.owner, &handle);
    const char* step = "CreateTexture";
    if (SUCCEEDED(hr) && !out.owner) hr = E_POINTER;
    if (SUCCEEDED(hr) && !handle) { hr = E_HANDLE; step = "CreateTexture returned a null sharing handle (OpenSharedResource not called)"; }
    if (SUCCEEDED(hr)) {
        step = "GetSurfaceLevel";
        hr = out.owner->GetSurfaceLevel(0, &out.surface);
        if (SUCCEEDED(hr) && !out.surface) hr = E_POINTER;
    }
    if (SUCCEEDED(hr)) {
        step = "OpenSharedResource";
        hr = dev11->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)&out.texture);
        if (SUCCEEDED(hr) && !out.texture) hr = E_POINTER;
    }
    if (FAILED(hr)) out.reset();
    return {hr, step};
}
} // namespace dvr::capture::interop
