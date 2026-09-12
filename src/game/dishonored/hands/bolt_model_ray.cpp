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
    // VR-57: which WEAPON this axis was measured for, and the finished palm-frame
    // ray it produced. Both are keyed on the weapon rather than the projectile on
    // purpose.
    //
    // `bolt_01` is not one mesh: the same asset name measured at length 44.531 and
    // at 20.708 in one run, so the regular and the poison bolt are different
    // shapes sharing a name and their tips sit in different places. Re-measuring
    // per ammunition therefore MOVED the dot when the ammunition changed, which is
    // exactly what the tester saw. One weapon is one barrel, so the first good
    // measurement for a weapon is kept until the weapon itself changes.
    //
    // And because the stored ray is in the PALM frame it is pose-independent: once
    // measured it stays true without the projectile being drawn again, which also
    // removes the dropout while no projectile is on screen.
    char weapon[64]={};
    // THE CACHE KEY IS THE ENGINE'S EQUIPPED ITEM, not a name.
    //
    // Keying on the weapon's asset name did not work: the name is resolved from the
    // component table and comes back EMPTY on the projectile's own draw, which is the
    // draw that measures. So the axis was adopted under weapon '?' and the name was
    // filled in later by whichever draw happened next - meaning after a weapon switch
    // the stored name and the stored axis could belong to two different weapons. The
    // tester saw the consequence directly: the crossbow came back mirrored to the
    // other side after a trip to the pistol.
    //
    // g_rflHeldObj is what the engine says is equipped in that hand. It changes
    // exactly when the weapon changes, needs no name matching, and is identical on
    // every draw of the frame including the projectile's.
    void* heldObj=nullptr;
    // The sign is latched once and then aimed with forever, so it must not be latched
    // from a transitional frame. It is confirmed across samples before adoption.
    int pendingSign=0,signVotes=0;
    bool fromBody=false;          // true = fitted from the weapon mesh, not a projectile
    float palmOrigin[3]={},palmDir[3]={};
    bool haveRay=false;
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

// A WEAPON BODY, as the fallback when a weapon has no visible loaded projectile.
// The pistol is the case that forced this: its loaded bullet is reported by the
// attach as a member whose component transform is UNREADABLE, all zeros, so it can
// never be a verified instance and no projectile axis exists for it. Its own mesh
// does have a readable transform.
//
// Kept deliberately narrow and excluded from the strict path: the body ref, and
// anything that is a projectile, are not weapon bodies.
// DOES THIS PROJECTILE BELONG TO THE EQUIPPED WEAPON?
//
// It was never asked, and that is the whole fault. A `bolt_01` draw happens even
// with the pistol equipped, so the crossbow's bolt was measured and stored as the
// PISTOL's ray - the log caught it exactly: "'bolt_01' axis adopted for weapon
// 'EliteGun'". Worse, its forward sign was then resolved against the pistol's
// forward, which is how the two weapons ended up mirrored: left and up on one,
// right and down on the other. Switching back handed the crossbow that corrupted
// result.
//
// A name pairing is the right tool here and not a guess: "is this the loaded
// ammunition of this weapon" is a question about game content, and the content
// answers it. An UNKNOWN weapon refuses rather than measuring against nothing,
// which is what let a bolt be adopted under a gun in the first place.
static bool BrProjectileMatchesWeapon(const char* proj,const char* weapon)
{
    if(!proj||!*proj||!weapon||!*weapon)return false;
    const bool projBolt  = !_stricmp(proj,"bolt_01")||!_strnicmp(proj,"Bolt",4);
    const bool projBullet= strstr(proj,"bullet")||strstr(proj,"Bullet");
    const bool wpnXbow   = strstr(weapon,"crossbow")||strstr(weapon,"Crossbow");
    const bool wpnGun    = strstr(weapon,"Gun")||strstr(weapon,"gun")||strstr(weapon,"Elite");
    if(projBolt)  return wpnXbow;
    if(projBullet)return wpnGun;
    return false;
}

static bool BrIsWeaponBody(const char* a)
{
    if (!a || !*a) return false;
    if (BrIsLoadedProjectile(a)) return false;
    if (strstr(a, "Skm_Player")) return false;       // the body mesh, the bridge anchor
    return true;
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
static bool BrReadGeometry(IDirect3DDevice9* dev,WaMesh* w,BrGeometry& g,float minRatio) {
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
        if(!valid||!dvr::hf::bolt_axis_ratio(points,n,minRatio,g.axis))break;
        g.bone=bone;g.ok=true;ok=true;
    }while(false);
    ib->Release();vb->Release();return ok;
}
// Which weapon is in this hand, taken from the component snapshot the attach has
// already built - no new engine read, and no name guess: a member on this hand that
// is not a loaded projectile is the weapon.
static const char* BrWeaponFor(const WaCommon* wc,int hand)
{
    if(!wc)return "";
    for(int i=0;i<wc->componentCount&&i<WA_MAX_COMP;++i){
        const WaComp* c=&wc->components[i];
        if(!c->isMember||c->hand!=hand||c->isRef)continue;
        if(BrIsLoadedProjectile(c->asset))continue;
        if(c->asset[0])return c->asset;
    }
    return "";
}

// The engine's equipped item for a hand. Slot 1 is the primary, slot 2 the
// secondary, and the two hand assignments say which is which.
static void* BrHeldFor(int hand)
{
    const int slot=(hand==g_waXbowHand)?2:1;
    uint8_t* o=g_rflHeldObj[slot];
    return (o&&LooksLikeObj(o))?o:nullptr;
}

static void BrMeasure(IDirect3DDevice9* dev,WaMesh* w,const float* palette,UINT regs,const dvr::hf::Xform& delta) {
    if(!dvr::aim::model_ray_requested()||w->hand<0||w->hand>1)return;
    const bool isProjectile=BrIsLoadedProjectile(w->asset);
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
    // THE WEAPON DECIDES, not the ammunition. A different weapon invalidates the
    // axis; a different projectile in the SAME weapon does not.
    // THE EQUIPPED ITEM decides, and it is the same on every draw of the frame - so
    // the axis and the identity it is stored under can no longer disagree.
    void* held=BrHeldFor(w->hand);
    if(held&&g.heldObj&&held!=g.heldObj){
        const bool had=g.haveRay;
        g={};
        g.heldObj=held;
        if(had)Log("modelray: the equipped item in that hand changed - the stored axis "
                   "is discarded and will be measured again for the new weapon.");
    } else if(held&&!g.heldObj){
        g.heldObj=held;
    }
    // The NAME is still recorded, for the log only. It is never the cache key.
    const char* weapon=BrWeaponFor(wc,w->hand);
    if(weapon[0]&&!g.weapon[0]) strncpy(g.weapon,weapon,sizeof(g.weapon)-1);
    // PUBLISH FROM ANY DRAW OF THIS HAND. The stored ray is in the palm frame and so
    // does not depend on the pose or on which mesh is being drawn; republishing it
    // needs no transform, no geometry and no verified instance. Gating this on the
    // weapon's own draw is what left the guide stale and fell back to head aim.
    if(g.haveRay){
        dvr::hands::ModelRaySnapshot out;out.ok=true;out.sampleMs=now;
        for(int i=0;i<3;++i){out.originPalm[i]=g.palmOrigin[i];out.dirPalm[i]=g.palmDir[i];}
        AcquireSRWLockExclusive(&g_brLock);g_brRay[w->hand]=out;ReleaseSRWLockExclusive(&g_brLock);
        return;
    }
    // MEASURED FROM A LOADED PROJECTILE ONLY.
    //
    // Fitting the weapon BODY was tried and removed the same hour, because it cannot
    // work through this reader: the geometry path accepts at most 1024 vertices and
    // the weapon meshes measured 1961 for the crossbow, 2481 for the sword and up to
    // 6330 elsewhere, so every body was rejected by the size check before any axis
    // was fitted - the refusal even quoted the variance test it never reached. They
    // are also skinned to more than one bone, which the single-slot rigid transform
    // this uses cannot carry. Making it work needs vertex subsampling and multi-bone
    // handling, which is its own piece of work and is not this.
    //
    // Nothing is lost by refusing: a weapon with no measurable axis now falls back to
    // the CONTROLLER ray rather than the head, so it still aims where it is pointed.
    const bool body=false;
    if(!isProjectile)return;
    // The projectile must be THIS weapon's. Without this a bolt drawn while the
    // pistol is equipped was measured as the pistol's axis and signed against the
    // pistol's forward, mirroring both weapons.
    if(!BrProjectileMatchesWeapon(w->asset,g.weapon)){
        DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,5000,
            "modelray: '%s' is not the loaded ammunition of the equipped weapon "
            "'%s', so it is NOT measured. Measuring it would store one weapon's "
            "barrel as another's and resolve its sign against the wrong forward. "
            "An empty weapon name also refuses: the axis waits for the weapon's own "
            "draw to name it rather than being adopted under nothing.",
            w->asset,g.weapon[0]?g.weapon:"(not yet known this frame)");
        return;
    }
    const bool same=g.vb==w->vb&&g.ib==w->ib&&g.decl==w->decl&&g.stride==w->stride&&g.offset==w->streamOffset&&
        g.start==w->startIndex&&g.count==w->numVerts&&g.prims==w->primCount&&g.base==w->baseVertex&&g.minIndex==w->minIndex;
        // A weapon body only needs a DOMINANT axis; a bolt must be nearly 1D. The
    // strict threshold is what keeps a body from ever being read as a barrel on the
    // precise path, so it is relaxed only where a body is what we asked for.
    const float minRatio=body?3.0f:16.0f;
    if(!same||(!g.ok&&now-g.tried>5000))if(!BrReadGeometry(dev,w,g,minRatio)){
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
        // CONFIRM, do not latch on one frame. The sign decides which END of the axis
        // is the muzzle, it is kept for the life of the weapon, and a single
        // transitional frame - a weapon switch is exactly that - used to be enough to
        // fix it backwards. The margin is raised from 0.70 to 0.85 and two
        // consecutive frames must agree before it is adopted.
        if(len>1e-8f&&fabsf(dot)/sqrtf(len)>=0.85f){
            const int vote=dot>0?1:-1;
            if(g.pendingSign==vote) ++g.signVotes; else { g.pendingSign=vote; g.signVotes=1; }
            if(g.signVotes<2) return;   // not yet confirmed; try again next draw
        }
        if(len<1e-8f||fabsf(dot)/sqrtf(len)<0.85f){
            BrRefuseAsset(w->asset,"its forward sign is ambiguous against the native view "
                                   "(the fitted axis is more than 45 degrees off the weapon's "
                                   "own forward, so which end is the tip cannot be decided)");
            return;
        }
        g.sign=g.pendingSign;
        g.fromBody=body;
        Log("modelray: measured '%s' axis - rigid slot %d, variance ratio %.1f (16:1 "
            "required), length %.3f, sign %+d. This is the loaded projectile's own "
            "lengthwise axis carried through the same transforms that draw it, so it "
            "is the barrel direction rather than an offset chosen by eye.",
            w->asset,g.bone,g.axis.ratio,g.axis.high-g.axis.low,g.sign);
    }
    dvr::hf::Xform invPalm;
    if(!dvr::wf::inverse(wc->palm,&invPalm)||wc->unitsPerMeter<1)return;
    const auto posed=dvr::hf::xform_mul(invPalm,dvr::hf::xform_mul(draw,dvr::hf::xform_mul(delta,skin)));
    float tip[3],axis[3],p[3],d[3];
    if(g.fromBody) dvr::hf::bolt_middle(g.axis,g.sign,tip,axis);
    else           dvr::hf::bolt_tip(g.axis,g.sign,tip,axis);
    dvr::hf::mulv3(posed.r,tip,p);dvr::hf::mulv3(posed.r,axis,d);
    float len=0;for(int i=0;i<3;++i)len+=d[i]*d[i];
    if(!std::isfinite(len)||len<1e-8f)return;
    dvr::hands::ModelRaySnapshot out;out.ok=true;out.sampleMs=now;
    for(int i=0;i<3;++i){out.originPalm[i]=(p[i]+posed.t[i])/wc->unitsPerMeter;out.dirPalm[i]=d[i]/sqrtf(len);
        if(!std::isfinite(out.originPalm[i])||fabsf(out.originPalm[i])>2)return;}
    measured[w->hand]=wc->present;
    for(int i=0;i<3;++i){g.palmOrigin[i]=out.originPalm[i];g.palmDir[i]=out.dirPalm[i];}
    g.haveRay=true;
    Log("modelray: '%s' axis adopted for weapon '%s' (%s) - this ray is now held for "
        "EVERY projectile this weapon loads, so changing ammunition cannot move it, "
        "and it survives frames where no projectile is drawn because it is stored in "
        "the palm frame. It is discarded when the weapon changes.",
        w->asset,g.weapon[0]?g.weapon:"?",
        g.fromBody?"from the WEAPON MESH, aimed from its centre - the fallback for a "
                   "weapon with no visible loaded projectile"
                 :"from the loaded PROJECTILE, aimed from its tip - the precise path");
    AcquireSRWLockExclusive(&g_brLock);g_brRay[w->hand]=out;ReleaseSRWLockExclusive(&g_brLock);
}
