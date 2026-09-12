// Included by weapon_attach. Measurement only: never alters a draw or game object.
#include "game/dishonored/hands/bolt_axis.h"
static SRWLOCK g_brLock=SRWLOCK_INIT;
static dvr::hands::ModelRaySnapshot g_brRay[2];
namespace dvr::hands {
ModelRaySnapshot model_ray_snapshot(int hand) {
    ModelRaySnapshot r;
    if(hand<0||hand>1)return r;
    AcquireSRWLockShared(&g_brLock);r=g_brRay[hand];ReleaseSRWLockShared(&g_brLock);
    return r;
}
}
struct BrGeometry {
    void *vb=nullptr,*ib=nullptr,*decl=nullptr;
    UINT offset=0,stride=0,start=0,count=0,prims=0,minIndex=0;
    INT base=0;
    dvr::hf::BoltAxis axis;
    int bone=-1,sign=0;
    bool ok=false;
    uint64_t tried=0;
};
static BrGeometry g_brGeom[2];
// WHICH meshes are candidates. A NAME test only, and the name is not what makes
// this safe: the geometry gates below are. Rigid single-bone skinning, 16:1 axial
// variance, an unambiguous forward sign, bounded counts and validated index ranges
// all still apply, so a weapon BODY cannot be aimed from even if one were named -
// a crossbow's widest axis is its bow arms, across the barrel, and it fails 16:1.
//
// The loaded projectile IS the barrel axis, for every ranged weapon in this game.
// The pistol already has one: asset `Gun_bullet_regular` on component pBulletMesh,
// exactly as the crossbow has `bolt_01` on pArrowMesh_HighRes. Only the regular
// bolt was accepted before, which is why the pistol and the poison bolt had no
// laser and fired to head aim - the fallback was correct, the gate was not.
static bool BrIsLoadedProjectile(const char* a)
{
    if (!a || !*a) return false;
    if (!_stricmp(a, "bolt_01")) return true;
    if (!_strnicmp(a, "Bolt", 4)) return true;          // Bolt_Flare and the variants
    if (strstr(a, "bullet") || strstr(a, "Bullet")) return true;   // Gun_bullet_regular
    return false;
}

static void BrRefuse(const char* why) {
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Warn,5000,"modelray: unavailable: %s",why);
}
// The same, naming the asset. A candidate that fails must say WHICH asset and
// WHICH test, or an unmeasurable ammunition type looks identical to the feature
// being switched off.
static void BrRefuseAsset(const char* asset, const char* why) {
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Warn,5000,
        "modelray: '%s' REFUSED: %s. The guide is off and firing keeps the engine's "
        "own aim for this weapon - that is the honest fallback, not a fix. Accepted "
        "candidates are the loaded bolt and the loaded bullet; a weapon body is "
        "deliberately never one, because its longest axis is not its barrel.",
        asset ? asset : "?", why);
}
static bool BrReadGeometry(IDirect3DDevice9* dev,WaMesh* w,BrGeometry& g) {
    g={};g.vb=w->vb;g.ib=w->ib;g.decl=w->decl;g.offset=w->streamOffset;
    g.stride=w->stride;g.start=w->startIndex;g.count=w->numVerts;g.prims=w->primCount;
    g.base=w->baseVertex;g.minIndex=w->minIndex;g.tried=GetTickCount64();
    if(w->type!=D3DPT_TRIANGLELIST||w->numVerts<16||w->numVerts>1024||w->primCount>4096||w->stride>128)return false;
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH];UINT en=MAXD3DDECLLENGTH;
    IDirect3DVertexDeclaration9* decl=nullptr;
    if(FAILED(dev->GetVertexDeclaration(&decl))||!decl)return false;
    const HRESULT dh=decl->GetDeclaration(el,&en);decl->Release();
    if(FAILED(dh)||en>MAXD3DDECLLENGTH)return false;
    MsElem pos={},wt={},bi={};
    for(UINT i=0;i<en;++i){
        if(el[i].Type==D3DDECLTYPE_UNUSED)continue;
        MsElem* dst=nullptr;
        if(el[i].Usage==D3DDECLUSAGE_POSITION&&el[i].UsageIndex==0)dst=&pos;
        if(el[i].Usage==D3DDECLUSAGE_BLENDWEIGHT)dst=&wt;
        if(el[i].Usage==D3DDECLUSAGE_BLENDINDICES)dst=&bi;
        if(dst){if(el[i].Stream||dst->have)return false;*dst={(int)el[i].Offset,(int)el[i].Type,1};}
    }
    const int weightBytes=wt.type<=D3DDECLTYPE_FLOAT4?(wt.type+1)*4:4;
    if(!pos.have||!wt.have||!bi.have||pos.type!=D3DDECLTYPE_FLOAT3||pos.off+12>(int)w->stride||
       wt.off+weightBytes>(int)w->stride||bi.off+4>(int)w->stride)return false;
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;UINT off=0,stride=0;
    if(FAILED(dev->GetStreamSource(0,&vb,&off,&stride))||!vb)return false;
    if(FAILED(dev->GetIndices(&ib))||!ib){vb->Release();return false;}
    bool ok=false;
    do {
        D3DVERTEXBUFFER_DESC vd;D3DINDEXBUFFER_DESC id;
        if(FAILED(vb->GetDesc(&vd))||FAILED(ib->GetDesc(&id)))break;
        if(id.Format!=D3DFMT_INDEX16&&id.Format!=D3DFMT_INDEX32)break;
        const UINT is=id.Format==D3DFMT_INDEX16?2:4;
        const uint64_t io=(uint64_t)w->startIndex*is,il=(uint64_t)w->primCount*3*is;
        const int64_t first=(int64_t)w->baseVertex+w->minIndex;
        const uint64_t vo=(uint64_t)off+(first<0?0:(uint64_t)first)*stride,vl=(uint64_t)w->numVerts*stride;
        if(first<0||stride!=w->stride||off!=w->streamOffset||io+il>id.Size||vo+vl>vd.Size)break;
        bool used[1024]={};void* data=nullptr;
        if(FAILED(ib->Lock((UINT)io,(UINT)il,&data,(id.Usage&D3DUSAGE_WRITEONLY)?0:D3DLOCK_READONLY))||!data)break;
        bool valid=true;
        for(UINT i=0;i<w->primCount*3;++i){const UINT x=is==2?((uint16_t*)data)[i]:((uint32_t*)data)[i];
            if(x<w->minIndex||x-w->minIndex>=w->numVerts){valid=false;break;}used[x-w->minIndex]=true;}
        ib->Unlock();if(!valid)break;
        if(FAILED(vb->Lock((UINT)vo,(UINT)vl,&data,(vd.Usage&D3DUSAGE_WRITEONLY)?0:D3DLOCK_READONLY))||!data)break;
        float points[1024][3];int n=0,bone=-1;
        for(UINT i=0;i<w->numVerts&&valid;++i)if(used[i]) {
            const uint8_t* v=(const uint8_t*)data+i*stride;float weights[4];uint8_t indices[4];
            if(!MsReadWeights(v,&wt,weights)||!MsReadIndices(v,&bi,indices)){valid=false;break;}
            int chosen=-1;float sum=0;
            for(int j=0;j<4;++j){if(!std::isfinite(weights[j])||weights[j]<0){valid=false;break;}
                sum+=weights[j];if(weights[j]>0.0001f){if(chosen>=0&&chosen!=indices[j])valid=false;chosen=indices[j];}}
            if(chosen<0||fabsf(sum-1)>0.001f||(bone>=0&&bone!=chosen)){valid=false;break;}
            bone=chosen;memcpy(points[n++],v+pos.off,12);
        }
        vb->Unlock();
        if(!valid||!dvr::hf::bolt_axis(points,n,g.axis))break;
        g.bone=bone;g.ok=true;ok=true;
    }while(false);
    ib->Release();vb->Release();return ok;
}
static void BrMeasure(IDirect3DDevice9* dev,WaMesh* w,const float* palette,UINT regs,const dvr::hf::Xform& delta) {
    if(!dvr::aim::model_ray_requested()||!BrIsLoadedProjectile(w->asset)||w->hand<0||w->hand>1)return;
    const WaCommon* wc=WaCommonFor(w->hand,nullptr);
    if(!wc||!w->heldOk||w->heldPresent!=(uint32_t)dvr::frame::count()||!w->lastL2WOk)return;
    // Only the color view, and only a currently verified HELD instance. No shadow/world samples.
    D3DVIEWPORT9 vp;DWORD color=0;
    if(FAILED(dev->GetViewport(&vp))||FAILED(dev->GetRenderState(D3DRS_COLORWRITEENABLE,&color))||!color||g_pcLayVp<0)return;
    IDirect3DSurface9* target=nullptr;
    if(FAILED(dev->GetRenderTarget(0,&target))||!target)return;
    const bool sameTarget=target==wc->target;target->Release();
    // Depth compression may have been removed for the corrected draw. XY extent
    // and target identity still distinguish the hand's actual color view.
    if(!sameTarget||vp.X!=wc->viewport.X||vp.Y!=wc->viewport.Y||
       vp.Width!=wc->viewport.Width||vp.Height!=wc->viewport.Height)return;
    static uint32_t measured[2]={~0u,~0u};
    if(measured[w->hand]==wc->present)return;
    auto& g=g_brGeom[w->hand];const uint64_t now=GetTickCount64();
    const bool same=g.vb==w->vb&&g.ib==w->ib&&g.decl==w->decl&&g.stride==w->stride&&g.offset==w->streamOffset&&
        g.start==w->startIndex&&g.count==w->numVerts&&g.prims==w->primCount&&g.base==w->baseVertex&&g.minIndex==w->minIndex;
    if(!same||(!g.ok&&now-g.tried>5000))if(!BrReadGeometry(dev,w,g)){
            BrRefuseAsset(w->asset,"not a supported rigid elongated mesh (needs single-bone "
                                   "rigid skinning, 16:1 axial variance, and a readable "
                                   "position/weight/index layout)");
            return;
        }
    if(!g.ok||g.bone<0||(UINT)(g.bone*3+3)>regs)return;
    dvr::hf::Xform skin;const float* b=palette+g.bone*12;
    for(int r=0;r<3;++r){for(int c=0;c<3;++c)skin.r.m[r*3+c]=b[r*4+c];skin.t[r]=b[r*4+3];}
    float l2w[16];
    if(g_pcLayL2W<0||FAILED(dev->GetVertexShaderConstantF(g_pcLayL2W,l2w,4)))return;
    dvr::hf::Xform draw;
    for(int r=0;r<3;++r){draw.t[r]=l2w[12+r];for(int c=0;c<3;++c)draw.r.m[r*3+c]=l2w[c*4+r];}
    const auto native=dvr::hf::xform_mul(draw,skin);
    if(!g.sign){float dir[3];dvr::hf::mulv3(native.r,g.axis.dir,dir);float len=0,dot=0;
        for(int i=0;i<3;++i){len+=dir[i]*dir[i];dot+=dir[i]*wc->forward[i];}
        if(len<1e-8f||fabsf(dot)/sqrtf(len)<0.7f){
            BrRefuseAsset(w->asset,"its forward sign is ambiguous against the native view "
                                   "(the fitted axis is more than 45 degrees off the weapon's "
                                   "own forward, so which end is the tip cannot be decided)");
            return;
        }
        g.sign=dot>0?1:-1;
        Log("modelray: measured '%s' axis - rigid slot %d, variance ratio %.1f (16:1 "
            "required), length %.3f, sign %+d. This is the loaded projectile's own "
            "lengthwise axis carried through the same transforms that draw it, so it "
            "is the barrel direction rather than an offset chosen by eye.",
            w->asset,g.bone,g.axis.ratio,g.axis.high-g.axis.low,g.sign);
    }
    dvr::hf::Xform invPalm;
    if(!dvr::wf::inverse(wc->palm,&invPalm)||wc->unitsPerMeter<1)return;
    const auto posed=dvr::hf::xform_mul(invPalm,dvr::hf::xform_mul(draw,dvr::hf::xform_mul(delta,skin)));
    float tip[3],axis[3],p[3],d[3];dvr::hf::bolt_tip(g.axis,g.sign,tip,axis);
    dvr::hf::mulv3(posed.r,tip,p);dvr::hf::mulv3(posed.r,axis,d);
    float len=0;for(int i=0;i<3;++i)len+=d[i]*d[i];
    if(!std::isfinite(len)||len<1e-8f)return;
    dvr::hands::ModelRaySnapshot out;out.ok=true;out.sampleMs=now;
    for(int i=0;i<3;++i){out.originPalm[i]=(p[i]+posed.t[i])/wc->unitsPerMeter;out.dirPalm[i]=d[i]/sqrtf(len);
        if(!std::isfinite(out.originPalm[i])||fabsf(out.originPalm[i])>2)return;}
    measured[w->hand]=wc->present;
    AcquireSRWLockExclusive(&g_brLock);g_brRay[w->hand]=out;ReleaseSRWLockExclusive(&g_brLock);
}
