// Actual production HUD shader on D3D11 WARP. No game, XR session or window.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
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
 // Hue regression: production shader must retain channel ratios at gamma .25.
 for(auto& p:pixels) p=0xffb4d2b4; // RGB 180,210,180
 ctx->UpdateSubresource(src,0,nullptr,pixels.data(),size*4,0);
 dvr::gfx::AlphaParams tint;tint.gamma=.25f;tint.gain=1.09f;
 blit.draw(ctx,srv,rtv,size,size,&tint);ctx->CopyResource(cpu,dst);
 D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(ctx->Map(cpu,0,D3D11_MAP_READ,0,&mapped)),"tint readback");
 const auto* sample=(const unsigned char*)mapped.pData;
 const float ratio=(float)sample[0]/sample[1];
 check(std::fabs(ratio-180.f/210.f)<.015f,"gamma retains input color ratio");
 check(std::fabs(std::pow(180.f/210.f,4.f)-180.f/210.f)>.25f,"old per-channel gamma fails this hue case");
 printf("hue-preserving gamma RGB=%u/%u/%u ratio=%.3f PASS\n",sample[0],sample[1],sample[2],ratio);
 ctx->Unmap(cpu,0);
 for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x)
     pixels[y*size+x]=x<size/2 ? 0xff0000ff : 0xff00ff00;
 ctx->UpdateSubresource(src,0,nullptr,pixels.data(),size*4,0);
 for(int side=0;side<3;++side) {
   dvr::gfx::AlphaParams crop;crop.mode=1;
   if(side<2) {crop.sourceRect[0]=side?.6f:.1f;crop.sourceRect[2]=side?.9f:.4f;}
   blit.draw(ctx,srv,rtv,size,size,&crop);ctx->CopyResource(cpu,dst);
   D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(ctx->Map(cpu,0,D3D11_MAP_READ,0,&m)),"crop readback");
   for(unsigned x: {32u,224u}) {
     auto p=(const unsigned char*)m.pData+128*m.RowPitch+x*4;
     const bool red=side==0 || (side==2 && x<size/2);
     check(p[red?0:1]>250 && p[red?1:0]<5 && p[3]==255,"crop selects intended source and identity reset restores full image");
   }
   ctx->Unmap(cpu,0);
 }
 puts("left/right side crops and identity restoration PASS");
 // Native marker composition must preserve black strokes, additive highlights,
 // alpha coverage and the destination when the target contains no marker.
 check(blit.composite_ready(),"premultiplied compositor ready");
 for(int mode=0;mode<4;++mode) {
   const unsigned color=mode==0?0u:mode==1?0x80000000u:mode==2?0x00000040u:0x80000040u;
   for(auto& pixel:pixels)pixel=color;
   ctx->UpdateSubresource(src,0,nullptr,pixels.data(),size*4,0);
   const float background[4]={.2f,.4f,.6f,1.f};ctx->ClearRenderTargetView(rtv,background);
   dvr::gfx::AlphaParams a;a.mode=1;blit.draw(ctx,srv,rtv,size,size,&a,true);
   ctx->CopyResource(cpu,dst);D3D11_MAPPED_SUBRESOURCE m{};
   check(SUCCEEDED(ctx->Map(cpu,0,D3D11_MAP_READ,0,&m)),"marker readback");
   const auto* p=(const unsigned char*)m.pData;
   const float alpha=(color>>24)/255.f;
   for(int channel=0;channel<3;++channel) {
     const float srcValue=((color>>(8*channel))&255)/255.f;
     const int expected=(int)((srcValue+background[channel]*(1-alpha))*255+.5f);
     check(std::abs((int)p[channel]-expected)<=1,"native RGB blend retained after composition");
   }
   check(p[3]==255,"opaque world alpha retained");ctx->Unmap(cpu,0);
   printf("marker composition mode=%d PASS\n",mode);
 }
 // Subtitle options must not recolor unrelated pixels, and a cropped source must
 // use full-texture UV for the backdrop's band.
 for(auto& pixel:pixels) pixel=0;
 for(unsigned y=120;y<136;++y) for(unsigned x=120;x<136;++x) pixels[y*size+x]=0xffffffff;
 ctx->UpdateSubresource(src,0,nullptr,pixels.data(),size*4,0);
 for(int option=0;option<4;++option) {
   dvr::gfx::AlphaParams a;a.mode=1;a.subtitle=option!=0;
   a.subtitleColor=option==1 ? 2 : 0;a.subtitleColorRgb[0]=1;a.subtitleColorRgb[1]=.88f;a.subtitleColorRgb[2]=.28f;
   a.subtitleOutline=option==2 ? 1.f:0.f;a.subtitleOutlinePx=2.f;a.invSize[0]=a.invSize[1]=1.f/size;
   a.subtitleBackground=option==3 ? .3f:0.f;
   a.subtitleRect[0]=.25f;a.subtitleRect[1]=.25f;a.subtitleRect[2]=.75f;a.subtitleRect[3]=.75f;
   blit.draw(ctx,srv,rtv,size,size,&a);ctx->CopyResource(cpu,dst);
   D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(ctx->Map(cpu,0,D3D11_MAP_READ,0,&m)),"subtitle readback");
   const auto at=[&](unsigned x,unsigned y){return (const unsigned char*)m.pData+y*m.RowPitch+x*4;};
   const auto glyph=at(128,128), edge=at(119,128), band=at(80,80), outside=at(16,16);
   check(outside[3]==0,"subtitle background stays in its configured band");
   if(option==0) check(glyph[0]==255 && glyph[3]==255 && edge[3]==0 && band[3]==0,"disabled options preserve the original glyph");
   if(option==1) check(glyph[0]==255 && std::abs((int)glyph[1]-224)<2 && std::abs((int)glyph[2]-71)<2,"warm subtitle color");
   if(option==2) check(edge[0]==0 && edge[3]>240 && band[3]==0,"black glyph outline");
   if(option==3) check(band[0]==0 && std::abs((int)band[3]-77)<2,"premultiplied black subtitle backdrop");
   ctx->Unmap(cpu,0);
 }
 puts("subtitle disabled/color/outline/backdrop PASS");
 blit.shutdown();rtv->Release();srv->Release();cpu->Release();dst->Release();src->Release();ctx->Release();dev->Release();
}
