// Draw the locally authored Heart back in the verified Heart material passes.
// Own buffers only: no UObject or engine-memory writes. The weapon router
// supplies current instance validation, palette and foreground depth marking.
#include "heart_back_data.h"
#include "heart_back_material_d3d.h"
namespace {
bool g_hbOn=false;std::atomic<bool> g_hbReload{false},g_hbDump{false};
struct HbEntry {
    void* source=nullptr;void* sourceIb=nullptr;IDirect3DDevice9* device=nullptr;
    UINT offset=0,stride=0,first=0,count=0,min=0,verts=0;INT base=0;
    std::vector<D3DVERTEXELEMENT9> decl;
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DVertexBuffer9* colors=nullptr;IDirect3DIndexBuffer9* ib=nullptr;
    unsigned numVerts=0,numTris=0,capTris=0,slots=0;bool tried=false,ready=false;
    dvr::heart::MaterialAsset material;
    std::array<IDirect3DTexture9*,dvr::heart::kMaterialCount> textures{};
    struct TextureMatch {void* source=nullptr;uint64_t serial=0;uint32_t writes=0;UINT width=0,height=0;int role=-1;};
    std::array<TextureMatch,32> matches{};unsigned nextMatch=0;
} g_hb;
const char* g_hbWhy="not drawn";unsigned g_hbDraws=0,g_hbFailures=0,g_hbTextured=0,g_hbMaterialWaits=0;
}
static void HbRelease(const char* why){
    if(g_hb.vb)g_hb.vb->Release();if(g_hb.ib)g_hb.ib->Release();
    if(g_hb.colors)g_hb.colors->Release();
    for(auto* t:g_hb.textures)if(t)t->Release();
    g_hb=HbEntry{};g_hbWhy=why;
}
static void HbSet(bool on){g_hbOn=on;ConfigWriteKey("Hands","HeartBack",on?"1":"0","Heart back");Log("heartback: %s",on?"ON":"off");}
static bool HbEnabled(){return g_hbOn;}
static void HbConfigure(const char* ini){g_hbOn=GetPrivateProfileIntA("Hands","HeartBack",1,ini)!=0;}
static bool HbCommand(const char* args){
    if(!strcmp(args,"on"))HbSet(true);else if(!strcmp(args,"off"))HbSet(false);
    else if(!strcmp(args,"reload"))g_hbReload=true;
    else if(!strcmp(args,"dump")){g_hbDump=true;g_hbReload=true;}
    Log("heartback: %s, ready=%d, draws=%u, failed=%u, textured=%u, material-waits=%u, %s (on|off|reload|status|dump)",g_hbOn?"ON":"off",g_hb.ready,g_hbDraws,g_hbFailures,g_hbTextured,g_hbMaterialWaits,g_hbWhy);return true;
}
static bool HbRefuse(const char* why){g_hbWhy=why;++g_hbFailures;Log("heartback: refused - %s",why);return false;}
static bool HbBytes(uint8_t* p,int type,const uint8_t b[4]){
    if(type==D3DDECLTYPE_UBYTE4||type==D3DDECLTYPE_UBYTE4N){memcpy(p,b,4);return true;}
    if(type==D3DDECLTYPE_D3DCOLOR){p[0]=b[2];p[1]=b[1];p[2]=b[0];p[3]=b[3];return true;}
    return false;
}
static bool HbVector(uint8_t* p,int type,const float v[3],float sign){
    if(type==D3DDECLTYPE_FLOAT3||type==D3DDECLTYPE_FLOAT4){memcpy(p,v,12);if(type==D3DDECLTYPE_FLOAT4)memcpy(p+12,&sign,4);return true;}
    uint8_t b[4];for(int k=0;k<3;++k)b[k]=(uint8_t)std::clamp((int)lroundf((v[k]+1)*127.5f),0,255);b[3]=(uint8_t)std::clamp((int)lroundf((sign+1)*127.5f),0,255);
    return HbBytes(p,type,b);
}
static int HbMaterialRole(IDirect3DTexture9* texture){
    D3DSURFACE_DESC d{};if(FAILED(texture->GetLevelDesc(0,&d))||d.Format!=D3DFMT_DXT1||d.Width!=d.Height||d.Width<64||d.Width>4096)return -1;
    uint64_t serial=0;uint32_t writes=0;
    const bool tracked=dvr::d3d9ex::texture_stamp(texture,&serial,&writes);
    if(tracked)for(const auto& c:g_hb.matches)if(c.source==texture&&c.serial==serial&&c.writes==writes&&c.width==d.Width&&c.height==d.Height)return c.role;
    D3DLOCKED_RECT lr{};if(FAILED(texture->LockRect(0,&lr,nullptr,D3DLOCK_READONLY)))return -1;
    int role=-1;
    if(lr.Pitch>0)role=dvr::heart::identify_bc1(g_hb.material,d.Width,d.Height,lr.pBits,(size_t)lr.Pitch);
    if(FAILED(texture->UnlockRect(0)))return -1;
    if(tracked)g_hb.matches[g_hb.nextMatch++%g_hb.matches.size()]={texture,serial,writes,d.Width,d.Height,role};
    if(role>=0)DVR_LOG_FIRST_N(DVR_CAT,::dvr::log::Level::Info,20,"heartback/material: exact native role=%d size=%ux%u generation=%llu writes=%u",role,d.Width,d.Height,serial,writes);
    return role;
}
static bool HbBuild(IDirect3DDevice9* dev,IDirect3DVertexBuffer9* source,UINT slots){
    using namespace dvr::ik;
    g_hb.tried=true;char path[MAX_PATH];FILE* f=nullptr;Rig rig;dvr::heart::Model model;
    dvr::paths::in_data_dir(path,"dishonored_vr_heart_rig.bin");fopen_s(&f,path,"rb");bool ok=rig.load(f);if(f)fclose(f);
    if(!ok)return HbRefuse("missing or invalid local Heart rig");
    if(rig.triangles!=g_hb.count)return HbRefuse("draw does not match Heart body reference");
    dvr::paths::in_data_dir(path,"dishonored_vr_heart_back.bin");f=nullptr;fopen_s(&f,path,"rb");ok=model.load(f,(unsigned)rig.bones.size());if(f)fclose(f);
    if(!ok)return HbRefuse("missing or invalid local Heart back");
    MsElem pos{},weight{},bones{},normal{},tangent{},uv{};bool colorStream=false;
    for(const auto& e:g_hb.decl){
        if(e.Stream==0xff)break;
        Log("heartback/decl: stream=%u off=%u type=%u usage=%u index=%u stride=%u",e.Stream,e.Offset,e.Type,e.Usage,e.UsageIndex,g_hb.stride);
        const int size=MsTypeSize(e.Type);
        if(e.Stream==1&&e.Offset==0&&e.Type==D3DDECLTYPE_D3DCOLOR&&e.Usage==D3DDECLUSAGE_COLOR&&e.UsageIndex==0){colorStream=true;continue;}
        if(e.Stream||!size||(unsigned)(e.Offset+size)>g_hb.stride)return HbRefuse("unsupported vertex stream or element size");
        if(e.UsageIndex)continue;
        MsElem* out=nullptr;
        if(e.Usage==D3DDECLUSAGE_POSITION)out=&pos;else if(e.Usage==D3DDECLUSAGE_BLENDWEIGHT)out=&weight;
        else if(e.Usage==D3DDECLUSAGE_BLENDINDICES)out=&bones;else if(e.Usage==D3DDECLUSAGE_NORMAL)out=&normal;
        else if(e.Usage==D3DDECLUSAGE_TANGENT)out=&tangent;else if(e.Usage==D3DDECLUSAGE_TEXCOORD)out=&uv;
        if(out){out->off=e.Offset;out->type=e.Type;out->have=true;}
    }
    if(!pos.have||pos.type!=D3DDECLTYPE_FLOAT3||!weight.have||!bones.have||!normal.have||!tangent.have||!uv.have)
        return HbRefuse("incomplete Heart vertex declaration");
    if(g_hb.verts<3||g_hb.verts>8192||g_hb.stride>256||slots<1||slots>128)return HbRefuse("draw exceeds bounded Heart limits");
    D3DVERTEXBUFFER_DESC desc{};if(FAILED(source->GetDesc(&desc)))return HbRefuse("cannot size current vertex buffer");
    int64_t vertex=(int64_t)g_hb.base+g_hb.min;
    int64_t begin=(int64_t)g_hb.offset+vertex*g_hb.stride,bytes=(int64_t)g_hb.verts*g_hb.stride;
    if(vertex<0||begin<0||begin+bytes>desc.Size)return HbRefuse("native vertex range out of bounds");
    void* ptr=nullptr;if(FAILED(source->Lock((UINT)begin,(UINT)bytes,&ptr,D3DLOCK_READONLY))||!ptr)return HbRefuse("cannot read current native vertices");
    std::vector<uint8_t> raw((uint8_t*)ptr,(uint8_t*)ptr+bytes);source->Unlock();
    std::vector<Vertex> native(g_hb.verts);
    for(unsigned i=0;i<g_hb.verts;++i){
        auto& v=native[i];const uint8_t* p=raw.data()+size_t(i)*g_hb.stride;memcpy(&v.p,p+pos.off,12);uint8_t bi[4];
        if(!MsReadWeights(p,&weight,v.weight)||!MsReadIndices(p,&bones,bi))return HbRefuse("unsupported native skin encoding");
        for(int j=0;j<4;++j)v.bone[j]=bi[j];
    }
    Mapping map;if(!map_skin(rig,native.data(),g_hb.verts,slots,map)){Log("heartback/map: %u vertices, distance %.6f, %s",map.matched,map.nearestDistance,map.why);return HbRefuse(map.why);}
    if(g_hbDump.exchange(false)){
        dvr::paths::in_data_dir(path,"heart-native-vertices.bin");f=nullptr;fopen_s(&f,path,"wb");
        if(f){uint32_t header[3]={g_hb.stride,g_hb.verts,(uint32_t)g_hb.decl.size()};fwrite(header,4,3,f);fwrite(g_hb.decl.data(),sizeof(D3DVERTEXELEMENT9),g_hb.decl.size(),f);fwrite(raw.data(),1,raw.size(),f);fclose(f);}
    }
    int inverse[128];std::fill(inverse,inverse+128,-1);for(unsigned s=0;s<slots;++s)if(map.reference[s]>=0)inverse[map.reference[s]]=(int)s;
    std::vector<uint8_t> packed(model.vertices.size()*g_hb.stride),colorNative,colorPacked;
    if(colorStream){
        IDirect3DVertexBuffer9* c=nullptr;UINT off=0,stride=0,freq=0;D3DVERTEXBUFFER_DESC cd{};
        struct ColorRef {IDirect3DVertexBuffer9*& p;~ColorRef(){if(p)p->Release();}} scope{c};
        if(FAILED(dev->GetStreamSource(1,&c,&off,&stride))||!c||FAILED(c->GetDesc(&cd))||FAILED(dev->GetStreamSourceFreq(1,&freq)))return HbRefuse("cannot inspect native color stream");
        Log("heartback/color: offset=%u stride=%u frequency=%u bytes=%u",off,stride,freq,cd.Size);
        if((stride!=0&&stride!=4)||freq!=1)return HbRefuse("unsupported color stream layout");
        int64_t cb=(int64_t)off+vertex*stride,cs=stride?(int64_t)g_hb.verts*stride:4;
        if(cb<0||cb+cs>cd.Size)return HbRefuse("color stream range out of bounds");
        if(FAILED(c->Lock((UINT)cb,(UINT)cs,&ptr,D3DLOCK_READONLY))||!ptr)return HbRefuse("cannot read native colors");
        colorNative.assign((uint8_t*)ptr,(uint8_t*)ptr+cs);c->Unlock();colorPacked.resize(model.vertices.size()*4);
    }
    for(size_t i=0;i<model.vertices.size();++i){
        const auto& v=model.vertices[i];uint8_t* dst=packed.data()+i*g_hb.stride;
        if(colorStream){
            unsigned nearest=0;if(colorNative.size()>4){float best=INFINITY;
                for(unsigned j=0;j<native.size();++j){Vec d=native[j].p-vec(v.p);float ds=dot(d,d);if(ds<best){best=ds;nearest=j;}}
            }
            memcpy(colorPacked.data()+i*4,colorNative.data()+nearest*4,4);
        }
        memcpy(dst,raw.data(),g_hb.stride);memcpy(dst+pos.off,v.p,12);
        // Native packed tangent W is 128 (zero); normal W carries handedness.
        if(!HbVector(dst+normal.off,normal.type,v.n,v.sign)||!HbVector(dst+tangent.off,tangent.type,v.t,0))return HbRefuse("unsupported tangent encoding");
        if(uv.type==D3DDECLTYPE_FLOAT2)memcpy(dst+uv.off,v.uv,8);
        else if(uv.type==D3DDECLTYPE_FLOAT16_2){uint16_t h[2]={dvr::heart::half(v.uv[0]),dvr::heart::half(v.uv[1])};memcpy(dst+uv.off,h,4);}
        else return HbRefuse("unsupported texture-coordinate encoding");
        uint8_t bi[4]{},bw[4]{};int sum=0;
        for(int j=0;j<4;++j){int s=inverse[v.bone[j]];if(v.weight[j]>0&&s<0)return HbRefuse("added vertex uses an unmapped bone");bi[j]=(uint8_t)std::max(0,s);bw[j]=(uint8_t)std::clamp((int)lroundf(v.weight[j]*255),0,255);sum+=bw[j];}
        bw[0]=(uint8_t)((int)bw[0]+255-sum);
        if(!HbBytes(dst+bones.off,bones.type,bi))return HbRefuse("unsupported blend-index encoding");
        if(weight.type==D3DDECLTYPE_FLOAT4)memcpy(dst+weight.off,v.weight,16);
        else if(!HbBytes(dst+weight.off,weight.type,bw))return HbRefuse("unsupported blend-weight encoding");
    }
    if(FAILED(dev->CreateVertexBuffer((UINT)packed.size(),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&g_hb.vb,nullptr))||
       FAILED(dev->CreateIndexBuffer((UINT)model.indices.size()*4,D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_MANAGED,&g_hb.ib,nullptr)))return HbRefuse("cannot allocate own Heart buffers");
    if(FAILED(g_hb.vb->Lock(0,(UINT)packed.size(),&ptr,0))||!ptr)return HbRefuse("cannot fill own Heart vertices");
    memcpy(ptr,packed.data(),packed.size());g_hb.vb->Unlock();
    if(FAILED(g_hb.ib->Lock(0,(UINT)model.indices.size()*4,&ptr,0))||!ptr)return HbRefuse("cannot fill own Heart indices");
    memcpy(ptr,model.indices.data(),model.indices.size()*4);g_hb.ib->Unlock();
    if(colorStream){
        if(FAILED(dev->CreateVertexBuffer((UINT)colorPacked.size(),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&g_hb.colors,nullptr))||
           FAILED(g_hb.colors->Lock(0,(UINT)colorPacked.size(),&ptr,0))||!ptr)return HbRefuse("cannot create own color buffer");
        memcpy(ptr,colorPacked.data(),colorPacked.size());g_hb.colors->Unlock();
    }
    g_hb.capTris=model.capTriangles;
    if(g_hb.capTris){
        dvr::paths::in_data_dir(path,"dishonored_vr_heart_material.bin");f=nullptr;fopen_s(&f,path,"rb");ok=g_hb.material.load(f);if(f)fclose(f);
        if(!ok)return HbRefuse("missing or invalid local Heart material");
        for(unsigned i=0;i<dvr::heart::kMaterialCount;++i){
            if(!dvr::heart::upload_image(dev,g_hb.material.images[i],&g_hb.textures[i]))return HbRefuse("cannot upload own Heart material");
            // References remain small; uploaded pixel copies can leave CPU memory.
            g_hb.material.images[i].levels.clear();
        }
        Log("heartback/material: loaded five matched material channels for %u cap triangles",g_hb.capTris);
    }
    g_hb.numVerts=(unsigned)model.vertices.size();g_hb.numTris=(unsigned)model.indices.size()/3;g_hb.ready=true;g_hbWhy="validated and drawn";
    Log("heartback: validated ALL %u native vertices, %u palette slots, %.7f max weight error; added %u vertices/%u triangles",map.matched,slots,map.worstWeight,g_hb.numVerts,g_hb.numTris);return true;
}
static void HbDraw(IDirect3DDevice9* dev,const WaMesh* w,UINT slots,D3DPRIMITIVETYPE type,INT base,UINT min,UINT verts,UINT first,UINT count){
    if(g_hbReload.exchange(false))HbRelease("reload requested");
    if(!g_hbOn||!w||strcmp(w->asset,"Heart")||type!=D3DPT_TRIANGLELIST||count!=3672)return;
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;UINT off=0,stride=0,n=MAXD3DDECLLENGTH+1;
    // Temporary Get* references are released in this scope; no game COM object
    // is retained by the cache. Contract invalidation drops our buffers on loads.
    struct Scope {IDirect3DVertexBuffer9*& v;IDirect3DIndexBuffer9*& i;IDirect3DVertexDeclaration9*& d;~Scope(){if(v)v->Release();if(i)i->Release();if(d)d->Release();}} refs{vb,ib,decl};
    D3DVERTEXELEMENT9 elems[MAXD3DDECLLENGTH+1];
    if(FAILED(dev->GetStreamSource(0,&vb,&off,&stride))||!vb||FAILED(dev->GetIndices(&ib))||!ib||FAILED(dev->GetVertexDeclaration(&decl))||!decl||FAILED(decl->GetDeclaration(elems,&n)))return;
    if(n>MAXD3DDECLLENGTH+1)return;
    if(g_hb.source!=vb||g_hb.sourceIb!=ib||g_hb.device!=dev||g_hb.offset!=off||g_hb.stride!=stride||g_hb.first!=first||g_hb.count!=count||g_hb.min!=min||g_hb.verts!=verts||g_hb.base!=base||g_hb.slots!=slots||g_hb.decl.size()!=n||memcmp(g_hb.decl.data(),elems,n*sizeof(elems[0]))){
        HbRelease("current Heart draw changed");g_hb.source=vb;g_hb.sourceIb=ib;g_hb.device=dev;g_hb.offset=off;g_hb.stride=stride;g_hb.first=first;g_hb.count=count;g_hb.min=min;g_hb.verts=verts;g_hb.base=base;g_hb.slots=slots;g_hb.decl.assign(elems,elems+n);
    }
    if(!g_hb.tried&&!HbBuild(dev,vb,slots))return;if(!g_hb.ready)return;
    dvr::heart::MaterialBinding material;
    if(g_hb.capTris){
        if(!material.prepare(dev,g_hb.textures,HbMaterialRole)){++g_hbFailures;DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Warn,3000,"heartback/material: cannot snapshot native textures");return;}
        DWORD color=0;if(FAILED(dev->GetRenderState(D3DRS_COLORWRITEENABLE,&color)))return;
        if(!material.roles&&color){
            ++g_hbMaterialWaits;
            DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Warn,3000,"heartback/material: color pass has no verified Heart textures; cap refused (waits=%u)",g_hbMaterialWaits);
            return;
        }
    }
    DWORD cull=0;if(FAILED(dev->GetRenderState(D3DRS_CULLMODE,&cull)))return;
    IDirect3DVertexBuffer9* colors=nullptr;UINT colorOffset=0,colorStride=0;
    struct ColorRestore {IDirect3DVertexBuffer9*& p;~ColorRestore(){if(p)p->Release();}} colorRef{colors};
    if(g_hb.colors&&(FAILED(dev->GetStreamSource(1,&colors,&colorOffset,&colorStride))||!colors))return;
    HRESULT hr=dev->SetStreamSource(0,g_hb.vb,0,stride);
    if(SUCCEEDED(hr)&&g_hb.colors)hr=dev->SetStreamSource(1,g_hb.colors,0,4);
    if(SUCCEEDED(hr))hr=dev->SetIndices(g_hb.ib);
    if(SUCCEEDED(hr))hr=dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    if(SUCCEEDED(hr)&&g_hb.capTris){
        if(!material.apply())hr=E_FAIL;
        else hr=dvr::frame::orig_draw_indexed(dev,D3DPT_TRIANGLELIST,0,0,g_hb.numVerts,0,g_hb.capTris);
        if(SUCCEEDED(hr)&&material.roles)++g_hbTextured;
    }
    const bool materialRestored=material.restore();
    // Preserve cap-then-detail ordering even in passes without depth writes.
    if(materialRestored&&SUCCEEDED(hr)&&g_hb.numTris>g_hb.capTris)
        hr=dvr::frame::orig_draw_indexed(dev,D3DPT_TRIANGLELIST,0,0,g_hb.numVerts,g_hb.capTris*3,g_hb.numTris-g_hb.capTris);
    const HRESULT rv=dev->SetStreamSource(0,vb,off,stride),ri=dev->SetIndices(ib),rc=dev->SetRenderState(D3DRS_CULLMODE,cull);
    const HRESULT rcolor=colors?dev->SetStreamSource(1,colors,colorOffset,colorStride):D3D_OK;
    if(materialRestored&&SUCCEEDED(hr)&&SUCCEEDED(rv)&&SUCCEEDED(ri)&&SUCCEEDED(rc)&&SUCCEEDED(rcolor))++g_hbDraws;
    else {++g_hbFailures;DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Warn,3000,"heartback: draw/restore failed %08lx/%08lx/%08lx/%08lx",hr,rv,ri,rc);}
}
