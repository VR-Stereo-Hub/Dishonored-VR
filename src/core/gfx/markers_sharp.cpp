// Native HUD draws travel with the colour capture serial, never a global latest eye.
#define DVR_CAT ::dvr::log::Cat::hud
#include "core/gfx/markers_sharp.h"
#include "core/gfx/shared_capture_texture.h"
#include "core/gfx/capture.h"
#include "core/gfx/dlss.h"
#include "core/gfx/stereo.h"
#include "core/gfx/blit_quad.h"
#include "core/framework/frame_hooks.h"
#include "core/util/log.h"
#include <atomic>
#include <cstring>
namespace dvr::markersharp {
namespace {
std::atomic<bool> wanted{true};
struct Slot {
    dvr::capture::interop::Image image;
    IDirect3DQuery9* fence=nullptr;
    ID3D11ShaderResourceView* srv=nullptr;
    ID3D11Query* read=nullptr;
    bool pending=false,reading=false;
    uint32_t serial=0,draws=0; int eye=0;
};
// Colour capture supports at most three presents of latency. Six prevents
// rewriting any such image while also tolerating a busy D3D11 reader.
Slot slots[6]; int current=-1,next=0;
uint32_t fw=0,fh=0,ow=0,oh=0;
IDirect3DSurface9* ds=nullptr; IDirect3DSurface9* savedDs=nullptr;
bool failed=false,inDraw=false;
dvr::gfx::BlitQuad blit;
uint64_t draws[3]{},composed[3]{},refused=0;
int bucket(int eye) {return eye<0?0:eye>0?1:2;}
void refuse(const char* why,HRESULT hr=E_FAIL) {
    ++refused;
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Warn,1000,
        "hud/markers-sharp: REFUSED owner=serial-overlay reason=%s hr=%08lx slot=%d render=%ux%u output=%ux%u total=%llu",
        why,(unsigned long)hr,current,fw,fh,ow,oh,(unsigned long long)refused);
}
}
bool enabled(){return wanted.load();}
void set_enabled(bool on,const char* owner) {
    wanted.store(on);
    DVR_INFO("hud/markers-sharp: MarkersSharp=%d owner=%s; native marker overlay after reconstruction, default on",(int)on,owner?owner:"?");
}
void reset() {
    current=-1;inDraw=false;savedDs=nullptr;
    for(auto& s:slots) {
        if(s.srv)s.srv->Release(); if(s.fence)s.fence->Release(); if(s.read)s.read->Release();
        s.srv=nullptr;s.fence=nullptr;s.read=nullptr;s.image.reset();
        s.pending=s.reading=false;s.serial=s.draws=0;s.eye=0;
    }
    if(ds)ds->Release();ds=nullptr;
    fw=fh=ow=oh=0;next=0;failed=false;blit.shutdown();
}
void prepare(IDirect3DDevice9* d9,ID3D11Device* d11,ID3D11DeviceContext* ctx,uint32_t w,uint32_t h) {
    current=-1;
    uint32_t x=0,y=0;
    if(!wanted.load() || dvr::dlss::mode()==dvr::dlss::ModeOff ||
       !dvr::dlss::sr_output_for(w,h,&x,&y) || x<=w || y<=h ||
       !dvr::stereo::reentry_family_active()) {   // VR-39: aer shares the reentry path
        if(wanted.load()) refuse("no reduced reentry upscaler");
        return;
    }
    if(!d9 || !d11 || !ctx || !w || !h) {refuse("missing device or dimensions");return;}
    if(failed) {refuse("resource failure latched until reset");return;}
    if(w!=fw || h!=fh || x!=ow || y!=oh || !ds) {
        reset();fw=w;fh=h;ow=x;oh=y;
        HRESULT hr=d9->CreateDepthStencilSurface(ow,oh,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&ds,nullptr);
        for(auto& s:slots) {
            if(FAILED(hr))break;
            const auto cr=dvr::capture::interop::create(d9,d11,ow,oh,D3DFMT_A8R8G8B8,s.image);hr=cr.hr;
            if(SUCCEEDED(hr))hr=d9->CreateQuery(D3DQUERYTYPE_EVENT,&s.fence);
            if(SUCCEEDED(hr))hr=d11->CreateShaderResourceView(s.image.texture,nullptr,&s.srv);
            D3D11_QUERY_DESC q={D3D11_QUERY_EVENT,0};
            if(SUCCEEDED(hr))hr=d11->CreateQuery(&q,&s.read);
        }
        if(FAILED(hr) || !blit.init(d11) || !blit.composite_ready()) {
            refuse("shared target/fence/compositor allocation",hr);reset();failed=true;return;
        }
        DVR_INFO("hud/markers-sharp: owner=serial-overlay ready %ux%u -> %ux%u; no scene depth (Z disabled); six fenced image slots",fw,fh,ow,oh);
    }
    Slot& s=slots[next];
    if(s.reading) {
        const HRESULT hr=ctx->GetData(s.read,nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(hr!=S_OK) {refuse("D3D11 reader busy",hr);return;}s.reading=false;
    }
    if(s.pending) {
        const HRESULT hr=s.fence->GetData(nullptr,0,D3DGETDATA_FLUSH);
        if(hr!=S_OK){refuse("D3D9 writer busy",hr);return;}s.pending=false;
    }
    s.serial=s.draws=0;s.eye=0;
    const HRESULT hr=d9->ColorFill(s.image.surface,nullptr,0);
    if(FAILED(hr)){refuse("clear transparent target",hr);return;}
    current=next;next=(next+1)%6;
}
bool begin(IDirect3DDevice9* dev,IDirect3DSurface9* rt,const D3DVIEWPORT9& vp) {
    if(!wanted.load())return false;
    if(current<0 || inDraw || !rt || vp.Width!=fw || vp.Height!=fh) {refuse("target unavailable or viewport mismatch");return false;}
    bool known=false;savedDs=dvr::frame::game_depth_stencil(&known);
    if(!known){refuse("depth-stencil shadow unknown");return false;}
    HRESULT hr=dvr::frame::orig_set_depth_stencil(dev,ds);
    if(SUCCEEDED(hr))hr=dvr::frame::orig_set_render_target(dev,0,slots[current].image.surface);
    if(FAILED(hr)) {
        dvr::frame::orig_set_render_target(dev,0,rt);dvr::frame::orig_set_depth_stencil(dev,savedDs);
        refuse("bind marker target",hr);return false;
    }
    inDraw=true;++slots[current].draws;return true;
}
void end(IDirect3DDevice9* dev,IDirect3DSurface9* rt,const D3DVIEWPORT9&) {
    if(!inDraw)return;
    const HRESULT a=dvr::frame::orig_set_render_target(dev,0,rt);
    const HRESULT b=dvr::frame::orig_set_depth_stencil(dev,savedDs);
    inDraw=false;savedDs=nullptr;
    if(FAILED(a)||FAILED(b)){refuse("restore game RT/DS",FAILED(a)?a:b);failed=true;current=-1;}
}
void seal(uint32_t serial,int eye) {
    // Called at the exact colour serial allocation, even when no markers drew.
    for(auto& s:slots)if(s.serial==serial)s.serial=0;
    if(current<0)return;
    auto& s=slots[current];s.serial=serial;s.eye=eye;
    draws[bucket(eye)]+=s.draws;
    const HRESULT hr=s.fence->Issue(D3DISSUE_END);s.pending=SUCCEEDED(hr);
    if(FAILED(hr)){s.serial=0;refuse("issue D3D9 fence",hr);}
    current=-1;
}
void composite(ID3D11DeviceContext* ctx,ID3D11RenderTargetView* dst,uint32_t w,uint32_t h,uint32_t serial,int eye) {
    // A toggle off cannot discard a marker-free colour image already in flight.
    Slot* match=nullptr;for(auto& s:slots)if(serial && s.serial==serial)match=&s;
    if(!match)return;
    if(!match->draws)return;
    if(w!=ow || h!=oh || !ctx || !dst){refuse("delivered output mismatch");return;}
    if(match->pending) {
        const HRESULT hr=match->fence->GetData(nullptr,0,D3DGETDATA_FLUSH);
        if(hr!=S_OK){refuse("marker write not ready",hr);return;}match->pending=false;
    }
    dvr::gfx::AlphaParams alpha;alpha.mode=1;
    blit.draw(ctx,match->srv,dst,w,h,&alpha,true);
    ctx->End(match->read);ctx->Flush();match->reading=true;
    ++composed[bucket(eye)];
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
        "hud/markers-sharp: owner=serial-overlay serial=%u capturedEye=%d deliveredEye=%d draws=%u; totals draws L/R/unknown=%llu/%llu/%llu composites=%llu/%llu/%llu refused=%llu",
        serial,match->eye,eye,match->draws,draws[0],draws[1],draws[2],composed[0],composed[1],composed[2],refused);
}
}
