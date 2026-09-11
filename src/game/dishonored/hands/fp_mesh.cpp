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


// VR-73 DISCOVERY IS NOT OWNERSHIP. The collector finds skeletal components by
// walking pointers out from the pawn, and a pawn standing on a mesh points at
// that mesh: the intro boat's 'SkeletalMeshComponent0' (EmpressBoat_anim) was
// collected mid-ride in every failing run, had its BlockActors cleared, and the
// player and the NPCs on it dropped through when the lift actor was destroyed.
// Every transform write therefore goes through here: only a component the
// ENGINE says belongs to the player may be written, and its values are saved
// the first time so a restore puts back what was there, not zeros.
static bool FpMayWrite(FpCand* k)
{
    if (!k || !k->owned || !k->obj) return false;
    if (!k->xfWrote) {
        const int32_t* r = (const int32_t*)(k->obj + kMeshRot);
        const float*   T = (const float*)(k->obj + kMeshTrans);
        for (int q = 0; q < 3; q++) { k->xfRot0[q] = r[q]; k->xfTrans0[q] = T[q]; }
        k->xfWrote = true;
    }
    return true;
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
        if (!FpMayWrite(&g_fpCand[i])) continue;   // VR-73: owned components only
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
    // VR-73: this used to zero the relative rotation AND translation of EVERY
    // candidate on every collect - including components the mod never wrote and
    // did not own (the intro boat's mesh among them). Only what FpMayWrite
    // recorded is put back, and it goes back to the value it had, not to zero.
    static int said = 0;
    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        k->lastCmdY = k->lastCmdP = k->lastCmdR = 0.0f;
        if (!k->xfWrote) continue;
        k->xfWrote = false;
        uint8_t* o = k->obj;
        if (!o || !LooksLikeObj(o) || !FpFieldsLookRight(o)) continue;
        int32_t* r = (int32_t*)(o + kMeshRot);
        float* T = (float*)(o + kMeshTrans);
        for (int q = 0; q < 3; q++) { r[q] = k->xfRot0[q]; T[q] = k->xfTrans0[q]; }
        if (said < 16) {
            ++said;
            Log("handmesh: restored '%s' asset=%s to its own transform - rot (%d %d %d) trans "
                "(%.1f %.1f %.1f)", k->name, k->asset, (int)k->xfRot0[0], (int)k->xfRot0[1],
                (int)k->xfRot0[2], k->xfTrans0[0], k->xfTrans0[1], k->xfTrans0[2]);
        }
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


// Force the candidate list to be rebuilt. `if (!g_fpCandN) FpCollect()` is the
// established rescan trigger, and skelcontrol already uses exactly this when it
// finds its selected mesh stale.
//
// WHY A LOAD MUST DO THIS. The list holds raw component pointers. A level load
// destroys those components, but the memory usually stays mapped, so
// LooksLikeObj still passes on a dead pointer and the list looks healthy while
// every transform read off it fails. The measured signature is the body mesh
// still listed by name with an UNREADABLE transform, and then a snapshot where
// its asset name will not even resolve:
//
//   wa/comp: [0] 'Skm_Player' (pMesh) REF xform UNREADABLE
//   wa/comp: [0] '?' (pMesh)          xform UNREADABLE
//   wa: NOTHING TO ATTACH - 0 usable as the bridge anchor (the body mesh)
//
// Without the bridge anchor nothing can attach at all, which is why the weapons
// stayed in their default positions for a whole session after a second load.
static void FpInvalidateCandidates(const char* why)
{
    // THE BOOKKEEPING RESET MUST NOT DEPEND ON THE OLD LIST BEING NON-EMPTY.
    // The early return used to come first, so a level change arriving with an
    // already-empty list left the anchor budget, the attempt count and the
    // settle window carrying the previous level's numbers - and a level that had
    // needed a few tries could push the next one straight into the "accept a
    // list with no anchor" branch, which attaches nothing and says so once.
    g_fpNoAnchorTries = 0;
    g_fpEnsureTries = 0;
    g_fpEnsureMs = 0.0;
    g_fpSettleLeft = 0;
    g_fpSettleMs = 0.0;
    g_fpEquipRevSeen = 0;          // no published list is for any revision now
    FpMarkDirty(why);
    if (!g_fpCandN) return;
    Log("handmesh: dropping %d candidate(s) - %s. They are raw component "
        "pointers and a load destroys what they point at; the memory usually "
        "stays mapped, so a dead list looks healthy while every transform read "
        "off it fails. The next tick rebuilds it.", g_fpCandN, why);
    g_fpCandN = 0; g_fpSel = -1;
    g_fpWritten = NULL; g_fpWritten2 = NULL;
}


// THE OWNER OF THE REBUILD, for both states the list can be wrong in.
//
// EMPTY is the load case: a level change drops the list and nothing else in the
// tree rebuilds it, because every other FpCollect call site is a one-shot that
// has already fired. STALE is the swap case: the list is full and describes the
// PREVIOUS equipment, which is how the crossbow's loaded bolt disappeared and
// never came back.
//
// Marking is separated from collecting on purpose. The change is detected on the
// script tick beside the equipment read; the collect happens here, on the same
// lane, before the component snapshot is published - so a rebuilt list is never
// half-visible to the snapshot, and rapid swaps coalesce into one collect at the
// latest revision instead of one collect each.
static void FpMarkDirty(const char* why)
{
    if (g_fpDirty && !strcmp(g_fpDirtyWhy, why)) return;
    g_fpDirty = true;
    _snprintf(g_fpDirtyWhy, sizeof(g_fpDirtyWhy), "%s", why ? why : "?");
    g_fpDirtyWhy[sizeof(g_fpDirtyWhy) - 1] = 0;
}


static void FpEnsureCandidates(const char* why)
{
    if (!g_fpAutoRecollect) return;

    // WHAT THE PUBLISHED LIST IS FOR. Captured before the collect so a swap that
    // lands mid-collect leaves the list marked dirty rather than being recorded
    // as satisfied by a walk that started before it.
    const uint32_t rev = g_rflEquipRev;

    if (!g_fpCandN) FpMarkDirty(why ? why : "the candidate list is empty");
    else if (g_rflEquipSigOk && rev != g_fpEquipRevSeen) {
        FpMarkDirty("the equipment changed - the list describes the previous one");
        // A REVISION CHANGE BUYS A SETTLE WINDOW. The equipment event and the
        // creation of the new weapon's child components are not promised to
        // land in the same tick, so one collect can legitimately win the race
        // and return a list with no loaded bolt in it. These extra passes are
        // what pick a late child up; without them the first collect would
        // declare success and the bolt would be missing until the next swap.
        if (g_fpSettleLeft <= 0) {
            g_fpSettleLeft = g_fpSettleTries;
            // A NEW REVISION IS A FRESH RECOVERY OPPORTUNITY. The anchor budget
            // inside FpCollect latches after 120 failures into accepting a list
            // with no anchor, which attaches nothing. A genuine equipment change
            // is new information and must not inherit the previous one's
            // exhaustion.
            g_fpNoAnchorTries = 0;
            g_fpEnsureTries = 0;
            InterlockedIncrement(&g_fpSwapRefresh);
            Log("handmesh: equipment revision %u -> %u, refreshing the candidate "
                "list and holding %d settle pass(es) open. A swap does not empty "
                "the list, it makes it describe the previous weapon, and the "
                "child components of the new one may not exist yet.",
                g_fpEquipRevSeen, rev, g_fpSettleTries);
        }
        g_fpSettleMs = 0.0;   // the first settle pass is due immediately
    }

    const double t = MaimNowMs();
    const bool settling = g_fpSettleLeft > 0 && t >= g_fpSettleMs;
    if (!g_fpDirty && !settling) return;
    if (g_fpDirty && t < g_fpEnsureMs) return;
    g_fpEnsureMs = t + 500.0;

    uint8_t* pawn = FpPawn();
    if (!pawn) return;            // no pawn, no rig; the mark stays for later
    ++g_fpEnsureTries;

    const uint32_t candRevBefore = g_fpCandRev;
    FpCollect();

    // SUCCESS IS NOT "THE FUNCTION RAN", AND IT IS NOT "THE COUNT IS NONZERO".
    // The bridge anchor is the one component that must exist for anything to
    // attach at all, and FpCollect already discards a list without it. So a
    // non-empty list here means the anchor is present; anything less left the
    // mark standing and will be retried.
    const bool ok = g_fpCandN > 0;
    if (settling) {
        --g_fpSettleLeft;
        g_fpSettleMs = t + g_fpSettleGapMs;
        if (candRevBefore != g_fpCandRev)
            Log("handmesh: settle pass for equipment revision %u CHANGED the set "
                "(candidate revision %u -> %u) - a component appeared after the "
                "swap, which is the case a single rebuild would have missed. "
                "%d pass(es) left.", rev, candRevBefore, g_fpCandRev,
                g_fpSettleLeft);
        else if (g_fpSettleLeft == 0)
            Log("handmesh: settle window for equipment revision %u closed with "
                "no further change - the list stabilised at candidate revision "
                "%u, %d component(s).", rev, g_fpCandRev, g_fpCandN);
    }

    if (!ok) {
        if (g_fpEnsureTries == 1 || (g_fpEnsureTries % 40) == 0)
            Log("handmesh: rebuild attempt %d produced no usable list (%s). The "
                "collector ran and returned nothing usable - that is a rig that "
                "is not up yet, not a missing trigger.", g_fpEnsureTries,
                g_fpDirtyWhy);
        return;                   // stays dirty on purpose
    }

    // A SWAP THAT LANDED DURING THE WALK IS NOT SATISFIED BY IT.
    if (g_rflEquipRev != rev) {
        Log("handmesh: the equipment changed again during the collect (%u -> "
            "%u) - the list is still marked stale so the next tick collects for "
            "the newer revision. Coalescing here is what stops a burst of swaps "
            "becoming a burst of walks.", rev, g_rflEquipRev);
        return;
    }

    const bool wasDirty = g_fpDirty;
    g_fpDirty = false;
    const uint32_t prevSeen = g_fpEquipRevSeen;
    g_fpEquipRevSeen = rev;
    if (wasDirty) {
        Log("handmesh: candidate list refreshed - %d component(s), candidate "
            "revision %u, equipment revision %u -> %u, after %d attempt(s). "
            "Reason: %s.", g_fpCandN, g_fpCandRev, prevSeen, rev,
            g_fpEnsureTries, g_fpDirtyWhy);
        // The contracts that belonged to what is no longer here.
        WaRetireContractsNotIn("the candidate list was refreshed");
    }
    g_fpEnsureTries = 0;
}


// VR-73: who the ENGINE says a component belongs to. ActorComponent.Owner names
// the actor; Actor.Owner is followed up to four hops, because a weapon's view
// model belongs to an inventory item that belongs to the pawn. Owned means that
// chain reaches the possessed pawn or one of the two held items. The offsets are
// resolved by name once; if either cannot be found every candidate reads as
// foreign, so the mod writes nothing - fail soft, and the log says why.
static bool FpOwnedByPlayer(uint8_t* comp, uint8_t* pawn, char* who, size_t whoN)
{
    static int state = 0;              // 0 unresolved, 1 resolved, -1 not found
    static uint32_t compOwner = 0, actorOwner = 0;
    if (!state) {
        compOwner  = FindPropOffset("ActorComponent", "Owner");
        actorOwner = FindPropOffset("Actor", "Owner");
        state = (compOwner && actorOwner) ? 1 : -1;
        Log("handmesh/owner: ActorComponent.Owner %s (+0x%x), Actor.Owner %s (+0x%x)%s",
            compOwner ? "found" : "NOT FOUND", compOwner, actorOwner ? "found" : "NOT FOUND",
            actorOwner, state == 1 ? " - candidate writes are limited to components the player owns"
                                   : " - EVERY candidate reads as foreign, so no collision or "
                                     "transform write will happen (fail soft)");
    }
    _snprintf(who, whoN, "%s", state == 1 ? "none" : "unresolved");
    who[whoN - 1] = 0;
    if (state != 1 || !comp || !RangeReadable(comp + compOwner, 4)) return false;
    uint8_t* a = *(uint8_t**)(comp + compOwner);
    if (!LooksLikeObj(a)) return false;
    const char* first = ObjClassName(a);
    _snprintf(who, whoN, "%s", first ? first : "?");
    who[whoN - 1] = 0;
    for (int hop = 0; hop < 4 && a && LooksLikeObj(a); ++hop) {
        if (a == pawn || a == g_rflHeldObj[1] || a == g_rflHeldObj[2]) return true;
        if (!RangeReadable(a + actorOwner, 4)) break;
        uint8_t* up = *(uint8_t**)(a + actorOwner);
        if (up == a) break;
        a = up;
    }
    return false;
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

    // THE EQUIPPED ITEMS ARE ROOTS TOO. The pawn walk reaches an item only when
    // a direct pointer chain happens to lead there, which is why the pistol has
    // never appeared in a snapshot and why the crossbow's loaded bolt comes and
    // goes with whatever the walk found that pass. VR-61 reads the held item per
    // hand off the engine, so its CHILDREN are reachable by construction rather
    // than by luck - and the bolt is a child of the crossbow, not of the pawn.
    //
    // The roots are validated objects from the inventory read, and the walk from
    // them is the same bounded one, with the same depth limit and the same
    // skeletal-component test. Nothing unrelated can enter: an object still has
    // to be a skeletal component with a writable transform to become a candidate.
    if (g_fpEquipRoots) {
        for (int u = 1; u <= 2; ++u) {
            uint8_t* item = g_rflHeldObj[u];
            if (!item || !LooksLikeObj(item)) continue;
            bool dup = false;
            for (int i = 0; i < seenN; i++) if (seen[i] == item) { dup = true; break; }
            if (dup || seenN >= 224 || tail >= 160) continue;
            seen[seenN++] = item;
            q[tail] = item; qd[tail] = 1; tail++;   // depth 1: its own children count
        }
    }

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
                k->owned = FpOwnedByPlayer(c, pawn, k->ownedBy, sizeof(k->ownedBy));
                k->xfWrote = false;   // the collect's own restore already ran
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

    // A COLLECT WITHOUT THE BODY MESH IS NOT A RESULT, IT IS A RETRY.
    //
    // The body mesh is the bridge anchor: without it nothing can attach at all.
    // Collecting during a load catches the rig half-built and returns one or two
    // components with no anchor, and because the rebuild trigger is
    // `if (!g_fpCandN) FpCollect()`, a NON-EMPTY partial list is never refreshed
    // - it sticks for the rest of the session. Measured: 2 components, 0 usable
    // as the bridge anchor, repeating every 4 s while the weapons sat in their
    // default positions.
    //
    // So a list with no anchor is discarded and the next tick tries again. The
    // attempt count is bounded: if the anchor genuinely never appears, the list
    // is accepted as-is and the log says so, because looping forever would be a
    // worse failure than a partial list and would hide itself.
    {
        int& noAnchorTries = g_fpNoAnchorTries;   // resettable across a load
        bool anchor = false;
        for (int i = 0; i < g_fpCandN && !anchor; i++)
            if (strstr(g_fpCand[i].asset, "Skm_Player")) anchor = true;
        if (g_fpCandN && !anchor && noAnchorTries < 120) {
            ++noAnchorTries;
            if (noAnchorTries == 1 || (noAnchorTries % 30) == 0)
                Log("handmesh: collected %d component(s) but NO body mesh - the "
                    "rig is still building, so this is a retry and not a result. "
                    "Without the bridge anchor nothing can attach, and a partial "
                    "list would stick because the rebuild only fires on an EMPTY "
                    "one. Attempt %d of 120.", g_fpCandN, noAnchorTries);
            g_fpCandN = 0; g_fpSel = -1;
            return;
        }
        if (anchor) noAnchorTries = 0;
        else if (g_fpCandN && noAnchorTries >= 120)
            Log("handmesh: ACCEPTING a %d component list with no body mesh after "
                "120 attempts. Nothing will attach with no bridge anchor, and "
                "this line is the reason - retrying forever would hide it.",
                g_fpCandN);
    }

    // LOG WHEN THE SET CHANGES, AND COMPARE THE WHOLE SET.
    //
    // The old test was the count and the first entry. That is exactly blind to
    // the fault this code exists to fix: the bolt was REPLACED by the pistol's
    // mesh in the middle of a six-entry list, so the count and the first entry
    // were identical and nothing was logged while the list went wrong.
    static uint8_t* lastSet[24]; static int lastSetN = -1;
    bool changed = (lastSetN != g_fpCandN);
    for (int i = 0; !changed && i < g_fpCandN; i++)
        if (lastSet[i] != g_fpCand[i].obj) changed = true;
    if (changed) {
        lastSetN = g_fpCandN;
        for (int i = 0; i < g_fpCandN && i < 24; i++) lastSet[i] = g_fpCand[i].obj;
        ++g_fpCandRev;
        Log("handmesh: ==== %d view model(s) ==== (candidate revision %u, for "
            "equipment revision %u)", g_fpCandN, g_fpCandRev, g_rflEquipRev);
        for (int i = 0; i < g_fpCandN; i++)
            Log("handmesh:   [%d] '%s' asset=%s%s | owner %s%s", i, g_fpCand[i].name,
                g_fpCand[i].asset,
                FpIsViewModel(&g_fpCand[i])
                    ? (FpHandFor(&g_fpCand[i]) ? "  <- RIGHT hand" : "  <- LEFT hand")
                    : "",
                g_fpCand[i].ownedBy,
                g_fpCand[i].owned ? " (the player's)" : " - FOREIGN, never written");
        // 38.23: the crouch wall - driven meshes must never block movement.
        // VR-73: and ONLY the player's. A foreign component keeps its collision.
        static int foreignSaid = 0;
        for (int i = 0; i < g_fpCandN; i++) {
            if (g_fpCand[i].owned) {
                FpNoBlock(g_fpCand[i].obj, g_fpCand[i].name);
            } else if (foreignSaid < 40) {
                ++foreignSaid;
                Log("collision: LEFT ALONE on '%s' asset=%s - its owner is %s, not the player. "
                    "The walk discovered it; discovery is not ownership (VR-73)%s",
                    g_fpCand[i].name, g_fpCand[i].asset, g_fpCand[i].ownedBy,
                    foreignSaid == 40 ? ". Later ones are not printed." : "");
            }
        }
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

    if (!FpMayWrite(k)) return NULL;         // VR-73: owned components only
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
