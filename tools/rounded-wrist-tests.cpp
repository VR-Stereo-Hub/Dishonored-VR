#include <d3d9.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include "game/dishonored/hands/rounded_wrist.h"
#include "core/gfx/hud_wheel_parts.h"
static int checks=0;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
constexpr int MS_MAX_CLIPV=512,MS_MAX_OUT=512,MS_MAX_STRIDE=32,MS_CLS_HAND_A=1;
static int g_msClipN=0,g_msOutN=0,g_msCapSegN=8,g_msCapTris=0,g_msVerts=8,g_msNel=2;
static UINT g_msStride=32;
static bool g_msCapTwo=true;
static float g_msRoundDepth=.35f,g_msAxis[3][3]={{},{0,0,1},{0,0,1}};
static uint8_t original[8][32],g_msClipRaw[MS_MAX_CLIPV*MS_MAX_STRIDE];
static D3DVERTEXELEMENT9 g_msEl[2]={{0,8,D3DDECLTYPE_UBYTE4,0,D3DDECLUSAGE_BLENDINDICES,0},{0,12,D3DDECLTYPE_UBYTE4N,0,D3DDECLUSAGE_BLENDWEIGHT,0}};
struct Segment {uint32_t a,b;int sd;};static Segment g_msCapSeg[8];
struct Tri {uint32_t a,b,c;};static std::vector<Tri> triangles;
static const uint8_t* MsRawOf(uint32_t i){return i<8?original[i]:g_msClipRaw+(i-8)*32;}
static void MsEmit(uint32_t a,uint32_t b,uint32_t c,int){triangles.push_back({a,b,c});++g_msOutN;}
static void Log(const char*,...){}
#include "rounded_wrist_production.inc"
int main(){
    uint32_t ring[16];const float center[3]={0,0,0};
    for(int i=0;i<8;++i){
        memset(original[i],i+1,32);const float a=i*6.28318530718f/8,p[3]={2*std::cos(a),2*std::sin(a),0};
        memcpy(original[i]+16,p,12);g_msCapSeg[i]={(uint32_t)i,(uint32_t)((i+1)%8),1};
        ring[i*2]=i;ring[i*2+1]=(i+1)%8;
    }
    CHECK(MsRoundedEnd(1,ring,16,0,center,original[0],16,-1,28,4));
    CHECK(g_msClipN==144 && g_msCapTris==112 && g_msOutN==112);
    for(int face=0;face<2;++face)for(int seg=0;seg<8;++seg){
        for(int layer=0;layer<4;++layer)for(int end=0;end<2;++end){
            const auto* v=MsRawOf(8+(face*8+seg)*9+layer*2+end);const auto* donor=original[(seg+end)%8];
            CHECK(!memcmp(v+8,donor+8,8));CHECK(!memcmp(v,donor,8));CHECK(!memcmp(v+28,original[0]+28,4));
            if(layer==0)CHECK(!memcmp(v+16,donor+16,12));
        }
        float tip[3];memcpy(tip,MsRawOf(8+(face*8+seg)*9+8)+16,12);
        CHECK(std::fabs(tip[0])<1e-6f && std::fabs(tip[1])<1e-6f && std::fabs(tip[2]+.7f)<1e-6f);
        CHECK(!memcmp(MsRawOf(8+(face*8+seg)*9+8)+8,original[0]+8,8));
    }
    for(size_t i=0;i<triangles.size();++i){
        float a[3],b[3],c[3],u[3],v[3],mid[3];auto t=triangles[i];
        memcpy(a,MsRawOf(t.a)+16,12);memcpy(b,MsRawOf(t.b)+16,12);memcpy(c,MsRawOf(t.c)+16,12);
        for(int k=0;k<3;++k){u[k]=b[k]-a[k];v[k]=c[k]-a[k];mid[k]=(a[k]+b[k]+c[k])/3;CHECK(std::isfinite(mid[k]));}
        const float cross[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
        const float dot=cross[0]*mid[0]/4+cross[1]*mid[1]/4+cross[2]*mid[2]/.49f;
        CHECK(i<56?dot>0:dot<0);
    }
    g_msClipN=MS_MAX_CLIPV-1;const int oldOut=g_msOutN;
    CHECK(!MsRoundedEnd(1,ring,16,0,center,original[0],16,-1,28,4));CHECK(g_msClipN==MS_MAX_CLIPV-1 && g_msOutN==oldOut);
    g_msClipN=0;g_msOutN=MS_MAX_OUT-1;
    CHECK(!MsRoundedEnd(1,ring,16,0,center,original[0],16,-1,28,4));CHECK(g_msClipN==0);
    g_msOutN=0;g_msAxis[1][2]=2;
    CHECK(!MsRoundedEnd(1,ring,16,0,center,original[0],16,-1,28,4));CHECK(g_msClipN==0);
    g_msAxis[1][2]=1;float pos[3],normal[3],rim[3]={2,0,0};
    CHECK(!dvr::wrist::point(rim,center,g_msAxis[1],.7f,std::numeric_limits<float>::quiet_NaN(),pos,normal));
    float box[4]={.02f,.29f,.995f,.31f},rect[4];
    CHECK(dvr::wheelparts::crop(0,1280,720,box,rect));CHECK(std::fabs((rect[3]-rect[1])*720-396.8f)<.001f);
    CHECK(dvr::wheelparts::crop(0,3012,3122,box,rect));CHECK(std::fabs((rect[3]-rect[1])*3122-933.72f)<.001f);
    CHECK(!dvr::wheelparts::crop(2,1280,720,box,rect));box[0]=.9f;
    CHECK(!dvr::wheelparts::crop(0,1280,720,box,rect));box[0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(!dvr::wheelparts::crop(0,1280,720,box,rect));
    std::printf("rounded wrist and wheel crop: %d checks PASS\n",checks);
}
