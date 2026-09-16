// Standalone native D3D9Ex test. No game, proxy DLL, OpenXR session or visible window.
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include "../src/core/gfx/desktop_eye.cpp"

static unsigned nativeCalls=0;
static void require(bool ok,const char* label) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",label); std::exit(1); }
}
static HRESULT __stdcall nativePresent(IDirect3DDevice9* d,const RECT* s,const RECT* t,HWND w,const RGNDATA* r) {
    ++nativeCalls; return d->Present(s,t,w,r);
}
int main() {
    dvr::clock::init();
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc={}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=instance; wc.lpszClassName=L"DvrDesktopPresentTest";
    require(RegisterClassW(&wc)!=0,"register hidden test window");
    HWND wnd=CreateWindowW(wc.lpszClassName,L"DVR graphics host",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,instance,nullptr);
    require(wnd!=nullptr,"create hidden test window");
    HMODULE lib=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(lib!=nullptr,"load SYSTEM32 D3D9, never the proxy");
    using CreateFn=HRESULT (WINAPI*)(UINT,IDirect3D9Ex**);
    auto create=reinterpret_cast<CreateFn>(GetProcAddress(lib,"Direct3DCreate9Ex"));
    IDirect3D9Ex* api=nullptr;
    require(create && SUCCEEDED(create(D3D_SDK_VERSION,&api)),"create native D3D9Ex");
    D3DPRESENT_PARAMETERS pp={}; pp.Windowed=TRUE; pp.hDeviceWindow=wnd;
    pp.BackBufferWidth=64; pp.BackBufferHeight=64; pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.BackBufferCount=1; pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9Ex* dev=nullptr;
    require(SUCCEEDED(api->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,wnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,nullptr,&dev)),"create HAL test device");
    IDirect3DQuery9* marker=nullptr;
    require(SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_EVENT,&marker)),"create independent GPU marker");
    IDirect3DSurface9* cpu=nullptr;
    require(SUCCEEDED(dev->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"create readback");
    using namespace dvr::desktop_eye;
    set_device(dev); set_mirror_off(true);
    unsigned completed=0;
    for(unsigned frame=1;frame<=120;++frame) {
        const D3DCOLOR color=(frame&1) ? 0x00123456u : 0x00654321u;
        require(SUCCEEDED(dev->Clear(0,nullptr,D3DCLEAR_TARGET,color,1,0)),"render synthetic color");
        // The marker is queued BEFORE the production tail. Poll it WITHOUT
        // FLUSH afterwards: only the production path can submit this work.
        require(SUCCEEDED(marker->Issue(D3DISSUE_END)),"issue pre-tail marker");
        begin_present(frame); note_drawn_eye((frame&1) ? -1 : 1); on_present((frame&1) ? 1 : -1);
        require(present(nativePresent,dev,nullptr,nullptr,nullptr,nullptr,true,true)==D3D_OK,"off tail succeeds");
        Record r; require(record_for(frame,r) && r.action=='O' && !r.nativeCalled,"off actually omits desktop call");
        HRESULT done=S_FALSE; const ULONGLONG deadline=GetTickCount64()+2000;
        do { done=marker->GetData(nullptr,0,0); if(done==S_FALSE) Sleep(1); }
        while(done==S_FALSE && GetTickCount64()<deadline);
        if(done!=D3D_OK) std::fprintf(stderr,"marker frame=%u hr=0x%08lx\n",frame,(unsigned long)done);
        require(done==D3D_OK,"GPU marker completes without Present or test-side FLUSH");
        ++completed;
        IDirect3DSurface9* bb=nullptr;
        require(SUCCEEDED(dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb)),"get synthetic frame");
        require(SUCCEEDED(dev->GetRenderTargetData(bb,cpu)),"read completed synthetic frame");
        bb->Release();
        D3DLOCKED_RECT pixels={}; require(SUCCEEDED(cpu->LockRect(&pixels,nullptr,D3DLOCK_READONLY)),"lock completed pixels");
        const unsigned pixel=*static_cast<const unsigned*>(pixels.pBits)&0x00ffffffu;
        cpu->UnlockRect(); require(pixel==color,"right/left synthetic pixels survive off tail");
    }
    require(nativeCalls==0,"zero original Presents in off interval");
    set_mirror_off(false);
    begin_present(121); note_drawn_eye(-1); on_present(-1);
    const HRESULT restored=present(nativePresent,dev,nullptr,nullptr,nullptr,nullptr,false,false);
    require(SUCCEEDED(restored) && nativeCalls==1,"full mode restores original Present");
    on_reset(); marker->Release(); cpu->Release();
    require(SUCCEEDED(dev->ResetEx(&pp,nullptr)),"no retained query/surface blocks ResetEx");
    shutdown(); dev->Release(); api->Release(); FreeLibrary(lib); DestroyWindow(wnd); UnregisterClassW(wc.lpszClassName,instance);
    std::printf("PASS: %u native D3D9Ex GPU markers/pixel checks without any desktop Present; full return and reset passed\n",completed);
}
