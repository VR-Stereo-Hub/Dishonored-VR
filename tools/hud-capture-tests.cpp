// Native D3D9Ex -> D3D11 production HUD transfer test. No game or XR session.
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "../src/core/gfx/hud_capture.cpp"
#undef DVR_CAT
#include "../src/core/gfx/blit_quad.cpp"
namespace dvr::log { uint8_t g_levels[(int)Cat::COUNT]={};void write(Cat,Level,const char*,...){} }
namespace dvr::status {void Writer::kv(const char*,int){} void Writer::kv(const char*,unsigned long){} void Writer::kv(const char*,bool){} void Writer::kv(const char*,double){} void Writer::kv(const char*,const char*){} void Writer::obj(const char*){} void Writer::end_obj(){} }
namespace dvr::perf {void part_mark(const char*){} bool parts_enabled(){return false;} }
namespace dvr::etw {bool enabled(){return false;}void begin(Phase,int64_t,int64_t){} void end(Phase,int64_t,int64_t){} }
namespace dvr::markersharp {bool enabled(){return false;}void set_enabled(bool,const char*){} }
namespace dvr::dlss {int mode(){return ModeOff;}bool sr_output_for(uint32_t,uint32_t,uint32_t*,uint32_t*){return false;} }
namespace dvr::stereo {IStereo* active(){return nullptr;} }
namespace dvr::vr {bool pair_open(){return false;} }
namespace dvr::hud {bool projection_mode(){return true;}bool gate(){return true;} }
namespace dvr::frame {
HRESULT orig_set_render_target(IDirect3DDevice9* d,DWORD i,IDirect3DSurface9* s){return d->SetRenderTarget(i,s);}
HRESULT orig_set_depth_stencil(IDirect3DDevice9* d,IDirect3DSurface9* s){return d->SetDepthStencilSurface(s);}
IDirect3DSurface9* game_depth_stencil(bool* k){*k=true;return nullptr;}
}
namespace dvr::hudlayout {
bool hidden=false,parts=false;float plate=0;
bool native_gameplay_reference(){return false;}
bool sink_in_use(int i){return i<4;}bool sink_hidden(int){return hidden;}const char* sink_label(int){return "test";}
AlphaCfg alpha_for_sink(int){AlphaCfg a{};a.mode=1;a.gain=a.gamma=a.mixK=1;return a;}
void backdrop_for_sink(int,float* a){a[0]=a[1]=a[2]=1;a[3]=plate;}
void circle_for_sink(int,uint32_t,uint32_t,float*){}
bool sink_is_subtitles(int){return false;}
const SubtitleReadabilityCfg& subtitle_readability(){static SubtitleReadabilityCfg c{};return c;}
const ElementCfg& element(int){static ElementCfg c{};return c;}
bool wheel_parts_for_sink(int i){return parts && i==0;}
bool wheel_part_crop(int i,int p,unsigned,unsigned,float* r){if(!wheel_parts_for_sink(i))return false;r[0]=p*.5f;r[1]=0;r[2]=r[0]+.5f;r[3]=1;return true;}
AlphaCfg wheel_parts_alpha(){return alpha_for_sink(0);}
void log_status(){}void status(dvr::status::Writer&){}bool command(const char*){return false;}
}
static unsigned checks=0;
static void check(bool ok,const char* what){++checks;if(!ok){printf("FAIL %s\n",what);exit(1);}}
static unsigned read(ID3D11Device* d,ID3D11DeviceContext* c,ID3D11Texture2D* t) {
 check(t!=nullptr,"delivered texture");D3D11_TEXTURE2D_DESC td{};t->GetDesc(&td);td.BindFlags=td.MiscFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ID3D11Texture2D* cpu=nullptr;check(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&cpu)),"readback allocation");c->CopyResource(cpu,t);D3D11_MAPPED_SUBRESOURCE m{};
 check(SUCCEEDED(c->Map(cpu,0,D3D11_MAP_READ,0,&m)),"map output");unsigned p=*(const unsigned*)m.pData;c->Unmap(cpu,0);cpu->Release();return p;
}
int main(){
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DvrHudHost";RegisterClassW(&wc);
 HWND win=CreateWindowW(wc.lpszClassName,L"HUD host",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,wc.hInstance,nullptr);
 IDirect3D9Ex* api=nullptr;check(SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION,&api)),"D3D9Ex");
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=win;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.BackBufferWidth=pp.BackBufferHeight=128;
 IDirect3DDevice9Ex* d9=nullptr;check(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,win,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,nullptr,&d9)),"D3D9 device");
 ID3D11Device* d11=nullptr;ID3D11DeviceContext* ctx=nullptr;check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d11,nullptr,&ctx)),"D3D11 device");
 using namespace dvr::hudcap;
 set_enabled(true);set_game_gate(true,false);set_once_per_pair(false);
 for(int n=0;n<4;++n)end_frame(d9,d11,ctx);
 check(armed(),"production capture arms");
 unsigned previous=0;
 for(int n=0;n<40;++n){
   const unsigned color=(n==2 || n==12) ? 0xffff0000u : n==3 ? 0xff00ff00u : 0;
   if(color){D3DVIEWPORT9 vp{0,0,128,128,0,1};check(begin(d9,vp,0),"redirect draw");check(SUCCEEDED(d9->Clear(0,nullptr,D3DCLEAR_TARGET,color,1,0)),"widget draw");end(d9,nullptr,vp);}
   end_frame(d9,d11,ctx);
   const unsigned expected=previous==0xffff0000u ? 0xff0000ffu : previous;
   check(read(d11,ctx,sink_texture(0,ctx))==expected,"one-frame delivery and first blank remove old widget");
   for(int i=1;i<4;++i)check(read(d11,ctx,sink_texture(i,ctx))==0,"unused panel stays transparent");
   previous=color;
 }
 check(g_sink[1].winCopySaved>=35 && g_sink[1].winConvertSaved>=35,"steady blank sink skips shared copies and conversions");
 auto saved=g_sink[0].winConvertSaved;dvr::hudlayout::plate=.5f;end_frame(d9,d11,ctx);
 auto pixel=read(d11,ctx,sink_texture(0,ctx));check((pixel>>24)>=127 && (pixel>>24)<=128,"live backdrop update invalidates blank output");check(saved==g_sink[0].winConvertSaved,"changed parameters force conversion");
 end_frame(d9,d11,ctx);check(g_sink[0].winConvertSaved>saved,"unchanged blank backdrop can be reused");
 dvr::hudlayout::parts=true;end_frame(d9,d11,ctx);check(wheel_part_texture(0,0)!=nullptr && wheel_part_texture(0,1)!=nullptr,"wheel side panels bypass cached output");
 saved=g_sink[0].winConvertSaved;end_frame(d9,d11,ctx);check(g_sink[0].winConvertSaved==saved,"wheel parts continue to update");dvr::hudlayout::parts=false;
 invalidate_content();end_frame(d9,d11,ctx);check(!sink_texture(0,ctx),"menu/content invalidation withdraws old output");end_frame(d9,d11,ctx);check(sink_texture(0,ctx)!=nullptr,"content delivery recovers");
 set_slot_scale(.75f);end_frame(d9,d11,ctx);end_frame(d9,d11,ctx);D3D11_TEXTURE2D_DESC td{};sink_texture(0,ctx)->GetDesc(&td);check(td.Width==96,"resize recreates output and cache");
 dvr::hudlayout::hidden=true;end_frame(d9,d11,ctx);check(!sink_texture(0,ctx),"hidden sink withdraws output");dvr::hudlayout::hidden=false;end_frame(d9,d11,ctx);end_frame(d9,d11,ctx);check(sink_texture(0,ctx)!=nullptr,"unhide recreates output");
 BlankCache failure;failure.copied(0,true,true);check(!failure.copy_needed(0,true),"successful clear can be reused");failure.copied(0,true,false);check(failure.copy_needed(0,true),"failed transfer cannot seed blank cache");check(failure.copy_needed(0,false),"unknown clear requires fresh copy");
 on_reset();check(!sink_texture(0,ctx),"device reset withdraws cached output");end_frame(d9,d11,ctx);end_frame(d9,d11,ctx);check(sink_texture(0,ctx)!=nullptr,"reset recovers shared output");
 printf("PASS %u checks: production D3D9Ex/D3D11 HUD pixels, delayed blank, layout, wheel, hide, resize, reset, cache refusal\n",checks);
 shutdown();ctx->Release();d11->Release();d9->Release();api->Release();DestroyWindow(win);
}
