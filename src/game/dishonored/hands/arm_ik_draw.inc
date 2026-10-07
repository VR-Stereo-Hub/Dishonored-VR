// Included after MsQualify: mod-owned full mesh and draw-scoped skin matrices.
// No engine object writes. The reference file is generated locally, not shipped.
namespace {
struct IkGeometry {
    dvr::ik::Rig rig;
    dvr::ik::Mapping mapping;
    dvr::ik::Chain chain[2];
    IDirect3DVertexBuffer9* vb=nullptr;
    IDirect3DIndexBuffer9* ib=nullptr;
    bool tried=false,ready=false;
    unsigned generation=~0u;
    int wristSlot[2]={-1,-1};
} ikMesh;
dvr::ik::PoseHistory ikTime;
struct IkSeat {bool measured,applied;float fwd,right,up,apart,turnDeg,stretch,wanted,left;} g_ikSeat[2]={};
}

static void IkResetPose(){ikTime=dvr::ik::PoseHistory{};}
static void IkDropGeometry()
{
    if(ikMesh.vb){ikMesh.vb->Release();ikMesh.vb=nullptr;}
    if(ikMesh.ib){ikMesh.ib->Release();ikMesh.ib=nullptr;}
    ikMesh.ready=false;ikMesh.tried=false;ikMesh.generation=~0u;
    IkResetPose();
}
static bool IkRefuse(const char* why)
{
    g_ikStatus=why;++g_ikRefusals;
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,3000,
        "ik: hands-only fallback - %s; solved=%u refused=%u",why,g_ikSolves.load(),g_ikRefusals.load());
    return false;
}
static bool IkBuildGeometry(IDirect3DDevice9* dev)
{
    using namespace dvr::ik;
    IkDropGeometry();ikMesh.tried=true;ikMesh.generation=g_mpSrcGen;
    char path[MAX_PATH];dvr::paths::in_data_dir(path,"dishonored_vr_arm_rig.bin");
    FILE* file=nullptr;fopen_s(&file,path,"rb");bool loaded=ikMesh.rig.load(file);if(file)fclose(file);
    if(!loaded)return IkRefuse("Local arm reference is missing, outdated or invalid. Prepare version 2 and reload it.");
    if(!g_msOwnVb||g_msVerts<1||g_msBones<1||(unsigned)g_msTris!=ikMesh.rig.triangles)
        return IkRefuse("The current mesh does not match the full-arm reference draw.");
    std::vector<Vertex> vertices((size_t)g_msVerts);
    for(int v=0;v<g_msVerts;++v){
        vertices[v].p=vec(g_msVert[v].p);
        for(int j=0;j<4;++j){vertices[v].bone[j]=g_msVert[v].bi[j];vertices[v].weight[j]=g_msVert[v].bw[j];}
    }
    if(!map_skin(ikMesh.rig,vertices.data(),g_msVerts,g_msBones,ikMesh.mapping)){
        const auto& m=ikMesh.mapping;
        Log("ik/map: refused after %u/%u vertices; vertex=%d position=(%.6f %.6f %.6f), nearest reference distance=%.6f (limit 0.03), reason=%s",
            m.matched,g_msVerts,m.failedVertex,m.failedPosition.x,m.failedPosition.y,m.failedPosition.z,m.nearestDistance,m.why);
        return IkRefuse(m.why);
    }
    Chain named[2];
    if(!named[0].build(ikMesh.rig,false)||!named[1].build(ikMesh.rig,true))return IkRefuse("Named arm hierarchy is incomplete or invalid.");
    // Preserve the established hand/controller assignment. Do not infer left
    // from the sign of an asset coordinate or from palette enumeration order.
    for(int h=0;h<2;++h){
        int slot=g_msHandBone[h+1];
        if(slot<0||slot>=g_msBones)return IkRefuse("Existing wrist slot is unavailable.");
        int ref=ikMesh.mapping.reference[slot];
        int side=ref==named[0].wrist?0:ref==named[1].wrist?1:-1;
        if(side<0)return IkRefuse("Named reference wrist disagrees with the current hand mapping.");
        ikMesh.chain[h]=named[side];ikMesh.wristSlot[h]=slot;
    }
    if(ikMesh.wristSlot[0]==ikMesh.wristSlot[1]||ikMesh.chain[0].wrist==ikMesh.chain[1].wrist)
        return IkRefuse("Both hands resolved to the same wrist.");
    for(int s=0;s<g_msBones;++s){
        if(g_msBoneW[s]<=0)continue;
        int ref=ikMesh.mapping.reference[s];
        if(ref<0||(!ikMesh.chain[0].region[ref]&&!ikMesh.chain[1].region[ref]))return IkRefuse("Weighted palette slot is outside both named arm chains.");
        Log("ik/map: palette %d -> reference %d %s region=%d/%d",s,ref,ikMesh.rig.bones[ref].name,
            ikMesh.chain[0].region[ref],ikMesh.chain[1].region[ref]);
    }
    const UINT vertexBytes=(UINT)g_msVerts*g_msStride,indexBytes=(UINT)g_msTris*3*sizeof(uint32_t);
    if(FAILED(dev->CreateVertexBuffer(vertexBytes,D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&ikMesh.vb,nullptr))||
       FAILED(dev->CreateIndexBuffer(indexBytes,D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_MANAGED,&ikMesh.ib,nullptr)))
        return IkRefuse("Could not allocate full-arm draw buffers.");
    void* dest=nullptr;
    if(FAILED(ikMesh.vb->Lock(0,vertexBytes,&dest,0))||!dest)return IkRefuse("Could not fill full-arm vertices.");
    for(int v=0;v<g_msVerts;++v)memcpy((uint8_t*)dest+(size_t)v*g_msStride,g_msRaw+(size_t)v*MS_MAX_STRIDE,g_msStride);
    ikMesh.vb->Unlock();
    if(FAILED(ikMesh.ib->Lock(0,indexBytes,&dest,0))||!dest)return IkRefuse("Could not fill full-arm indices.");
    bool indicesOk=true;
    for(int i=0;i<g_msTris*3;++i){
        uint32_t index=g_msIdx[i]-g_msMinIndex;
        if(index>=(unsigned)g_msVerts)indicesOk=false;
        ((uint32_t*)dest)[i]=index;
    }
    ikMesh.ib->Unlock();if(!indicesOk)return IkRefuse("Full-arm index is outside its vertex window.");
    ikMesh.ready=true;
    Log("ik: reference validated against ALL %u draw vertices, %u palette slots, %u triangles; max position error %.6f, max relative weight error %.7f; wrists %d/%d; generation %u",
        ikMesh.mapping.matched,g_msBones,g_msTris,ikMesh.mapping.worstPosition,ikMesh.mapping.worstWeight,
        ikMesh.wristSlot[0],ikMesh.wristSlot[1],g_mpSrcGen);
    return true;
}
static dvr::ik::PoseFrame& IkFrame(const MpDrawCtx& ctx)
{
    const unsigned gen=ctx.ikPoseGen;
    // Both matched and fallback heads carry a runtime locate generation.
    // pose.gen counts publications and cannot be compared with that clock.
    // XR forward is -Z. Yaw is measured in the tracking frame so a game-side
    // artificial turn is carried once by the established tracking->draw map.
    float yaw=atan2f(-ctx.pose.head[0][2],ctx.pose.head[2][2]);
    return ikTime.acquire(gen,yaw,(double)GetTickCount64());
}
static bool IkTryDraw(IDirect3DDevice9* dev,D3DPRIMITIVETYPE type)
{
    using namespace dvr::ik;
    if(g_ikReload.exchange(false)){IkDropGeometry();ikMesh.rig=Rig{};}
    // A menu owning INPUT does not invalidate a rendered player-arm draw.
    // Wheel/pause menus use the same view-matched hand placement as gameplay.
    // Keep the title screen blocked; current draw/palette/source-generation
    // and tracking guards below still validate every replacement.
    if(g_mainMenu){IkResetPose();return false;}
    if(!g_mpOn||!g_mpWorld||g_mpPalN==0||g_mpCacheN!=g_mpPalN||g_mpCacheN>256)
        return IkRefuse("Waiting for a complete hand palette and tracked hand placement.");
    if(!ikMesh.tried||ikMesh.generation!=g_mpSrcGen){if(!IkBuildGeometry(dev))return false;}
    if(!ikMesh.ready)return false;
    PcRefreshLayout(dev);
    MpDrawCtx ctx{};
    ++g_mpDrawsEntered;
    if(!MpAcquireCtx(dev,&ctx)||!ctx.poseOk||!ctx.pose.headOk||!ctx.ikPoseGen||!ctx.basisProper)
        return IkRefuse(ctx.why?ctx.why:"Head tracking is unavailable.");
    ++g_mpDrawsSampled;
    MpDrawCompare(&ctx);
    auto& temporal=IkFrame(ctx);
    Settings settings;
    settings.forwardCm=g_ikForward.load();settings.rightCm=g_ikRight.load();settings.upCm=g_ikUp.load();settings.widthCm=g_ikWidth.load();
    settings.lengthScale=g_ikLength.load();settings.elbowOut=g_ikElbowOut.load();
    // Identical bridge and units to MpWorldTarget. This includes head tilt
    // cancellation; using the view's horizontal axes alone would roll shoulders.
    Mat3 head{};for(int r=0;r<3;++r)for(int c=0;c<3;++c)head.m[r*3+c]=ctx.pose.head[r][c];
    Mat3 trackingToLocal=tracking_to_local(ctx.R_L,ctx.B,head);
    float k=(g_skcWorldScale>1?g_skcWorldScale:100.f)*g_mpDriveGain;
    if(!std::isfinite(k)||k<1)return IkRefuse("Invalid arm tracking scale.");
    Vec right{cosf(temporal.yaw),0,sinf(temporal.yaw)},forward{sinf(temporal.yaw),0,-cosf(temporal.yaw)},up{0,1,0};
    Body body;
    body.right=rotate(trackingToLocal,right);body.forward=rotate(trackingToLocal,forward);body.up=rotate(trackingToLocal,up);
    body.origin=rotate(dvr::hf::transpose3(ctx.R_L),vec(ctx.t)*-1);
    if(g_mpEyeOffset&&g_mpEyeState)body.origin=body.origin+rotate(dvr::hf::transpose3(ctx.R_L),vec(ctx.r))*(-g_mpEyeState*.5f*g_ipdM*k);
    float finalPalette[4*256];memcpy(finalPalette,g_mpCache,g_mpCacheN*4*sizeof(float));
    ArmPose solved[2];
    for(int h=0;h<2;++h){
        const int cls=h?MS_CLS_HAND_B:MS_CLS_HAND_A;
        Xform correction{};float anchor[3];const char* why=nullptr;
        if(!MpAnchorPos(cls,g_mpCache,g_mpCacheN,anchor)||!MpWorldTarget(&ctx,h,cls,anchor,&correction,&why))
            return IkRefuse(why?why:"Current animated wrist target is unavailable.");
        InterlockedIncrement(&g_mpWorldOk);
        float handPalette[4*256];MpBuild(handPalette,g_mpCache,g_mpCacheN,&correction);
        if(h==1&&OhActive())OhApply(handPalette,g_mpCache,g_mpCacheN);
        GrReadyApply(handPalette,g_mpCache,g_mpCacheN,cls);   // in reach of a grab: the hand opens (mesh_split.cpp)
        GrApply(handPalette,g_mpCache,g_mpCacheN,cls);   // the grab animation (mesh_split.cpp, THE GRAB)
        Xform wrist{};const float* rows=handPalette+ikMesh.wristSlot[h]*12;
        for(int r=0;r<3;++r){for(int c=0;c<3;++c)wrist.r.m[r*3+c]=rows[r*4+c];wrist.t[r]=rows[r*4+3];}
        Vec nominal=shoulder(body,settings,h,k*.01f);
        float side=h?1.f:-1.f;
        Vec pole=body.up*-1+body.right*(side*settings.elbowOut)+body.forward*-.3f;
        Vec oldPole=body.right*temporal.prior[h].x+body.up*temporal.prior[h].y+body.forward*temporal.prior[h].z;
        if(!pose_arm(ikMesh.rig,ikMesh.chain[h],nominal,pole,body.right*side,oldPole,temporal.twist[h],temporal.fresh,
                     wrist,settings.lengthScale,.5f*k*.01f,solved[h]))return IkRefuse("Arm solve refused invalid wrist, scale or reference lengths.");
        // ArmIKGameArmInAnim: the game's share of this arm, 1 - the hand-back weight. At 1 the
        // arm is exactly the game's (its own shoulder and elbow for the clip); a choke's wrist
        // on an IK forearm from the tracked shoulder twisted the arm (2026-10-04 run).
        const float gameShare=g_ikGameArmInAnim.load()?1.0f-dvr::anim::weight_for(h):0.0f;
        // ArmIKGameArmShoulder: the game's arm is drawn for the game's own camera and body, the IK
        // shoulder hangs from the tracked head. Re-seat the game's arm on the IK shoulder, about
        // the game's own wrist, so the hand stays where the clip put it (shoulder_fit, arm_rig.h).
        ShoulderFit fit;bool seated=false;Vec nativeShoulder{},nativeWrist{};
        const int seatMode=g_ikGameArmShoulder.load();
        if(gameShare>0.0001f){
            int upperSlot=-1;
            for(int slot=0;slot<g_msBones;++slot)if(ikMesh.mapping.reference[slot]==ikMesh.chain[h].upper){upperSlot=slot;break;}
            if(upperSlot>=0){
                auto native=[&](int slot,Vec p){const float* m=g_mpCache+slot*12;
                    return Vec{m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};};
                nativeShoulder=native(upperSlot,ikMesh.rig.bones[ikMesh.chain[h].upper].head);
                nativeWrist=native(ikMesh.wristSlot[h],ikMesh.rig.bones[ikMesh.chain[h].wrist].head);
                // Measured whatever the lever says, so a log of the lever OFF still carries the numbers.
                fit=shoulder_fit(nativeShoulder,nativeWrist,solved[h].joints.shoulder,.8f,g_ikGameArmMaxStretch.load(),45*3.14159265359f/180);
                seated=fit.ok&&(seatMode==2||(seatMode==1&&!dvr::anim::choke_owns_arms()));
                const Vec off=solved[h].joints.shoulder-nativeShoulder;
                g_ikSeat[h]={fit.ok,seated,dot(off,body.forward),dot(off,body.right),dot(off,body.up),fit.offset,fit.angle*180/3.14159265359f,fit.stretch,fit.wanted,seated?fit.residual:fit.offset};
            }
        }
        for(int slot=0;slot<g_msBones;++slot){
            int ref=ikMesh.mapping.reference[slot];if(ref<0)continue;
            int region=ikMesh.chain[h].region[ref];if(!region)continue;
            float* dst=finalPalette+slot*12;
            if(region==3)memcpy(dst,handPalette+slot*12,12*sizeof(float));
            else {
                const Xform& m=solved[h].skin[ref];
                for(int r=0;r<3;++r){for(int c=0;c<3;++c)dst[r*4+c]=m.r.m[r*3+c];dst[r*4+3]=m.t[r];}
                // Linear blend of the two skin matrices: the arm slots carry the IK length scale,
                // which the proper-rotation blend refuses; a matrix lerp of one bone's two poses is
                // what an animation blend does, and it lasts only the hand-back's entry/return.
                if(gameShare>0.0001f){
                    const float* native=g_mpCache+slot*12;float moved[12];
                    if(seated){   // move * native, row by row
                        for(int r=0;r<3;++r){
                            for(int col=0;col<4;++col)moved[r*4+col]=fit.move.r.m[r*3]*native[col]+fit.move.r.m[r*3+1]*native[4+col]+fit.move.r.m[r*3+2]*native[8+col];
                            moved[r*4+3]+=fit.move.t[r];
                        }
                        native=moved;
                    }
                    for(int i=0;i<12;++i)dst[i]+=(native[i]-dst[i])*gameShare;
                }
            }
        }
        g_ikGameShare[h]=gameShare;
    }
    // The derived numbers, while the game has an arm: where the game's shoulder is against the IK
    // shoulder (in body axes, so "too far forward" is arithmetic) and what the re-seat did with it.
    if(g_ikGameShare[0]>.5f||g_ikGameShare[1]>.5f)DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,500,
        "ik/gamearm: ArmIKGameArmShoulder=%d choke=%d share L=%.2f R=%.2f | IK shoulder minus the game's shoulder, uu forward/right/up: "
        "L %+.1f %+.1f %+.1f (%.1f apart) R %+.1f %+.1f %+.1f (%.1f apart); negative forward = the game's shoulder is in front | re-seat about the game's wrist: "
        "L %s turn %.1f deg stretch %.3f (wanted %.3f) left over %.1f uu | R %s turn %.1f deg stretch %.3f (wanted %.3f) left over %.1f uu "
        "(bounds: stretch 0.80..%.2f [Hands] ArmIKGameArmMaxStretch, turn 45 deg; wanted above the bound = the shoulder stays in front by the left over)",
        g_ikGameArmShoulder.load(),(int)dvr::anim::choke_owns_arms(),g_ikGameShare[0],g_ikGameShare[1],
        g_ikSeat[0].fwd,g_ikSeat[0].right,g_ikSeat[0].up,g_ikSeat[0].apart,g_ikSeat[1].fwd,g_ikSeat[1].right,g_ikSeat[1].up,g_ikSeat[1].apart,
        !g_ikSeat[0].measured?"not measured":g_ikSeat[0].applied?"APPLIED":"not applied",g_ikSeat[0].turnDeg,g_ikSeat[0].stretch,g_ikSeat[0].wanted,g_ikSeat[0].left,
        !g_ikSeat[1].measured?"not measured":g_ikSeat[1].applied?"APPLIED":"not applied",g_ikSeat[1].turnDeg,g_ikSeat[1].stretch,g_ikSeat[1].wanted,g_ikSeat[1].left,
        g_ikGameArmMaxStretch.load());
    // Preserve all device state even when a bind or draw fails. No engine-owned
    // COM reference survives this scope; only our copied mesh buffers persist.
    struct Saved {
        IDirect3DDevice9* dev;IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;
        UINT offset=0,stride=0;D3DVIEWPORT9 vp{};float palette[4*256];UINT n;
        bool changed=false,viewport=false;
        ~Saved(){if(changed){dvr::frame::orig_set_vs_const(dev,6,palette,n);dev->SetStreamSource(0,vb,offset,stride);dev->SetIndices(ib);if(viewport)dev->SetViewport(&vp);}if(vb)vb->Release();if(ib)ib->Release();}
    } saved{dev};
    saved.n=g_mpCacheN;
    if(FAILED(dev->GetStreamSource(0,&saved.vb,&saved.offset,&saved.stride))||!saved.vb||
       FAILED(dev->GetIndices(&saved.ib))||!saved.ib||
       FAILED(dev->GetVertexShaderConstantF(6,saved.palette,saved.n)))return IkRefuse("Could not save the original arm draw state.");
    saved.changed=true;
    if(FAILED(dev->SetStreamSource(0,ikMesh.vb,0,g_msStride))||FAILED(dev->SetIndices(ikMesh.ib))||
       FAILED(dvr::frame::orig_set_vs_const(dev,6,finalPalette,g_mpCacheN)))return IkRefuse("Could not bind the IK arm draw.");
    if(g_mpDepth&&SUCCEEDED(dev->GetViewport(&saved.vp))&&saved.vp.MaxZ<.5f){
        D3DVIEWPORT9 full=saved.vp;full.MinZ=0;full.MaxZ=1;
        saved.viewport=SUCCEEDED(dev->SetViewport(&full));
    }
    dvr::depthprobe::fgmask_mark_piece();
    if(FAILED(dvr::frame::orig_draw_indexed(dev,type,0,0,g_msVerts,0,g_msTris)))return IkRefuse("Full-arm draw failed.");
    for(int h=0;h<2;++h)if(!temporal.written[h]){
        const Vec p=solved[h].joints.pole;
        ikTime.commit(temporal,h,{dot(p,body.right),dot(p,body.up),dot(p,body.forward)},solved[h].trackedTwist);
    }
    ++g_ikSolves;++g_msDraws;InterlockedIncrement(&g_mpDraws);
    g_msPassThrough=false;g_ikStatus="Active - IK arms, animated hands";
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,3000,
        "ik: ACTIVE generation=%u headLocate=%u publication=%u matched=%d fresh=%d eye=%d draws=%u menuContext=%d; history advance/reuse/old=%u/%u/%u; shoulder reach shift uu L=%.3f R=%.3f; hand blend %.3f/%.3f; game arm share L=%.2f R=%.2f (ArmIKGameArmInAnim=%d); original skin weights retained",
        g_mpSrcGen,temporal.gen,ctx.pose.gen,ctx.viewMatched?1:0,temporal.fresh?1:0,ctx.viewEye,g_ikSolves.load(),UiSurfaceContext(),
        ikTime.advanced,ikTime.reused,ikTime.old,solved[0].joints.shoulderShift,solved[1].joints.shoulderShift,
        dvr::anim::weight_for(0),dvr::anim::weight_for(1),g_ikGameShare[0],g_ikGameShare[1],(int)g_ikGameArmInAnim.load());
    return true;
}
