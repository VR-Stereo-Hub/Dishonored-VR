// Actual production HUD shader on D3D11 WARP. No game, XR session or window.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/core/gfx/blit_quad.cpp"
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT]={};
void write(Cat,Level,const char* format,...) { va_list ap;va_start(ap,format);vprintf(format,ap);puts("");va_end(ap); }
}
static void check(bool ok,const char* what) { if(!ok) {printf("FAIL %s\n",what);exit(1);} }
int main() {
 ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;
 check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx)),"WARP device");
 dvr::gfx::BlitQuad blit;check(blit.init(dev) && blit.alpha_ready(),"production shader compiles");
 const unsigned size=256;std::vector<unsigned> pixels(size*size,0xffffffff);
 D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=size;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA initial{pixels.data(),size*4,0};ID3D11Texture2D *src=nullptr,*dst=nullptr,*cpu=nullptr;
 check(SUCCEEDED(dev->CreateTexture2D(&d,&initial,&src)),"source");d.BindFlags=D3D11_BIND_RENDER_TARGET;
 check(SUCCEEDED(dev->CreateTexture2D(&d,nullptr,&dst)),"target");d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 check(SUCCEEDED(dev->CreateTexture2D(&d,nullptr,&cpu)),"readback");
 ID3D11ShaderResourceView* srv=nullptr;ID3D11RenderTargetView* rtv=nullptr;
 check(SUCCEEDED(dev->CreateShaderResourceView(src,nullptr,&srv)) && SUCCEEDED(dev->CreateRenderTargetView(dst,nullptr,&rtv)),"views");
 for(int mask=0;mask<2;++mask) {
   dvr::gfx::AlphaParams a;a.mode=1;if(mask) {a.ellipse[0]=a.ellipse[1]=.5f;a.ellipse[2]=a.ellipse[3]=.2f;}
   blit.draw(ctx,srv,rtv,size,size,&a);ctx->CopyResource(cpu,dst);
   D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(ctx->Map(cpu,0,D3D11_MAP_READ,0,&m)),"map");
   unsigned clear=0,soft=0,solid=0;
   for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) {
     auto p=(const unsigned char*)m.pData+y*m.RowPitch+x*4;
     if(p[3]==0) {++clear;check(p[0]==0 && p[1]==0 && p[2]==0,"transparent corners have no color bleed");}
     else if(p[3]<255) ++soft;else ++solid;
     if(x==128 && y==128) check(p[3]==255,"center retained");
   }
   if(mask) check(clear>50000 && soft>100 && solid>7000,"circle masks corners with feathered edge");
   else check(solid==size*size && clear==0 && soft==0,"ordinary HUD unchanged");
   printf("mask=%d clear=%u feather=%u solid=%u PASS\n",mask,clear,soft,solid);ctx->Unmap(cpu,0);
 }
 blit.shutdown();rtv->Release();srv->Release();cpu->Release();dst->Release();src->Release();ctx->Release();dev->Release();
}
