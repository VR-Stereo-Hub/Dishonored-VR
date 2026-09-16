// Standalone D3D11 test. No window, headset or game. Flush is the test driver's
// submission mechanism, never part of the production profiler.
#include "../src/core/framework/bridge_profile.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT]={};
void write(Cat,Level,const char* fmt,...) { va_list a;va_start(a,fmt);vprintf(fmt,a);puts("");va_end(a); }
}
void require(bool b,const char* why) { if(!b) { fprintf(stderr,"FAIL: %s\n",why);exit(1); } }
int main() {
    using namespace dvr::bridge_profile;
    for(auto& l:dvr::log::g_levels) l=(uint8_t)dvr::log::Level::Info;
    HMODULE lib=LoadLibraryA("d3d11.dll");require(lib!=nullptr,"load D3D11");
    auto create=(PFN_D3D11_CREATE_DEVICE)GetProcAddress(lib,"D3D11CreateDevice");
    require(create!=nullptr,"device entry point");
    ID3D11Device* d=nullptr;ID3D11DeviceContext* c=nullptr;
    auto hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,nullptr,&c);
    require(SUCCEEDED(hr),"hardware D3D11 device");
    D3D11_TEXTURE2D_DESC desc={};desc.Width=64;desc.Height=64;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *a=nullptr,*b=nullptr;ID3D11RenderTargetView* r=nullptr;
    require(SUCCEEDED(d->CreateTexture2D(&desc,nullptr,&a)),"source texture");
    require(SUCCEEDED(d->CreateTexture2D(&desc,nullptr,&b)),"target texture");
    require(SUCCEEDED(d->CreateRenderTargetView(a,nullptr,&r)),"render target");
    require(begin(d,c,Conversion,-1)==-1,"disabled profiler refuses");
    set_enabled(true);set_gameplay(true);
    const float color[4]={0.2f,0.3f,0.4f,1};
    for(int i=0;i<1024;++i) {
        present();
        { Scope s(d,c,Conversion,i&1?1:-1);c->ClearRenderTargetView(r,color); }
        { Scope s(d,c,EyeCopy,i&1?1:-1);c->CopyResource(b,a); }
        if(i%16==0) { c->Flush();Sleep(1); }
    }
    c->Flush();Sleep(20);
    for(int i=0;i<32;++i) {
        gate.chosen=-1;begin(d,c,Conversion,-1);begin(d,c,EyeCopy,1);
    }
    unsigned resolved[2]={};
    for(int s=0;s<2;++s)for(auto& stat:banks[s].stats) {
        resolved[s]+=stat.resolved;require(stat.errors==0,"query read has no HRESULT errors");
    }
    require(resolved[0]>10&&resolved[1]>10,"real GPU results resolved for both stages");
    printf("device test: conversion %u, copy %u valid results\n",resolved[0],resolved[1]);
    set_gameplay(false);gate.chosen=-1;begin(d,c,Conversion,0);
    require(banks[0].stats[0].resolved==0,"context change discards aggregate");
    reset();require(device==nullptr,"reset releases device identity");
    for(auto& bank:banks)for(auto& q:bank.slot)require(!q.a&&!q.b&&!q.disjoint&&!q.state.pending,"reset releases pending and idle queries");
    set_enabled(false);r->Release();a->Release();b->Release();c->Release();d->Release();FreeLibrary(lib);
    puts("bridge profile device lifecycle passed");
}
