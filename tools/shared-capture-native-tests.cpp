// Native graphics-only integration test; no game, OpenXR or visible window.
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/gfx/shared_capture_texture.h"
static void require(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
static void check(HRESULT hr, const char* label) {
    if (FAILED(hr)) std::fprintf(stderr, "%s: 0x%08lx\n", label, (unsigned long)hr);
    require(SUCCEEDED(hr), label);
}
// Simulate the reported success-without-handle driver response using a real
// allocated owner. The helper must release it and must not call D3D11.
struct MissingHandle {
    IDirect3DDevice9* device;
    HRESULT CreateTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt,
                          D3DPOOL pool, IDirect3DTexture9** out, HANDLE* handle) {
        const HRESULT hr = device->CreateTexture(w,h,levels,usage,fmt,pool,out,nullptr);
        *handle = nullptr; return hr;
    }
};
struct RefuseOpen {
    unsigned calls = 0;
    HRESULT OpenSharedResource(HANDLE, REFIID, void**) { ++calls; return E_ACCESSDENIED; }
};
int main() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc = {}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=instance; wc.lpszClassName=L"DvrSharedCaptureTest";
    require(RegisterClassW(&wc)!=0,"window class");
    HWND wnd=CreateWindowW(wc.lpszClassName,L"Capture test",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,instance,nullptr);
    require(wnd!=nullptr,"hidden window");
    HMODULE lib=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(lib!=nullptr,"system D3D9");
    using CreateFn=HRESULT (WINAPI*)(UINT,IDirect3D9Ex**);
    auto create=reinterpret_cast<CreateFn>(GetProcAddress(lib,"Direct3DCreate9Ex"));
    IDirect3D9Ex* api=nullptr; require(create!=nullptr,"9Ex entry"); check(create(D3D_SDK_VERSION,&api),"9Ex");
    LUID luid={}; check(api->GetAdapterLUID(0,&luid),"D3D9 adapter LUID");
    IDXGIFactory1* factory=nullptr; check(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&factory),"DXGI");
    IDXGIAdapter1* adapter=nullptr;
    for (UINT i=0;;++i) {
        IDXGIAdapter1* candidate=nullptr;
        if (factory->EnumAdapters1(i,&candidate)==DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc={}; candidate->GetDesc1(&desc);
        if (desc.AdapterLuid.HighPart==luid.HighPart && desc.AdapterLuid.LowPart==luid.LowPart) { adapter=candidate; break; }
        candidate->Release();
    }
    require(adapter!=nullptr,"same D3D11 adapter");
    ID3D11Device* dev11=nullptr; ID3D11DeviceContext* ctx=nullptr;
    check(D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                           nullptr,0,D3D11_SDK_VERSION,&dev11,nullptr,&ctx),"D3D11");
    D3DPRESENT_PARAMETERS pp={}; pp.Windowed=TRUE; pp.hDeviceWindow=wnd; pp.BackBufferWidth=64; pp.BackBufferHeight=64;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8; pp.BackBufferCount=1; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9Ex* dev=nullptr;
    check(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,wnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,nullptr,&dev),"D3D9 device");
    using namespace dvr::capture::interop;
    require(preferred_format(D3DFMT_X8R8G8B8)==D3DFMT_A8R8G8B8,"X8 probe prefers A8 like slots");
    Image image;
    MissingHandle noHandle{dev}; RefuseOpen refused;
    auto result=dvr::capture::interop::create(&noHandle,&refused,64,64,D3DFMT_A8R8G8B8,image);
    require(result.hr==E_HANDLE && refused.calls==0 && !image.owner && !image.surface && !image.texture,"null handle refuses before open and cleans owner");
    result=dvr::capture::interop::create(dev,&refused,64,64,D3DFMT_A8R8G8B8,image);
    require(result.hr==E_ACCESSDENIED && refused.calls==1 && !image.owner && !image.surface && !image.texture,"open failure releases partial resources");
    result=dvr::capture::interop::create(dev,dev11,64,64,D3DFMT_UNKNOWN,image);
    require(FAILED(result.hr) && !image.owner && !image.surface && !image.texture,"unsupported format fails cleanly");
    unsigned checked=0;
    for (unsigned cycle=0;cycle<3;++cycle) {
        const UINT w=cycle==1?2750:64, h=cycle==1?2850:64;
        result=dvr::capture::interop::create(dev,dev11,w,h,D3DFMT_A8R8G8B8,image);
        check(result.hr,result.step);
        ID3D11ShaderResourceView* srv=nullptr; check(dev11->CreateShaderResourceView(image.texture,nullptr,&srv),"shared SRV");
        D3D11_TEXTURE2D_DESC td={}; image.texture->GetDesc(&td);
        require(td.Width==w && td.Height==h && td.MipLevels==1 && td.SampleDesc.Count==1,"opened dimensions/mips/MSAA");
        td.Usage=D3D11_USAGE_STAGING; td.BindFlags=0; td.MiscFlags=0; td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* readback=nullptr; check(dev11->CreateTexture2D(&td,nullptr,&readback),"pixel readback");
        IDirect3DSurface9* source=nullptr; check(dev->CreateRenderTarget(w,h,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&source,nullptr),"X8 source");
        IDirect3DQuery9* fence=nullptr; check(dev->CreateQuery(D3DQUERYTYPE_EVENT,&fence),"producer fence");
        for (unsigned frame=0;frame<6;++frame) {
            const DWORD color=frame&1?0x00123456:0x00654321;
            check(dev->ColorFill(source,nullptr,color),"synthetic frame");
            check(dev->StretchRect(source,nullptr,image.surface,nullptr,D3DTEXF_NONE),"X8->A8 shared blit");
            check(fence->Issue(D3DISSUE_END),"issue fence");
            HRESULT done=S_FALSE; const ULONGLONG deadline=GetTickCount64()+3000;
            do { done=fence->GetData(nullptr,0,D3DGETDATA_FLUSH); if(done==S_FALSE) Sleep(1); }
            while(done==S_FALSE && GetTickCount64()<deadline);
            require(done==S_OK,"producer completed");
            ctx->CopyResource(readback,image.texture);
            D3D11_MAPPED_SUBRESOURCE mapped={}; check(ctx->Map(readback,0,D3D11_MAP_READ,0,&mapped),"D3D11 read");
            const UINT xs[]={0,w/2,w-1}, ys[]={0,h/2,h-1};
            for(UINT y:ys) for(UINT x:xs) {
                const DWORD pixel=*(const DWORD*)((const char*)mapped.pData+y*mapped.RowPitch+x*4);
                require((pixel&0xffffff)==color,"independent D3D11 pixel matches this D3D9 frame"); ++checked;
            }
            ctx->Unmap(readback,0);
        }
        fence->Release(); source->Release(); readback->Release(); srv->Release(); image.reset();
        ctx->ClearState(); ctx->Flush();
        check(dev->ResetEx(&pp,nullptr),"all texture/surface owners released before reset");
    }
    dev->Release(); ctx->Release(); dev11->Release(); adapter->Release(); factory->Release(); api->Release();
    FreeLibrary(lib); DestroyWindow(wnd); UnregisterClassW(wc.lpszClassName,instance);
    std::printf("PASS: %u pixel checks at 64x64 and 2750x2850, three resets, null-handle/open/format failure cleanup\n",checked);
}
