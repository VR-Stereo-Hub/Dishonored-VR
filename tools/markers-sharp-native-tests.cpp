// Production marker transport on native D3D9Ex/D3D11; no game or XR.
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cmath>
#include "../src/core/gfx/blit_quad.cpp"
#undef DVR_CAT
#include "../src/core/gfx/markers_sharp.cpp"
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT]={};
void write(Cat,Level,const char* fmt,...) {va_list ap;va_start(ap,fmt);vprintf(fmt,ap);puts("");va_end(ap);}
}
static bool reduced=true;
namespace dvr::dlss {
int mode(){return reduced?ModeDlaa:ModeOff;}
bool sr_output_for(uint32_t w,uint32_t h,uint32_t* x,uint32_t* y){*x=w*2;*y=h*2;return reduced;}
}
namespace dvr::stereo {const char* active_name(){return "reentry";} bool reentry_family_active(){return true;}}
static bool afwClean=false;   // VR-39 run 15: AFW's clean sources, which redirect the markers at the render size
namespace dvr::afw {bool clean_wanted(){return afwClean;}}
namespace dvr::frame {
HRESULT orig_set_render_target(IDirect3DDevice9* d,DWORD i,IDirect3DSurface9* rt){return d->SetRenderTarget(i,rt);}
HRESULT orig_set_depth_stencil(IDirect3DDevice9* d,IDirect3DSurface9* ds){return d->SetDepthStencilSurface(ds);}
IDirect3DSurface9* game_depth_stencil(bool* known){*known=true;return nullptr;}
}
static void require(bool ok,const char* name){if(!ok){printf("FAIL %s\n",name);exit(1);}}
static void check(HRESULT hr,const char* name){if(FAILED(hr)){printf("%s hr=%08lx\n",name,(unsigned long)hr);exit(1);}}
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
    IDirect3DSurface9* game=nullptr;check(dev->GetRenderTarget(0,&game),"fixture RT");
    D3DVIEWPORT9 vp={0,0,64,64,0,1};
    IDirect3DQuery9* fence=nullptr;check(dev->CreateQuery(D3DQUERYTYPE_EVENT,&fence),"fixture fence");
    auto wait9=[&](){check(fence->Issue(D3DISSUE_END),"issue fixture fence");
        const DWORD start=GetTickCount();HRESULT hr=S_FALSE;
        while(hr==S_FALSE && GetTickCount()-start<2000){hr=fence->GetData(nullptr,0,D3DGETDATA_FLUSH);Sleep(0);}
        require(hr==S_OK,"fixture GPU completion");};
    D3D11_TEXTURE2D_DESC td={};td.Width=td.Height=128;td.MipLevels=td.ArraySize=1;
    td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *output=nullptr,*cpu=nullptr;ID3D11RenderTargetView* rtv=nullptr;
    check(dev11->CreateTexture2D(&td,nullptr,&output),"output");check(dev11->CreateRenderTargetView(output,nullptr,&rtv),"output view");
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(dev11->CreateTexture2D(&td,nullptr,&cpu),"readback");
    using namespace dvr::markersharp;
    set_enabled(false,"test");prepare(dev,dev11,ctx,64,64);require(!begin(dev,game,vp),"off retains native target");
    set_enabled(true,"test");
    for(unsigned i=0;i<2;++i) {
        prepare(dev,dev11,ctx,64,64);require(begin(dev,game,vp),"redirect ready");
        check(dev->Clear(0,nullptr,D3DCLEAR_TARGET,i?0x80004000u:0x80400000u,1,0),"marker fixture pixels");
        end(dev,game,vp);dev->SetViewport(&vp);
        IDirect3DSurface9* restored=nullptr;check(dev->GetRenderTarget(0,&restored),"restored RT");
        require(restored==game,"original RT restored");restored->Release();
        seal(101+i,i?1:-1);
    }
    wait9();
    // Deliver in reverse order, then toggle off and drain the earlier image.
    for(unsigned pass=0;pass<4;++pass) {
        const unsigned serial=pass==0?102:pass==1?101:999;
        if(pass==1)set_enabled(false,"test in-flight drain");
        const float bg[4]={.2f,.4f,.6f,1};ctx->ClearRenderTargetView(rtv,bg);
        composite(ctx,rtv,128,128,serial,pass==0?1:-1);
        ctx->CopyResource(cpu,output);D3D11_MAPPED_SUBRESOURCE m={};
        check(ctx->Map(cpu,0,D3D11_MAP_READ,0,&m),"read pixels");const auto* p=(const unsigned char*)m.pData;
        for(int c=0;c<3;++c) {
            const int expected=pass<2 ? (int)(bg[c]*127+.5f)+((pass==0?c==1:c==0)?64:0) : (int)(bg[c]*255+.5f);
            require(std::abs((int)p[c]-expected)<=1,"delivered serial owns exact marker pixels");
        }
        ctx->Unmap(cpu,0);
    }
    set_enabled(true,"test");reduced=false;prepare(dev,dev11,ctx,64,64);
    require(!begin(dev,game,vp),"no reduced upscaler retains native target");
    afwClean=true;prepare(dev,dev11,ctx,64,64);
    require(begin(dev,game,vp),"AFW clean sources redirect the markers at the render size");
    end(dev,game,vp);dev->SetViewport(&vp);seal(201,1);
    set_enabled(false,"test");prepare(dev,dev11,ctx,64,64);
    require(begin(dev,game,vp),"AFW clean sources redirect even with MarkersSharp off");
    end(dev,game,vp);dev->SetViewport(&vp);seal(202,-1);afwClean=false;
    reset();game->Release();fence->Release();check(dev->ResetEx(&pp,nullptr),"DEFAULT resources released before reset");
    rtv->Release();cpu->Release();output->Release();ctx->ClearState();ctx->Flush();ctx->Release();dev11->Release();
    dev->Release();api->Release();adapter->Release();factory->Release();DestroyWindow(wnd);FreeLibrary(lib);
    puts("marker transport: serial separation, reversed delivery, toggle drain, old-path guards and Reset PASS");
}
