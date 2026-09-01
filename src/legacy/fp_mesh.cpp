// legacy/fp_mesh.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ============================================================================
// Stage 9.0 - WEAPON OBJECT FINDER (the BioShock-mod approach, properly)
//
// What the BioShock Remastered VR dev actually did (per their repo): they did
// NOT fingerprint shader constants. They hooked the ENGINE - PlayerCalcView
// for the camera - and moved the weapon at the ACTOR/skeleton level, with
// per-weapon grip offsets and the forearms hidden. Our 8.x shader-constant
// hunt was the wrong tree: Dishonored doesn't separate the FP weapon into its
// own view-projection, so there is nothing to fingerprint there.
//
// The good news: writing UE3 object fields DOES work in this game - the
// projectile velocity rewrite provably steers shots. So the weapon mesh
// should move the same way: find the first-person weapon's mesh COMPONENT in
// GObjects and write its relative rotation each frame.
//
// This is the discovery step: press INSERT while a weapon is drawn and we log
// every weapon-ish object plus every object holding a vector near the camera
// (= attached to the player), dumping their FRotator/FVector fields with
// offsets. Press it with the sword out, then again with the crossbow out - 
// what changes between the two dumps is the active weapon. READ-ONLY.
// ============================================================================
static bool ObjNearCamVec(uint8_t* o, const float* camPos, uint32_t maxOff,
                          uint32_t* outOff, float* outDist)
{
    for (uint32_t off = 0x10; off + 12 <= maxOff; off += 4) {
        if (!RangeReadable(o + off, 12)) return false;
        float* f = (float*)(o + off);
        bool fin = true;
        for (int k = 0; k < 3; k++) {
            float v = f[k];
            if (v != v || v > 3.0e38f || v < -3.0e38f) { fin = false; break; }
        }
        if (!fin) continue;
        float dx = f[0]-camPos[0], dy = f[1]-camPos[1], dz = f[2]-camPos[2];
        float d = sqrtf(dx*dx + dy*dy + dz*dz);
        if (d < 600.0f && (fabsf(f[0]) + fabsf(f[1])) > 1.0f) {
            *outOff = off; *outDist = d; return true;
        }
    }
    return false;
}


static void WeaponObjScan()
{
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!CamStillValid()) { Log("wpnscan: no live camera - retry in gameplay"); return; }
    if (!RangeReadable(g_camObj + 0x80, 12)) { Log("wpnscan: camera unreadable"); return; }
    float* cp = (float*)(g_camObj + 0x80);
    float camPos[3] = { cp[0], cp[1], cp[2] };
    float* cf = (float*)(g_camObj + 0x50);

    if (!RangeReadable((void*)kGObjHdr, 12)) { Log("wpnscan: GObjects unreadable"); return; }
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) {
        Log("wpnscan: GObjects looks wrong (num=%u)", num); return;
    }

    Log("wpnscan: ==== BEGIN  cam=(%.0f,%.0f,%.0f) fwd=(%.2f,%.2f,%.2f) objs=%u ====",
        camPos[0], camPos[1], camPos[2], cf[0], cf[1], cf[2], num);

    // Histogram every INSTANCE that carries a vector near the camera - those
    // are the things attached to the player (arms, weapon, held items). Keyed
    // by class pointer so the walk stays cheap.
    struct Bucket { void* cls; const char* name; uint32_t n; float minD;
                    uint8_t* best; uint32_t bestOff; };
    Bucket b[64]; int bn = 0;
    uint32_t scanned = 0, hits = 0;

    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        if (*(uint32_t*)(o + kNameOff + 4) == 0) continue;        // instances only
        scanned++;
        uint32_t vOff = 0; float vDist = 0;
        if (!ObjNearCamVec(o, camPos, 0x240, &vOff, &vDist)) continue;
        hits++;
        void* cls = *(void**)(o + kClassOff);
        int slot = -1;
        for (int k = 0; k < bn; k++) if (b[k].cls == cls) { slot = k; break; }
        if (slot < 0) {
            if (bn >= 64) continue;
            slot = bn++;
            b[slot].cls = cls; b[slot].name = ObjClassName(o);
            b[slot].n = 0; b[slot].minD = 1e9f; b[slot].best = NULL; b[slot].bestOff = 0;
        }
        b[slot].n++;
        if (vDist < b[slot].minD) {
            b[slot].minD = vDist; b[slot].best = o; b[slot].bestOff = vOff;
        }
    }

    Log("wpnscan: %u instances scanned, %u hold a vector within 600uu of the camera",
        scanned, hits);
    Log("wpnscan: --- classes attached to / near the player ---");
    for (int pass = 0; pass < bn && pass < 30; pass++) {
        int best = -1; float bd = 1e9f;
        for (int k = 0; k < bn; k++)
            if (b[k].best && b[k].minD < bd) { bd = b[k].minD; best = k; }
        if (best < 0) break;
        Log("wpnscan:   %-44s x%-3u nearest=%.1fuu @+0x%03x",
            b[best].name ? b[best].name : "?", b[best].n, b[best].minD,
            (unsigned)b[best].bestOff);
        b[best].minD = 1e9f;       // so the next pass picks the next class
        b[best].n |= 0x80000000u;  // mark as listed
    }

    // Field dump for the most weapon-like classes (rotation offsets we'd write)
    int dumped = 0;
    for (int k = 0; k < bn && dumped < 6; k++) {
        const char* cn = b[k].name;
        if (!cn || !b[k].best) continue;
        if (!(strstr(cn, "Weapon") || strstr(cn, "Mesh") || strstr(cn, "Pawn") ||
              strstr(cn, "Crossbow") || strstr(cn, "Sword") || strstr(cn, "Hand") ||
              strstr(cn, "Arm"))) continue;
        dumped++;
        Log("wpnscan: --- fields of '%s' @ %p ---", cn, (void*)b[k].best);
        DumpAimFields(b[k].best, cf);
    }
    Log("wpnscan: ==== END  classes=%d dumped=%d ====", bn, dumped);
}


// ============================================================================
// Stage 8.3 - FIRST-PERSON WEAPON MESH FINDER (BioShock-style engine approach)
//
// The shader-matrix trick (8.0-8.2) can't isolate the weapon - Dishonored
// doesn't draw it with a separable projection. BioShock's mod worked at the
// ENGINE level: it moved the actual first-person arm/weapon skeletal mesh.
// Unlike the camera matrix (renderer ignores writes to it), a mesh's transform
// MUST be read to draw it, so moving the component should show on screen.
//
// This finds the candidate: any SkeletalMeshComponent whose cached world
// location sits within ~2.5m of the camera = the first-person arms/weapon
// (nothing else is that close to the view in first person). Hexdumps each so
// we can spot the relative-rotation / LocalToWorld fields to overwrite next.
// Also lists equipped-weapon actors by name for reference. Gated [Weapon]
// FindMesh=1; runs once ~frame 900 and on HOME.
// ============================================================================
static void WeaponMeshFind()
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    float cam[3] = {0,0,0}; bool haveCam = false;
    if (CamStillValid() && RangeReadable(g_camObj + 0x80, 12)) {
        float* p = (float*)(g_camObj + 0x80);
        cam[0]=p[0]; cam[1]=p[1]; cam[2]=p[2]; haveCam = true;
    }
    Log("wpnfind: ==== camera pos=(%.1f,%.1f,%.1f) haveCam=%d ====",
        cam[0], cam[1], cam[2], (int)haveCam);
    if (!haveCam) { Log("wpnfind: no camera - aborting"); return; }

    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;

    int dumped = 0, wpnActors = 0;
    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        if (*(uint32_t*)(o + kNameOff + 4) == 0) continue;   // instances only
        const char* cn = ObjClassName(o);
        if (!cn) continue;

        // list equipped-weapon actors by name (cheap reference)
        if (strstr(cn, "Weapon") && !strstr(cn, "Component") && wpnActors < 12) {
            const char* nm = RealName(*(uint32_t*)(o + kNameOff));
            Log("wpnfind: weapon-actor obj[%u] '%s' class '%s'", i, nm?nm:"?", cn);
            wpnActors++;
        }

        // skeletal mesh components near the camera = FP arms/weapon
        if (!strstr(cn, "SkeletalMeshComponent")) continue;
        if (!RangeReadable(o, 0x400)) continue;
        int locOff = -1; float best2 = 250.0f * 250.0f;
        for (uint32_t off = 0x40; off + 12 <= 0x3f0; off += 4) {
            float* f = (float*)(o + off);
            bool fin = true;
            for (int k=0;k<3;k++){float v=f[k]; if(v!=v||v>3e38f||v<-3e38f){fin=false;break;}}
            if (!fin) continue;
            float dx=f[0]-cam[0], dy=f[1]-cam[1], dz=f[2]-cam[2];
            float d2 = dx*dx+dy*dy+dz*dz;
            if ((fabsf(f[0])+fabsf(f[1])+fabsf(f[2])) < 1.0f) continue;  // skip ~origin
            if (d2 < best2) { best2 = d2; locOff = (int)off; }
        }
        if (locOff < 0) continue;                       // not near the camera
        if (dumped >= 5) continue;
        const char* nm = RealName(*(uint32_t*)(o + kNameOff));
        Log("wpnfind: >>> FP-CANDIDATE obj[%u] '%s' class '%s'  loc@+0x%03x dist=%.0fuu",
            i, nm?nm:"?", cn, (unsigned)locOff, sqrtf(best2));
        HexDumpObject("fpcand", o, 0x300);
        dumped++;
    }
    Log("wpnfind: ==== done: %d FP candidate(s), %d weapon actor(s) ====", dumped, wpnActors);
}


static void WeaponDiagAccumulate(const float* m)
{
    // signature = projection-ish terms that separate FOV/near-plane
    for (int i = 0; i < g_vpClusterN; i++) {
        float* r = g_vpClusters[i].rep;
        if (fabsf(r[0]-m[0]) < 0.01f && fabsf(r[5]-m[5]) < 0.01f &&
            fabsf(r[10]-m[10]) < 0.002f && fabsf(r[14]-m[14]) < 0.5f) {
            g_vpClusters[i].hits++;
            g_vpClusters[i].lastFrame = g_frame;
            return;
        }
    }
    if (g_vpClusterN < 12) {
        VpCluster* c = &g_vpClusters[g_vpClusterN++];
        memcpy(c->rep, m, sizeof(float) * 16);
        c->hits = 1; c->lastFrame = g_frame;
    }
}


static void WeaponDiagDump()
{
    Log("wpn: ---- c0 VP clusters this window (frame %lu) ----", (unsigned long)g_frame);
    // sort by hits (simple selection, N<=12)
    for (int pass = 0; pass < g_vpClusterN; pass++) {
        int best = -1; uint32_t bh = 0;
        for (int i = 0; i < g_vpClusterN; i++)
            if (g_vpClusters[i].hits > bh) { bh = g_vpClusters[i].hits; best = i; }
        if (best < 0) break;
        float* m = g_vpClusters[best].rep;
        // recover approx horizontal FOV from projection term m[0] = 1/tan(fovx/2)
        float fovx = (fabsf(m[0]) > 1e-4f)
            ? 2.0f * atanf(1.0f / fabsf(m[0])) * 57.29578f : 0.0f;
        Log("wpn: cluster hits=%-6u  m0=%.3f m5=%.3f m10=%.4f m14=%.1f  ~fovX=%.1f",
            g_vpClusters[best].hits, m[0], m[5], m[10], m[14], fovx);
        g_vpClusters[best].hits = 0;   // so next pass finds the next
    }
    Log("wpn: ---- end (the MINORITY cluster with a different ~fovX = weapon) ----");
    g_vpClusterN = 0;
}


// Stage 8.1 - the weapon-VP fingerprint discovered by the 8.0 diagnostic:
// first-person weapon/arms draws use a camera-locked VP whose z-translation
// is CONSTANT (-5.0) and whose z-scale is tiny (depth-compressed slab),
// while the world VP's z-terms track the camera through the level.
static inline bool WeaponVpMatch(const float* m)
{
    // 8.1 lesson (user test): the m14==-5 / tiny-m10 family is the WORLD
    // (rotating it made the world follow the hand). The OTHER family - 
    // m10 ~1.8 with large scene-varying negative m14 - is the weapon pass.
    return m[10] > 0.8f && m[10] < 3.0f && m[14] < -50.0f;
}


// Per-frame hand-delta rotation for the weapon draws (row-vector convention:
// vertices are in the camera-attached weapon space, x=right y=up z=fwd; we
// re-point z at the hand ray). TestYawDeg overrides with a fixed yaw (debug:
// proves the fingerprint + math without controller variables).
static bool WeaponHandRot(float* R)
{
    static uint32_t cachedFrame = 0xffffffff;
    static bool ok = false;
    static float Rc[16];
    if (cachedFrame != g_frame) {
        cachedFrame = g_frame;
        ok = false;
        float rel[3];
        if (g_wpnTestYaw != 0.0f) {
            float a = g_wpnTestYaw * 0.0174533f;
            rel[0] = sinf(a); rel[1] = 0.0f; rel[2] = cosf(a);
            ok = true;
        } else {
            ok = MaimHandRel(rel);
        }
        if (ok) {
            float Z[3] = { g_wpnFlipX ? -rel[0] : rel[0],
                           g_wpnFlipY ? -rel[1] : rel[1],
                           rel[2] };
            V3Norm(Z);
            float up[3] = { 0, 1, 0 };
            float X[3]; V3Cross(up, Z, X);
            if (V3Norm(X) < 0.2f) ok = false;      // pointing straight up/down
            else {
                float Y[3]; V3Cross(Z, X, Y);
                memset(Rc, 0, sizeof(Rc));
                Rc[0] = X[0]; Rc[1]  = X[1]; Rc[2]  = X[2];
                Rc[4] = Y[0]; Rc[5]  = Y[1]; Rc[6]  = Y[2];
                Rc[8] = Z[0]; Rc[9]  = Z[1]; Rc[10] = Z[2];
                Rc[15] = 1.0f;
            }
        }
    }
    if (ok) memcpy(R, Rc, sizeof(float) * 16);
    return ok;
}


static bool BlockFpMatches(int ci, float fp)
{
    if (ci < 0 || ci >= 32) return false;
    float want = g_blk[ci].fp;
    if (want <= 0.0f) return true;              // never captured - allow
    float tol = g_wpnFpTol * (want > 1.0f ? want : 1.0f);
    float d = fp - want; if (d < 0) d = -d;
    return d <= tol;
}


static const char* BlockModeName(uint8_t m)
{
    return m == BM_LEFT ? "LEFT hand" : m == BM_RIGHT ? "RIGHT hand" :
           m == BM_HIDE ? "HIDDEN" : "untouched";
}


static int CatIndex(uint32_t reg, uint32_t count)
{
    for (int i = 0; i < g_catN; i++)
        if (g_cat[i].reg == reg && g_cat[i].count == count) return i;
    if (g_catN >= 32) return -1;
    int idx = g_catN++;
    g_cat[idx].reg = reg; g_cat[idx].count = count;
    g_blk[idx].mode = BM_OFF; g_blk[idx].hideMask = 0; g_blk[idx].fp = 0.0f;
    for (int k = 0; k < g_savedN; k++)
        if (g_saved[k].reg == reg && g_saved[k].count == count) {
            g_blk[idx].mode = g_saved[k].mode;
            g_blk[idx].hideMask = g_saved[k].hideMask;
            g_blk[idx].fp = g_saved[k].fp;
            Log("wpnblk: [%d] c%ux%u restored from ini -> %s", idx, reg, count,
                BlockModeName(g_blk[idx].mode));
            break;
        }
    return idx;
}


static void NearAccum(uint32_t reg, const float* m, float d)
{
    for (int i = 0; i < g_nearN; i++) {
        if (g_near[i].reg == reg &&
            fabsf(g_near[i].t[0] - m[12]) < 30.0f &&
            fabsf(g_near[i].t[1] - m[13]) < 30.0f &&
            fabsf(g_near[i].t[2] - m[14]) < 30.0f) {
            g_near[i].n++;
            if (d < g_near[i].d) {
                g_near[i].d = d;
                g_near[i].t[0] = m[12]; g_near[i].t[1] = m[13]; g_near[i].t[2] = m[14];
            }
            return;
        }
    }
    int slot = -1;
    if (g_nearN < 12) slot = g_nearN++;
    else {
        float worst = -1;
        for (int i = 0; i < 12; i++) if (g_near[i].d > worst) { worst = g_near[i].d; slot = i; }
        if (d >= worst) return;
    }
    g_near[slot].d = d; g_near[slot].reg = reg; g_near[slot].n = 1;
    g_near[slot].t[0] = m[12]; g_near[slot].t[1] = m[13]; g_near[slot].t[2] = m[14];
}


static void NearDump()
{
    Log("wpnnear: ---- per-draw LocalToWorld closest to the camera ----");
    Log("wpnnear: cam=(%.0f,%.0f,%.0f) affine-uploads=%u rotated=%u attach=%d r=%.0fuu",
        g_waCamPos[0], g_waCamPos[1], g_waCamPos[2], g_waAffine, g_waHits,
        (int)g_wpnAttach, g_wpnRadius);
    for (int pass = 0; pass < 12; pass++) {
        int best = -1; float bd = 1e30f;
        for (int i = 0; i < g_nearN; i++)
            if (g_near[i].n && g_near[i].d < bd) { bd = g_near[i].d; best = i; }
        if (best < 0) break;
        Log("wpnnear:   c%-3u dist=%7.1fuu pos=(%.0f,%.0f,%.0f) draws=%u",
            g_near[best].reg, g_near[best].d, g_near[best].t[0], g_near[best].t[1],
            g_near[best].t[2], g_near[best].n);
        g_near[best].n = 0;
    }
    g_nearN = 0; g_waAffine = 0; g_waHits = 0;
    Log("wpnnear: ---- end (something ~10-100uu away EVERY frame = arms/weapon) ----");
}


// Build this frame's rotation: maps the camera's basis onto the hand's, then
// wraps it as a rotation about the camera position.
static void WeaponAttachPrepare()
{
    if (g_waFrame == g_frame) return;
    g_waFrame = g_frame;
    g_waOk = false; g_waHaveCam = false; g_waLOk = false;

    // MESH-LOCAL hand rotation. The bone dumps show translations of only
    // 3-150uu, so Dishonored uploads bone matrices in mesh-local space, not
    // world space - which is exactly why a camera-distance filter could never
    // match them. In local space we don't need the camera at all: rotate the
    // skeleton about its own origin (which for the FP mesh IS the viewpoint).
    for (int h = 0; h < 2; h++) {
        g_handOk[h] = false;
        float rel[3], pos[3];
        if (!HandRelFull(h, rel, pos)) continue;
        float Fp[3] = { rel[2], rel[0], rel[1] };       // UE3 local: X fwd,Y right,Z up
        if (V3Norm(Fp) < 0.5f) continue;
        float up[3] = {0,0,1};
        float Rp[3]; V3Cross(up, Fp, Rp);
        if (V3Norm(Rp) < 0.2f) continue;
        float Up[3]; V3Cross(Fp, Rp, Up); V3Norm(Up);
        float* L = g_handL[h];
        L[0]=Fp[0]; L[1]=Fp[1]; L[2]=Fp[2];
        L[3]=Rp[0]; L[4]=Rp[1]; L[5]=Rp[2];
        L[6]=Up[0]; L[7]=Up[1]; L[8]=Up[2];

        if (!g_haveRest[h]) {                          // neutral = where it is now
            g_restPos[h][0]=pos[0]; g_restPos[h][1]=pos[1]; g_restPos[h][2]=pos[2];
            g_haveRest[h] = true;
        }
        float d[3] = { pos[0]-g_restPos[h][0], pos[1]-g_restPos[h][1],
                       pos[2]-g_restPos[h][2] };
        float o[3] = { d[2]*g_wpnPosScale,      // X = forward/back  (push & pull!)
                       d[0]*g_wpnPosScale,      // Y = right
                       d[1]*g_wpnPosScale };    // Z = up
        for (int k = 0; k < 3; k++) {
            if (o[k] >  g_wpnPosMax) o[k] =  g_wpnPosMax;
            if (o[k] < -g_wpnPosMax) o[k] = -g_wpnPosMax;
            g_handPos[h][k] = o[k];
        }
        g_handOk[h] = true;
        if (h == g_maimHand) { memcpy(g_waL, L, sizeof(g_waL)); g_waLOk = true; }
    }

    if (!CamStillValid()) {
        static int cool = 0;
        g_camObj = NULL;
        if (cool-- <= 0) { cool = 200; FindLiveCamera(); }
        if (!CamStillValid()) return;
    }
    if (!RangeReadable(g_camObj + 0x50, 0x40)) return;
    float* cf = (float*)(g_camObj + 0x50);
    float* cr = (float*)(g_camObj + 0x60);
    float* cu = (float*)(g_camObj + 0x70);
    float* cp = (float*)(g_camObj + 0x80);
    float F[3] = {cf[0],cf[1],cf[2]}, R[3] = {cr[0],cr[1],cr[2]}, U[3] = {cu[0],cu[1],cu[2]};
    if (V3Norm(F) < 0.5f || V3Norm(R) < 0.5f || V3Norm(U) < 0.5f) return;
    g_waCamPos[0] = cp[0]; g_waCamPos[1] = cp[1]; g_waCamPos[2] = cp[2];
    g_waHaveCam = true;

    float rel[3];
    if (!MaimHandRel(rel)) return;                 // no controller pose yet

    // hand ray in world space, built on the camera basis (same math the
    // projectile redirect uses - that one is confirmed working)
    float Fp[3] = { F[0]*rel[2] + R[0]*rel[0] + U[0]*rel[1],
                    F[1]*rel[2] + R[1]*rel[0] + U[1]*rel[1],
                    F[2]*rel[2] + R[2]*rel[0] + U[2]*rel[1] };
    if (V3Norm(Fp) < 0.5f) return;
    float up[3] = {0,0,1};
    float Rp[3]; V3Cross(up, Fp, Rp);
    if (V3Norm(Rp) < 0.2f) return;                 // pointing straight up/down
    float Up[3]; V3Cross(Fp, Rp, Up); V3Norm(Up);

    // Rdelta = camBasis^T * handBasis  (row-vector convention)
    float D[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            D[i*3+j] = F[i]*Fp[j] + R[i]*Rp[j] + U[i]*Up[j];

    memcpy(g_waD, D, sizeof(D));
    memset(g_waA, 0, sizeof(g_waA));
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) g_waA[i*4+j] = D[i*3+j];
    g_waA[15] = 1.0f;
    for (int j = 0; j < 3; j++) {
        float s = g_waCamPos[0]*D[0*3+j] + g_waCamPos[1]*D[1*3+j] + g_waCamPos[2]*D[2*3+j];
        g_waA[12+j] = g_waCamPos[j] - s;
    }
    g_waOk = true;
}


static void ShapeAccum(uint32_t reg, uint32_t count, const float* m)
{
    int slot = -1;
    for (int i = 0; i < g_shapeN; i++)
        if (g_shapes[i].reg == reg && g_shapes[i].count == count) { slot = i; break; }
    if (slot < 0) {
        if (g_shapeN >= 28) return;
        slot = g_shapeN++;
        ShapeRec* s = &g_shapes[slot];
        s->reg = reg; s->count = count; s->n = 0;
        s->dRow = s->dCol = 1e30f; s->magCol = 0;
    }
    ShapeRec* s = &g_shapes[slot];
    s->n++;
    float tr[3] = { m[12], m[13], m[14] };          // row-major 4x4
    float tc[3] = { m[3],  m[7],  m[11] };          // transposed 3x4
    for (int k = 0; k < 3; k++) {
        if (tr[k] != tr[k] || tc[k] != tc[k]) return;
        if (fabsf(tr[k]) > 3.0e38f || fabsf(tc[k]) > 3.0e38f) return;
    }
    float dr = sqrtf((tr[0]-g_waCamPos[0])*(tr[0]-g_waCamPos[0]) +
                     (tr[1]-g_waCamPos[1])*(tr[1]-g_waCamPos[1]) +
                     (tr[2]-g_waCamPos[2])*(tr[2]-g_waCamPos[2]));
    float dc = sqrtf((tc[0]-g_waCamPos[0])*(tc[0]-g_waCamPos[0]) +
                     (tc[1]-g_waCamPos[1])*(tc[1]-g_waCamPos[1]) +
                     (tc[2]-g_waCamPos[2])*(tc[2]-g_waCamPos[2]));
    if (dr < s->dRow) { s->dRow = dr; memcpy(s->tRow, tr, sizeof(tr)); }
    if (dc < s->dCol) {
        s->dCol = dc; memcpy(s->tCol, tc, sizeof(tc));
        s->magCol = sqrtf(tc[0]*tc[0] + tc[1]*tc[1] + tc[2]*tc[2]);
    }
}


static void ShapeDump()
{
    Log("wpnshape: ---- constant-upload blocks (bone palettes live here) ----");
    for (int pass = 0; pass < 28; pass++) {
        int best = -1; uint32_t bn = 0;
        for (int i = 0; i < g_shapeN; i++)
            if (g_shapes[i].n > bn) { bn = g_shapes[i].n; best = i; }
        if (best < 0) break;
        ShapeRec* s = &g_shapes[best];
        {
            int ci = CatIndex(s->reg, s->count);
            uint8_t md = (ci >= 0) ? g_blk[ci].mode : BM_OFF;
            bool fb = g_wpnAttach && md == BM_OFF &&
                      (int)(s->count/3) <= g_wpnMaxBones;
            Log("wpnshape:  [%d] c%-3u x%-3u (%2u bones) uploads=%-6u %s%s%s",
                ci, s->reg, s->count, s->count / 3, s->n,
                md != BM_OFF ? BlockModeName(md) : (fb ? "follows default hand" : ""),
                (ci >= 0 && g_blk[ci].hideMask) ? "  +bones hidden" : "",
                (ci == g_wpnTarget) ? "   <== TARGET" : "");
        }
        s->n = 0;
    }
    g_shapeN = 0;
    Log("wpnshape: ---- end (small colT d, or small |t|, = player-attached) ----");
}


// Rotate a bone palette: each bone is 3 registers, transposed 3x4, so its
// world position is the 4th column. Bones within the radius get rotated about
// the camera onto the hand ray - this is the skinned-mesh equivalent of the
// rigid path, and the one that should actually catch the arms/weapon.
// Collapse every bone to a point: the mesh becomes degenerate triangles and
// stops being visible. This is how we remove the forearms without touching
// the engine.
static void WeaponHideBones(float* blk, uint32_t count)
{
    memset(blk, 0, sizeof(float) * 4 * count);
}


// Apply a block's assignment: hide it, hide individual bones inside it, and/or
// move it with a hand (rotation + 6DoF translation).
static int WeaponApplyBlock(float* blk, uint32_t count, int ci, int fallbackHand)
{
    uint32_t bones = count / 3;
    if (bones > 64) bones = 64;
    BlockState* st = (ci >= 0 && ci < 32) ? &g_blk[ci] : NULL;
    uint8_t mode = st ? st->mode : BM_OFF;

    if (mode == BM_HIDE) { memset(blk, 0, sizeof(float) * 4 * count); return (int)bones; }

    int hand = (mode == BM_LEFT) ? 0 : (mode == BM_RIGHT) ? 1 : fallbackHand;
    bool move = (hand >= 0 && hand <= 1 && g_handOk[hand]);
    uint64_t hide = st ? st->hideMask : 0;
    if (!move && !hide) return 0;

    const float* L = move ? g_handL[hand] : NULL;
    const float* P = move ? g_handPos[hand] : NULL;
    int touched = 0;

    for (uint32_t b = 0; b < bones; b++) {
        float* m = blk + b * 12;
        if (hide & (1ull << b)) {          // collapse this bone only
            memset(m, 0, sizeof(float) * 12);
            touched++;
            continue;
        }
        if (!move) continue;
        float nr[12];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++) {  // j==3 is the translation column
                float s = 0;
                for (int k = 0; k < 3; k++) s += L[k*3+i] * m[k*4+j];
                nr[i*4+j] = s;
            }
        nr[3]  += P[0];                    // 6DoF: push/pull, strafe, raise
        nr[7]  += P[1];
        nr[11] += P[2];
        memcpy(m, nr, sizeof(nr));
        touched++;
    }
    return touched;
}


// If this LocalToWorld belongs to something attached to the player, rotate it.
static bool WeaponAttachApply(float* m)
{
    if (!g_waOk) return false;
    float dx = m[12]-g_waCamPos[0], dy = m[13]-g_waCamPos[1], dz = m[14]-g_waCamPos[2];
    if (dx*dx + dy*dy + dz*dz > g_wpnRadius * g_wpnRadius) return false;
    float out[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += m[i*4+k] * g_waA[k*4+j];
            out[i*4+j] = s;
        }
    memcpy(m, out, sizeof(out));
    return true;
}


// A skeletal COMPONENT under this actor - not a SkeletalMesh asset, which is
// what the 26.2 census kept turning up.
static uint8_t* FpMeshUnder(uint8_t* owner, uint32_t* offOut)
{
    if (!owner || !RangeReadable(owner, 0x40)) return NULL;
    for (uint32_t o2 = 0x20; o2 + 4 <= 0x600; o2 += 4) {
        if (!RangeReadable(owner + o2, 4)) break;
        uint8_t* c = *(uint8_t**)(owner + o2);
        if (!LooksLikeObj(c) || !RangeReadable(c, 0x120)) continue;
        const char* cc = ObjClassName(c);
        if (!cc || !strstr(cc, "Skeletal") || !strstr(cc, "Component")) continue;
        if (offOut) *offOut = o2;
        return c;
    }
    return NULL;
}


static void FpCensus()
{
    Log("census: ==== latched actors ====");
    Log("census:  controller = %p (%s)", (void*)g_peCtrl,
        (g_peCtrl && LooksLikeObj(g_peCtrl)) ? ObjClassName(g_peCtrl) : "none yet");
    Log("census:  pawn       = %p (%s)", (void*)g_pePawn,
        (g_pePawn && LooksLikeObj(g_pePawn)) ? ObjClassName(g_pePawn) : "none yet");

    uint8_t* pawn = g_pePawn;
    if (!pawn && g_peCtrl && RangeReadable(g_peCtrl + 0x248, 4)) {
        uint8_t* p = *(uint8_t**)(g_peCtrl + 0x248);
        if (LooksLikeObj(p)) { pawn = p; Log("census:  controller +0x248 -> %p (%s)",
                                            (void*)p, ObjClassName(p)); }
    }
    if (!pawn) { Log("census: no pawn yet - move around in gameplay, then retry"); return; }

    int shown = 0;
    for (uint32_t o2 = 0x20; o2 + 4 <= 0x600 && shown < 60; o2 += 4) {
        if (!RangeReadable(pawn + o2, 4)) break;
        uint8_t* c = *(uint8_t**)(pawn + o2);
        if (!LooksLikeObj(c)) continue;
        const char* cn = RealName(*(uint32_t*)(c + kNameOff));
        Log("census:   pawn +0x%03x '%s' (%s)", (unsigned)o2, cn ? cn : "?",
            ObjClassName(c));
        shown++;
    }
    Log("census: ==== %d field(s) under the pawn ====", shown);
    g_fpNextMs = 0.0; FindFpMesh();
    FpRawDump();
}


// The delta write lands, then the engine rebuilds the matrix before the next
// frame - "engine-left-it-alone=0" over 1800 writes says so. We are painting
// over a computed RESULT. UE3 components carry the INPUTS the engine composes
// that result from: a relative Translation, a relative Rotation (FRotator) and
// Scale/Scale3D. Write the input and the engine bakes it in for us, every
// frame, for free. So dump the raw block and find them - Scale3D reads (1,1,1)
// and Scale reads 1.0, which is a fingerprint we can spot by eye.
static void FpRawDump()
{
    if (!g_fpMesh || !RangeReadable(g_fpMesh, 0x300)) {
        Log("rawdump: no mesh (press HOME in gameplay first)"); return;
    }
    Log("rawdump: ==== pMesh @ %p ====", (void*)g_fpMesh);
    for (uint32_t o = 0; o < 0x300; o += 16) {
        if (!RangeReadable(g_fpMesh + o, 16)) break;
        const float*   f = (const float*)(g_fpMesh + o);
        const int32_t* i = (const int32_t*)(g_fpMesh + o);
        char fb[128];
        for (int k = 0; k < 4; k++) {
            float v = f[k];
            char one[32];
            if (v != v || v > 1.0e9f || v < -1.0e9f) strcpy(one, "  - ");
            else snprintf(one, sizeof(one), "%8.3f", v);
            if (k == 0) strcpy(fb, one); else { strcat(fb, " "); strcat(fb, one); }
        }
        Log("rawdump: +0x%03x f(%s)  i(%11d %11d %11d %11d)",
            (unsigned)o, fb, i[0], i[1], i[2], i[3]);
    }
    Log("rawdump: ==== end ====");
}


static bool FindFpMesh()
{
    if (g_fpMesh && RangeReadable(g_fpMesh, 0x120)) {
        const char* cn = ObjClassName(g_fpMesh);
        if (cn && strstr(cn, "Skeletal") && strstr(cn, "Component")) return true;
        g_fpMesh = NULL; g_fpHaveBase = false;
    }
    double now = MaimNowMs();
    if (now < g_fpNextMs) return false;
    g_fpNextMs = now + 500.0;

    g_fpMesh = NULL; g_fpPawn = NULL;
    uint8_t* pawn = (g_pePawn && LooksLikeObj(g_pePawn)) ? g_pePawn : NULL;
    const char* route = "latched pawn";
    if (!pawn && g_peCtrl && RangeReadable(g_peCtrl + 0x248, 4)) {
        uint8_t* p = *(uint8_t**)(g_peCtrl + 0x248);
        if (LooksLikeObj(p)) { pawn = p; route = "controller+0x248"; }
    }
    if (!pawn) { FpWhy("no pawn latched yet - the event stream has not shown one"); return false; }

    uint32_t off = 0;
    uint8_t* mesh = FpMeshUnder(pawn, &off);
    if (!mesh) { FpWhy("pawn latched, but no skeletal COMPONENT under it (Shift+HOME for a census)"); return false; }

    g_fpPawn = pawn; g_fpMesh = mesh; g_fpHaveBase = false;
    static void* lastLogged = NULL;
    if (lastLogged != (void*)mesh) {
        lastLogged = (void*)mesh;
        Log("handmesh: mesh '%s' @ %p via %s (pawn %p +0x%03x)",
            ObjClassName(mesh), (void*)mesh, route, (void*)pawn, (unsigned)off);
        FpDumpMatrix("found");
    }
    return true;
}


static void FpDumpMatrix(const char* tag)
{
    if (!g_fpMesh || !RangeReadable(g_fpMesh + 0x60, 64)) {
        Log("handmesh: %s - no mesh", tag); return;
    }
    const float* m = (const float*)(g_fpMesh + 0x60);
    Log("handmesh: %s rows (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) T(%.1f,%.1f,%.1f)",
        tag, m[0],m[1],m[2], m[4],m[5],m[6], m[8],m[9],m[10], m[12],m[13],m[14]);
}
