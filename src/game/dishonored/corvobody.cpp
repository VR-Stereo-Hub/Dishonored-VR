// game/dishonored/corvobody.cpp - the CorvoBody first-person body mod (Nexus 453) meets the
// VR hands at the shoulder. docs/dishonored/CORVOBODY.md is the record and the plan.
//
// CorvoBody is a dinput8 proxy that builds a second SkeletalMeshComponent on the pawn
// (Corvo's DLC body) animated by an AnimTree it constructs at runtime, and re-places that
// body under the pawn after every UGameEngine::Tick (its own MinHook detour). This module:
//
//   1. detects the mod (their dinput8.dll exports BlinkBootstrap_Register, their ini is
//      next to the exe) and reads the two settings of theirs that collide with VR;
//   2. finds their VISIBLE body component (outer = the pawn, mesh name *skm_Corvo_Body,
//      not HiddenGame; the HiddenGame twin is their shadow body);
//   3. chains a second detour over theirs at UGameEngine::Tick so, AFTER their per-tick
//      placement and before the draw, the body is moved so its shoulder midpoint sits on the
//      arm IK's shoulder centre (head-relative, in the IK's body-yaw frame) and faces the IK's
//      body yaw: the full-arm IK arms and the body then share one shoulder line, and the body
//      follows the head in room scale instead of standing under the pawn;
//   4. hides the body's own arms at the shoulder while the full-arm IK draws ours (ArmMode vr);
//      without the IK (ArmMode body) it drives the body's own arm IK controls
//      (SkelControlLimb hand_L_jnt / hand_R_jnt, left at strength 0 by the mod) with the
//      world point the hand drive handed the first-person hand bone, reach-clamped, and hides
//      the body's hand bones instead. Every hide is read back from BoneVisibilityStates and
//      re-issued when the mod's own weapon-change UnHide undoes it.
//
// Nothing of CorvoBody's is modified: no file, no code, no import. Their ini is READ.
// Every offset is resolved by name and refused on a mismatch with the generated SDK.
//
// LANES: detection, lookup and the body-mode effector run in the ProcessEvent hook right
// after ApplyHandToMesh (script lane). The placement runs on the game thread inside the
// Tick chain, after CorvoBody. The IK publishes its body yaw from the render lane through
// two atomics. Their component and controls are revalidated with IsLiveObject + name on
// every use and dropped on any pawn change or on their own `world changed` rebuild.

static bool CbContainsNoCase(const char* hay, const char* needle)
{
    if (!hay || !needle) return false;
    size_t n = strlen(needle);
    for (const char* p = hay; *p; ++p)
        if (_strnicmp(p, needle, n) == 0) return true;
    return false;
}

static void CbWhy(const char* why)
{
    if (strncmp(g_cbWhy, why, sizeof(g_cbWhy) - 1) == 0) return;
    strncpy(g_cbWhy, why, sizeof(g_cbWhy) - 1);
    g_cbWhy[sizeof(g_cbWhy) - 1] = 0;
    Log("corvobody: %s", why);
}

// A UFunction by name whose OUTER is the named class. FindFunctionObj takes the first
// GObjects hit by name alone; FindSkelControl also exists on AnimTree, and dispatching
// AnimTree's native on a component would be a wrong-this call into the engine.
static uint8_t* CbFindFunctionIn(const char* cls, const char* fname)
{
    uint32_t fi = FindNameIdx(fname), ci = FindNameIdx(cls);
    if (fi == 0xffffffffu || ci == 0xffffffffu) return NULL;
    if (!RangeReadable((void*)kGObjHdr, 12)) return NULL;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return NULL;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        if (*(uint32_t*)(o + kNameOff) != fi) continue;
        const char* cn = ObjClassName(o);
        if (!cn || strcmp(cn, "Function")) continue;
        uint8_t* outer = *(uint8_t**)(o + kOuterOff);
        if (!outer || !RangeReadable(outer, kNameOff + 4)) continue;
        if (*(uint32_t*)(outer + kNameOff) == ci) return o;
    }
    return NULL;
}

struct CbFName { uint32_t idx; int32_t num; };
static bool CbName(const char* s, CbFName* out)
{
    uint32_t idx = FindNameIdx(s);
    if (idx == 0xffffffffu) return false;
    out->idx = idx; out->num = 0;
    return true;
}

static uint8_t* CbFindSkelControl(uint8_t* body, const char* name)
{
    CbFName n; if (!CbName(name, &n) || !g_cbFnFind) return NULL;
    struct { CbFName name; uint8_t* ret; } p = { n, NULL };
    ((PFN_ProcessEventCall)kProcessEvent)(body, g_cbFnFind, &p, NULL);
    return p.ret;
}

static bool CbBoneLocation(uint8_t* body, const char* bone, float out[3])
{
    CbFName n; if (!CbName(bone, &n) || !g_cbFnBoneLoc) return false;
    struct { CbFName name; int32_t space; float ret[3]; } p = { n, 0, { 0, 0, 0 } };
    ((PFN_ProcessEventCall)kProcessEvent)(body, g_cbFnBoneLoc, &p, NULL);
    if (p.ret[0] != p.ret[0] || p.ret[1] != p.ret[1] || p.ret[2] != p.ret[2]) return false;
    out[0] = p.ret[0]; out[1] = p.ret[1]; out[2] = p.ret[2];
    return true;
}

static int CbMatchBone(uint8_t* body, const char* bone)
{
    CbFName n; if (!CbName(bone, &n) || !g_cbFnMatchBone) return -1;
    struct { CbFName name; int32_t ret; } p = { n, -1 };
    ((PFN_ProcessEventCall)kProcessEvent)(body, g_cbFnMatchBone, &p, NULL);
    return p.ret;
}

static void CbHideBone(uint8_t* body, const char* bone, bool hide)
{
    CbFName n; if (!CbName(bone, &n)) return;
    if (hide) {
        if (!g_cbFnHide) return;
        struct { CbFName name; uint8_t physOp; uint8_t pad[3]; } p = { n, 0, { 0, 0, 0 } };   // PBO_None
        ((PFN_ProcessEventCall)kProcessEvent)(body, g_cbFnHide, &p, NULL);
    } else {
        if (!g_cbFnUnhide) return;
        struct { CbFName name; } p = { n };
        ((PFN_ProcessEventCall)kProcessEvent)(body, g_cbFnUnhide, &p, NULL);
    }
}

static void CbSetTranslation(uint8_t* comp, const float t[3])
{
    if (!g_cbFnSetTrans) return;
    struct { float v[3]; } p = { { t[0], t[1], t[2] } };
    ((PFN_ProcessEventCall)kProcessEvent)(comp, g_cbFnSetTrans, &p, NULL);
}

static void CbSetRotation(uint8_t* comp, int32_t pitch, int32_t yaw, int32_t roll)
{
    if (!g_cbFnSetRot) return;
    struct { int32_t r[3]; } p = { { pitch, yaw, roll } };
    ((PFN_ProcessEventCall)kProcessEvent)(comp, g_cbFnSetRot, &p, NULL);
}

// BoneVisibilityStates[idx]: 2 = visible (UE3 BVS_Visible), 0/1 = hidden. -1 = unreadable.
static int CbBoneVis(uint8_t* body, int idx)
{
    if (!g_cbOffBoneVis || idx < 0 || !RangeReadable(body + g_cbOffBoneVis, 12)) return -1;
    uint8_t* data = *(uint8_t**)(body + g_cbOffBoneVis);
    int32_t num = *(int32_t*)(body + g_cbOffBoneVis + 4);
    if (!data || num <= idx || num > 512 || !RangeReadable(data, (size_t)num)) return -1;
    return data[idx];
}

// BoneVisibilityStates is NOT a reflected property in this engine build (the generated SDK
// has no such member on USkeletalMeshComponent; version 2 refused the whole module on it,
// 2026-10-07). It is found by scanning the component after a hide: the one TArray<BYTE> of
// bone-count length whose values are all 0..2 and whose entry for a bone we just hid is not 2
// (RequiredBones, SkelControlIndex and the other byte arrays hold indices, 0xFF or both).
static void CbLocateBoneVis(uint8_t* body, int hiddenIdx, int otherIdx)
{
    if (g_cbOffBoneVis || hiddenIdx < 0) return;
    int maxIdx = hiddenIdx > otherIdx ? hiddenIdx : otherIdx;
    int cands = 0, shaped = 0; uint32_t first = 0;
    for (uint32_t o = 0x100; o + 12 <= 0x500; o += 4) {
        if (!RangeReadable(body + o, 12)) break;
        uint8_t* d = *(uint8_t**)(body + o);
        int32_t num = *(int32_t*)(body + o + 4), mx = *(int32_t*)(body + o + 8);
        if (num <= maxIdx || num > 256 || mx < num || mx > 512 || !d || ((uintptr_t)d & 3) || !RangeReadable(d, (size_t)num)) continue;
        bool ok = true; int twos = 0;
        for (int b = 0; b < num; b++) { if (d[b] > 2) { ok = false; break; } if (d[b] == 2) twos++; }
        if (!ok) continue;
        // A bone-count array of 0..2 values: the shape of BoneVisibilityStates. Say what the
        // bone we just hid reads there, so "the hide did nothing" is visible even when the
        // array is found only by shape.
        shaped++;
        Log("corvobody: byte array +0x%X num %d max %d: %d of %d read 2 (visible); hidden bone %d reads %d, bone %d reads %d",
            o, num, mx, twos, num, hiddenIdx, d[hiddenIdx], otherIdx, otherIdx >= 0 ? d[otherIdx] : -1);
        if (twos < num / 2) continue;                 // mostly visible bones, a few hidden
        if (d[hiddenIdx] == 2) continue;              // the bone we just hid must read hidden here
        cands++; if (!first) first = o;
    }
    if (cands == 1) {
        g_cbOffBoneVis = first;
        Log("corvobody: BoneVisibilityStates located at +0x%X on %p by scan (bone %d reads hidden there); hides are verified from now on", first, (void*)body, hiddenIdx);
    } else {
        Log("corvobody: BoneVisibilityStates not located (%d array(s) of the right shape, %d with the hidden bone hidden); hides stay "
            "unverified and are re-issued every 2 s%s", shaped, cands,
            shaped && !cands ? ". A right-shaped array whose hidden bone still reads 2 means HideBoneByName did NOT take on this component" : "");
    }
}

static bool CbResolveOffsets()
{
    if (g_cbRefused) return false;
    if (g_cbOffEff && g_cbOffLoc && g_cbOffRot && g_cbFnSetTrans) return true;
    struct { const char* cls; const char* prop; uint32_t expect; uint32_t* out; } want[] = {
        { "SkelControlLimb",       "EffectorLocation",      0xB8,  &g_cbOffEff },
        { "SkelControlLimb",       "EffectorLocationSpace", 0xC4,  &g_cbOffEffSpace },
        { "SkelControlBase",       "StrengthTarget",        0x78,  &g_cbOffStrT },
        { "SkeletalMeshComponent", "SkeletalMesh",          0x1D4, &g_cbOffSkelMesh },
        { "Actor",                 "Location",              0xC4,  &g_cbOffLoc },
        { "Actor",                 "Rotation",              0xD0,  &g_cbOffRot },
    };
    for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++) {
        uint32_t off = FindPropOffset(want[i].cls, want[i].prop);
        if (off != want[i].expect) {
            DVR_WARN("corvobody: %s.%s resolved to 0x%X, the SDK says 0x%X - REFUSING for this session "
                     "(not the engine build the plan was read from; see docs/dishonored/CORVOBODY.md)",
                     want[i].cls, want[i].prop, off, want[i].expect);
            g_cbRefused = true; CbWhy("refused: an offset is not what the SDK says");
            return false;
        }
        *want[i].out = off;
    }
    uint32_t cs = FindPropOffset("SkelControlBase", "ControlStrength");
    uint32_t tr = FindPropOffset("PrimitiveComponent", "Translation");
    uint32_t ro = FindPropOffset("PrimitiveComponent", "Rotation");
    if (cs != kSkcStr || tr != kMeshTrans || ro != kMeshRot) {
        DVR_WARN("corvobody: ControlStrength 0x%X (patterns.h 0x%X), Translation 0x%X (0x%X), Rotation 0x%X (0x%X) - REFUSING",
                 cs, kSkcStr, tr, kMeshTrans, ro, kMeshRot);
        g_cbRefused = true; CbWhy("refused: an offset disagrees with patterns.h");
        return false;
    }
    if (!FindBoolProp("PrimitiveComponent", "HiddenGame", &g_cbOffHidden, &g_cbMaskHidden) ||
        g_cbOffHidden != 0x114 || g_cbMaskHidden != 0x4) {
        DVR_WARN("corvobody: PrimitiveComponent.HiddenGame resolved to 0x%X mask 0x%X, the SDK says 0x114 mask 0x4 - REFUSING",
                 g_cbOffHidden, g_cbMaskHidden);
        g_cbRefused = true; CbWhy("refused: HiddenGame bit disagrees with the SDK");
        return false;
    }
    g_cbOffBoneVis = FindPropOffset("SkeletalMeshComponent", "BoneVisibilityStates");   // 0 on this build: native only, located by scan later
    g_cbFnFind      = CbFindFunctionIn("SkeletalMeshComponent", "FindSkelControl");
    g_cbFnHide      = CbFindFunctionIn("SkeletalMeshComponent", "HideBoneByName");
    g_cbFnUnhide    = CbFindFunctionIn("SkeletalMeshComponent", "UnHideBoneByName");
    g_cbFnBoneLoc   = CbFindFunctionIn("SkeletalMeshComponent", "GetBoneLocation");
    g_cbFnMatchBone = CbFindFunctionIn("SkeletalMeshComponent", "MatchRefBone");
    g_cbFnSetTrans  = CbFindFunctionIn("PrimitiveComponent", "SetTranslation");
    g_cbFnSetRot    = CbFindFunctionIn("PrimitiveComponent", "SetRotation");
    if (!g_cbFnFind || !g_cbFnBoneLoc || !g_cbFnSetTrans || !g_cbFnSetRot || !g_cbFnMatchBone) {
        DVR_WARN("corvobody: FindSkelControl=%p GetBoneLocation=%p SetTranslation=%p SetRotation=%p MatchRefBone=%p "
                 "- a UFunction did not resolve, REFUSING",
                 (void*)g_cbFnFind, (void*)g_cbFnBoneLoc, (void*)g_cbFnSetTrans, (void*)g_cbFnSetRot, (void*)g_cbFnMatchBone);
        g_cbRefused = true; CbWhy("refused: a UFunction did not resolve");
        return false;
    }
    Log("corvobody: offsets resolved and cross-checked: EffectorLocation 0x%X space 0x%X StrengthTarget 0x%X SkeletalMesh 0x%X "
        "HiddenGame 0x%X/0x%X Actor.Location 0x%X Rotation 0x%X; BoneVisibilityStates %s; UFunctions FindSkelControl %p "
        "HideBoneByName %p UnHideBoneByName %p GetBoneLocation %p MatchRefBone %p SetTranslation %p SetRotation %p",
        g_cbOffEff, g_cbOffEffSpace, g_cbOffStrT, g_cbOffSkelMesh, g_cbOffHidden, g_cbMaskHidden, g_cbOffLoc, g_cbOffRot,
        g_cbOffBoneVis ? "reflected" : "not reflected in this build (will be located by scan after the first hide)",
        (void*)g_cbFnFind, (void*)g_cbFnHide, (void*)g_cbFnUnhide, (void*)g_cbFnBoneLoc, (void*)g_cbFnMatchBone,
        (void*)g_cbFnSetTrans, (void*)g_cbFnSetRot);
    return true;
}

// Is the mod next to the exe? Their proxy is loaded by the game itself (dinput8 is a
// static import), so the module handle is the proof; the export distinguishes it from
// the system DLL. Re-checked every few seconds: a user can drop the files in later.
static bool CorvoDetect(bool force)
{
    double now = MaimNowMs();
    if (!force && now < g_cbDetectMs) return g_cbPresent;
    g_cbDetectMs = now + (g_cbPresent ? 15000.0 : 4000.0);
    HMODULE h = GetModuleHandleA("dinput8.dll");
    bool exp = h && GetProcAddress(h, "BlinkBootstrap_Register") != NULL;
    char ini[MAX_PATH]; dvr::paths::in_game_dir(ini, "CorvoBody.ini");
    bool iniThere = GetFileAttributesA(ini) != INVALID_FILE_ATTRIBUTES;
    bool present = exp && iniThere;
    if (present == g_cbPresent && !force) return g_cbPresent;
    g_cbPresent = present;
    g_cbModule = present ? h : NULL;
    if (!present) {
        Log("corvobody: not present (dinput8.dll with BlinkBootstrap_Register: %s, CorvoBody.ini: %s) - the module idles; "
            "F6-F9 and the F10 overlay keep their keys", exp ? "yes" : "no", iniThere ? "yes" : "no");
        CbWhy("mod not present");
        return false;
    }
    g_cbTheirEnabled  = GetPrivateProfileIntA("CorvoBody", "Enabled", 1, ini) != 0;
    g_cbTheirRigid    = GetPrivateProfileIntA("CorvoBody", "RigidCamera", 1, ini) != 0;
    g_cbTheirHideArms = GetPrivateProfileIntA("CorvoBody", "HideBodyArms", 1, ini) != 0;
    int turnStep = GetPrivateProfileIntA("CorvoBody", "TurnStepThreshold", 1, ini);
    Log("corvobody: PRESENT (module %p). Their ini: Enabled=%d RigidCamera=%d HideBodyArms=%d TurnStepThreshold=%d. Their F6-F11 are "
        "hard-coded, so the VR mod's F7/F8/F9 debug keys are parked and the overlay opens on %s instead of F10 (the pad chord is unchanged)",
        (void*)h, (int)g_cbTheirEnabled, (int)g_cbTheirRigid, (int)g_cbTheirHideArms, turnStep,
        g_cbOverlayVk ? "[Overlay] Key" : "Insert");
    if (g_cbTheirRigid)
        DVR_WARN("corvobody: CorvoBody.ini has RigidCamera=1. Its GetPlayerViewPoint detour learns the look-up/down arc from "
                 "the camera POV cache, which in VR carries the head and eye offsets, and persists it to CorvoBodyCamera.bin; "
                 "it also doubles the [Neck] cancel. Set RigidCamera=0 in CorvoBody.ini (F6 in game reloads it).");
    if (g_cbTheirHideArms)
        DVR_WARN("corvobody: CorvoBody.ini has HideBodyArms=1: the body's shoulder vanishes while that hand holds gear. "
                 "Set HideBodyArms=0 in CorvoBody.ini; this module hides what it needs itself.");
    if (turnStep < 30)
        Log("corvobody: CorvoBody.ini TurnStepThreshold=%d deg/s: in VR the pawn yaw follows the head, so every head motion is "
            "a 'turn' and the feet shuffle. 45 or more steps only on real turns.", turnStep);
    if (!g_cbTheirEnabled) CbWhy("their ini has Enabled=0: no body to attach to");
    return true;
}

static int CbEffectiveMode()
{
    if (g_cbArmMode == 1) return 1;
    if (g_cbArmMode == 2) return 2;
    return g_ikOn.load() ? 1 : 2;
}

static void CbReleaseControls()
{
    for (int h = 0; h < 2; h++) {
        if (g_cbCtl[h] && IsLiveObject(g_cbCtl[h])) {
            *(float*)(g_cbCtl[h] + kSkcStr) = 0.0f;
            if (g_cbOffStrT) *(float*)(g_cbCtl[h] + g_cbOffStrT) = 0.0f;
        }
    }
}

static void CbUnhideAll(uint8_t* body)
{
    static const char* names[4] = { "hand_L_jnt", "hand_R_jnt", "shoulder_L_jnt", "shoulder_R_jnt" };
    for (int i = 0; i < 4; i++) CbHideBone(body, names[i], false);
}

static void CbDrop(const char* why)
{
    if (g_cbBody && IsLiveObject(g_cbBody) && *(uint32_t*)(g_cbBody + kNameOff) == g_cbBodyName) {
        if (g_cbAppliedMode) CbUnhideAll(g_cbBody);
        CbReleaseControls();
    }
    for (int h = 0; h < 2; h++) { g_cbCtl[h] = NULL; g_cbArmLen[h] = 0.0f; }
    for (int i = 0; i < 8; i++) g_cbBoneIdx[i] = -1;
    if (g_cbBody) Log("corvobody: body %p released (%s)", (void*)g_cbBody, why);
    g_cbBody = NULL; g_cbPawn = NULL; g_cbBodyName = 0; g_cbAppliedMode = 0; g_cbWidthDone = false;
    CbWhy(why);
}

// Their visible body: a SkeletalMeshComponent whose Outer is the pawn, whose mesh is
// named *skm_Corvo_Body, and which is not HiddenGame (that twin is their shadow body).
static uint8_t* CbFindBody(uint8_t* pawn)
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return NULL;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return NULL;
    uint32_t ci = FindNameIdx("SkeletalMeshComponent");
    if (ci == 0xffffffffu) return NULL;
    uint8_t* found = NULL; int visible = 0; g_cbShadowSeen = 0;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, g_cbOffSkelMesh + 4)) continue;
        if (*(uint8_t**)(o + kOuterOff) != pawn) continue;
        uint8_t* cls = *(uint8_t**)(o + kClassOff);
        if (!cls || !RangeReadable(cls, kNameOff + 4) || *(uint32_t*)(cls + kNameOff) != ci) continue;
        uint8_t* mesh = *(uint8_t**)(o + g_cbOffSkelMesh);
        if (!mesh || !LooksLikeObj(mesh)) continue;
        const char* mn = RealName(*(uint32_t*)(mesh + kNameOff));
        if (!mn || !CbContainsNoCase(mn, "skm_Corvo_Body")) continue;
        if ((*(uint32_t*)(o + g_cbOffHidden) & g_cbMaskHidden) != 0) { g_cbShadowSeen++; continue; }
        visible++; found = o;   // the newest wins, as in their own lookup
    }
    if (visible > 1) Log("corvobody: %d visible components carry the body mesh; using the newest %p", visible, (void*)found);
    return found;
}

// The hide for the current mode, read back from BoneVisibilityStates. Issued on mode change
// and re-issued when the array says a bone came back (CorvoBody UnHides the shoulders on
// every weapon draw/holster with HideBodyArms=0, which would undo ours).
static void CbApplyHides(uint8_t* body, int mode, bool force)
{
    static const char* names[4] = { "hand_L_jnt", "hand_R_jnt", "shoulder_L_jnt", "shoulder_R_jnt" };
    bool want[4] = { mode == 2 && g_cbHideHands, mode == 2 && g_cbHideHands, mode == 1, mode == 1 };
    if (!g_cbOffBoneVis && !force) {
        // Unverifiable on this build until the scan finds the array: re-issue the wanted hides
        // every 2 s, since CorvoBody's weapon-change UnHide would otherwise win silently.
        static double blindMs = 0.0; double now = MaimNowMs();
        if (now - blindMs < 2000.0) return;
        blindMs = now;
        for (int i = 0; i < 4; i++) if (want[i]) CbHideBone(body, names[i], true);
        return;
    }
    int issued = 0, re = 0;
    for (int i = 0; i < 4; i++) {
        int vis = CbBoneVis(body, g_cbBoneIdx[i]);
        if (vis < 0) {                                 // unreadable: act blind, only on a mode change
            if (!force) continue;
            CbHideBone(body, names[i], want[i]); issued++;
            continue;
        }
        bool hiddenNow = vis != 2;
        if (want[i] && !hiddenNow) { CbHideBone(body, names[i], true); issued++; if (!force) re++; }
        else if (!want[i] && hiddenNow) { CbHideBone(body, names[i], false); issued++; }
    }
    if (re) { g_cbRehides += re; if (g_cbRehides <= 5 || (g_cbRehides % 50) == 0) Log("corvobody: re-issued %d hide(s) (total %ld): their UnHide undid ours", re, g_cbRehides); }
    if (force) {
        if (!g_cbOffBoneVis) CbLocateBoneVis(body, mode == 1 ? g_cbBoneIdx[2] : g_cbBoneIdx[0], mode == 1 ? g_cbBoneIdx[3] : g_cbBoneIdx[1]);
        int v[4]; for (int i = 0; i < 4; i++) v[i] = CbBoneVis(body, g_cbBoneIdx[i]);
        Log("corvobody: mode %s applied on %p: %d hide calls; BoneVisibilityStates after (2 = visible): hand_L %d hand_R %d shoulder_L %d "
            "shoulder_R %d (bone idx %d %d %d %d; -1 = the array was not located, unverified). A hidden bone still at 2 means HideBoneByName did nothing on this component.",
            mode == 1 ? "vr (body arms hidden at the shoulder; the full-arm IK draws ours)" : "body (the body's own arms reach the VR hands)",
            (void*)body, issued, v[0], v[1], v[2], v[3], g_cbBoneIdx[0], g_cbBoneIdx[1], g_cbBoneIdx[2], g_cbBoneIdx[3]);
    }
}

static bool CbResolveBody(uint8_t* pawn, uint8_t* body)
{
    for (int h = 0; h < 2; h++) {
        const char* ctlName = h ? "hand_R_jnt" : "hand_L_jnt";
        uint8_t* c = CbFindSkelControl(body, ctlName);
        const char* cn = c ? ObjClassName(c) : NULL;
        if (!c || !cn || strcmp(cn, "SkelControlLimb")) {
            Log("corvobody: FindSkelControl('%s') on %p = %p (%s) - not a SkelControlLimb; their tree is not the one the plan "
                "read (CorvoBody %s?). Retrying in a few seconds.", ctlName, (void*)body, (void*)c, cn ? cn : "null",
                c ? "changed its control names" : "has not built its tree yet");
            g_cbCtl[0] = g_cbCtl[1] = NULL;
            return false;
        }
        g_cbCtl[h] = c;
        float ua[3], la[3], hd[3];
        if (CbBoneLocation(body, h ? "upper_arm_R_jnt" : "upper_arm_L_jnt", ua) &&
            CbBoneLocation(body, h ? "lower_arm_R_jnt" : "lower_arm_L_jnt", la) &&
            CbBoneLocation(body, ctlName, hd)) {
            float a = sqrtf((ua[0]-la[0])*(ua[0]-la[0]) + (ua[1]-la[1])*(ua[1]-la[1]) + (ua[2]-la[2])*(ua[2]-la[2]));
            float b = sqrtf((la[0]-hd[0])*(la[0]-hd[0]) + (la[1]-hd[1])*(la[1]-hd[1]) + (la[2]-hd[2])*(la[2]-hd[2]));
            g_cbArmLen[h] = (a > 1.0f && b > 1.0f && a < 200.0f && b < 200.0f) ? a + b : 0.0f;
        } else g_cbArmLen[h] = 0.0f;
    }
    static const char* boneNames[8] = { "hand_L_jnt", "hand_R_jnt", "shoulder_L_jnt", "shoulder_R_jnt", "upper_arm_L_jnt", "upper_arm_R_jnt", "lower_arm_L_jnt", "lower_arm_R_jnt" };
    for (int i = 0; i < 8; i++) { g_cbBoneIdx[i] = CbMatchBone(body, boneNames[i]); g_cbNameIdx[i] = FindNameIdx(boneNames[i]); }
    g_cbBody = body; g_cbPawn = pawn; g_cbBodyName = *(uint32_t*)(body + kNameOff);
    g_cbAppliedMode = CbEffectiveMode();
    CbApplyHides(body, g_cbAppliedMode, true);
    // The IK's "shoulder" is the arm's root joint (the deltoid), which on this rig is
    // upper_arm_X_jnt; shoulder_X_jnt is the clavicle root beside the neck (21.5 cm apart,
    // measured 2026-10-07: matching the IK width to THAT pulled the IK arms into the neck).
    float sl[3], sr[3], ul[3], ur[3]; g_cbShoulderCm = 0.0f; float clavCm = 0.0f;
    const float k = (g_skcWorldScale > 1.0f ? g_skcWorldScale : 100.0f) / 100.0f;   // uu per cm
    if (CbBoneLocation(body, "shoulder_L_jnt", sl) && CbBoneLocation(body, "shoulder_R_jnt", sr))
        clavCm = sqrtf((sl[0]-sr[0])*(sl[0]-sr[0]) + (sl[1]-sr[1])*(sl[1]-sr[1]) + (sl[2]-sr[2])*(sl[2]-sr[2])) / k;
    if (CbBoneLocation(body, "upper_arm_L_jnt", ul) && CbBoneLocation(body, "upper_arm_R_jnt", ur))
        g_cbShoulderCm = sqrtf((ul[0]-ur[0])*(ul[0]-ur[0]) + (ul[1]-ur[1])*(ul[1]-ur[1]) + (ul[2]-ur[2])*(ul[2]-ur[2])) / k;
    Log("corvobody: body %p on pawn %p (shadow twins seen: %d): hand_L_jnt %p hand_R_jnt %p, arm length L %.1f R %.1f uu, "
        "upper-arm joint spacing %.1f cm, clavicle root spacing %.1f cm (the IK's ArmShoulderWidthCm is %.1f%s). Mode %s; head anchor %s, torso yaw %s.",
        (void*)body, (void*)pawn, g_cbShadowSeen, (void*)g_cbCtl[0], (void*)g_cbCtl[1], g_cbArmLen[0], g_cbArmLen[1],
        g_cbShoulderCm, clavCm, g_ikWidth.load(), g_cbMatchWidth ? ", matched to the upper-arm spacing below" : "",
        g_cbAppliedMode == 1 ? "vr" : "body", g_cbHeadAnchor ? "on" : "off", g_cbTorsoYaw ? "IK body yaw" : "head yaw");
    if (g_cbMatchWidth && !g_cbWidthDone && g_cbShoulderCm > 25.0f && g_cbShoulderCm < 60.0f) {
        float was = g_ikWidth.load();
        if (fabsf(was - g_cbShoulderCm) > 0.3f) {
            g_ikWidth.store(g_cbShoulderCm);
            Log("corvobody: [Hands] ArmShoulderWidthCm %.1f -> %.1f (the body's upper-arm joint spacing) for this session; "
                "[CorvoBody] MatchShoulderWidth=0 keeps your fit", was, g_cbShoulderCm);
        }
        g_cbWidthDone = true;
    }
    g_cbShoulderOffOk = false; g_cbShoulderOffMs = 0.0;
    CbWhy("driving");
    return true;
}

// ---- the post-Tick chain: our jump over their jump at UGameEngine::Tick ----
static void __cdecl CorvoPostTickC();

static bool CbModuleRange(HMODULE h, uintptr_t* lo, uintptr_t* hi)
{
    if (!h || !RangeReadable((void*)h, 0x400)) return false;
    const uint8_t* b = (const uint8_t*)h;
    if (b[0] != 'M' || b[1] != 'Z') return false;
    uint32_t e = *(const uint32_t*)(b + 0x3C);
    if (e > 0x1000 || !RangeReadable((void*)(b + e), 0x100)) return false;
    const uint8_t* nt = b + e;
    if (nt[0] != 'P' || nt[1] != 'E') return false;
    uint32_t size = *(const uint32_t*)(nt + 0x50);   // OptionalHeader.SizeOfImage (PE32)
    if (size < 0x1000 || size > 0x10000000) return false;
    *lo = (uintptr_t)h; *hi = (uintptr_t)h + size;
    return true;
}

static void CbInstallPostTick()
{
    if (g_cbTickState == 2 || g_cbTickState == 3) return;
    double now = MaimNowMs();
    if (now < g_cbTickRetryMs) return;
    g_cbTickRetryMs = now + 2000.0;
    uint8_t* at = (uint8_t*)kGameEngineTick;
    if (!RangeReadable(at, 8)) { g_cbTickState = 3; Log("corvobody: UGameEngine::Tick unreadable - no post-Tick placement"); return; }
    if (at[0] == 0x55 && at[1] == 0x8B && at[2] == 0xEC && at[3] == 0x6A && at[4] == 0xFF) {
        if (g_cbTickState != 1) Log("corvobody: UGameEngine::Tick still has its own prologue - CorvoBody has not hooked it yet; waiting (our "
                                     "placement must run after theirs, so our jump goes over theirs)");
        g_cbTickState = 1;
        return;
    }
    if (at[0] != 0xE9) {
        g_cbTickState = 3;
        DVR_WARN("corvobody: UGameEngine::Tick starts %02X %02X %02X %02X %02X - neither the engine's prologue nor a jump; "
                 "no post-Tick placement this session (HeadAnchor/TorsoYaw inert; the arms still attach)", at[0], at[1], at[2], at[3], at[4]);
        return;
    }
    int32_t rel; memcpy(&rel, at + 1, 4);
    uintptr_t theirs = (uintptr_t)at + 5 + rel;
    uintptr_t lo = 0, hi = 0;
    if (!CbModuleRange(g_cbModule, &lo, &hi) || theirs < lo || theirs >= hi) {
        g_cbTickState = 3;
        DVR_WARN("corvobody: the jump at UGameEngine::Tick goes to 0x%08X, outside CorvoBody's module (0x%08X..0x%08X) - another "
                 "hook owns it; no post-Tick placement this session", theirs, lo, hi);
        return;
    }
    uint8_t* stub = (uint8_t*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { g_cbTickState = 3; Log("corvobody: stub alloc failed"); return; }
    uint8_t s[64]; int n = 0;
    // We are at the function's entry: ecx = this, [esp] = return, [esp+4] = DeltaSeconds.
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x04;        // push [esp+4]       (DeltaSeconds again)
    s[n++] = 0xE8;                                                      // call their detour (thiscall: ecx intact, it does ret 4)
    { int32_t r = (int32_t)(theirs - ((uintptr_t)stub + n + 4)); memcpy(s + n, &r, 4); n += 4; }
    s[n++] = 0x9C; s[n++] = 0x60;                                       // pushfd; pushad
    s[n++] = 0xE8;                                                      // call CorvoPostTickC (cdecl, no args)
    { int32_t r = (int32_t)((uintptr_t)&CorvoPostTickC - ((uintptr_t)stub + n + 4)); memcpy(s + n, &r, 4); n += 4; }
    s[n++] = 0x61; s[n++] = 0x9D;                                       // popad; popfd
    s[n++] = 0xC2; s[n++] = 0x04; s[n++] = 0x00;                        // ret 4 (the caller's DeltaSeconds)
    memcpy(stub, s, n);
    memcpy(g_cbTickSaved, at, 5);
    DWORD op;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) { g_cbTickState = 3; Log("corvobody: VirtualProtect failed"); return; }
    int32_t jrel = (int32_t)((uintptr_t)stub - ((uintptr_t)at + 5));
    at[0] = 0xE9; memcpy(at + 1, &jrel, 4);
    VirtualProtect(at, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    g_cbTickStub = stub; g_cbTickTheirs = theirs; g_cbTickState = 2;
    Log("corvobody: post-Tick chain installed at 0x%08X: our jump -> stub %p -> their detour 0x%08X (ret 4) -> our placement -> ret 4. "
        "The body is placed after CorvoBody's own update every tick.", (unsigned)kGameEngineTick, (void*)stub, (unsigned)theirs);
}

static inline void CbRotYaw(float yaw, const float v[3], float out[3])
{
    float c = cosf(yaw), s = sinf(yaw);
    out[0] = v[0]*c - v[1]*s; out[1] = v[0]*s + v[1]*c; out[2] = v[2];
}

// After CorvoBody placed the body under the pawn: move it so its shoulder midpoint sits on
// the arm IK's shoulder centre and it faces the IK's body yaw. All in engine world space;
// the write is a relative transform on the pawn (the body is attached to it).
static void __cdecl CorvoPostTickC()
{
    static bool inside = false;
    if (inside) return;
    inside = true;
    do {
        if (!g_cbBody || !g_cbHeadAnchor || !g_cbPawn) break;
        if (!IsLiveObject(g_cbBody) || *(uint32_t*)(g_cbBody + kNameOff) != g_cbBodyName || !IsLiveObject(g_cbPawn)) break;
        if (!CamStillValid() || !RangeReadable(g_camObj + 0x50, 0x40)) break;
        if (!RangeReadable(g_cbPawn + g_cbOffRot, 12) || !RangeReadable(g_cbPawn + g_cbOffLoc, 12)) break;
        if (!RangeReadable(g_cbBody + kMeshRot, 12) || !RangeReadable(g_cbBody + kMeshTrans, 12)) break;
        const float* cf = (const float*)(g_camObj + 0x50);
        const float* cu = (const float*)(g_camObj + 0x70);
        const float* cp = (const float*)(g_camObj + 0x80);
        float head[3] = { cp[0] - cu[0]*g_crouchDropUU, cp[1] - cu[1]*g_crouchDropUU, cp[2] - cu[2]*g_crouchDropUU };
        if (!(cf[0] == cf[0] && cf[1] == cf[1]) || (fabsf(cf[0]) + fabsf(cf[1])) < 1e-4f) break;
        float viewYaw = atan2f(cf[1], cf[0]);
        float delta = 0.0f;
        if (g_cbTorsoYaw) {
            unsigned long long ms = g_cbIkYawMs.load();
            if (ms && (GetTickCount64() - ms) < 500) delta = (float)g_flipYaw * g_cbIkYawDelta.load();
        }
        float bodyYaw = viewYaw + delta;
        float k = (g_skcWorldScale > 1.0f ? g_skcWorldScale : 100.0f) / 100.0f;   // uu per cm
        float F[3] = { cosf(bodyYaw), sinf(bodyYaw), 0.0f }, R[3] = { -sinf(bodyYaw), cosf(bodyYaw), 0.0f };
        float fwdCm = g_ikForward.load() + g_cbBodyFwdCm, rightCm = g_ikRight.load() + g_cbBodyRightCm, upCm = g_ikUp.load() + g_cbBodyUpCm;
        float C[3];
        for (int i = 0; i < 3; i++) C[i] = head[i] + k * (fwdCm * F[i] + rightCm * R[i]) + (i == 2 ? k * upCm : 0.0f);
        float sl[3], sr[3];
        if (!CbBoneLocation(g_cbBody, "upper_arm_L_jnt", sl) || !CbBoneLocation(g_cbBody, "upper_arm_R_jnt", sr)) break;
        float S0[3] = { (sl[0]+sr[0])*0.5f, (sl[1]+sr[1])*0.5f, (sl[2]+sr[2])*0.5f };
        const float* pawnLoc = (const float*)(g_cbPawn + g_cbOffLoc);
        const int32_t* pawnRot = (const int32_t*)(g_cbPawn + g_cbOffRot);
        float pawnYaw = (float)pawnRot[1] / kUEPerRad;
        float T0[3]; memcpy(T0, g_cbBody + kMeshTrans, 12);   // a copy: the write below lands in the same field
        const int32_t* R0 = (const int32_t*)(g_cbBody + kMeshRot);
        float yaw0 = pawnYaw + (float)R0[1] / kUEPerRad;
        float P0[3], tmp[3];
        CbRotYaw(pawnYaw, T0, tmp);
        for (int i = 0; i < 3; i++) P0[i] = pawnLoc[i] + tmp[i];
        float d[3] = { S0[0]-P0[0], S0[1]-P0[1], S0[2]-P0[2] }, o[3];
        CbRotYaw(-yaw0, d, o);                       // the shoulder midpoint in the body's own yaw frame (pose of this tick)
        float ro[3]; CbRotYaw(bodyYaw, o, ro);
        float P1[3] = { C[0]-ro[0], C[1]-ro[1], g_cbAnchorZ ? C[2]-ro[2] : P0[2] };
        float rel[3] = { P1[0]-pawnLoc[0], P1[1]-pawnLoc[1], P1[2]-pawnLoc[2] }, T1[3];
        CbRotYaw(-pawnYaw, rel, T1);
        for (int i = 0; i < 3; i++) if (!(T1[i] == T1[i]) || fabsf(T1[i]) > 400.0f) { T1[0] = T0[0]; T1[1] = T0[1]; T1[2] = T0[2]; break; }
        float relYaw = bodyYaw - pawnYaw;
        while (relYaw > 3.14159265f) relYaw -= 6.2831853f;
        while (relYaw < -3.14159265f) relYaw += 6.2831853f;
        int32_t yawU = (int32_t)(relYaw * kUEPerRad);
        CbSetTranslation(g_cbBody, T1);
        CbSetRotation(g_cbBody, R0[0], yawU, R0[2]);
        g_cbPlacements++;
        for (int i = 0; i < 3; i++) { g_cbLastC[i] = C[i]; g_cbLastS[i] = S0[i]; }
        g_cbLastYawDeg = bodyYaw * 57.2958f; g_cbLastDeltaDeg = delta * 57.2958f;
        double now = MaimNowMs();
        if (g_cbPlacements == 1 || now - g_cbPlaceLogMs > 5000.0) {
            g_cbPlaceLogMs = now;
            Log("corvobody/place #%ld: head (%.0f %.0f %.0f) view yaw %.0f body yaw %.0f (IK delta %+.1f deg, %s) -> shoulder centre C (%.0f %.0f %.0f); "
                "their shoulder mid was (%.0f %.0f %.0f), off by (%.0f %.0f %.0f) uu; wrote rel T (%.1f %.1f %.1f) from (%.1f %.1f %.1f), rel yaw %.0f deg",
                g_cbPlacements, head[0], head[1], head[2], viewYaw * 57.2958f, bodyYaw * 57.2958f, delta * 57.2958f,
                g_cbIkYawMs.load() ? "fresh" : "no IK publication, head yaw used", C[0], C[1], C[2], S0[0], S0[1], S0[2],
                C[0]-S0[0], C[1]-S0[1], C[2]-S0[2], T1[0], T1[1], T1[2], T0[0], T0[1], T0[2], relYaw * 57.2958f);
        }
    } while (0);
    inside = false;
}

// ---- the placement, carried by CorvoBody's own SetTranslation / SetRotation call ----
// CorvoBody re-places the body every tick through the natives (ProcessEvent). Our hook sees
// that call before the engine does and REWRITES ITS PARAMETERS, so the body gets exactly one
// transform update per tick, ours. (Version 2 chained a second jump after their Tick detour
// and wrote again: two updates per tick, and the two eye draws of the re-entry method picked
// up different ones - the body and a ghost of it circling each other, 2026-10-07.)
//
// The target: the midpoint of the body's upper-arm joints on the arm IK's shoulder centre
// (head minus the crouch drop, plus the IK fit in the body-yaw frame), the body facing the
// IK's body yaw. The joint midpoint's offset from the component origin, in the body's own
// yaw frame, is read on the script lane every 100 ms (two GetBoneLocation reads); it moves
// only with the spine's pose.
static void CbRefreshShoulderOffset(double now)
{
    if (!g_cbBody || now - g_cbShoulderOffMs < 100.0) return;
    g_cbShoulderOffMs = now;
    if (!RangeReadable(g_cbPawn + g_cbOffRot, 12) || !RangeReadable(g_cbPawn + g_cbOffLoc, 12) ||
        !RangeReadable(g_cbBody + kMeshRot, 12) || !RangeReadable(g_cbBody + kMeshTrans, 12)) { g_cbShoulderOffOk = false; return; }
    float ul[3], ur[3];
    if (!CbBoneLocation(g_cbBody, "upper_arm_L_jnt", ul) || !CbBoneLocation(g_cbBody, "upper_arm_R_jnt", ur)) { g_cbShoulderOffOk = false; return; }
    float S0[3] = { (ul[0]+ur[0])*0.5f, (ul[1]+ur[1])*0.5f, (ul[2]+ur[2])*0.5f };
    const float* pawnLoc = (const float*)(g_cbPawn + g_cbOffLoc);
    const int32_t* pawnRot = (const int32_t*)(g_cbPawn + g_cbOffRot);
    const float* T0 = (const float*)(g_cbBody + kMeshTrans);
    const int32_t* R0 = (const int32_t*)(g_cbBody + kMeshRot);
    float pawnYaw = (float)pawnRot[1] / kUEPerRad, yaw0 = pawnYaw + (float)R0[1] / kUEPerRad;
    float tmp[3]; CbRotYaw(pawnYaw, T0, tmp);
    float P0[3] = { pawnLoc[0] + tmp[0], pawnLoc[1] + tmp[1], pawnLoc[2] + tmp[2] };
    float d[3] = { S0[0]-P0[0], S0[1]-P0[1], S0[2]-P0[2] };
    CbRotYaw(-yaw0, d, g_cbShoulderOff);
    g_cbShoulderOffOk = fabsf(g_cbShoulderOff[0]) < 200.0f && fabsf(g_cbShoulderOff[1]) < 200.0f && fabsf(g_cbShoulderOff[2]) < 300.0f;
    for (int i = 0; i < 3; i++) g_cbLastS[i] = S0[i];
}

static void CorvoRewriteParms(void* fn, void* parms)
{
    if (!g_cbHeadAnchor || !g_cbShoulderOffOk || !parms || !g_cbBody || !g_cbPawn) return;
    if (fn != g_cbFnSetTrans && fn != g_cbFnSetRot) return;
    if (!CamStillValid() || !RangeReadable(g_camObj + 0x50, 0x40)) return;
    if (!RangeReadable(g_cbPawn + g_cbOffRot, 12) || !RangeReadable(g_cbPawn + g_cbOffLoc, 12)) return;
    const float* cf = (const float*)(g_camObj + 0x50);
    const float* cu = (const float*)(g_camObj + 0x70);
    const float* cp = (const float*)(g_camObj + 0x80);
    if (!(cf[0] == cf[0] && cf[1] == cf[1]) || (fabsf(cf[0]) + fabsf(cf[1])) < 1e-4f) return;
    float head[3] = { cp[0] - cu[0]*g_crouchDropUU, cp[1] - cu[1]*g_crouchDropUU, cp[2] - cu[2]*g_crouchDropUU };
    float viewYaw = atan2f(cf[1], cf[0]), delta = 0.0f;
    if (g_cbTorsoYaw) {
        unsigned long long ms = g_cbIkYawMs.load();
        if (ms && (GetTickCount64() - ms) < 500) delta = (float)g_flipYaw * g_cbIkYawDelta.load();
    }
    float bodyYaw = viewYaw + delta;
    const float* pawnLoc = (const float*)(g_cbPawn + g_cbOffLoc);
    const int32_t* pawnRot = (const int32_t*)(g_cbPawn + g_cbOffRot);
    float pawnYaw = (float)pawnRot[1] / kUEPerRad;
    float relYaw = bodyYaw - pawnYaw;
    while (relYaw > 3.14159265f) relYaw -= 6.2831853f;
    while (relYaw < -3.14159265f) relYaw += 6.2831853f;
    if (fn == g_cbFnSetRot) {
        int32_t* r = (int32_t*)parms;
        r[0] = 0; r[1] = (int32_t)(relYaw * kUEPerRad); r[2] = 0;   // upright: their copy of the 1P mesh rotation carried whatever pitch it had
        return;
    }
    float* t = (float*)parms;
    float theirs[3] = { t[0], t[1], t[2] };
    float k = (g_skcWorldScale > 1.0f ? g_skcWorldScale : 100.0f) / 100.0f;
    float F[3] = { cosf(bodyYaw), sinf(bodyYaw), 0.0f }, R[3] = { -sinf(bodyYaw), cosf(bodyYaw), 0.0f };
    float fwdCm = g_ikForward.load() + g_cbBodyFwdCm, rightCm = g_ikRight.load() + g_cbBodyRightCm, upCm = g_ikUp.load() + g_cbBodyUpCm;
    float C[3];
    for (int i = 0; i < 3; i++) C[i] = head[i] + k * (fwdCm * F[i] + rightCm * R[i]) + (i == 2 ? k * upCm : 0.0f);
    float ro[3]; CbRotYaw(bodyYaw, g_cbShoulderOff, ro);
    float P1[3] = { C[0]-ro[0], C[1]-ro[1], C[2]-ro[2] };
    float rel[3] = { P1[0]-pawnLoc[0], P1[1]-pawnLoc[1], P1[2]-pawnLoc[2] }, T1[3];
    CbRotYaw(-pawnYaw, rel, T1);
    if (!g_cbAnchorZ) T1[2] = theirs[2];
    for (int i = 0; i < 3; i++) if (!(T1[i] == T1[i]) || fabsf(T1[i]) > 400.0f) return;   // leave theirs
    t[0] = T1[0]; t[1] = T1[1]; t[2] = T1[2];
    g_cbRewrites++;
    for (int i = 0; i < 3; i++) g_cbLastC[i] = C[i];
    g_cbLastYawDeg = bodyYaw * 57.2958f; g_cbLastDeltaDeg = delta * 57.2958f;
    double now = MaimNowMs();
    if (g_cbRewrites == 1 || now - g_cbRewriteLogMs > 5000.0) {
        g_cbRewriteLogMs = now;
        Log("corvobody/place #%ld (their SetTranslation rewritten): head (%.0f %.0f %.0f) view yaw %.0f body yaw %.0f (IK delta %+.1f, %s) -> "
            "shoulder centre C (%.0f %.0f %.0f); joint mid was (%.0f %.0f %.0f); rel T theirs (%.1f %.1f %.1f) -> ours (%.1f %.1f %.1f), rel yaw %.0f deg",
            g_cbRewrites, head[0], head[1], head[2], viewYaw * 57.2958f, bodyYaw * 57.2958f, delta * 57.2958f,
            g_cbIkYawMs.load() ? "fresh" : "no IK publication, head yaw", C[0], C[1], C[2], g_cbLastS[0], g_cbLastS[1], g_cbLastS[2],
            theirs[0], theirs[1], theirs[2], T1[0], T1[1], T1[2], relYaw * 57.2958f);
    }
}

// Called by skelcontrol.cpp at the hand drive's write: the hand point as a WORLD point.
static void CorvoHandSample(int hand, const float v[3], uint8_t space)
{
    if (hand < 0 || hand > 1) return;
    g_cbHandTgt[hand][0] = v[0]; g_cbHandTgt[hand][1] = v[1]; g_cbHandTgt[hand][2] = v[2];
    g_cbHandSpace[hand] = space;
    g_cbHandMs[hand] = MaimNowMs();
}

static void CorvoTick()
{
    if (!CorvoDetect(false)) { if (g_cbBody) CbDrop("mod no longer present"); return; }
    if (!g_cbTheirEnabled) return;
    if (!g_cbEnabled) { if (g_cbBody) CbDrop("disabled ([CorvoBody] Enabled=0)"); return; }
    if (!CbResolveOffsets()) return;
    uint8_t* pawn = g_pePawn;
    double now = MaimNowMs();
    if (g_cbBody) {
        if (pawn != g_cbPawn) { CbDrop("pawn changed"); }
        else if (!IsLiveObject(g_cbBody) || *(uint32_t*)(g_cbBody + kNameOff) != g_cbBodyName ||
                 !IsLiveObject(g_cbCtl[0]) || !IsLiveObject(g_cbCtl[1])) {
            g_cbCtl[0] = g_cbCtl[1] = NULL; g_cbBody = NULL; g_cbAppliedMode = 0;
            CbDrop("their component or a control was freed (a level change rebuilds it)");
        }
    }
    if (!g_cbBody) {
        if (!pawn || !IsLiveObject(pawn)) { CbWhy("no live pawn yet"); return; }
        if (now < g_cbScanMs) return;
        g_cbScanMs = now + 2500.0;   // a GObjects walk is paid on the game thread; CorvoBody takes seconds to build the body anyway
        uint8_t* body = CbFindBody(pawn);
        if (!body) { CbWhy(g_cbShadowSeen ? "only their hidden shadow body found" : "their body component not found yet"); return; }
        if (!CbResolveBody(pawn, body)) return;
    }
    if (g_cbHeadAnchor) CbRefreshShoulderOffset(now);
    if (g_cbHeadAnchor && g_cbChain) CbInstallPostTick();   // the retired route, [CorvoBody] PostTickChain=1 only
    // Mode: vr (shoulders hidden, the IK arms are ours) or body (their arm IK reaches our hands).
    int mode = CbEffectiveMode();
    if (mode != g_cbAppliedMode) {
        CbReleaseControls();
        g_cbAppliedMode = mode;
        CbApplyHides(g_cbBody, mode, true);
    } else {
        static double visMs = 0.0;
        if (now - visMs > 250.0) { visMs = now; CbApplyHides(g_cbBody, mode, false); }
    }
    // The drive, in BOTH modes: in body mode the body's arm reaches the hand; in vr mode the
    // arm geometry past the deltoid is cut at draw time (body_cut.cpp) and the stub that stays
    // follows the real arm's direction instead of the holstered swing. The point is this
    // dispatch's (ApplyHandToMesh ran just before); a stale or non-world point releases the arm.
    for (int h = 0; h < 2; h++) {
        uint8_t* c = g_cbCtl[h];
        if (!c || !RangeReadable(c, g_cbOffEffSpace + 1)) continue;
        bool fresh = g_cbHandMs[h] > 0.0 && (now - g_cbHandMs[h]) < 500.0;
        if (!fresh || g_cbHandSpace[h] != 0 || dvr::anim::hand_owned(h)) {
            *(float*)(c + kSkcStr) = 0.0f; *(float*)(c + g_cbOffStrT) = 0.0f;
            if (fresh && g_cbHandSpace[h] != 0 && now - g_cbLogMs > 5000.0) {
                g_cbLogMs = now;
                Log("corvobody: no world hand point this dispatch (drive space %u, camera not valid?); arms released", (unsigned)g_cbHandSpace[h]);
            }
            continue;
        }
        float t[3] = { g_cbHandTgt[h][0], g_cbHandTgt[h][1], g_cbHandTgt[h][2] };
        if (g_cbReach > 0.0f && g_cbArmLen[h] > 0.0f) {
            float sh[3];
            if (CbBoneLocation(g_cbBody, h ? "upper_arm_R_jnt" : "upper_arm_L_jnt", sh)) {
                float d[3] = { t[0]-sh[0], t[1]-sh[1], t[2]-sh[2] };
                float len = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
                float lim = g_cbArmLen[h] * g_cbReach;
                if (len > lim && len > 0.001f) {
                    float kk = lim / len;
                    t[0] = sh[0] + d[0]*kk; t[1] = sh[1] + d[1]*kk; t[2] = sh[2] + d[2]*kk;
                    g_cbClamped++;
                    if (now - g_cbLogMs > 3000.0) {
                        g_cbLogMs = now;
                        Log("corvobody: %s hand %.0f uu from the shoulder, arm reaches %.0f: effector shortened (the 1P hand stays "
                            "at the controller; the gap is the deficit)", h ? "right" : "left", len, lim);
                    }
                }
            }
        }
        float* eff = (float*)(c + g_cbOffEff);
        eff[0] = t[0]; eff[1] = t[1]; eff[2] = t[2];
        *(uint8_t*)(c + g_cbOffEffSpace) = 0;   // BCS_WorldSpace
        *(float*)(c + kSkcStr) = g_cbStrength;
        *(float*)(c + g_cbOffStrT) = g_cbStrength;
        g_cbWrites++;
        if (g_cbWrites == 1 || (g_cbWrites % 2000) == 0)
            Log("corvobody: effector write #%ld %s (%.0f %.0f %.0f) strength %.2f", g_cbWrites, h ? "R" : "L", t[0], t[1], t[2], g_cbStrength);
    }
}

static void CorvoSet(bool on, const char* who)
{
    g_cbEnabled = on;
    Log("corvobody: %s (%s)", on ? "ON" : "OFF - CorvoBody's body as the mod ships it", who);
    if (!on && g_cbBody) CbDrop("switched off");
}

static void CorvoHideHandsSet(bool on, const char* who)
{
    g_cbHideHands = on;
    Log("corvobody: body-mode hand hide %s (%s)", on ? "on" : "off", who);
    if (g_cbBody && IsLiveObject(g_cbBody) && g_cbAppliedMode == 2) CbApplyHides(g_cbBody, 2, true);
}

static void CorvoArmModeSet(int mode, const char* who)
{
    g_cbArmMode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
    Log("corvobody: ArmMode=%s (%s)", g_cbArmMode == 0 ? "auto" : g_cbArmMode == 1 ? "vr" : "body", who);
}

static int CbParseKey(const char* s)
{
    if (!s || !*s) return 0;
    if ((s[0] == 'F' || s[0] == 'f') && s[1] >= '1' && s[1] <= '9') {
        int n = atoi(s + 1);
        if (n >= 1 && n <= 24) return VK_F1 + (n - 1);
    }
    struct { const char* n; int vk; } tbl[] = {
        { "Insert", VK_INSERT }, { "Delete", VK_DELETE }, { "Home", VK_HOME }, { "End", VK_END },
        { "Pause", VK_PAUSE }, { "ScrollLock", VK_SCROLL }, { "PageUp", VK_PRIOR }, { "PageDown", VK_NEXT },
        { "Backspace", VK_BACK }, { "Tab", VK_TAB },
    };
    for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
        if (_stricmp(tbl[i].n, s) == 0) return tbl[i].vk;
    return 0;
}

// The overlay's key: [Overlay] Key when set; otherwise F10, or Insert while CorvoBody is
// present (its F10 flips its attach mode; its keys are hard-coded).
static int CorvoOverlayVk()
{
    if (g_cbOverlayVk) return g_cbOverlayVk;
    return g_cbPresent ? VK_INSERT : VK_F10;
}

// Our F6-F9 debug toggles share CorvoBody's keys; they park while it is present.
static bool CorvoParksDebugKeys() { return g_cbPresent; }

static void CorvoConfigure(const char* ini)
{
    g_cbEnabled   = IniFloat(ini, "CorvoBody", "Enabled", 1) != 0.0f;
    char mode[16] = ""; GetPrivateProfileStringA("CorvoBody", "ArmMode", "auto", mode, sizeof(mode), ini);
    g_cbArmMode   = !_stricmp(mode, "vr") ? 1 : !_stricmp(mode, "body") ? 2 : 0;
    g_cbHideHands = IniFloat(ini, "CorvoBody", "HideBodyHands", 1) != 0.0f;
    g_cbStrength  = IniFloat(ini, "CorvoBody", "ArmStrength", 1.0f);
    if (g_cbStrength < 0.0f) g_cbStrength = 0.0f;
    if (g_cbStrength > 1.0f) g_cbStrength = 1.0f;
    g_cbReach     = IniFloat(ini, "CorvoBody", "ReachClamp", 0.98f);
    if (g_cbReach < 0.0f) g_cbReach = 0.0f;
    if (g_cbReach > 1.2f) g_cbReach = 1.2f;
    g_cbHeadAnchor = IniFloat(ini, "CorvoBody", "HeadAnchor", 1) != 0.0f;
    g_cbAnchorZ    = IniFloat(ini, "CorvoBody", "AnchorZ", 1) != 0.0f;
    g_bcOn         = IniFloat(ini, "CorvoBody", "ArmCut", 1) != 0.0f;
    g_bcRadius     = IniFloat(ini, "CorvoBody", "ArmCutRadiusUu", 12.0f);
    g_bcStartUu    = IniFloat(ini, "CorvoBody", "ArmCutStartUu", 11.0f);
    g_bcMinArm     = (int)IniFloat(ini, "CorvoBody", "ArmCutMinVerts", 2);
    if (g_bcRadius < 4.0f) g_bcRadius = 4.0f; if (g_bcRadius > 30.0f) g_bcRadius = 30.0f;
    if (g_bcStartUu < 0.0f) g_bcStartUu = 0.0f; if (g_bcStartUu > 40.0f) g_bcStartUu = 40.0f;
    if (g_bcMinArm < 1) g_bcMinArm = 1; if (g_bcMinArm > 3) g_bcMinArm = 3;
    g_cbTorsoYaw   = IniFloat(ini, "CorvoBody", "TorsoYaw", 1) != 0.0f;
    g_cbBodyFwdCm  = IniFloat(ini, "CorvoBody", "BodyForwardCm", 0.0f);
    g_cbBodyRightCm = IniFloat(ini, "CorvoBody", "BodyRightCm", 0.0f);
    g_cbBodyUpCm   = IniFloat(ini, "CorvoBody", "BodyUpCm", 0.0f);
    for (float* p : { &g_cbBodyFwdCm, &g_cbBodyRightCm, &g_cbBodyUpCm }) { if (*p < -60.0f) *p = -60.0f; if (*p > 60.0f) *p = 60.0f; }
    g_cbMatchWidth = IniFloat(ini, "CorvoBody", "MatchShoulderWidth", 1) != 0.0f;
    g_cbChain      = IniFloat(ini, "CorvoBody", "PostTickChain", 0) != 0.0f;
    if (g_cbChain) Log("config: [CorvoBody] PostTickChain=1 - the retired placement route (a second transform write per tick; the eyes disagreed on 2026-10-07). On by request only.");
    char key[32] = ""; GetPrivateProfileStringA("Overlay", "Key", "", key, sizeof(key), ini);
    g_cbOverlayVk = CbParseKey(key);
    if (key[0] && !g_cbOverlayVk) DVR_WARN("config: [Overlay] Key='%s' is not a key name this build knows (F1..F24, Insert, Delete, Home, End, Pause, ScrollLock, PageUp, PageDown, Backspace, Tab) - default used", key);
    Log("config: [CorvoBody] ArmCut=%d radius %.0f start %.0f minVerts %d", (int)g_bcOn, g_bcRadius, g_bcStartUu, g_bcMinArm);
    Log("config: [CorvoBody] Enabled=%d ArmMode=%s HeadAnchor=%d AnchorZ=%d TorsoYaw=%d Body fwd/right/up cm %.1f/%.1f/%.1f MatchShoulderWidth=%d "
        "HideBodyHands=%d ArmStrength=%.2f ReachClamp=%.2f; [Overlay] Key=%s - acts only when the CorvoBody mod (Nexus 453) is next to the exe",
        (int)g_cbEnabled, g_cbArmMode == 0 ? "auto" : g_cbArmMode == 1 ? "vr" : "body", (int)g_cbHeadAnchor, (int)g_cbAnchorZ, (int)g_cbTorsoYaw,
        g_cbBodyFwdCm, g_cbBodyRightCm, g_cbBodyUpCm, (int)g_cbMatchWidth, (int)g_cbHideHands, g_cbStrength, g_cbReach,
        key[0] ? key : "(default: F10, Insert while CorvoBody is present)");
    g_cbDetectMs = 0.0;
    CorvoDetect(true);
}

static void CorvoStatus(dvr::status::Writer& w)
{
    w.kv("present", g_cbPresent);
    w.kv("enabled", g_cbEnabled);
    w.kv("mode", g_cbAppliedMode == 1 ? "vr" : g_cbAppliedMode == 2 ? "body" : "none");
    w.kv("theirRigidCamera", g_cbTheirRigid);
    w.kv("theirHideBodyArms", g_cbTheirHideArms);
    w.kv("body", g_cbBody != NULL);
    w.kv("postTick", g_cbTickState);
    w.kv("placements", (unsigned long)g_cbPlacements);
    w.kv("rewrites", (unsigned long)g_cbRewrites);
    w.kv("jointOffOk", g_cbShoulderOffOk);
    w.kv("bodyYawDeg", (double)g_cbLastYawDeg);
    w.kv("ikDeltaDeg", (double)g_cbLastDeltaDeg);
    w.kv("shoulderCm", (double)g_cbShoulderCm);
    w.kv("armLenL", (double)g_cbArmLen[0]);
    w.kv("armLenR", (double)g_cbArmLen[1]);
    w.kv("writes", (unsigned long)g_cbWrites);
    w.kv("clamped", (unsigned long)g_cbClamped);
    w.kv("rehides", (unsigned long)g_cbRehides);
    w.kv("why", g_cbWhy);
}

static bool CorvoCommand(const char* args)
{
    if (!args || !*args || !strcmp(args, "status")) {
        int v[4] = { -1, -1, -1, -1 };
        if (g_cbBody && IsLiveObject(g_cbBody)) for (int i = 0; i < 4; i++) v[i] = CbBoneVis(g_cbBody, g_cbBoneIdx[i]);
        Log("corvobody: present=%d enabled=%d mode=%s their(Enabled=%d RigidCamera=%d HideBodyArms=%d) body=%p ctl L=%p R=%p armLen L=%.1f R=%.1f "
            "shoulderCm=%.1f postTick=%d placements=%ld bodyYaw=%.0f ikDelta=%+.1f C=(%.0f %.0f %.0f) S=(%.0f %.0f %.0f) writes=%ld clamped=%ld "
            "rehides=%ld vis(handL handR shL shR)=%d %d %d %d why='%s'",
            (int)g_cbPresent, (int)g_cbEnabled, g_cbAppliedMode == 1 ? "vr" : g_cbAppliedMode == 2 ? "body" : "none",
            (int)g_cbTheirEnabled, (int)g_cbTheirRigid, (int)g_cbTheirHideArms, (void*)g_cbBody, (void*)g_cbCtl[0], (void*)g_cbCtl[1],
            g_cbArmLen[0], g_cbArmLen[1], g_cbShoulderCm, g_cbTickState, g_cbPlacements, g_cbLastYawDeg, g_cbLastDeltaDeg,
            g_cbLastC[0], g_cbLastC[1], g_cbLastC[2], g_cbLastS[0], g_cbLastS[1], g_cbLastS[2], g_cbWrites, g_cbClamped, g_cbRehides,
            v[0], v[1], v[2], v[3], g_cbWhy);
        return true;
    }
    if (!strcmp(args, "on"))  { CorvoSet(true, "seam");  return true; }
    if (!strcmp(args, "off")) { CorvoSet(false, "seam"); return true; }
    if (!strcmp(args, "rescan")) { if (g_cbBody) CbDrop("rescan asked"); g_cbScanMs = 0.0; g_cbDetectMs = 0.0; return true; }
    if (!strcmp(args, "hands on"))  { CorvoHideHandsSet(false, "seam"); return true; }
    if (!strcmp(args, "hands off")) { CorvoHideHandsSet(true, "seam");  return true; }
    if (!strncmp(args, "mode ", 5)) { CorvoArmModeSet(!strcmp(args + 5, "vr") ? 1 : !strcmp(args + 5, "body") ? 2 : 0, "seam"); return true; }
    if (!strcmp(args, "anchor on"))  { g_cbHeadAnchor = true;  Log("corvobody: HeadAnchor on (seam)"); return true; }
    if (!strcmp(args, "anchor off")) { g_cbHeadAnchor = false; Log("corvobody: HeadAnchor off (seam) - CorvoBody's own placement"); return true; }
    if (!strcmp(args, "torso on"))   { g_cbTorsoYaw = true;  Log("corvobody: TorsoYaw on (seam)"); return true; }
    if (!strcmp(args, "torso off"))  { g_cbTorsoYaw = false; Log("corvobody: TorsoYaw off (seam) - the body faces the head"); return true; }
    if (!strncmp(args, "body ", 5)) {
        float f = 0, r = 0, u = 0;
        if (sscanf_s(args + 5, "%f %f %f", &f, &r, &u) >= 1) {
            g_cbBodyFwdCm = f; g_cbBodyRightCm = r; g_cbBodyUpCm = u;
            Log("corvobody: body trim fwd/right/up = %.1f/%.1f/%.1f cm (seam)", f, r, u);
        }
        return true;
    }
    Log("corvobody: usage - corvobody on|off|status|rescan|mode auto|vr|body|anchor on|off|torso on|off|body <fwdCm> <rightCm> <upCm>|hands on|off");
    return true;
}
