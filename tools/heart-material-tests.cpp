#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "game/dishonored/hands/heart_back_material_d3d.h"
#include <string>
using namespace dvr::heart;
static int checks=0,failed=0;
static void check(bool yes,const char* name){++checks;if(!yes){++failed;printf("FAIL %s\n",name);}}
static std::vector<uint8_t> bytes(const std::string& path){FILE* f=nullptr;fopen_s(&f,path.c_str(),"rb");if(!f)return {};fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);std::vector<uint8_t> b(n);fread(b.data(),1,b.size(),f);fclose(f);return b;}
static bool load(const std::vector<uint8_t>& b,MaterialAsset& a){FILE* f=nullptr;tmpfile_s(&f);if(!f)return false;fwrite(b.data(),1,b.size(),f);rewind(f);bool ok=a.load(f);fclose(f);return ok;}
struct Dds {unsigned w=0,h=0;std::vector<uint8_t> raw;bool read(const std::string& p){raw=bytes(p);if(raw.size()<128||memcmp(raw.data(),"DDS ",4)||memcmp(raw.data()+84,"DXT1",4))return false;memcpy(&h,raw.data()+12,4);memcpy(&w,raw.data()+16,4);return raw.size()==128+size_t(w)*h/2;}};
static IDirect3DTexture9* upload_bc1(IDirect3DDevice9* d,const Dds& src){IDirect3DTexture9* t=nullptr;if(FAILED(d->CreateTexture(src.w,src.h,1,0,D3DFMT_DXT1,D3DPOOL_MANAGED,&t,nullptr)))return nullptr;D3DLOCKED_RECT l{};if(FAILED(t->LockRect(0,&l,nullptr,0))){t->Release();return nullptr;}for(unsigned y=0;y<src.h/4;++y)memcpy((uint8_t*)l.pBits+y*l.Pitch,src.raw.data()+128+y*(src.w/4)*8,(src.w/4)*8);t->UnlockRect(0);return t;}
static bool bound(IDirect3DDevice9* d,unsigned stage,IDirect3DBaseTexture9* wanted){IDirect3DBaseTexture9* p=nullptr;bool ok=SUCCEEDED(d->GetTexture(stage,&p))&&p==wanted;if(p)p->Release();return ok;}
int main(int argc,char** argv){
    if(argc<3){puts("usage: heart_material_test material.bin native-dds-folder [mip-fixture-folder]");return 2;}
    auto raw=bytes(argv[1]);MaterialAsset a;check(load(raw,a),"approved material loads");if(failed)return 1;
    MaterialAsset temp;auto bad=raw;bad[0]='X';check(!load(bad,temp),"bad magic refused");
    bad=raw;bad[8]=6;check(!load(bad,temp),"extra channel refused");
    bad=raw;bad[12]=8;check(!load(bad,temp),"oversized references refused");
    bad=raw;bad[16]=12;check(!load(bad,temp),"oversized mip count refused");
    bad=raw;bad.resize(4096);check(!load(bad,temp),"truncated pixels refused");
    bad=raw;bad.push_back(0);check(!load(bad,temp),"trailing bytes refused");
    const char* names[]={"D","N","S","SP","E"};std::array<Dds,5> source;
    for(unsigned i=0;i<5;++i){
        check(source[i].read(std::string(argv[2])+"/Heart_"+names[i]+".dds"),"native BC1 fixture loads");
        check(identify_bc1(a,source[i].w,source[i].h,source[i].raw.data()+128,source[i].w*2)==int(i),"exact native channel identified");
        if(argc>3)for(unsigned w=source[i].w/2;w>=64;w/=2){Dds mip;bool ok=mip.read(std::string(argv[3])+"/"+names[i]+"-"+std::to_string(w)+".dds");float err=0;int role=ok?identify_bc1(a,w,w,mip.raw.data()+128,w*2,&err):-1;
            if(role!=int(i))printf("mip role=%u size=%u got=%d rms=%.2f\n",i,w,role,err);check(ok&&role==int(i),"streamed BC1 mip identified");}
    }
    auto noise=source[0];for(size_t i=128;i<noise.raw.size();++i)noise.raw[i]=(uint8_t)(i*71+i/29);
    check(identify_bc1(a,noise.w,noise.h,noise.raw.data()+128,noise.w*2)<0,"unrelated compressed texture refused");
    check(identify_bc1(a,source[0].w,source[0].h,source[0].raw.data()+128,1)<0,"short pitch refused");
    WNDCLASSA wc{};wc.lpfnWndProc=DefWindowProcA;wc.hInstance=GetModuleHandleA(nullptr);wc.lpszClassName="DvrHeartMaterialTest";RegisterClassA(&wc);
    HWND window=CreateWindowA(wc.lpszClassName,"Heart material host",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);IDirect3DDevice9* dev=nullptr;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=64;pp.BackBufferHeight=64;
    bool device=api&&SUCCEEDED(api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev));check(device,"hidden real D3D9 device");
    if(device){
        std::array<IDirect3DTexture9*,5> native{},replacement{};unsigned stage[]={5,9,2,12,14};
        for(unsigned i=0;i<5;++i){native[i]=upload_bc1(dev,source[i]);check(native[i]!=nullptr,"upload native BC1");check(upload_image(dev,a.images[i],&replacement[i]),"upload approved full mip chain");dev->SetTexture(stage[i],native[i]);}
        dev->SetTexture(0,native[0]);
        auto classify=[&](IDirect3DTexture9* t){D3DSURFACE_DESC d{};D3DLOCKED_RECT l{};if(FAILED(t->GetLevelDesc(0,&d))||d.Format!=D3DFMT_DXT1||FAILED(t->LockRect(0,&l,nullptr,D3DLOCK_READONLY)))return -1;int role=identify_bc1(a,d.Width,d.Height,l.pBits,l.Pitch);t->UnlockRect(0);return role;};
        {
            MaterialBinding b;check(b.prepare(dev,replacement,classify)&&b.roles==31,"classify arbitrary sampler slots");check(b.apply(),"bind replacement textures");
            for(unsigned i=0;i<5;++i)check(bound(dev,stage[i],replacement[i]),"correct material at arbitrary slot");check(bound(dev,0,replacement[0]),"duplicate binding replaced");
            check(b.restore(),"explicit restore");
        }
        for(unsigned i=0;i<5;++i)check(bound(dev,stage[i],native[i]),"native binding restored");
        {
            MaterialBinding b;check(b.prepare(dev,replacement,classify)&&b.apply(),"binding before failed draw");
            check(FAILED(dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1)),"deliberately invalid draw fails");
        }
        for(unsigned i=0;i<5;++i)check(bound(dev,stage[i],native[i]),"RAII restore after failed draw");
        {
            MaterialBinding b;check(!b.prepare(dev,replacement,[](IDirect3DTexture9*){return 99;}),"invalid classifier refuses without binding");
        }
        check(bound(dev,0,native[0]),"prepare failure leaves duplicate slot intact");
        for(unsigned i=0;i<16;++i)dev->SetTexture(i,nullptr);
        for(auto* p:native)if(p)p->Release();for(auto* p:replacement)if(p)p->Release();dev->Release();
    }
    if(api)api->Release();if(window)DestroyWindow(window);
    printf("Heart material: %d checks, %d failures\n",checks,failed);return failed?1:0;
}
