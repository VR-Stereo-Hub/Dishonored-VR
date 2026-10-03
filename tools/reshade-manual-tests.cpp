// Native GPU test; no game, real settings or visible window.
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include "../src/core/gfx/reshade_runtime.cpp"
#undef DVR_CAT
#include "../src/core/gfx/d3d9ex.cpp"
#undef DVR_CAT
#include "../src/core/gfx/device_census.cpp"
#undef DVR_CAT
#include "../src/core/hooks/vtable.cpp"
namespace dvr::log { uint8_t g_levels[(int)Cat::COUNT]={}; void write(Cat,Level,const char* f,...){va_list a;va_start(a,f);vprintf(f,a);puts("");va_end(a);} }
namespace dvr::status { void Writer::kv(const char*,int){} void Writer::kv(const char*,unsigned long){} void Writer::kv(const char*,double){} void Writer::kv(const char*,bool){} void Writer::kv(const char*,const char*){} }
namespace dvr::native_profile { Scope::Scope(Kind k):kind(k){} void Scope::finish(){} }
namespace dvr::depthprobe { void note_texture(IDirect3DTexture9*,UINT,UINT,DWORD,D3DFORMAT){} }

static unsigned checks=0;
static void require(bool ok,const char* what) { ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);} }
static unsigned pixel(IDirect3DDevice9* d, IDirect3DSurface9* cpu) {
    IDirect3DSurface9* bb=nullptr;require(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb)),"backbuffer");
    require(SUCCEEDED(d->GetRenderTargetData(bb,cpu)),"readback");bb->Release();
    D3DLOCKED_RECT lock{};require(SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"read pixels");
    unsigned result=*reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+400*lock.Pitch+400*4)&0xffffff;
    cpu->UnlockRect();return result;
}
int main(int argc, char**) {
    memset(dvr::log::g_levels,2,sizeof(dvr::log::g_levels));
    const bool noEffects = argc > 1;
    using namespace dvr::reshade_runtime;
    SetEnvironmentVariableW(L"RESHADE_DISABLE_GRAPHICS_HOOK",L"host-sentinel");
    load_optional();require(manual(),"manual selected");
    wchar_t restored[64]{};GetEnvironmentVariableW(L"RESHADE_DISABLE_GRAPHICS_HOOK",restored,64);
    require(wcscmp(restored,L"host-sentinel")==0,"environment restored");
    require(createRuntime && updateRuntime && destroyRuntime,"official exports present");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DvrReShadeTest";
    require(RegisterClassW(&wc)!=0,"window class");
    HWND wnd=CreateWindowW(wc.lpszClassName,L"DVR ReShade host",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
    require(wnd!=nullptr,"hidden window");
    HMODULE lib=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    using Create9=HRESULT(WINAPI*)(UINT,IDirect3D9Ex**);
    auto create=reinterpret_cast<Create9>(GetProcAddress(lib,"Direct3DCreate9Ex"));
    IDirect3D9Ex* api=nullptr;require(create && SUCCEEDED(create(D3D_SDK_VERSION,&api)),"system D3D9Ex");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=wnd;pp.BackBufferWidth=640;pp.BackBufferHeight=480;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9Ex* dev=nullptr;
    require(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,wnd,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE|D3DCREATE_PUREDEVICE,&pp,nullptr,&dev)),"native device");
    dvr::d3d9ex::cs_init(); dvr::d3d9ex::g_dev=dev;
    dvr::d3d9ex::g_deviceLive=dvr::d3d9ex::g_deviceIsEx=true;
    dvr::d3d9ex::g_managed=dvr::d3d9ex::Managed::Paged;
    dvr::census::install(dev,api,0,D3DDEVTYPE_HAL,0,&pp);
    IDirect3DTexture9* before=nullptr;
    require(SUCCEEDED(dev->CreateTexture(256,256,1,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&before,nullptr)),"managed before ReShade");before->Release();
    void* creationHook=(*(void***)dev)[23];
    IDirect3DSurface9* cpu=nullptr;require(SUCCEEDED(dev->CreateOffscreenPlainSurface(640,480,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"readback surface");
    for(unsigned generation=0;generation<2;++generation) {
        unsigned actual=0;
        for(unsigned frame=0;frame<400;++frame) {
            require(SUCCEEDED(dev->Clear(0,nullptr,D3DCLEAR_TARGET,0x00ff0000,1,0)),"red frame");
            dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            const D3DVIEWPORT9 vp{7,9,320,240,0,1};dev->SetViewport(&vp);
            render(dev);require((*(void***)dev)[23]==creationHook,"texture hook survives ReShade state blocks");require(runtime!=nullptr,"runtime created");require(!inside,"guard restored");
            DWORD alpha=0;dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha);require(alpha==TRUE,"render state restored");
            D3DVIEWPORT9 after{};dev->GetViewport(&after);require(after.X==7 && after.Y==9 && after.Width==320 && after.Height==240,"viewport restored");
            IDirect3DTexture9* gameTex=nullptr;
            const HRESULT textureHr=dev->CreateTexture(256,256,1,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&gameTex,nullptr);
            if(FAILED(textureHr)) { std::printf("vtable before=%p after=%p translations=%u calls=%u failures=%u\n",creationHook,(*(void***)dev)[23],dvr::d3d9ex::g_texTranslated,dvr::census::g_creations,dvr::census::g_failures);dvr::census::log_summary("failure"); }
            if(FAILED(textureHr)) std::printf("CreateTexture failed hr=%08lx inside=%d ex=%d\n",textureHr,int(inside),int(dvr::d3d9ex::device_is_ex()));
            require(SUCCEEDED(textureHr),"game DXT5 managed texture after ReShade");
            D3DLOCKED_RECT gameLock{};require(SUCCEEDED(gameTex->LockRect(0,&gameLock,nullptr,0)),"game texture lock after ReShade");
            memset(gameLock.pBits,0,64*gameLock.Pitch);require(SUCCEEDED(gameTex->UnlockRect(0)),"game texture upload after ReShade");gameTex->Release();
            actual=pixel(dev,cpu);if(actual==(noEffects ? 0xff0000u : 0x00ffffu) && frame>=30)break;
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(15);
        }
        std::printf("generation=%u pixel=%06x expected=%06x\n",generation,actual,noEffects ? 0xff0000u : 0x00ffffu);
        require(actual==(noEffects ? 0xff0000u : 0x00ffffu),"enabled/disabled preset gives expected pixels with zero Present calls");
        reset();require(runtime==nullptr,"runtime released");
        require(SUCCEEDED(dev->ResetEx(&pp,nullptr)),"reset after runtime destruction");
    }
    cpu->Release();dev->Release();api->Release();DestroyWindow(wnd);
    std::printf("PASS: %u checks; effect pixels, state restore, reset/recreate, zero native Present\n",checks);
}
