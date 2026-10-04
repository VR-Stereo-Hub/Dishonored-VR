// Tests the actual asynchronous image writer with a software D3D11 device.
// No game process, headset, or engine memory is involved.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <process.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
static ID3D11Device* g_dev11=nullptr;
static ID3D11DeviceContext* g_ctx11=nullptr;
static void Log(const char*,...){}
#include "core/gfx/frame_dump_io.inc"
static int failures=0;
static void check(bool ok,const char* name){if(!ok){++failures;printf("FAIL: %s\n",name);}}
static bool drain(){
    const auto until=GetTickCount64()+10000;
    while(g_dumpPngPending&&GetTickCount64()<until)Sleep(1);
    return g_dumpPngPending==0&&g_dumpBytes.load()==0;
}
int main(){
    D3D_FEATURE_LEVEL level;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&g_dev11,&level,&g_ctx11);
    if(FAILED(hr)){printf("D3D11 WARP failed %08lx\n",(unsigned long)hr);return 2;}
    // Asymmetric pixel channels expose an accidental RGBA/BGRA swap.
    const uint8_t rgba[16]={11,42,93,255, 101,33,7,255, 5,144,68,255, 211,166,34,255};
    for(int format=0;format<2;++format){
        uint8_t pixels[16];memcpy(pixels,rgba,16);
        if(format)for(int p=0;p<4;++p)std::swap(pixels[p*4],pixels[p*4+2]);
        D3D11_TEXTURE2D_DESC d{};d.Width=2;d.Height=2;d.MipLevels=1;d.ArraySize=1;
        d.Format=format?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA init{};init.pSysMem=pixels;init.SysMemPitch=8;
        ID3D11Texture2D* tex=nullptr;hr=g_dev11->CreateTexture2D(&d,&init,&tex);
        check(SUCCEEDED(hr)&&tex,"create test image");if(!tex)continue;
        for(int frame=0;frame<16;++frame){
            char path[64];sprintf_s(path,"burst-%d-%02d.bmp",format,frame);
            check(DumpTexturePng(path,tex,false,true),"queue exact production BMP writer");
        }
        check(drain(),"all workers finish and release their accounted memory");
        for(int frame=0;frame<16;++frame){
            char path[64];sprintf_s(path,"burst-%d-%02d.bmp",format,frame);
            FILE* f=nullptr;fopen_s(&f,path,"rb");check(f!=nullptr,"written frame exists");if(!f)continue;
            uint8_t data[70]{};size_t count=fread(data,1,sizeof(data),f);int extra=fgetc(f);fclose(f);
            check(count==70&&extra==EOF&&data[0]=='B'&&data[1]=='M',"BMP has complete header and exact row length");
            bool correct=true;for(int p=0;p<4;++p)for(int c=0;c<4;++c){int source=c==0?2:c==2?0:c;correct&=data[54+p*4+c]==rgba[p*4+source];}
            check(correct,"all pixel channels and row order survive readback and worker write");
        }
        const unsigned pngFailures=g_dumpFailures.load();
        check(DumpTexturePng(format?"bgra.png":"rgba.png",tex),"existing PNG dump still queues");
        check(drain()&&g_dumpFailures.load()==pngFailures,"existing PNG job finishes successfully");
        FILE* png=nullptr;fopen_s(&png,format?"bgra.png":"rgba.png","rb");
        uint8_t signature[8]{};const uint8_t expected[8]={137,80,78,71,13,10,26,10};
        check(png&&fread(signature,1,8,png)==8&&!memcmp(signature,expected,8),"existing PNG path writes a PNG signature");
        if(png)fclose(png);
        const unsigned before=g_dumpFailures.load();
        check(DumpTexturePng("missing-parent/failed.bmp",tex,false,true),"failed disk destination is detected on worker");
        check(drain()&&g_dumpFailures.load()==before+1,"disk write failure is reported and storage released");
        tex->Release();
    }
    g_ctx11->Release();g_dev11->Release();
    printf("frame burst IO: 32 BMPs, RGBA/BGRA pixel verification, PNG compatibility, failure cleanup; %d failures\n",failures);
    return failures?1:0;
}
