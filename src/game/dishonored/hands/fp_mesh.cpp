// game/dishonored/hands/fp_mesh.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// Never write unless the block still reads like that block. This is the gate
// the shader-constant builds lacked, and it is why they wrecked world
// geometry: they wrote wherever the shape looked plausible.
static bool FpFieldsLookRight(uint8_t* m)
{
    if (!m || !RangeReadable(m + kMeshTrans, 0x40)) return false;
    float sc = *(const float*)(m + kMeshScale);
    if (!(sc > 0.01f && sc < 100.0f)) return false;
    const float* s3 = (const float*)(m + kMeshScl3D);
    for (int k = 0; k < 3; k++)
        if (!(s3[k] > 0.01f && s3[k] < 100.0f)) return false;
    const int32_t* r = (const int32_t*)(m + kMeshRot);
    for (int k = 0; k < 3; k++)
        if (r[k] < -0x200000 || r[k] > 0x200000) return false;
    return true;
}


static void FpZero(uint8_t** slot)
{
    uint8_t* o = *slot;
    *slot = NULL;
    if (!o || !LooksLikeObj(o) || !FpFieldsLookRight(o)) return;
    int32_t* r = (int32_t*)(o + kMeshRot);
    r[0] = 0; r[1] = 0; r[2] = 0;
    float* T = (float*)(o + kMeshTrans);
    T[0] = T[1] = T[2] = 0.0f;
    Log("handmesh: '%s' put back to (0,0,0)", RealName(*(uint32_t*)(o + kNameOff)));
}


static bool FpIsViewModel(const FpCand* k)
{
    return k && !strcmp(k->name, "pPlayerMesh");
}


// Corvo holds the blade in his right hand and everything else in his left.
// The "owner" class was just whichever node the walk arrived from - it read
// PowerBlink for every single item, which is exactly why the sword stayed
// stuck to the left controller. The MESH ASSET name is the honest label:
// Wpn_PlySword01, crossbow_01, SpringRazor, Heart.
static int FpHandFor(const FpCand* k)
{
    int h = 0;
    if (k && (strstr(k->asset, "Sword") || strstr(k->asset, "sword") ||
              strstr(k->asset, "Blade") || strstr(k->asset, "blade")))
        h = 1;
    return g_fpSwap ? (1 - h) : h;
}


// A weapon held out to the side sits a long way from the actor's origin, so
// rotating the component swings it through a huge arc - that is why the sword
// flew off to the right and out of view. Rotate it about ITS OWN centre
// instead and it spins in place like something held in a hand.
//
// The component already tells us where its centre is: FBoxSphereBounds.Origin
// at +0x0cc, in world space, next to the world translation at +0x090. The
// difference between them, projected onto the world matrix rows (which ARE the
// actor's axes while our own rotation is zero), is the centre in actor space.
// So zero the rotations, let the engine rebuild for a frame, then measure.
// The component's rotation is relative to the PAWN, and I had been assuming
// the pawn turns with your head. It does not. Head tracking rewrites the
// camera's view rotation through ProcessViewRotation; it never touches the
// pawn, which only turns when you push the stick. So a head-relative angle was
// being applied against a pawn that had not moved - and the weapon swung with
// your head. Read the pawn's real yaw and take it out of the sum.
// Reuse the AIMING pipeline instead of a parallel reimplementation of it.
// HandRelFull + MaimDirFromView is the path that measured dot(hand)=+1.00 on
// live projectiles - it is the one piece of hand maths in this mod that has
// been verified against the game rather than reasoned about. My separate
// HandAnglesPos version kept disagreeing with it by a sign, which is exactly
// the sort of bug that does not happen when there is only one implementation.
//
// It gives a WORLD direction; the component's rotation is relative to the
// pawn, so take the pawn's yaw back out at the end.
static bool FpHandAngles(int hand, float* yawOut, float* pitchOut)
{
    float rel[3];
    if (!HandRelFull(hand, rel, NULL)) return false;

    float dir[3];
    MaimDirFromView(g_viewYawRad, g_viewPitchRad, rel, dir);
    float z = dir[2];
    if (z >  1.0f) z =  1.0f;
    if (z < -1.0f) z = -1.0f;

    float worldPitch = asinf(z);
    float worldYaw   = atan2f(dir[1], dir[0]);

    float pawnYaw;
    *yawOut   = FpPawnYaw(&pawnYaw) ? FpWrapPi(worldYaw - pawnYaw) : worldYaw;
    *pitchOut = worldPitch;
    return true;
}


static bool FpPawnYaw(float* out)
{
    uint8_t* pawn = FpPawn();
    if (!pawn || !RangeReadable(pawn + 0x9c, 12)) return false;
    const int32_t* r = (const int32_t*)(pawn + 0x9c);
    if (r[0] < -0x40000 || r[0] > 0x40000) return false;   // pitch sane?
    if (r[2] < -0x40000 || r[2] > 0x40000) return false;   // roll sane?
    *out = (float)r[1] / kUEPerRad;
    return true;
}


static float FpWrapPi(float a)
{
    while (a >  3.14159265f) a -= 6.28318531f;
    while (a < -3.14159265f) a += 6.28318531f;
    return a;
}


// Where the hand points, in GAME WORLD space, via the aiming maths.
static bool FpHandWorld(int hand, float* yaw, float* pitch)
{
    float rel[3];
    if (!HandRelFull(hand, rel, NULL)) return false;
    float dir[3];
    MaimDirFromView(g_viewYawRad, g_viewPitchRad, rel, dir);
    float z = dir[2];
    if (z >  1.0f) z =  1.0f;
    if (z < -1.0f) z = -1.0f;
    *pitch = asinf(z);
    *yaw   = atan2f(dir[1], dir[0]);
    return true;
}


// Where the component actually ENDED UP, read back out of the engine.
static bool FpWorldAngles(uint8_t* o, float* yaw, float* pitch)
{
    if (!LooksLikeObj(o) || !RangeReadable(o + 0x60, 16)) return false;
    const float* m = (const float*)(o + 0x60);
    float z = m[2];
    if (z >  1.0f) z =  1.0f;
    if (z < -1.0f) z = -1.0f;
    *yaw   = atan2f(m[1], m[0]);
    *pitch = asinf(z);
    return true;
}


static bool FpRefAngles(float* yaw, float* pitch)
{
    if (!g_fpRef || !LooksLikeObj(g_fpRef)) {
        g_fpRef = NULL;
        for (int i = 0; i < g_fpCandN; i++)
            if (!FpIsViewModel(&g_fpCand[i]) && LooksLikeObj(g_fpCand[i].obj)) {
                g_fpRef = g_fpCand[i].obj; break;
            }
        if (!g_fpRef) return false;
    }
    return FpWorldAngles(g_fpRef, yaw, pitch);
}


static void FpCommandAll(float yawDeg, float pitchDeg)
{
    for (int i = 0; i < g_fpCandN; i++) {
        uint8_t* o = g_fpCand[i].obj;
        if (!LooksLikeObj(o) || !FpFieldsLookRight(o)) continue;
        int32_t* r = (int32_t*)(o + kMeshRot);
        r[0] = (int32_t)(pitchDeg / 57.2958f * kUEPerRad);
        r[1] = (int32_t)(yawDeg   / 57.2958f * kUEPerRad);
        r[2] = 0;
        float* T = (float*)(o + kMeshTrans);
        T[0] = T[1] = T[2] = 0.0f;
    }
}


// I have now corrected these signs by hand three times and been wrong twice,
// so stop asserting them. Nudge each weapon by a known angle, read back what
// the engine did with it, and derive the sign from the difference. A mesh
// whose parent frame is rolled 180 degrees answers -1 and one that is not
// answers +1, and neither of us has to know which is which.
static void FpCalibrateTick()
{
    // 30.1: probe the VIEW MODELS only. The old FpCommandAll also commanded
    // pMesh (a handoff rule violation - it carries the camera, hence the
    // "calibration twitch"), and if the weapons hang off pMesh bones it
    // contaminated the very sign measurement this function existed for.
    // 30.2: three translation probes added (phases 5-7). E, the engine's
    // response to a written Translation, is measured per weapon and inverted,
    // so position writes stop assuming the written frame IS the world frame.
    const float kProbe = 15.0f;
    const float kTP    = 30.0f;                       // uu translation probe
    if      (g_fpCalPhase == 1) GtCommandVM(0.0f,   0.0f, 0.0f);
    else if (g_fpCalPhase == 2) GtCommandVM(0.0f, kProbe, 0.0f);
    else if (g_fpCalPhase == 3) GtCommandVM(kProbe, 0.0f, 0.0f);
    else if (g_fpCalPhase == 5) GtCommandVM(0.0f, 0.0f, 0.0f, kTP, 0.0f, 0.0f);
    else if (g_fpCalPhase == 6) GtCommandVM(0.0f, 0.0f, 0.0f, 0.0f, kTP, 0.0f);
    else if (g_fpCalPhase == 7) GtCommandVM(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, kTP);
    else                        GtCommandVM(0.0f,   0.0f, 0.0f);   // 4 and 8

    if (++g_fpCalFrame < 3) return;      // let the engine rebuild first
    g_fpCalFrame = 0;

    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        if (g_fpCalPhase >= 5 && g_fpCalPhase <= 7) {
            float pw[3];
            if (FpWorldPos(k->obj, pw)) {
                int r = g_fpCalPhase - 5;
                for (int q = 0; q < 3; q++)
                    k->emap[r*3+q] = (pw[q] - k->calT0[q]) / kTP;
            }
            continue;
        }
        float wy, wp;
        if (!FpWorldAngles(k->obj, &wy, &wp)) continue;
        if (g_fpCalPhase == 1) {
            k->calY0 = wy; k->calP0 = wp;
            k->yawSign = 1; k->pitchSign = 1;
            k->haveE = false;
            k->haveLive = false;             // fresh parent estimate too
            FpBasis(k->obj, k->basis0);
        } else if (g_fpCalPhase == 2 || g_fpCalPhase == 3) {
            float b1[9], ax[3];
            if (!FpBasis(k->obj, b1)) continue;
            FpDeltaAxis(b1, k->basis0, ax);
            // +yaw turns about world Z and shows up negative in this
            // convention; +pitch turns about the parent's Y and shows up
            // positive. Anything smaller than this is noise, and on noise we
            // keep +1 rather than inventing a flip.
            if (g_fpCalPhase == 2)
                k->yawSign = (ax[2] < -0.10f) ? 1 : ((ax[2] > 0.10f) ? -1 : 1);
            else
                k->pitchSign = (ax[1] > 0.10f) ? 1 : ((ax[1] < -0.10f) ? -1 : 1);
        } else if (g_fpCalPhase == 4) {
            k->parentY = wy; k->parentP = wp;        // rotation is zero here
            k->lastCmdY = 0.0f; k->lastCmdP = 0.0f;
            FpWorldPos(k->obj, k->calT0);            // rest pos for E probes
        }
    }

    if (g_fpCalPhase == 8) {
        for (int i = 0; i < g_fpCandN; i++) {
            FpCand* k = &g_fpCand[i];
            if (!FpIsViewModel(k)) continue;
            k->haveE = M3Inv(k->emap, k->einv);
            Log("handmesh: '%s' E r0=(%+.2f,%+.2f,%+.2f) r1=(%+.2f,%+.2f,%+.2f) r2=(%+.2f,%+.2f,%+.2f)%s",
                k->asset,
                k->emap[0], k->emap[1], k->emap[2],
                k->emap[3], k->emap[4], k->emap[5],
                k->emap[6], k->emap[7], k->emap[8],
                k->haveE ? "" : "  ** UNINVERTIBLE - position writes off for this weapon **");
        }
        for (int hand = 0; hand < 2; hand++) {
            float hy, hp;
            if (FpHandWorld(hand, &hy, &hp)) {
                g_fpHandNeutY[hand] = hy; g_fpHandNeutP[hand] = hp;
            }
        }
        float ry, rp;
        g_fpHaveRef = FpRefAngles(&ry, &rp);
        if (g_fpHaveRef) {
            g_fpRefY0 = ry; g_fpRefP0 = rp;
            if (!FpBasis(g_fpRef, g_fpRefBas0)) g_fpHaveRef = false;
        }
        for (int hand = 0; hand < 2; hand++)
            g_fpHaveHandBas[hand] = FpHandBasisWorld(hand, g_fpHandBas0[hand]);
        Log("handmesh: hand bases captured L=%d R=%d",
            (int)g_fpHaveHandBas[0], (int)g_fpHaveHandBas[1]);
        FpComputePivots();
        for (int i = 0; i < g_fpCandN; i++)
            if (FpIsViewModel(&g_fpCand[i]))
                Log("handmesh: '%s' yawSign=%+d pitchSign=%+d parent(y=%.0f p=%.0f)",
                    g_fpCand[i].asset, g_fpCand[i].yawSign, g_fpCand[i].pitchSign,
                    g_fpCand[i].parentY * 57.2958f, g_fpCand[i].parentP * 57.2958f);
        Log("handmesh: calibration done");
    }
    if (++g_fpCalPhase > 8) g_fpCalPhase = 0;
}


static void FpComputePivots()
{
    int got = 0;
    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        k->havePivot = false;
        uint8_t* m = k->obj;
        if (!LooksLikeObj(m) || !RangeReadable(m + 0x60, 0x80)) continue;
        const float* M  = (const float*)(m + 0x60);      // rows at +0x60/70/80
        const float* Tw = (const float*)(m + 0x90);      // world translation
        const float* Ow = (const float*)(m + 0xcc);      // bounds origin
        float d[3] = { Ow[0] - Tw[0], Ow[1] - Tw[1], Ow[2] - Tw[2] };
        bool ok = true;
        for (int q = 0; q < 3; q++)
            if (d[q] != d[q] || d[q] > 1.0e5f || d[q] < -1.0e5f) ok = false;
        if (!ok) continue;
        for (int r = 0; r < 3; r++) {
            const float* row = M + r * 4;
            k->pivot[r] = d[0]*row[0] + d[1]*row[1] + d[2]*row[2];
        }
        float len = sqrtf(k->pivot[0]*k->pivot[0] + k->pivot[1]*k->pivot[1] +
                          k->pivot[2]*k->pivot[2]);
        if (len > 400.0f) continue;                      // nonsense - skip it
        k->havePivot = true;
        got++;
        if (FpIsViewModel(k))
            Log("handmesh: pivot for '%s' = (%.1f, %.1f, %.1f) uu",
                k->asset, k->pivot[0], k->pivot[1], k->pivot[2]);
    }
    Log("handmesh: measured %d pivot(s)", got);
}


static void FpCaptureNeutral(const char* why)
{
    for (int hand = 0; hand < 2; hand++) {
        float yr, pa, pos[3];
        float cy, cp;
        if (HandAnglesPos(hand, &yr, &pa, pos) && FpHandAngles(hand, &cy, &cp)) {
            g_fpNeutral[hand][0] = pos[0];
            g_fpNeutral[hand][1] = pos[1];
            g_fpNeutral[hand][2] = pos[2];
            g_fpNeutralYaw[hand]   = cy;      // wherever you are holding it NOW
            g_fpNeutralPitch[hand] = cp;      // counts as "straight ahead"
            g_fpHaveNeutral[hand] = true;
            g_fpHaveNeutralRoom[hand] =       // 30.23: relative to body anchor
                g_bodyAnchorOk && HandRoomPos(hand, g_fpNeutralRoom[hand]);
            if (g_fpHaveNeutralRoom[hand])
                for (int q = 0; q < 3; q++)
                    g_fpNeutralRoom[hand][q] -= g_bodyAnchor[q];
        } else {
            g_fpHaveNeutral[hand] = false;
            g_fpHaveNeutralRoom[hand] = false;
        }
    }
    g_rtdReq = 3;   // 30.70/71: END recaptures BOTH hands' neutrals
    Log("handmesh: neutral hand pose captured (%s) L=%d R=%d", why,
        (int)g_fpHaveNeutral[0], (int)g_fpHaveNeutral[1]);
    Log("handmesh:   neutral angles L(y=%.0f p=%.0f) R(y=%.0f p=%.0f) deg",
        g_fpNeutralYaw[0]*57.2958f, g_fpNeutralPitch[0]*57.2958f,
        g_fpNeutralYaw[1]*57.2958f, g_fpNeutralPitch[1]*57.2958f);
    g_fpPivotPend = 3;          // hold still for a few frames, then measure
}


static void FpRestoreRotation()
{
    FpZero(&g_fpWritten);
    FpZero(&g_fpWritten2);
    for (int i = 0; i < g_fpCandN; i++) {
        g_fpCand[i].lastCmdY = g_fpCand[i].lastCmdP = g_fpCand[i].lastCmdR = 0.0f;
        uint8_t* o = g_fpCand[i].obj;
        if (!o || !LooksLikeObj(o) || !FpFieldsLookRight(o)) continue;
        int32_t* r = (int32_t*)(o + kMeshRot);
        r[0] = 0; r[1] = 0; r[2] = 0;
        float* T = (float*)(o + kMeshTrans);
        T[0] = T[1] = T[2] = 0.0f;
    }
}


static uint8_t* FpPawn()
{
    if (g_pePawn && LooksLikeObj(g_pePawn)) return g_pePawn;
    if (g_peCtrl && RangeReadable(g_peCtrl + 0x248, 4)) {
        uint8_t* p = *(uint8_t**)(g_peCtrl + 0x248);
        if (LooksLikeObj(p)) return p;
    }
    return NULL;
}


// What the crossbow test taught us: 'pPlayerMesh' on a
// DishonoredItemSkeletalComponent is the first-person view model, and writing
// its relative Rotation moves it. What it ALSO taught us is that caching one
// pointer is wrong - every inventory item owns its own view model, created and
// swapped as you equip things, so a list captured once only ever drives the
// weapon you happened to be holding. Hence "only the crossbow".
//
// So: re-collect on a timer, keep EVERY view model, and drive them all. The
// item you are not holding is not on screen, so driving it costs nothing and
// the moment you switch weapons the new one is already being driven.
static const char* FpAssetName(uint8_t* comp)
{
    // the SkeletalMesh asset this component renders - its name tells us
    // whether the arms and the weapon are one mesh or two
    for (uint32_t o = 0x20; o + 4 <= 0x400; o += 4) {
        if (!RangeReadable(comp + o, 4)) break;
        uint8_t* a = *(uint8_t**)(comp + o);
        if (!LooksLikeObj(a)) continue;
        const char* ac = ObjClassName(a);
        if (ac && !strcmp(ac, "SkeletalMesh"))
            return RealName(*(uint32_t*)(a + kNameOff));
    }
    return NULL;
}


static void FpCollect()
{
    FpRestoreRotation();
    for (int i = 0; i < g_fpCandN && i < 24; i++) g_fpPrev[i] = g_fpCand[i];
    g_fpPrevN = g_fpCandN;
    for (int i = g_fpPrevN; i < 24; i++) g_fpPrev[i].obj = NULL;
    g_fpCandN = 0; g_fpSel = -1;
    uint8_t* pawn = FpPawn();
    if (!pawn) { Log("handmesh: collect - no pawn latched yet"); return; }
    g_fpLastPawn = pawn;

    uint8_t* q[160]; int qd[160]; int head = 0, tail = 0;
    uint8_t* seen[224]; int seenN = 0;
    q[tail] = pawn; qd[tail] = 0; tail++;
    seen[seenN++] = pawn;

    int visited = 0;
    while (head < tail && visited < 140) {
        uint8_t* o = q[head]; int d = qd[head]; head++; visited++;
        const char* oc = ObjClassName(o);
        if (d >= 3) continue;
        for (uint32_t off = 0x20; off + 4 <= 0x600; off += 4) {
            if (!RangeReadable(o + off, 4)) break;
            uint8_t* c = *(uint8_t**)(o + off);
            if (!LooksLikeObj(c)) continue;
            bool dup = false;
            for (int i = 0; i < seenN; i++) if (seen[i] == c) { dup = true; break; }
            if (dup) continue;
            if (seenN >= 224) break;
            seen[seenN++] = c;

            const char* cc = ObjClassName(c);
            if (!cc) continue;

            // skeletal components only - static world meshes were just noise
            if (strstr(cc, "Skeletal") && strstr(cc, "Component") &&
                FpFieldsLookRight(c) && g_fpCandN < 24) {
                const char* nm = RealName(*(uint32_t*)(c + kNameOff));
                FpCand* k = &g_fpCand[g_fpCandN++];
                // 30.2: carry the WHOLE calibration record across a re-collect
                // (basis0, E map, pivot, signs). Before this, a same-slot
                // re-collect only survived by the accident of static storage;
                // a reordered list silently drove one weapon with another
                // weapon's calibration.
                bool hadPrev = false;
                for (int z = 0; z < 24; z++)
                    if (g_fpPrev[z].obj == c) { *k = g_fpPrev[z]; hadPrev = true; break; }
                if (!hadPrev) memset(k, 0, sizeof(FpCand));
                k->obj = c;
                snprintf(k->name,  sizeof(k->name),  "%s", nm ? nm : "?");
                snprintf(k->cls,   sizeof(k->cls),   "%s", cc);
                snprintf(k->owner, sizeof(k->owner), "%s", oc ? oc : "?");
                const char* as = FpAssetName(c);
                snprintf(k->asset, sizeof(k->asset), "%s", as ? as : "?");
            }
            // expand only through things that can OWN a view model
            if (tail < 160 &&
                (strstr(cc, "Inventory") || strstr(cc, "Container") ||
                 strstr(cc, "Weapon")    || strstr(cc, "Item")      ||
                 strstr(cc, "Power")     || strstr(cc, "Pawn"))) {
                q[tail] = c; qd[tail] = d + 1; tail++;
            }
        }
    }

    for (int i = 0; i < g_fpCandN && g_fpSel < 0; i++)
        if (!strcmp(g_fpCand[i].name, "pPlayerMesh")) g_fpSel = i;
    if (g_fpSel < 0 && g_fpCandN) g_fpSel = 0;

    for (int i = 0; i < g_fpCandN; i++)
        if (FpIsViewModel(&g_fpCand[i]) && !g_fpCand[i].havePivot) { g_fpPivotPend = 3; break; }

    // log only when the set actually changes, or this spams once a second
    static int lastN = -1; static void* lastFirst = NULL;
    void* first = g_fpCandN ? (void*)g_fpCand[0].obj : NULL;
    if (lastN != g_fpCandN || lastFirst != first) {
        lastN = g_fpCandN; lastFirst = first;
        Log("handmesh: ==== %d view model(s) ====", g_fpCandN);
        for (int i = 0; i < g_fpCandN; i++)
            Log("handmesh:   [%d] '%s' asset=%s%s", i, g_fpCand[i].name,
                g_fpCand[i].asset,
                FpIsViewModel(&g_fpCand[i])
                    ? (FpHandFor(&g_fpCand[i]) ? "  <- RIGHT hand" : "  <- LEFT hand")
                    : "");
        // 38.23: the crouch wall - driven meshes must never block movement
        for (int i = 0; i < g_fpCandN; i++)
            FpNoBlock(g_fpCand[i].obj, g_fpCand[i].name);
    }
}


static void FpCycle()
{
    if (!g_fpCandN) { FpCollect(); return; }
    FpRestoreRotation();
    g_fpSel = (g_fpSel + 1) % g_fpCandN;
    Log("handmesh: selected [%d/%d] '%s'  (%s)", g_fpSel, g_fpCandN - 1,
        g_fpCand[g_fpSel].name, g_fpCand[g_fpSel].cls);
}


// Point one component's relative rotation down a controller's ray. Returns the
// object written so we can put it back later.
static uint8_t* FpDrive(int idx, int hand)
{
    if (idx < 0 || idx >= g_fpCandN) return NULL;
    uint8_t* mesh = g_fpCand[idx].obj;
    if (!LooksLikeObj(mesh) || !FpFieldsLookRight(mesh)) return NULL;
    FpCand* k = &g_fpCand[idx];
    float yawRel = 0.0f, pitchAbs = 0.0f, pos[3];
    if (!HandAnglesPos(hand, &yawRel, &pitchAbs, pos)) return NULL;
    float hw_y, hw_p;
    if (!FpHandWorld(hand, &hw_y, &hw_p)) return NULL;
    // Sign flips retired in 30.1: the ground-truth run proved the engine's
    // composition is globally uniform (identical delta on both weapons), so
    // there is nothing per-weapon or per-hand left for a sign to express.

    // No more sign hunting. Euler signs kept coming out unstable because I was
    // reading angles off a matrix whose local axes are permuted, and a sign is
    // the wrong abstraction for this anyway - it cannot express a parent that
    // is rolled or a mesh that is mounted sideways. Solve for the matrix.
    //
    //   world basis with no command = the parent, call it P
    //   we want the weapon's world orientation to be its rest orientation
    //   turned by however your hand has turned since calibration, Q
    //   so   R = P * Q * D^T * P^T
    // where D is how far the parent itself has drifted, measured off the
    // reference component we never write to. Then read pitch/yaw/roll straight
    // out of R. Every convention cancels; nothing is assumed.
    float Q[9];
    if (g_fpRollOn) {
        if (!g_fpHaveHandBas[hand]) return NULL;
        float Cn[9], C0T[9];
        if (!FpHandBasisWorld(hand, Cn)) return NULL;
        M3T(g_fpHandBas0[hand], C0T);
        M3Mul(C0T, Cn, Q);                   // full hand rotation, wrist roll in
    } else {
        // 30.23: roll-free BY CONSTRUCTION. The old way solved the full
        // rotation and then zeroed the output roll - which is only harmless
        // at the calibration facing. The same physical rotation needs a roll
        // component 180 degrees later, and dropping it there is what made
        // the weapons go weird after turning around. Build the hand frame
        // from the aim ray's world yaw/pitch only (the proven aiming math),
        // and let the extraction write whatever roll the FRAME needs.
        float hy2, hp2;
        if (!FpHandWorld(hand, &hy2, &hp2)) return NULL;
        float B1[9], B0[9], B0T[9];
        BasisFromYawPitch(hy2, hp2, B1);
        BasisFromYawPitch(g_fpHandNeutY[hand], g_fpHandNeutP[hand], B0);
        M3T(B0, B0T);
        M3Mul(B0T, B1, Q);                   // roll-free rotation since cal
    }

    // 30.4: the drift D used to come from pMesh, which only sees ACTOR-level
    // motion. The ground-truth head test showed the weapons' real parents are
    // ANIMATED BONES that swing 20-90 degrees with the game's arm sway while
    // pMesh moves 2 - that sway was the residual "weapons move with my head".
    // Recover the live parent from the weapon's own world matrix by removing
    // exactly what we commanded last frame. 28.0 tried this with sign-based
    // approximation and made a runaway loop; the difference now is the removal
    // is EXACT under the measured composition law (M_engine = T*M_ue3*T^T),
    // so our own signal cancels completely and what remains is pure parent
    // motion, one frame late, smoothed 50% per frame. pMesh stays as the
    // fallback when the read fails.
    float D[9] = { 1,0,0, 0,1,0, 0,0,1 };
    bool liveOk = false;
    {
        float Bnow[9];
        if (FpBasis(mesh, Bnow)) {
            float MuL[9], tA[9], TT[9], Me[9], MeT[9], Pl[9];
            GtUE3Rot(k->lastCmdP * 57.2958f, k->lastCmdY * 57.2958f,
                     k->lastCmdR * 57.2958f, MuL);
            M3Mul(kGtT, MuL, tA);
            M3T(kGtT, TT);
            M3Mul(tA, TT, Me);               // what the engine applied for us
            M3T(Me, MeT);
            M3Mul(MeT, Bnow, Pl);            // ... removed: the naked parent
            if (k->haveLive) {
                for (int q = 0; q < 9; q++)
                    k->plive[q] = 0.5f * k->plive[q] + 0.5f * Pl[q];
                M3OrthoRows(k->plive);
            } else {
                for (int q = 0; q < 9; q++) k->plive[q] = Pl[q];
                k->haveLive = true;
            }
            float b0T[9];
            M3T(k->basis0, b0T);
            M3Mul(b0T, k->plive, D);         // per-weapon TRUE drift
            liveOk = true;
        }
    }
    if (!liveOk && g_fpHaveRef) {
        float refNow[9];
        if (FpBasis(g_fpRef, refNow)) {
            float r0T[9];
            M3T(g_fpRefBas0, r0T);
            M3Mul(r0T, refNow, D);           // actor-level drift, best we have
        }
    }

    float DT[9], P0T[9], t1[9], t2[9], Rm[9];
    M3T(D, DT);
    M3T(k->basis0, P0T);
    M3Mul(k->basis0, Q, t1);
    M3Mul(t1, DT, t2);
    M3Mul(t2, P0T, Rm);

    // GROUND TRUTH (build 30.0 run, 2026-08-06): commanding the rotator moves
    // the world basis by the SAME left-delta for both weapons - the parent
    // cancels, so the solve above has the right shape - but the delta's axes
    // are a fixed permutation of what FRotationMatrix predicts:
    //       yaw   -> storage +Y   (the extraction assumed -Z)
    //       pitch -> storage -X   (assumed +Y)
    //       roll  -> storage +Z   (assumed +X)
    // One global change of basis  T: X->Z, Y->-X, Z->-Y  explains all six
    // probes on both weapons: M_engine = T * M_ue3 * T^T. So conjugate the
    // desired delta back into the rotator's own frame, M_ue3 = T^T * Rm * T,
    // and only then read pitch/yaw/roll out with the standard extraction.
    // Checked against the probe data by hand: this maps the measured yaw /
    // pitch / roll deltas back to exactly (30,0,0), (0,30,0), (0,0,30).
    float TT9[9], u1[9], Mu[9];
    M3T(kGtT, TT9);
    M3Mul(TT9, Rm, u1);
    M3Mul(u1, kGtT, Mu);

    float sinP = Mu[0*3+2];
    if (sinP >  1.0f) sinP =  1.0f;
    if (sinP < -1.0f) sinP = -1.0f;
    float pitch = asinf(sinP);
    float yaw   = atan2f(Mu[0*3+1], Mu[0*3+0]);
    float roll  = atan2f(-Mu[1*3+2], Mu[2*3+2]);
    if (yaw   >  1.75f) yaw   =  1.75f;      // hard stops stay: a bug must
    if (yaw   < -1.75f) yaw   = -1.75f;      // never become a spinning weapon
    if (pitch >  1.30f) pitch =  1.30f;
    if (pitch < -1.30f) pitch = -1.30f;
    if (roll  >  1.30f) roll  =  1.30f;
    if (roll  < -1.30f) roll  = -1.30f;
    if (yaw != yaw || pitch != pitch || roll != roll) return NULL;
    k->lastCmdY = yaw; k->lastCmdP = pitch;
    k->lastCmdR = roll;                      // what actually reaches r[2]

    int32_t yawU   = (int32_t)(yaw   * kUEPerRad);
    int32_t pitchU = (int32_t)(pitch * kUEPerRad);
    if (pitchU >  16000) pitchU =  16000;
    if (pitchU < -16000) pitchU = -16000;

    int32_t* r = (int32_t*)(mesh + kMeshRot);
    r[0] = pitchU; r[1] = yawU;
    // 30.23: ALWAYS write the extracted roll. In roll-free mode Q was built
    // without wrist roll, so this roll is purely what the frame math needs.
    r[2] = (int32_t)(roll * kUEPerRad);

    // DEPTH. Translation sits right beside Rotation in the same block the
    // engine composes from, so push/pull costs us nothing extra. It is an
    // OFFSET from where your hand was resting when you switched this on, so
    // the weapon starts exactly where the game put it and moves from there.
    // Head frame is x=right, y=up, z=forward; actor space is X=forward,
    // Y=right, Z=up.
    float t[3] = { 0.0f, 0.0f, 0.0f };

    // 30.2: the old path measured the hand's offset FROM THE HEAD, in the
    // head's own yaw frame - so every lean, turn and bob of the head read as
    // hand motion and dragged the weapon with it, and a real push forward got
    // scrambled through the frame mismatch. Measure in TRACKING (room) space
    // instead: the room does not move when your head does. Room -> game world
    // is the same pure-yaw mapping the orientation path uses (A changes only
    // on stick turns, proven clean by the 30.0 head test), and world ->
    // written units goes through the measured, per-weapon E inverse.
    if (g_fpPosOn && hand >= 0 && hand <= 1 && g_fpHaveNeutralRoom[hand] &&
        k->haveE) {
        float pr[3];
        if (HandRoomPos(hand, pr) && g_bodyAnchorOk) {
            float sc = g_fpPosScale * (float)g_fpPosSign;
            float dx = (pr[0] - g_bodyAnchor[0]) - g_fpNeutralRoom[hand][0];
            float dy = (pr[1] - g_bodyAnchor[1]) - g_fpNeutralRoom[hand][1];
            float dz = (pr[2] - g_bodyAnchor[2]) - g_fpNeutralRoom[hand][2];
            float gx = -dz, gy = dx, gz = dy;      // VR room -> game axes
            float A2 = g_viewYawRad - g_hmdYaw;    // stick turns only
            float c2 = cosf(A2), s2 = sinf(A2);
            float dW[3] = { (gx*c2 - gy*s2) * sc,
                            (gx*s2 + gy*c2) * sc,
                             gz * sc };
            // 30.3: E was measured at calibration and the parent has turned
            // by D since - and the pawn's yaw FOLLOWS THE VIEW (view==pawn in
            // every log line ever taken), so it turns when your head does.
            // The live response is E*D; its inverse applies D^T first.
            // Without this the offset rotated away from the hand by exactly
            // your head yaw - the 30.2 residual drag.
            float dWp[3];
            for (int q = 0; q < 3; q++)
                dWp[q] = dW[0]*D[q*3+0] + dW[1]*D[q*3+1] + dW[2]*D[q*3+2];
            for (int q = 0; q < 3; q++)
                t[q] = dWp[0]*k->einv[0*3+q] + dWp[1]*k->einv[1*3+q] +
                       dWp[2]*k->einv[2*3+q];
        }
    }

    // rotate about the weapon's own centre. The pivot was measured in world
    // space, Rm is a world-side delta, but the Translation field is written
    // in the ENGINE'S OWN frame - three frames the old code treated as one.
    // The error scaled with how far the pivot sits from the mesh origin,
    // which is why the sword (pivot 38 uu out) always wobbled worse than the
    // crossbow (23 uu). Exact version: world correction c^T (I - Rm) B_rest,
    // then world -> written units through the measured E inverse.
    float pmix = (hand >= 0 && hand <= 1) ? g_fpPivotMix2[hand] : 1.0f;
    if (k->havePivot && pmix > 0.001f && k->haveE) {
        float Bn[9];
        M3Mul(k->basis0, D, Bn);               // rest basis with drift applied
        const float* c = k->pivot;
        float ci[3], dpW[3];
        for (int j = 0; j < 3; j++)
            ci[j] = c[j] - (c[0]*Rm[0*3+j] + c[1]*Rm[1*3+j] + c[2]*Rm[2*3+j]);
        for (int q = 0; q < 3; q++)
            dpW[q] = ci[0]*Bn[0*3+q] + ci[1]*Bn[1*3+q] + ci[2]*Bn[2*3+q];
        float dpp[3];                          // same D^T as the position path
        for (int q = 0; q < 3; q++)
            dpp[q] = dpW[0]*D[q*3+0] + dpW[1]*D[q*3+1] + dpW[2]*D[q*3+2];
        for (int q = 0; q < 3; q++)
            t[q] += (dpp[0]*k->einv[0*3+q] + dpp[1]*k->einv[1*3+q] +
                     dpp[2]*k->einv[2*3+q]) * pmix;
    }

    if (hand >= 0 && hand <= 1)
        for (int q = 0; q < 3; q++) t[q] += g_fpBias[hand][q];

    for (int q = 0; q < 3; q++) {
        if (t[q] != t[q]) t[q] = 0.0f;
        if (t[q] >  g_fpPosMax * 4.0f) t[q] =  g_fpPosMax * 4.0f;
        if (t[q] < -g_fpPosMax * 4.0f) t[q] = -g_fpPosMax * 4.0f;
    }
    float* T = (float*)(mesh + kMeshTrans);
    T[0] = t[0]; T[1] = t[1]; T[2] = t[2];
    return mesh;
}


// Read-only: find the TArrays inside a component. USkeletalMeshComponent keeps
// per-section visibility in HiddenMaterials (TArray<UBOOL>, one entry per
// material slot) and per-bone visibility in BoneVisibilityStates
// (TArray<BYTE>, 2=visible). Those are the two levers that can hide a forearm
// while keeping the hand, so find them before writing anything.
static void FpArrayDump()
{
    if (g_fpSel < 0 || g_fpSel >= g_fpCandN) { Log("arrays: nothing selected"); return; }
    uint8_t* m = g_fpCand[g_fpSel].obj;
    if (!LooksLikeObj(m)) { Log("arrays: selection is stale"); return; }
    Log("arrays: ==== [%d] '%s' (%s) @ %p ====", g_fpSel, g_fpCand[g_fpSel].name,
        g_fpCand[g_fpSel].cls, (void*)m);
    int found = 0;
    for (uint32_t o = 0x100; o + 12 <= 0x800 && found < 40; o += 4) {
        if (!RangeReadable(m + o, 12)) break;
        uint8_t* d  = *(uint8_t**)(m + o);
        int32_t num = *(int32_t*)(m + o + 4);
        int32_t max = *(int32_t*)(m + o + 8);
        if (num <= 0 || num > 512 || max < num || max > 1024) continue;
        if (!d || ((uintptr_t)d & 3) || !RangeReadable(d, (size_t)num)) continue;
        char pre[96]; pre[0] = 0;
        int nb = num < 12 ? num : 12;
        for (int i = 0; i < nb; i++) {
            char one[8];
            snprintf(one, sizeof(one), "%02x ", (unsigned)d[i]);
            strcat(pre, one);
        }
        Log("arrays:  +0x%03x data=%p num=%d max=%d  bytes: %s",
            (unsigned)o, (void*)d, (int)num, (int)max, pre);
        found++;
    }
    Log("arrays: ==== %d candidate array(s) ====", found);
}

// where the component actually sits in the world, read back from the engine
static bool FpWorldPos(uint8_t* o, float* p)
{
    if (!LooksLikeObj(o) || !RangeReadable(o + 0x90, 12)) return false;
    const float* w = (const float*)(o + 0x90);
    p[0] = w[0]; p[1] = w[1]; p[2] = w[2];
    return true;
}

// UE3's FRotationMatrix, row-vector, exactly the convention FpDrive's Euler
// extraction assumes - so a zero error here means that extraction is valid too
static void GtUE3Rot(float pitchDeg, float yawDeg, float rollDeg, float* M)
{
    float P = pitchDeg / 57.2958f, Y = yawDeg / 57.2958f, R = rollDeg / 57.2958f;
    float SP = sinf(P), CP = cosf(P), SY = sinf(Y);
    float CY = cosf(Y), SR = sinf(R), CR = cosf(R);
    M[0] = CP*CY;               M[1] = CP*SY;               M[2] = SP;
    M[3] = SR*SP*CY - CR*SY;    M[4] = SR*SP*SY + CR*CY;    M[5] = -SR*CP;
    M[6] = -(CR*SP*CY + SR*SY); M[7] = CY*SR - CR*SP*SY;    M[8] = CR*CP;
}
