// game/dishonored/arm_follow.cpp - VR-30, the arm-follow probe (read-only).
//
// See mod/state/54_game_dishonored_arm_follow.inc for why this exists and
// what the fields are. This file only READS. It resolves the arm-follow
// fields on the live DishonoredPlayerCamera once, then reports what they do
// while the player plays.
//
// Lane: the script lane. Called from PeHandler's camera pass, right after
// FovLeverApply, which is where g_camObj is already proven live. It is NOT
// gated by [Mode] GamepadOnly - the whole hands subsystem is dead on a
// shipped build (skelcontrol.cpp's `if (!g_handMesh) return`), and a probe
// that only runs on a configuration nobody ships is a probe nobody runs.
//
// It logs on its OWN - on change, plus a periodic summary - rather than
// waiting to be asked through the command seam, because the tester's harness
// scripts do not work on their machine and the log is the whole instrument.

// Resolve every offset once. Returns true when the camera-side fields are
// usable; the influence side is optional and reported separately.
static void ArmFollowResolve()
{
    if (g_afTried) return;
    // GNames is empty until the exe's static initializers have run, so this
    // is retried rather than failed permanently (config.cpp's own rule).
    const uint32_t now = (uint32_t)GetTickCount64();
    if (now < g_afNextTryMs) return;
    g_afNextTryMs = now + 3000;

    if (!RangeReadable((void*)kGNamesData, 8)) return;
    const char* n0 = NameFromIndex(0);
    if (!n0 || strcmp(n0, "None") != 0) return;          // GNames sane?

    g_afTried = true;

    g_afVtOff = FindPropOffset("DishonoredPlayerCamera", "m_DishonoredVTSettings");
    for (int i = 0; i < kAfFieldCount; i++) {
        // The struct is declared in DishonoredCameraInfluenceGroup, not on the
        // camera; FindPropOffset matches on the OUTER's name, which for a
        // struct member is the ScriptStruct itself.
        g_afFieldOff[i]   = FindPropOffset("DishonoredVTSettings", kAfFieldNames[i]);
        g_afFieldFound[i] = (g_afFieldOff[i] != 0) || (i == kAfWeight && g_afVtOff != 0);
    }
    for (int i = 0; i < kAfInfCount; i++) {
        g_afInfPtrOff[i] = FindPropOffset("DishonoredPlayerCamera", kAfInfNames[i]);
        g_afInfFound[i]  = g_afInfPtrOff[i] != 0;
    }
    // VR-30: the Rotator offsets - the yaw lane. Same struct, same lookup.
    g_afRotOffPrim = FindPropOffset("DishonoredVTSettings", "m_ArmFollowOffset_Rot_Primary");
    g_afRotOffSec  = FindPropOffset("DishonoredVTSettings", "m_ArmFollowOffset_Rot_Secondary");
    // VR-30: the viewmodel's own lens (the yaw counter has to be scaled by it)
    g_afFovOff = FindPropOffset("DishonoredPlayerSkeletalComponent", "m_FOV");
    FindBoolProp("DishonoredPlayerSkeletalComponent", "m_bUseFOV", &g_afUseFovOff, &g_afUseFovMask);
    g_afActorRotOff = FindPropOffset("Actor", "Rotation");   // VR-30: the body-yaw hold
    g_afInfWeightOff = FindPropOffset("DishonoredCameraInfluence", "m_Weight");
    g_afInfTargetOff = FindPropOffset("DishonoredCameraInfluence", "m_TargetWeight");
    g_afInfSpeedOff  = FindPropOffset("DishonoredCameraInfluence", "m_TransitionSpeed");
    FindBoolProp("DishonoredCameraInfluence", "m_bActive", &g_afInfActiveOff, &g_afInfActiveMask);
    g_afInfMembersOk = (g_afInfWeightOff != 0 && g_afInfTargetOff != 0);

    g_afReady = (g_afVtOff != 0);

    // Every refused lookup says so with its own name. A 0 here is the
    // difference between "the game does not have this" and "we looked wrong",
    // and the next reader needs to be able to tell those apart.
    DVR_INFO("armfollow: resolve - m_DishonoredVTSettings %s0x%x on DishonoredPlayerCamera",
             g_afVtOff ? "+" : "NOT FOUND (", g_afVtOff);
    for (int i = 0; i < kAfFieldCount; i++)
        DVR_INFO("armfollow:   %-34s %s+0x%x (within the struct)", kAfFieldNames[i],
                 g_afFieldOff[i] ? "" : "NOT FOUND ", g_afFieldOff[i]);
    for (int i = 0; i < kAfInfCount; i++)
        DVR_INFO("armfollow:   %-40s %s+0x%x", kAfInfNames[i],
                 g_afInfFound[i] ? "" : "NOT FOUND ", g_afInfPtrOff[i]);
    DVR_INFO("armfollow:   m_ArmFollowOffset_Rot_Primary %s+0x%x  _Secondary %s+0x%x  (Rotators - the YAW lane)",
             g_afRotOffPrim ? "" : "NOT FOUND ", g_afRotOffPrim, g_afRotOffSec ? "" : "NOT FOUND ", g_afRotOffSec);
    DVR_INFO("armfollow:   DishonoredCameraInfluence: m_Weight+0x%x m_TargetWeight+0x%x "
             "m_TransitionSpeed+0x%x m_bActive+0x%x mask 0x%x%s",
             g_afInfWeightOff, g_afInfTargetOff, g_afInfSpeedOff, g_afInfActiveOff, g_afInfActiveMask,
             g_afInfMembersOk ? "" : "  <-- INCOMPLETE, the influence side cannot be read");

    if (!g_afReady)
        DVR_WARN("armfollow: the camera has no m_DishonoredVTSettings we can find, so NOTHING below is "
                 "measurable. Either the class name is wrong or this build differs from the decompiled "
                 "corpus. The arm-follow lane is not disproved by this - it is unmeasured.");
}

// VR-30: the viewmodel's own lens, and the ratio the counter-rotation needs.
// Found by class name on GObjects, never by index (31.4: iteration order is not
// stable across probes). Re-validated the same way the look-at node is.
static void ArmFovTick()
{
    // THE LAG SPIKES WERE THIS. The search below is a full GObjects walk - up
    // to 4M objects with a RangeReadable each - and when it found nothing it
    // ran again every 5 s, which is a visible hitch every 5 s forever. Two
    // fixes: it only runs when the lever that needs it is actually on, and the
    // walk is now INCREMENTAL, a bounded slice per call with a cursor, so a
    // failed search costs a slice rather than the whole table.
    if (g_afCounterYaw < 0.0f) return;
    if (!g_afFovOff && !g_afUseFovOff) return;               // nothing resolved to read
    const uint32_t now = (uint32_t)GetTickCount64();
    // liveness: slot identity plus the class pointer, as SkcAlive does
    if (g_afVmComp) {
        void**   objs = *(void***)kGObjHdr;
        uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
        if (!objs || g_afVmIdx >= num || (uint8_t*)objs[g_afVmIdx] != g_afVmComp ||
            *(void**)(g_afVmComp + kClassOff) != g_afVmCls)
            g_afVmComp = NULL;
    }
    if (!g_afVmComp) {
        if (!RangeReadable((void*)kGObjHdr, 12)) return;
        void**   objs = *(void***)kGObjHdr;
        uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
        if (!objs || onum < 1000 || onum > 4000000) return;
        // A bounded slice per call. 4096 objects is microseconds; the cursor
        // wraps and tries again rather than stalling the frame for a whole
        // sweep. Never GObjects order as identity - the cursor is only where
        // to resume looking (31.4).
        if (g_afVmScan < 1 || g_afVmScan >= onum) g_afVmScan = 1;
        const uint32_t end = (g_afVmScan + 4096 < onum) ? g_afVmScan + 4096 : onum;
        for (uint32_t i = g_afVmScan; i < end; i++) {
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x200)) continue;
            const uint32_t nnum = *(uint32_t*)(o + kNameOff + 4);
            if (nnum == 0) continue;                          // want an instance, not the CDO
            const char* cn = ObjClassName(o);
            if (!cn || !strstr(cn, "PlayerSkeletalComponent")) continue;
            g_afVmComp = o; g_afVmIdx = i; g_afVmCls = *(void**)(o + kClassOff);
            break;
        }
        g_afVmScan = (end >= onum) ? 1 : end;
        if (!g_afVmComp) return;
    }
    // read the lens
    bool useFov = false;
    if (g_afUseFovOff && g_afUseFovMask && RangeReadable(g_afVmComp + g_afUseFovOff, 4))
        useFov = (*(uint32_t*)(g_afVmComp + g_afUseFovOff) & g_afUseFovMask) != 0;
    float armsFov = 0.0f;
    if (g_afFovOff && RangeReadable(g_afVmComp + g_afFovOff, 4)) armsFov = *(float*)(g_afVmComp + g_afFovOff);
    if (armsFov < 5.0f || armsFov > 175.0f) armsFov = 0.0f;   // not a field of view
    g_afArmsFov = armsFov;

    const float worldFov = dvr::camera::rendered_fov_deg() > 5.0f ? dvr::camera::rendered_fov_deg()
                                                                  : dvr::camera::fov_deg();
    float ratio = 1.0f;
    if (useFov && armsFov > 0.0f && worldFov > 5.0f) {
        ratio = armsFov / worldFov;
        if (ratio < 0.25f) ratio = 0.25f;                     // refuse an absurd correction
        if (ratio > 4.0f)  ratio = 4.0f;
    }
    g_afFovRatio = ratio;
    if (!g_afSaidFov) {
        g_afSaidFov = true;
        DVR_INFO("armfollow: viewmodel lens - m_bUseFOV=%d m_FOV=%.1f, world %.1f deg -> counter scaled by %.3f. "
                 "A narrower arms lens is more pixels per degree, so the raw head angle over-corrects; this is the "
                 "scale that nulls it. m_bUseFOV=0 or an unreadable FOV leaves the ratio at 1.000, which is what "
                 "shipped.", useFov ? 1 : 0, armsFov, worldFov, ratio);
    }
}

// Is the cached look-at node still the object we found? Slot identity plus the
// class pointer, the same pair SkcAlive checks - a recycled slot fails both.
static inline bool ArmLookAlive()
{
    uint8_t* o = g_afLookObj;
    if (!o || ((uintptr_t)o & 3) || !g_afLookIdx) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || g_afLookIdx >= num) return false;
    if ((uint8_t*)objs[g_afLookIdx] != o) return false;
    return *(void**)(o + kClassOff) == g_afLookCls;
}

// ---- the FaceRotation intercept -------------------------------------------
// Called from the stub BEFORE the engine's own FaceRotation body runs, with a
// pointer to the caller's argument block. Editing rot[1] in place is what the
// engine then faces to - we change the REQUEST, not the result, so the engine
// keeps doing its own rotation bookkeeping and dependent updates instead of
// having a field assignment fought out from under it every tick.
extern "C" void __cdecl FaceRotationHandler(void* self, int32_t* rot)
{
    ++g_frSeen;
    if (g_frWant < 0.0f || !rot) return;                 // lever off
    if (!self || (uint8_t*)self != g_yawPawn) return;    // not our pawn: untouched
    ++g_frOurs;
    if (!g_yawValid) { ++g_frStale; return; }            // no fresh target: pass through
    if (!RangeReadable(rot, 12)) return;
    const int32_t asked = rot[1];
    const int32_t body  = g_yawBodyTarget;
    // The blend is on the DIFFERENCE so 0 reproduces stock exactly and 1 is the
    // fully separated heading; nothing here integrates anything.
    const int32_t want = (g_frWant >= 0.999f)
        ? body
        : (int32_t)((uint32_t)asked - (uint32_t)(int32_t)(
              (float)(int32_t)((uint32_t)asked - (uint32_t)body) * g_frWant));
    rot[1] = want;
    ++g_frReplaced;
    if (!g_frSaid) {
        g_frSaid = true;
        DVR_INFO("armfollow/facing: INTERCEPTING FaceRotation on the possessed pawn. It was asked to "
                 "face %+.1f deg (the view); it is now asked to face %+.1f (the separated body "
                 "target). Pitch, roll and delta time are untouched, every other actor is untouched, "
                 "and the engine's own function still runs - we change the REQUEST, not the result. "
                 "If the arms stop following head yaw while the stick still turns you, this was the "
                 "operation. `arms facing off` is the live A/B.",
                 (float)asked * (360.0f / 65536.0f), (float)want * (360.0f / 65536.0f));
    }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                     "armfollow/facing: asked %+.1f -> faced %+.1f (body target %+.1f) | seen %u "
                     "ours %u replaced %u stale %u",
                     (float)asked * (360.0f / 65536.0f), (float)want * (360.0f / 65536.0f),
                     (float)body * (360.0f / 65536.0f), g_frSeen, g_frOurs, g_frReplaced, g_frStale);
}

// The stub: save flags and registers, hand the handler `this` and a pointer to
// the caller's argument block, restore, replay the displaced prologue, jump
// back. ecx still holds `this` at the push - pushfd/pushad do not touch it.
// At entry esp -> return address, so &Pitch is esp+4; after pushfd (4) and
// pushad (32) that is esp+0x28.
static bool InstallFaceRotationHook()
{
    if (g_frDet.on) return true;
    g_frStub = (uint8_t*)VirtualAlloc(NULL, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_frStub) { DVR_WARN("armfollow/facing: stub alloc failed - hook not installed"); return false; }
    uint8_t* p = g_frStub;
    *p++ = 0x9C;                                              // pushfd
    *p++ = 0x60;                                              // pushad
    *p++ = 0x8D; *p++ = 0x44; *p++ = 0x24; *p++ = 0x28;       // lea eax,[esp+0x28]  (&Pitch)
    *p++ = 0x50;                                              // push eax
    *p++ = 0x51;                                              // push ecx            (this)
    *p++ = 0xE8;
    *(int32_t*)p = (int32_t)((uintptr_t)&FaceRotationHandler - (uintptr_t)(p + 4)); p += 4;
    *p++ = 0x83; *p++ = 0xC4; *p++ = 0x08;                    // add esp,8
    *p++ = 0x61;                                              // popad
    *p++ = 0x9D;                                              // popfd
    for (int i = 0; i < 5; i++) *p++ = kFaceRotationBytes[i]; // the displaced prologue
    *p++ = 0xE9;
    *(int32_t*)p = (int32_t)((kFaceRotation + 5) - ((uintptr_t)p + 4)); p += 4;
    if (!dvr::hooks::detour_install(g_frDet, "facing", kFaceRotation,
                                    kFaceRotationBytes, 5, g_frStub)) {
        VirtualFree(g_frStub, 0, MEM_RELEASE); g_frStub = NULL;
        return false;                                          // detour_install logged the bytes
    }
    DVR_INFO("armfollow/facing: hooked FaceRotation at 0x%08X (vtable slot 252, derived and "
             "byte-verified). The handler only edits the Yaw argument, and only for the validated "
             "possessed pawn.", (unsigned)kFaceRotation);
    return true;
}

// ---- the native-facing probe: one shot, read-only --------------------------
static void NativeFacingProbe()
{
    if (g_nfpDone) return;
    if (!g_peCtrl || !g_pePawn) return;              // wait for gameplay
    g_nfpDone = true;

    const uintptr_t txLo = kModBase + 0x1000, txHi = kModBase + 0xB9341F;

    // 1. the UFunction, and which class actually owns the override
    for (int pass = 0; pass < 2; pass++) {
        const char* want = pass ? "Pawn" : "DishonoredPlayerPawn";
        uint8_t* fn = NULL;
        void**   objs = *(void***)kGObjHdr;
        uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
        if (!objs || onum < 1000 || onum > 4000000) break;
        for (uint32_t i = 1; i < onum; i++) {
            if ((i & 1023) == 0 && !RangeReadable(objs + i, 1024 * sizeof(void*))) break;
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x100)) continue;
            const char* cn = ObjClassName(o);
            if (!cn || strcmp(cn, "Function")) continue;
            const char* nm = RealName(*(uint32_t*)(o + kNameOff));
            if (!nm || strcmp(nm, "FaceRotation")) continue;
            uint8_t* outer = *(uint8_t**)(o + kOuterOff);
            const char* on = (outer && RangeReadable(outer, 0x40)) ? RealName(*(uint32_t*)(outer + kNameOff)) : NULL;
            if (!on || strcmp(on, want)) continue;
            fn = o;
            DVR_INFO("armfollow/nfp: FaceRotation UFunction on '%s' obj[%u] @ %p", on, i, (void*)o);
            break;
        }
        if (!fn) { DVR_INFO("armfollow/nfp: no FaceRotation UFunction with Outer '%s'", want); continue; }
        // every .text pointer in the tail - the exec thunk is one of them
        for (uint32_t off = 0x40; off + 4 <= 0x120; off += 4) {
            if (!RangeReadable(fn + off, 4)) break;
            const uintptr_t v = *(uintptr_t*)(fn + off);
            if (v < txLo || v >= txHi) continue;
            DVR_INFO("armfollow/nfp:   +0x%02x -> .text RVA 0x%06X   <-- candidate exec thunk; "
                     "disassemble it offline and follow its inner call to the C++ method",
                     (unsigned)off, (unsigned)(v - kModBase));
        }
    }

    // 2. the pawn's vtable head - FaceRotation is an override, so if it is
    //    virtual it differs here from a plain Pawn's
    if (RangeReadable(g_pePawn, 4)) {
        uintptr_t* vt = *(uintptr_t**)g_pePawn;
        if (vt && RangeReadable(vt, 64 * sizeof(uintptr_t))) {
            DVR_INFO("armfollow/nfp: pawn %p vtable @ %p (RVA 0x%06X)",
                     (void*)g_pePawn, (void*)vt, (unsigned)((uintptr_t)vt - kModBase));
            char line[512]; int n = 0;
            for (int k = 0; k < 48; k++) {
                const uintptr_t f = vt[k];
                if (f < txLo || f >= txHi) continue;
                n += sprintf(line + n, "%d:%06X ", k, (unsigned)(f - kModBase));
                if (n > 420) break;
            }
            DVR_INFO("armfollow/nfp: vtable .text slots (index:RVA) %s", line);
        }
    }

    DVR_INFO("armfollow/nfp: FaceRotation seen through ProcessEvent %u times so far. A count that "
             "STAYS AT ZERO through gameplay is the proof that the engine reaches it "
             "native-to-native, and therefore that hooking the script exec wrapper would catch "
             "nothing. A non-zero count means the script route is live and is the cheaper hook.",
             g_nfpFaceHits);
}

// ---- option A: bRemoveMeshRotation on the two hand controls ---------------
// Liveness on every write, per 32.6: writes into a freed AnimTree after a
// CollectGarbage on a save load corrupted the heap.
static inline bool AsrAlive(int h)
{
    uint8_t* o = g_asrCtl[h];
    if (!o || ((uintptr_t)o & 3) || !g_asrIdx[h]) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || g_asrIdx[h] >= num) return false;
    if ((uint8_t*)objs[g_asrIdx[h]] != o) return false;
    return *(void**)(o + kClassOff) == g_asrCls[h];
}

// The bit, by REFLECTION. skelcontrol.cpp hardcodes 0x10 for this bool; that
// number is a guess and is deliberately not reused - if the property cannot be
// resolved this lever refuses and says so rather than writing a guessed bit
// into a live AnimTree node.
static void AsrResolveBit()
{
    if (g_asrTriedOff) return;
    g_asrTriedOff = true;
    if (!FindBoolProp("SkelControlBase", "bRemoveMeshRotation", &g_asrOff, &g_asrMask) &&
        !FindBoolProp("SkelControlSingleBone", "bRemoveMeshRotation", &g_asrOff, &g_asrMask)) {
        g_asrOff = 0; g_asrMask = 0;
        DVR_WARN("armfollow/striprot: bRemoveMeshRotation did not resolve on SkelControlBase or "
                 "SkelControlSingleBone - the lever REFUSES. It will not write skelcontrol.cpp's "
                 "hardcoded 0x10 on a guess.");
        return;
    }
    DVR_INFO("armfollow/striprot: bRemoveMeshRotation at +0x%x mask 0x%x (reflected). "
             "For reference skelcontrol.cpp assumes bit 0x10 in kSkcBools (+0x%x) - %s.",
             (unsigned)g_asrOff, (unsigned)g_asrMask, (unsigned)kSkcBools,
             (g_asrOff == kSkcBools && g_asrMask == 0x10) ? "which agrees"
                                                          : "which DISAGREES with this");
}

// Find both hand controls by NAME. Never by index - 31.4 records GObjects
// iteration order as unstable across probes in one session.
static void AsrFind()
{
    const uint32_t now = (uint32_t)GetTickCount64();
    if (now < g_asrNextFindMs) return;
    g_asrNextFindMs = now + 5000;
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;
    int found = 0;
    for (uint32_t i = 1; i < onum && found < 2; i++) {
        if ((i & 1023) == 0 && !RangeReadable(objs + i, 1024 * sizeof(void*))) break;
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kSkcStr + 4)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "SkelControl")) continue;
        const char* nm = RealName(*(uint32_t*)(o + kSkcName));
        if (!nm) continue;
        int h = -1;
        if (strstr(nm, "LookAtControl_LeftHand"))  h = 0;
        else if (strstr(nm, "LookAtControl_RightHand")) h = 1;
        if (h < 0 || g_asrCtl[h]) continue;
        g_asrCtl[h] = o; g_asrIdx[h] = i; g_asrCls[h] = *(void**)(o + kClassOff);
        if (g_asrOff && RangeReadable(o + g_asrOff, 4))
            g_asrWasSet[h] = (*(uint32_t*)(o + g_asrOff) & g_asrMask) != 0;
        ++found;
        DVR_INFO("armfollow/striprot: %s hand control obj[%u] '%s' class '%s' @ %p, "
                 "bRemoveMeshRotation reads %d",
                 h ? "RIGHT" : "LEFT", i, nm, cn, (void*)o, (int)g_asrWasSet[h]);
    }
    if (!g_asrCtl[0] && !g_asrCtl[1])
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 30000,
                         "armfollow/striprot: no SkelControl named LookAtControl_LeftHand or "
                         "_RightHand in GObjects yet (searched %u). Not in gameplay, or this rig "
                         "does not carry them - either way the lever is doing nothing.", onum);
}

// Forced every dispatch: these nodes are restamped continuously, which is the
// same reason SkcRotApply exists.
static void AsrTick(bool slow)
{
    if (g_asrWant < 0.0f) return;                 // lever off
    AsrResolveBit();
    if (!g_asrOff) return;                        // refused, already logged
    if (slow) {
        for (int h = 0; h < 2; h++)
            if (g_asrCtl[h] && !AsrAlive(h)) { g_asrCtl[h] = NULL; g_asrIdx[h] = 0; }
        if (!g_asrCtl[0] || !g_asrCtl[1]) AsrFind();
    }
    const bool want = g_asrWant >= 0.5f;
    for (int h = 0; h < 2; h++) {
        if (!AsrAlive(h)) continue;
        if (!RangeReadable(g_asrCtl[h] + g_asrOff, 4)) continue;
        uint32_t* b = (uint32_t*)(g_asrCtl[h] + g_asrOff);
        if (want) *b |= g_asrMask; else *b &= ~g_asrMask;
        ++g_asrWrites;
    }
    if (!g_asrSaid && (AsrAlive(0) || AsrAlive(1))) {
        g_asrSaid = true;
        DVR_INFO("armfollow/striprot: FORCING bRemoveMeshRotation = %d on the hand controls every "
                 "dispatch (they read L=%d R=%d before). This strips the component's own rotation "
                 "from those bones. If the arms stop following head YAW, this was the owner - and "
                 "unlike every body lever it is not fighting a per-tick recompute of the pawn. If "
                 "nothing changes, the arms do not rotate through this component's rotation and "
                 "that kills the lane. `arms striprot off` is the live A/B.",
                 (int)want, (int)g_asrWasSet[0], (int)g_asrWasSet[1]);
    }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                     "armfollow/striprot: want=%d writes=%u | L %s R %s",
                     (int)want, g_asrWrites,
                     AsrAlive(0) ? "live" : "-", AsrAlive(1) ? "live" : "-");
}

// Find LookAtControl_Camera by NAME. Never by index: 31.4 records GObjects
// iteration order as unstable across probes in one session.
static void ArmLookFind()
{
    const uint32_t now = (uint32_t)GetTickCount64();
    if (now < g_afLookNextFindMs) return;
    g_afLookNextFindMs = now + 5000;
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;
    for (uint32_t i = 1; i < onum; i++) {
        if ((i & 1023) == 0 && !RangeReadable(objs + i, 1024 * sizeof(void*))) break;
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kSkcStr + 4)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "SkelControl")) continue;
        const char* nm = RealName(*(uint32_t*)(o + kSkcName));
        if (!nm || !strstr(nm, "LookAtControl_Camera")) continue;
        g_afLookObj = o; g_afLookIdx = i; g_afLookCls = *(void**)(o + kClassOff);
        g_afLookWasStr = *(float*)(o + kSkcStr);
        DVR_INFO("armfollow: LookAtControl_Camera found obj[%u] '%s' class '%s' @ %p, ControlStrength reads "
                 "%.3f. This is the control attempt 5 named as aiming the arms at the view, and it has never "
                 "been judged because its usual writer is dead behind the hand-mesh gate.",
                 i, nm, cn, (void*)o, g_afLookWasStr);
        return;
    }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 30000,
                     "armfollow: no SkelControl named LookAtControl_Camera in GObjects yet (searched %u "
                     "objects). It is created with the player's AnimTree, so this is expected before a level "
                     "is up and is a real absence after one.", onum);
}

// One read of the live values, on the script lane. Logs only CHANGES (plus a
// periodic summary), so a run where nothing moves stays quiet and a run where
// something moves says so the moment it does.
static void ArmFollowTick()
{
    // THIS RUNS ON EVERY ProcessEvent DISPATCH - thousands per second. The
    // first cut did the full job here and cost frame rate: CamStillValid does
    // a name lookup and a strstr, GetTickCount64 is a syscall-ish read, and
    // every field did its own RangeReadable. All of that is now on the SLOW
    // path. The fast path is a cheap liveness compare plus the writes, because
    // only the writes need the dispatch cadence - the reads are the
    // instrument, and an instrument that costs frame rate changes what it
    // measures.
    static LONG afTick = 0;
    const LONG n = InterlockedIncrement(&afTick);
    const bool slow = (n & 127) == 0;

    if (!g_afTried) { if (slow) ArmFollowResolve(); return; }

    // Cheap every-dispatch liveness: one GObjects slot compare. The expensive
    // identity check (name + class) rides the slow path.
    if (!CamAlive()) {
        g_camObj = NULL;
        for (int i = 0; i < kAfInfCount; i++) g_afInfCache[i] = NULL;   // never write through a dead camera's influences
        if (slow) {
            ++g_afNoCam;
            // The seam only revalidates g_camObj while it has something to
            // WRITE, and the lever only while it is armed; a probe has
            // neither, so it finds the camera itself - rarely, because
            // FindLiveCamera walks GObjects.
            FindLiveCamera();
        }
        return;
    }
    uint8_t* camObj = g_camObj;

    if (slow && !CamStillValid()) { g_camObj = NULL; return; }

    if (slow) ++g_afTicks;

    if (slow) ArmFovTick();   // VR-30: refresh the viewmodel lens ratio off the hot path

    if (slow && g_afReady) {
        for (int i = 0; i < kAfFieldCount; i++) {
            if (!g_afFieldOff[i] && i != kAfWeight) continue;
            const uint32_t off = g_afVtOff + g_afFieldOff[i];
            if (!RangeReadable(camObj + off, 4)) continue;
            const float v = *(const float*)(camObj + off);
            if (v < -1000.0f || v > 1000.0f) continue;         // not a weight
            if (!g_afFieldSeen[i]) {
                g_afFieldSeen[i] = true; g_afFieldLast[i] = v;
                DVR_INFO("armfollow: %s = %.3f (first read)", kAfFieldNames[i], v);
            } else if (v != g_afFieldLast[i]) {
                ++g_afFieldMoves[i];
                DVR_INFO("armfollow: %s %.3f -> %.3f  <-- DRIVEN AT RUNTIME (change #%u)",
                         kAfFieldNames[i], g_afFieldLast[i], v, g_afFieldMoves[i]);
                g_afFieldLast[i] = v;
            }
        }
    }

    if (slow && g_afInfMembersOk) {
        for (int i = 0; i < kAfInfCount; i++) {
            if (!g_afInfFound[i]) continue;
            if (!RangeReadable(camObj + g_afInfPtrOff[i], 4)) continue;
            uint8_t* inf = *(uint8_t**)(camObj + g_afInfPtrOff[i]);
            if (!inf || ((uintptr_t)inf & 3) || !RangeReadable(inf, 0x80)) continue;
            g_afInfCache[i] = inf;                     // the fast path writes through this
            const float w = *(const float*)(inf + g_afInfWeightOff);
            const float t = *(const float*)(inf + g_afInfTargetOff);
            if (w < -1000.0f || w > 1000.0f || t < -1000.0f || t > 1000.0f) continue;
            if (!g_afInfSeen[i]) {
                g_afInfSeen[i] = true; g_afInfWLast[i] = w; g_afInfTLast[i] = t;
                DVR_INFO("armfollow: %s live @ %p  weight %.3f target %.3f (first read)",
                         kAfInfNames[i], (void*)inf, w, t);
            } else if (w != g_afInfWLast[i] || t != g_afInfTLast[i]) {
                ++g_afInfMoves[i];
                DVR_INFO("armfollow: %s weight %.3f -> %.3f target %.3f -> %.3f  <-- THE GAME DRIVES THIS "
                         "(change #%u)", kAfInfNames[i], g_afInfWLast[i], w, g_afInfTLast[i], t, g_afInfMoves[i]);
                g_afInfWLast[i] = w; g_afInfTLast[i] = t;
            }
        }
    }

    // ---- the write, last, so this dispatch's value is ours ----------------
    // Every dispatch, because the read above measured these being recomputed
    // continuously: one write per frame would lose that race. Rotation only -
    // m_ArmFollowOffset_Weight_* is the POSITION channel and belongs to VR-33.
    if (g_afReady && g_afForceWeight >= 0.0f) {
        const float v = g_afForceWeight > 1.0f ? 1.0f : g_afForceWeight;
        // One readability check for the whole struct instead of one per field,
        // and the offsets were resolved at startup - CamAlive above already
        // said the object is the one we resolved against.
        if (RangeReadable(camObj + g_afVtOff, 0x40)) {
            *(float*)(camObj + g_afVtOff + g_afFieldOff[kAfWeight])  = v;
            if (g_afFieldOff[kAfRotPrim]) *(float*)(camObj + g_afVtOff + g_afFieldOff[kAfRotPrim]) = v;
            if (g_afFieldOff[kAfRotSec])  *(float*)(camObj + g_afVtOff + g_afFieldOff[kAfRotSec])  = v;
        }
        ++g_afWrites;
        if (!g_afSaidWriting) {
            g_afSaidWriting = true;
            DVR_INFO("armfollow: FORCING the rotational follow weights to %.3f every dispatch "
                     "(m_ArmFollowWeight, _Rot_Primary, _Rot_Secondary). 0 = the arms should stop "
                     "following the view. The position channel is untouched. `arms follow off` restores "
                     "the game's own value.", v);
        }
    }

    // ---- the influence write ----------------------------------------------
    // Both DisableArmFollow influences, every dispatch: m_Weight AND
    // m_TargetWeight AND m_bActive. Writing the weight itself (not just the
    // target) is what makes this hold - the engine's blend walks m_Weight
    // toward m_TargetWeight, and the caller that cancels the request only
    // touches the target.
    if (g_afInfMembersOk && g_afForceInf >= 0.0f) {
        const float v = g_afForceInf > 1.0f ? 1.0f : g_afForceInf;
        static const int kFollowInf[2] = { kAfInfFollowPrim, kAfInfFollowSec };
        for (int k = 0; k < 2; k++) {
            const int i = kFollowInf[k];
            // The pointer was fetched and range-checked on the last slow tick.
            // Re-deref-ing and re-checking it thousands of times a second is
            // what cost the frame rate; the slow tick re-validates it, and a
            // camera that dies clears the cache through CamAlive above.
            uint8_t* inf = g_afInfCache[i];
            if (!inf) continue;
            *(float*)(inf + g_afInfWeightOff) = v;   // what the blend reads
            *(float*)(inf + g_afInfTargetOff) = v;   // what the game's caller cancels
            if (g_afInfActiveOff && g_afInfActiveMask) {
                uint32_t* b = (uint32_t*)(inf + g_afInfActiveOff);
                if (v > 0.0f) *b |= g_afInfActiveMask;
                else          *b &= ~g_afInfActiveMask;
            }
        }
        ++g_afInfWrites;
        if (!g_afSaidInfWrite) {
            g_afSaidInfWrite = true;
            DVR_INFO("armfollow: FORCING both DisableArmFollow influences to weight %.3f every dispatch "
                     "(m_Weight, m_TargetWeight and m_bActive together). The engine's own recompute should "
                     "now derive an arm follow of %.3f and HOLD it, instead of racing our write. "
                     "`arms disable off` restores the game's own.", v, 1.0f - v);
        }
    }

    // ---- the counter-yaw write, the yaw answer ----------------------------
    // Equal and opposite to how far the head has turned since the reference.
    // Written into the game's OWN authored arm offset, which the engine
    // already applies and already scales by a weight it respects - so
    // attachments follow for free, which a matrix patch would not give us.
    if (g_afReady && g_afCounterYaw >= 0.0f && (g_afRotOffPrim || g_afRotOffSec)) {
        if (!g_afCntHaveRef) {
            // Prefer the head path's own reference so a recentre (F5) moves
            // both together; fall back to wherever the head is right now.
            g_afCntRefYaw = g_haveInjRef ? g_injRefYaw : g_hmdYaw;
            g_afCntHaveRef = true;
        }
        float dy = g_hmdYaw - g_afCntRefYaw;              // radians
        while (dy >  3.14159265f) dy -= 6.2831853f;
        while (dy < -3.14159265f) dy += 6.2831853f;

        // VR-30: THE FOV SCALE. The residual is a gain error, and the reason is
        // that the viewmodel is drawn through its OWN lens:
        // DishonoredPlayerSkeletalComponent carries m_bUseFOV and m_FOV, blended
        // by the camera's m_MeshSpecificFOVWeight. A narrower lens means more
        // pixels per degree, so a given world-space rotation of the arms moves
        // further on screen than the same rotation of the world. Countering by
        // the raw head angle therefore OVER-corrects, which is exactly the
        // reported artifact - turn right, the arms drift left.
        //
        // The ratio is armsFOV / worldFOV, below 1 for a narrower viewmodel
        // lens. It also explains why only YAW shows it: pitch is switched off at
        // the source by the influence, so no angle is ever scaled there. Nothing
        // to get wrong.
        //
        // If the arms' FOV has not been resolved the ratio stays 1 and this is
        // the raw counter, i.e. exactly the behaviour that already shipped.
        dy *= g_afFovRatio;
        const float kUEPerRadLocal = 10430.378f;          // 65536 / 2pi
        const int32_t cy = (int32_t)(-dy * kUEPerRadLocal * g_afCounterYaw * (float)g_flipYaw);
        g_afCntLastDeg = -dy * 57.2957795f * g_afCounterYaw;
        if (RangeReadable(camObj + g_afVtOff, 0x40)) {
            if (g_afRotOffPrim) ((int32_t*)(camObj + g_afVtOff + g_afRotOffPrim))[1] = cy;   // [0]=Pitch [1]=Yaw [2]=Roll
            if (g_afRotOffSec)  ((int32_t*)(camObj + g_afVtOff + g_afRotOffSec))[1]  = cy;
        }
        ++g_afCntWrites;
        // VR-30: "it stops having an effect past about 90 degrees" splits into two
        // very different faults and this line is what tells them apart. If the
        // ANGLE below keeps growing past 90 while the picture stops responding,
        // the engine is clamping the offset it applies and the ceiling is theirs.
        // If the angle itself flattens, the fault is ours - either g_hmdYaw
        // saturating or the wrap folding the delta back. Track the extreme so a
        // slow deliberate head turn leaves the evidence behind.
        {
            const float a = g_afCntLastDeg < 0.0f ? -g_afCntLastDeg : g_afCntLastDeg;
            if (a > g_afCntMaxDeg) g_afCntMaxDeg = a;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 500,
                             "armfollow: counter-yaw head %+.1f deg -> writing %+d UE (%.1f deg), biggest so far "
                             "%.1f deg. Turn your head slowly past 90 and watch this: if the ANGLE keeps climbing "
                             "while the arms stop moving, the engine is clamping the offset; if the ANGLE stops "
                             "climbing, the fault is on our side.",
                             dy * 57.2957795f, cy, g_afCntLastDeg, g_afCntMaxDeg);
        }
        if (!g_afSaidCounter) {
            g_afSaidCounter = true;
            DVR_INFO("armfollow: COUNTER-YAW armed at %.2f - m_ArmFollowOffset_Rot_Primary/_Secondary get a yaw "
                     "equal and opposite to the head's turn since the reference, every dispatch. The arms should "
                     "hold still in the world while the view turns. Stick turning moves the BODY yaw, not the "
                     "head's, so it is untouched and the arms come round with the body.", g_afCounterYaw);
        }
    }

    // ---- the yaw census (VR-30) -------------------------------------------
    // The A/B result this exists to explain: BodyYawLock=1 stops the arms
    // following head yaw AND stops the right stick turning the character;
    // BodyYawLock=-1 restores the stick and restores the coupling. One writer,
    // both effects. Nothing here writes; it samples the whole yaw chain on one
    // line so head-only and stick-only input can be compared with the writer on
    // and off.
    //
    // ANSWERED 2026-09-05. The old hold pinned the body for two reasons at
    // once: `back` was always zero (its reference was never assigned), and the
    // controller it read was the PREVIOUS SCENE's, held by the camera scan for
    // the whole gameplay window while the event stream had the live one. A dead
    // controller's rotation does not move, so the pawn was stamped with a
    // constant - the arms could not follow the head and the stick could not
    // turn the body. Both faults are fixed elsewhere now; this census stays as
    // the instrument that would catch either coming back.
    //
    // It runs whatever the levers are set to: the lever-off cells are the
    // control, and a probe that only runs on the suspect configuration cannot
    // produce them.
    // PERF: the clock read is on the SLOW path. c5ecea34 moved this function's
    // reads off the per-dispatch lane for exactly this reason - GetTickCount64
    // thousands of times a second buys nothing when the line it gates prints
    // twice a second. 1-in-128 still oversamples a 500 ms window comfortably.
    if (slow) {
        const double nowC = MaimNowMs();
        if (nowC >= g_afCenNextMs) {
            g_afCenNextMs = nowC + 500.0;
            uint8_t* pawnC = FpPawn();
            int32_t ctrlC = 0, pawnYawC = 0;
            bool haveCtrl = false, havePawn = false;
            // MEASURED 2026-09-05, both runs: patterns.h kPcRotBase[0] (0x9c) is NOT
            // the PlayerController's Rotation. It is the CAMERA's - kCamRotBase[0] is
            // the identical literal - and on the controller that offset holds a float
            // (0xbcc8cea0 = -0.0245), which read as an int32 is the -1127690592 the
            // census reported as 'ctrl' for a whole run. PlayerController is an Actor,
            // so Actor.Rotation resolved by reflection is its offset exactly as it is
            // the pawn's: one lookup, both objects, nothing guessed.
            // The census reads the EVENT-LATCHED controller, not the camera
            // scan's. Measured 2026-09-05: the scan held the menu's controller
            // (23CB3800) for the whole gameplay window while the event stream
            // had the real one (16D86800), so every ctrl reading was the
            // previous scene's dead value. Print both when they disagree - a
            // census that cannot show the split cannot diagnose it.
            uint8_t* ctrlObj = g_peCtrl ? g_peCtrl : g_pcObj;
            if (ctrlObj && g_afActorRotOff && RangeReadable(ctrlObj + g_afActorRotOff, 12)) {
                ctrlC = ((const int32_t*)(ctrlObj + g_afActorRotOff))[1];
                haveCtrl = true;
            }
            if (pawnC && g_afActorRotOff && RangeReadable(pawnC + g_afActorRotOff, 12)) {
                pawnYawC = ((const int32_t*)(pawnC + g_afActorRotOff))[1];
                havePawn = true;
            }
            const float headC = g_hmdYaw, viewC = g_viewYawRad;
            const float kDegPerUE = 360.0f / 65536.0f;
            if (!g_afSaidCensus) {
                g_afSaidCensus = true;
                DVR_INFO("armfollow/yaw: census armed. head=HMD yaw, ctrl=PlayerController "
                         "Rotation.Yaw (+0x%x), pawn=Actor.Rotation.Yaw, view=what the head write "
                         "put in the ProcessViewRotation parms. The /s columns are the last "
                         "half-second scaled to a second. HOLD STILL AND TURN ONE THING AT A TIME: "
                         "head only with the stick centred, then stick only with the head still, "
                         "with BodyYawLock=1 and again with -1. A /s that reads ~0 while you are "
                         "moving that input is the finding.",
                         (unsigned)g_afActorRotOff);
            }
            if (g_afCenHave && haveCtrl && havePawn) {
                const double dt = (nowC - g_afCenLastMs) * 0.001;
                if (dt > 0.05) {
                    float dHead = headC - g_afCenHead;
                    while (dHead >  3.14159265f) dHead -= 6.2831853f;
                    while (dHead < -3.14159265f) dHead += 6.2831853f;
                    float dView = viewC - g_afCenView;
                    while (dView >  3.14159265f) dView -= 6.2831853f;
                    while (dView < -3.14159265f) dView += 6.2831853f;
                    // rotator wrap: the short way round, in UE units
                    int32_t dCtrl = (int32_t)(int16_t)(uint16_t)((uint32_t)ctrlC - (uint32_t)g_afCenCtrl);
                    int32_t dPawn = (int32_t)(int16_t)(uint16_t)((uint32_t)pawnYawC - (uint32_t)g_afCenPawn);
                    const double inv = 1.0 / dt;
                    DVR_INFO("armfollow/yaw: head %+7.1f (%+7.1f/s)  ctrl %+7.1f "
                             "(%+7.1f/s)  pawn %+7.1f (%+7.1f/s)  view %+7.1f (%+7.1f/s)%s",
                             headC * 57.2957795f, dHead * 57.2957795f * (float)inv,
                             (float)ctrlC * kDegPerUE, (float)dCtrl * kDegPerUE * (float)inv,
                             (float)pawnYawC * kDegPerUE, (float)dPawn * kDegPerUE * (float)inv,
                             viewC * 57.2957795f, dView * 57.2957795f * (float)inv,
                             (dCtrl == 0 && dPawn == 0 && (dView > 0.004f || dView < -0.004f))
                                 ? "   <-- view moved, ctrl and pawn did NOT" : "");
                }
            } else if (!haveCtrl || !havePawn) {
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                 "armfollow/yaw: census idle - %s%s%s (no sample this pass)",
                                 haveCtrl ? "" : "no PlayerController rotator; ",
                                 havePawn ? "" : "no pawn Actor.Rotation; ",
                                 (haveCtrl && havePawn) ? "ready" : "waiting for gameplay");
            }
            if (haveCtrl && havePawn) {
                g_afCenHead = headC; g_afCenView = viewC;
                g_afCenCtrl = ctrlC; g_afCenPawn = pawnYawC;
                g_afCenLastMs = nowC;
                g_afCenHave = true;
            }
        }
    }

    // ---- the body-yaw hold: MOVED (VR-30) ---------------------------------
    // It used to write pawn.Rotation.Yaw = ctrlYaw - back from here. Two
    // measured faults killed that: `back` was always zero (its reference was
    // never assigned), and g_pcObj was the PREVIOUS SCENE's controller for the
    // whole gameplay window, so ctrlYaw was a dead constant and the write
    // pinned the body - which is why the arms froze AND the stick died.
    //
    // The replacement lives in head_track.cpp (YawPublish / YawApplyBody) and
    // runs from the ProcessViewRotation dispatch, after the head injection.
    // This function runs BEFORE that injection, so it is the wrong lane for it:
    // anything it read here would be one dispatch stale.

    if (slow) NativeFacingProbe();   // VR-30: where does the engine face the body?
    AsrTick(slow);   // VR-30 option A: strip the mesh rotation at the hand bones

    // ---- the camera look-at write -----------------------------------------
    // Same cadence as the rest, and the same reason. Liveness is re-checked on
    // EVERY write, not just the slow tick: this is the store that corrupted the
    // heap in 32.6 when an AnimTree was freed under it, and a slot compare is
    // cheap enough to pay for on every dispatch.
    if (g_afLookStrength >= 0.0f) {
        if (slow && !ArmLookAlive()) { g_afLookObj = NULL; g_afLookIdx = 0; }
        if (slow && !g_afLookObj) ArmLookFind();
        if (ArmLookAlive()) {
            *(float*)(g_afLookObj + kSkcStr) = g_afLookStrength;
            ++g_afLookWrites;
            if (!g_afSaidLook) {
                g_afSaidLook = true;
                DVR_INFO("armfollow: FORCING LookAtControl_Camera ControlStrength to %.3f every dispatch "
                         "(it read %.3f before). 0 = the control stops aiming the arms at the view. If the "
                         "HORIZONTAL follow goes away, this was the yaw owner.",
                         g_afLookStrength, g_afLookWasStr);
            }
        }
    }

    // The clock read is on the slow path too: GetTickCount64 thousands of
    // times a second buys nothing when the line it gates prints twice a minute.
    if (slow) {
        const uint32_t now = (uint32_t)GetTickCount64();
        if (g_afNextLogMs == 0) g_afNextLogMs = now + 30000;
        if (now >= g_afNextLogMs) { g_afNextLogMs = now + 30000; ArmFollowReport("30 s"); }
    }
}

// The verdict. Both branches print - "nothing moved" is the answer that kills
// the lane, and an instrument that cannot say it is not evidence.
static void ArmFollowReport(const char* why)
{
    if (!g_afTried) {
        DVR_INFO("armfollow: %s - not resolved yet (GNames not up, or no camera has been seen). "
                 "%u ticks, %u without a camera.", why ? why : "?", g_afTicks, g_afNoCam);
        return;
    }
    uint32_t moves = 0, infMoves = 0;
    for (int i = 0; i < kAfFieldCount; i++) moves += g_afFieldMoves[i];
    for (int i = 0; i < kAfInfCount; i++)  infMoves += g_afInfMoves[i];

    DVR_INFO("armfollow: %s - %u script-lane reads (%u had no camera)", why ? why : "?", g_afTicks, g_afNoCam);
    for (int i = 0; i < kAfFieldCount; i++)
        if (g_afFieldSeen[i])
            DVR_INFO("armfollow:   %-34s = %.3f, changed %u time(s)", kAfFieldNames[i],
                     g_afFieldLast[i], g_afFieldMoves[i]);
    for (int i = 0; i < kAfInfCount; i++)
        if (g_afInfSeen[i])
            DVR_INFO("armfollow:   %-40s weight %.3f target %.3f, changed %u time(s)", kAfInfNames[i],
                     g_afInfWLast[i], g_afInfTLast[i], g_afInfMoves[i]);

    if (g_afCntWrites)
        DVR_INFO("armfollow:   counter-yaw: %u writes, last %+.1f deg, biggest %.1f deg. If the biggest sticks near "
                 "90 after a deliberate turn further than that, something is clamping: OURS if the number stopped "
                 "growing, the ENGINE'S if it grew and the arms did not follow it.",
                 g_afCntWrites, g_afCntLastDeg, g_afCntMaxDeg);

    if (!g_afReady && !g_afInfMembersOk)
        DVR_WARN("armfollow: %s - VERDICT NOTHING RESOLVED. No offset was found, so no claim can be made "
                 "either way about the arm-follow lane. Check the class names against the build before "
                 "concluding anything.", why ? why : "?");
    else if (moves == 0 && infMoves == 0)
        DVR_INFO("armfollow: %s - VERDICT RESOLVED BUT NEVER MOVED. Every field sits at its shipped "
                 "default and the game never touched one. That means these are NOT driven at runtime, so "
                 "a write here has nothing competing with it - the lever is worth trying. It also means "
                 "the game never disables arm follow itself, so there is no free A/B to copy.",
                 why ? why : "?");
    else
        DVR_INFO("armfollow: %s - VERDICT DRIVEN AT RUNTIME (%u settings changes, %u influence changes). "
                 "The game moves these itself, so a plain write will be recomputed over: drive the "
                 "influence's target weight the way the game does, and expect to write on the dispatch "
                 "cadence rather than once.", why ? why : "?", moves, infMoves);
}

// `arms` / `arms probe` on the command seam.
static void ArmFollowSetForce(float v, const char* who)
{
    g_afForceWeight = v;
    g_afSaidWriting = false;
    if (v < 0.0f)
        DVR_INFO("armfollow: follow OFF (%s) - the game's own weight stands again. %u writes were made. "
                 "The engine recomputes these every frame, so it takes over on the next dispatch.",
                 who ? who : "?", g_afWrites);
    else
        DVR_INFO("armfollow: follow weight forced to %.3f (%s) - written every dispatch from now on. "
                 "0 = the arms should stop following the view; 1 = the game's shipped behaviour, written "
                 "by us instead of by it (a useful control: if 1 looks stock, the write is landing).",
                 v, who ? who : "?");
}

static void ArmFollowSetInfluence(float v, const char* who)
{
    g_afForceInf = v;
    g_afSaidInfWrite = false;
    if (v < 0.0f)
        DVR_INFO("armfollow: disable-influence OFF (%s) - the game owns the influences again. %u writes were "
                 "made.", who ? who : "?", g_afInfWrites);
    else
        DVR_INFO("armfollow: DisableArmFollow influences forced to %.3f (%s) - written every dispatch. This is "
                 "the engine's own switch, so the recompute should produce an arm follow of %.3f and hold it "
                 "rather than racing us. 1.0 is full decoupling.", v, who ? who : "?", 1.0f - v);
}

static void ArmFollowSetCounterYaw(float v, const char* who)
{
    g_afCounterYaw = v;
    g_afSaidCounter = false;
    g_afCntHaveRef = false;             // re-reference on every arm
    if (v < 0.0f) {
        // Put the offset back to zero - a forced yaw left behind would outlive
        // the lever and read as the arms being permanently crooked.
        if (g_camObj && g_afReady && RangeReadable(g_camObj + g_afVtOff, 0x40)) {
            if (g_afRotOffPrim) ((int32_t*)(g_camObj + g_afVtOff + g_afRotOffPrim))[1] = 0;
            if (g_afRotOffSec)  ((int32_t*)(g_camObj + g_afVtOff + g_afRotOffSec))[1]  = 0;
        }
        DVR_INFO("armfollow: counter-yaw OFF (%s) - the arm offset is back to 0 after %u writes.",
                 who ? who : "?", g_afCntWrites);
    } else {
        DVR_INFO("armfollow: counter-yaw set to %.2f (%s). 1.0 = the arms hold still while you look; 0.5 = they "
                 "follow at half rate; 0 = writes a zero offset, which is the control that proves the write "
                 "lands without changing anything.", v, who ? who : "?");
    }
}

static void ArmFollowSetFacing(float v, const char* who)
{
    g_frWant = v; g_frSaid = false;
    if (v >= 0.0f) InstallFaceRotationHook();
    Log("armfollow/facing: %s -> %s. Raw pawn-yaw assignment is measured futile (4 of 186 writes "
        "survived); this changes what the engine is ASKED to face instead, and lets its own "
        "function run. -1 = off, 1 = the separated body target, between blends the difference.",
        who ? who : "?", v < 0.0f ? "off" : "ON");
}

static void ArmFollowSetStripRot(float v, const char* who)
{
    g_asrWant = v; g_asrSaid = false;
    Log("armfollow/striprot: %s -> %s. The body levers are measured futile (the engine re-derives "
        "the pawn yaw from the controller every tick), so this one works at the BONE instead: "
        "bRemoveMeshRotation on the two hand controls. -1 = off, 1 = strip, 0 = force it clear.",
        who ? who : "?", v < 0.0f ? "off" : (v >= 0.5f ? "STRIP" : "force-clear"));
}


static void ArmFollowSetLookAt(float v, const char* who)
{
    g_afLookStrength = v;
    g_afSaidLook = false;
    if (v < 0.0f) {
        // Put back what the engine had, if we still hold a live node - leaving
        // a forced 0 behind would outlive the lever.
        if (ArmLookAlive() && g_afLookWasStr >= 0.0f) *(float*)(g_afLookObj + kSkcStr) = g_afLookWasStr;
        DVR_INFO("armfollow: look-at OFF (%s) - ControlStrength restored to %.3f after %u writes.",
                 who ? who : "?", g_afLookWasStr, g_afLookWrites);
    } else {
        g_afLookNextFindMs = 0;   // look for it now rather than on the next 5 s tick
        DVR_INFO("armfollow: LookAtControl_Camera strength forced to %.3f (%s). 0 stops that control aiming "
                 "the arms at the view - the remaining suspect for the HORIZONTAL follow.", v, who ? who : "?");
    }
}

static bool ArmFollowCommand(const char* args)
{
    if (!args || !args[0] || !strcmp(args, "status") || !strcmp(args, "probe")) {
        ArmFollowReport("seam");
        return true;
    }
    char sub[16] = ""; float v = 0.0f;
    const int n = sscanf(args, "%15s %f", sub, &v);
    if (n >= 1 && !strcmp(sub, "follow")) {
        if (strstr(args, "off")) { ArmFollowSetForce(-1.0f, "seam"); return true; }
        if (n == 2 && v >= 0.0f && v <= 1.0f) { ArmFollowSetForce(v, "seam"); return true; }
        Log("armfollow: usage - arms follow <0..1> | arms follow off  (0 = decoupled, off = the game's own)");
        return true;
    }
    if (n >= 1 && !strcmp(sub, "disable")) {
        if (strstr(args, "off")) { ArmFollowSetInfluence(-1.0f, "seam"); return true; }
        if (n == 2 && v >= 0.0f && v <= 1.0f) { ArmFollowSetInfluence(v, "seam"); return true; }
        Log("armfollow: usage - arms disable <0..1> | arms disable off  (1 = ask the engine to disable arm "
            "follow itself, off = the game's own)");
        return true;
    }
    if (n >= 1 && (!strcmp(sub, "yaw") || !strcmp(sub, "counteryaw"))) {
        if (strstr(args, "off")) { ArmFollowSetCounterYaw(-1.0f, "seam"); return true; }
        if (n == 2 && v >= 0.0f && v <= 2.0f) { ArmFollowSetCounterYaw(v, "seam"); return true; }
        Log("armfollow: usage - arms yaw <0..2> | arms yaw off  (1 = the arms hold still while you look)");
        return true;
    }
    if (n >= 1 && (!strcmp(sub, "facing") || !strcmp(sub, "face"))) {
        if (strstr(args, "off")) { ArmFollowSetFacing(-1.0f, "seam"); return true; }
        if (n == 2 && v >= 0.0f && v <= 1.0f) { ArmFollowSetFacing(v, "seam"); return true; }
        Log("armfollow: usage - arms facing <0..1> | arms facing off  (1 = the engine is asked to "
            "face the separated body target instead of the view)");
        return true;
    }
    if (n >= 1 && (!strcmp(sub, "striprot") || !strcmp(sub, "strip"))) {
        if (strstr(args, "off")) { ArmFollowSetStripRot(-1.0f, "seam"); return true; }
        if (n == 2 && v >= 0.0f && v <= 1.0f) { ArmFollowSetStripRot(v, "seam"); return true; }
        Log("armfollow: usage - arms striprot <0|1> | arms striprot off  (1 = strip the component's "
            "own rotation from the hand bones; the body levers are measured futile)");
        return true;
    }
    if (n >= 1 && !strcmp(sub, "lookat")) {
        if (strstr(args, "off")) { ArmFollowSetLookAt(-1.0f, "seam"); return true; }
        if (n == 2 && v >= 0.0f && v <= 1.0f) { ArmFollowSetLookAt(v, "seam"); return true; }
        Log("armfollow: usage - arms lookat <0..1> | arms lookat off  (0 = stop LookAtControl_Camera aiming "
            "the arms at the view; the yaw candidate)");
        return true;
    }
    return false;
}
