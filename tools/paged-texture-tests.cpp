#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include "core/gfx/d3d9ex.cpp"
namespace dvr::log { uint8_t g_levels[(int)Cat::COUNT]={}; void write(Cat,Level,const char* f,...){va_list a;va_start(a,f);vprintf(f,a);puts("");va_end(a);} }
namespace dvr::status { void Writer::kv(const char*,int){} void Writer::kv(const char*,unsigned long){} void Writer::kv(const char*,double){} void Writer::kv(const char*,bool){} void Writer::kv(const char*,const char*){} }
#define CHECK(x) do { if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);} }while(0)
using namespace dvr::d3d9ex;
void check_readback(IDirect3DDevice9* dev, IDirect3DTexture9* t, UINT level, DWORD color) {
 D3DSURFACE_DESC desc{};t->GetLevelDesc(level,&desc);
 IDirect3DSurface9 *src=nullptr,*rt=nullptr,*cpu=nullptr;
 CHECK(SUCCEEDED(t->GetSurfaceLevel(level,&src)));
 CHECK(SUCCEEDED(dev->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&rt,nullptr)));
 CHECK(SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr)));
 CHECK(SUCCEEDED(dev->StretchRect(src,nullptr,rt,nullptr,D3DTEXF_NONE)));
 CHECK(SUCCEEDED(dev->GetRenderTargetData(rt,cpu)));
 D3DLOCKED_RECT lr{};CHECK(SUCCEEDED(cpu->LockRect(&lr,nullptr,D3DLOCK_READONLY)));
 CHECK(*(DWORD*)lr.pBits==color);cpu->UnlockRect();cpu->Release();rt->Release();src->Release();
}
int main(){
 HWND hwnd=CreateWindowExW(0,L"STATIC",L"Paged texture host",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 IDirect3D9Ex* api=nullptr;CHECK(SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION,&api)));
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
 IDirect3DDevice9Ex* dev=nullptr;CHECK(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,nullptr,&dev)));
 cs_init();g_dev=dev;g_deviceLive=g_deviceIsEx=g_deviceFromEx=g_exWanted=true;g_managed=Managed::Paged;
 IDirect3DTexture9* tex=nullptr;CHECK(SUCCEEDED(dev->CreateTexture(64,64,0,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&tex,nullptr)));
 shadow_register_texture(dev,tex,64,64,0,D3DFMT_A8R8G8B8);CHECK(shadow_tracked(tex));
 D3DLOCKED_RECT a{},b{};CHECK(paged_lock_rect(tex,0,-1,&a,nullptr,0)==S_OK);CHECK(paged_lock_rect(tex,1,-1,&b,nullptr,0)==S_OK);
 for(int y=0;y<64;y++)for(int x=0;x<64;x++)((DWORD*)((BYTE*)a.pBits+y*a.Pitch))[x]=0xff123456;
 for(int y=0;y<32;y++)for(int x=0;x<32;x++)((DWORD*)((BYTE*)b.pBits+y*b.Pitch))[x]=0xff654321;
 CHECK(paged_lock_rect(tex,0,-1,&b,nullptr,0)==D3DERR_INVALIDCALL);
 CHECK(paged_unlock_rect(tex,1,-1)==S_OK);CHECK(paged_unlock_rect(tex,0,-1)==S_OK);
 check_readback(dev,tex,0,0xff123456);check_readback(dev,tex,1,0xff654321);
 CHECK(g_pagedConcurrent==0 && g_pagedMappedBytes==0);
 RECT bad{-1,0,2,2};CHECK(FAILED(paged_lock_rect(tex,0,-1,&a,&bad,0)));CHECK(FAILED(paged_lock_rect(tex,7,-1,&a,nullptr,0)));
 auto uploads=g_shadowUpdates;CHECK(paged_lock_rect(tex,0,-1,&a,nullptr,D3DLOCK_READONLY)==S_OK);CHECK(*(DWORD*)a.pBits==0xff123456);CHECK(paged_unlock_rect(tex,0,-1)==S_OK);CHECK(g_shadowUpdates==uploads);
 CHECK(FAILED(paged_unlock_rect(tex,0,-1)));shadow_released(tex);tex->Release();
 for(auto fmt:{D3DFMT_DXT1,D3DFMT_DXT5}) {
 CHECK(SUCCEEDED(dev->CreateTexture(64,64,0,0,fmt,D3DPOOL_DEFAULT,&tex,nullptr)));
 shadow_register_texture(dev,tex,64,64,0,fmt);
 for(int mip=0;mip<7;mip++){CHECK(paged_lock_rect(tex,mip,-1,&a,nullptr,0)==S_OK);memset(a.pBits,0,(size_t)level_bytes(fmt,std::max(1,64>>mip),std::max(1,64>>mip)));CHECK(paged_unlock_rect(tex,mip,-1)==S_OK);}
 shadow_released(tex);tex->Release();
 }
 IDirect3DCubeTexture9* cube=nullptr;CHECK(SUCCEEDED(dev->CreateCubeTexture(32,0,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&cube,nullptr)));shadow_register_cube(dev,cube,32,0,D3DFMT_A8R8G8B8);
 for(int face=0;face<6;face++)CHECK(paged_lock_rect(cube,0,face,&a,nullptr,D3DLOCK_READONLY)==S_OK);
 for(int face=0;face<6;face++)CHECK(paged_unlock_rect(cube,0,face)==S_OK);
 shadow_released(cube);cube->Release();CHECK(g_pagedSections==0 && g_pagedConcurrent==0);
 // Compare resident normal RAM shadow versus mapping work on the same GPU workload.
 for(auto mode:{Managed::Shadow,Managed::Paged}){
 g_managed=mode;set_full_copy(true);CHECK(SUCCEEDED(dev->CreateTexture(1024,1024,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&tex,nullptr)));shadow_register_texture(dev,tex,1024,1024,1,D3DFMT_A8R8G8B8);
 std::vector<double> us;
 for(int n=0;n<80;n++){LARGE_INTEGER start;QueryPerformanceCounter(&start);
 if(mode==Managed::Paged){CHECK(paged_lock_rect(tex,0,-1,&a,nullptr,0)==S_OK);memset(a.pBits,n,4*1024*1024);CHECK(paged_unlock_rect(tex,0,-1)==S_OK);}
 else {auto twin=(IDirect3DTexture9*)shadow_twin_for_lock(tex,0,0);CHECK(twin);CHECK(SUCCEEDED(twin->LockRect(0,&a,nullptr,0)));for(int y=0;y<1024;y++)memset((BYTE*)a.pBits+y*a.Pitch,n,4096);twin->UnlockRect(0);shadow_unlocked(tex,0,-1);}
 us.push_back((double)elapsed_us(start));}
 std::sort(us.begin(),us.end());printf("BENCH %s 4MiB upload n=80 p50=%.3fms p95=%.3fms max=%.3fms\n",managed_name(mode),us[40]/1000,us[76]/1000,us.back()/1000);
 shadow_released(tex);tex->Release();clear_staging();}
 MEMORYSTATUSEX before{};before.dwLength=sizeof(before);GlobalMemoryStatusEx(&before);PERFORMANCE_INFORMATION pi{};GetPerformanceInfo(&pi,sizeof(pi));auto commit=pi.CommitTotal;
 std::vector<HANDLE> mappings;for(int i=0;i<64;i++){auto h=make_page_section(4*1024*1024);CHECK(h);mappings.push_back(h);void* p=MapViewOfFile(h,FILE_MAP_WRITE,0,0,0);CHECK(p);memset(p,1,4*1024*1024);UnmapViewOfFile(p);}
 MEMORYSTATUSEX after{};after.dwLength=sizeof(after);GlobalMemoryStatusEx(&after);GetPerformanceInfo(&pi,sizeof(pi));printf("MEMORY 256MiB mappings unmapped: VA consumed %.2fMiB; system commit delta %.2fMiB (other processes can move this)\n",(double)(before.ullAvailVirtual-after.ullAvailVirtual)/1048576,(double)((int64_t)pi.CommitTotal-commit)*pi.PageSize/1048576);
 for(auto h:mappings)CloseHandle(h);
 CHECK(g_mapCount==0 && g_pagedMappedBytes==0 && g_stageBytes==0 && g_shadowUpdateFailed==0);dev->Release();api->Release();DestroyWindow(hwnd);puts("PASS paged texture GPU upload, mip/face concurrency, validation, read-only, release and staging cleanup");
}
