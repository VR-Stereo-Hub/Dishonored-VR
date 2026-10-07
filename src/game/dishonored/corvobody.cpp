// game/dishonored/corvobody.cpp - the CorvoBody first-person body mod (Nexus 453) meets the
// VR hands at the shoulder. docs/dishonored/CORVOBODY.md is the record and the plan.
//
// CorvoBody is a dinput8 proxy that builds a second SkeletalMeshComponent on the pawn
// (Corvo's DLC body) animated by an AnimTree it constructs at runtime. That tree carries a
// SkelControlLimb per arm NAMED after the hand bone (hand_L_jnt, hand_R_jnt), left at
// strength 0 on the visible body (only its hidden shadow body drives them). This module:
//
//   1. detects the mod (their dinput8.dll exports BlinkBootstrap_Register, their ini is
//      next to the exe) and reads the two settings of theirs that collide with VR;
//   2. finds their VISIBLE body component (outer = the pawn, mesh name *skm_Corvo_Body,
//      not HiddenGame; the HiddenGame twin is their shadow body);
//   3. resolves the two controls through the engine's own FindSkelControl, hides the
//      body's hand bones (our cut hands show at the wrist) and measures each arm's
//      segment lengths once;
//   4. every dispatch writes effector = the identical world point the SkelControl hand
//      drive just handed the first-person hand bone (one point, one ray), strength 1,
//      clamped to the arm's reach so the wrist seam never opens.
//
// Nothing of CorvoBody's is modified: no file, no code, no import. Their ini is READ.
//
// LANE: the script lane (ProcessEvent), right after ApplyHandToMesh so the point is this
// dispatch's. Their component and controls are revalidated with IsLiveObject on every
// use and dropped on any pawn change or on their own `world changed` rebuild.
// Every refusal names why, with the values, in g_cbWhy (status.json, `corvobody status`).

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

// Names first, SDK numbers as the cross-check (docs/SDK_WORKFLOW.md). A mismatch means
// the engine is not the build the plan was read from: refuse for the session, loudly.
static bool CbResolveOffsets()
{
    if (g_cbRefused) return false;
    if (g_cbOffEff && g_cbOffEffSpace && g_cbOffStrT && g_cbOffSkelMesh && g_cbOffHidden) return true;
    struct { const char* cls; const char* prop; uint32_t expect; uint32_t* out; } want[] = {
        { "SkelControlLimb",       "EffectorLocation",      0xB8,  &g_cbOffEff },
        { "SkelControlLimb",       "EffectorLocationSpace", 0xC4,  &g_cbOffEffSpace },
        { "SkelControlBase",       "StrengthTarget",        0x78,  &g_cbOffStrT },
        { "SkeletalMeshComponent", "SkeletalMesh",          0x1D4, &g_cbOffSkelMesh },
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
    if (cs != kSkcStr) {
        DVR_WARN("corvobody: SkelControlBase.ControlStrength resolved to 0x%X, patterns.h kSkcStr is 0x%X - REFUSING", cs, kSkcStr);
        g_cbRefused = true; CbWhy("refused: ControlStrength offset disagrees with patterns.h");
        return false;
    }
    if (!FindBoolProp("PrimitiveComponent", "HiddenGame", &g_cbOffHidden, &g_cbMaskHidden) ||
        g_cbOffHidden != 0x114 || g_cbMaskHidden != 0x4) {
        DVR_WARN("corvobody: PrimitiveComponent.HiddenGame resolved to 0x%X mask 0x%X, the SDK says 0x114 mask 0x4 - REFUSING",
                 g_cbOffHidden, g_cbMaskHidden);
        g_cbRefused = true; CbWhy("refused: HiddenGame bit disagrees with the SDK");
        return false;
    }
    g_cbFnFind    = CbFindFunctionIn("SkeletalMeshComponent", "FindSkelControl");
    g_cbFnHide    = CbFindFunctionIn("SkeletalMeshComponent", "HideBoneByName");
    g_cbFnUnhide  = CbFindFunctionIn("SkeletalMeshComponent", "UnHideBoneByName");
    g_cbFnBoneLoc = CbFindFunctionIn("SkeletalMeshComponent", "GetBoneLocation");
    if (!g_cbFnFind || !g_cbFnBoneLoc) {
        DVR_WARN("corvobody: SkeletalMeshComponent.FindSkelControl=%p GetBoneLocation=%p - a UFunction is missing, REFUSING",
                 (void*)g_cbFnFind, (void*)g_cbFnBoneLoc);
        g_cbRefused = true; CbWhy("refused: a SkeletalMeshComponent UFunction did not resolve");
        return false;
    }
    Log("corvobody: offsets resolved and cross-checked against the SDK: EffectorLocation 0x%X space 0x%X StrengthTarget 0x%X "
        "SkeletalMesh 0x%X HiddenGame 0x%X/0x%X; UFunctions FindSkelControl %p HideBoneByName %p UnHideBoneByName %p GetBoneLocation %p",
        g_cbOffEff, g_cbOffEffSpace, g_cbOffStrT, g_cbOffSkelMesh, g_cbOffHidden, g_cbMaskHidden,
        (void*)g_cbFnFind, (void*)g_cbFnHide, (void*)g_cbFnUnhide, (void*)g_cbFnBoneLoc);
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
    if (!present) {
        Log("corvobody: not present (dinput8.dll with BlinkBootstrap_Register: %s, CorvoBody.ini: %s) - the module idles; "
            "F6-F9 and the F10 overlay keep their keys", exp ? "yes" : "no", iniThere ? "yes" : "no");
        CbWhy("mod not present");
        return false;
    }
    g_cbTheirEnabled  = GetPrivateProfileIntA("CorvoBody", "Enabled", 1, ini) != 0;
    g_cbTheirRigid    = GetPrivateProfileIntA("CorvoBody", "RigidCamera", 1, ini) != 0;
    g_cbTheirHideArms = GetPrivateProfileIntA("CorvoBody", "HideBodyArms", 1, ini) != 0;
    Log("corvobody: PRESENT (module %p). Their ini: Enabled=%d RigidCamera=%d HideBodyArms=%d. Their F6-F11 are hard-coded, so "
        "the VR mod's F7/F8/F9 debug keys are parked and the overlay opens on %s instead of F10 (the pad chord is unchanged)",
        (void*)h, (int)g_cbTheirEnabled, (int)g_cbTheirRigid, (int)g_cbTheirHideArms,
        g_cbOverlayVk ? "[Overlay] Key" : "Insert");
    if (g_cbTheirRigid)
        DVR_WARN("corvobody: CorvoBody.ini has RigidCamera=1. Its GetPlayerViewPoint detour learns the look-up/down arc from "
                 "the camera POV cache, which in VR carries the head and eye offsets, and persists it to CorvoBodyCamera.bin; "
                 "it also doubles the [Neck] cancel. Set RigidCamera=0 in CorvoBody.ini (F6 in game reloads it).");
    if (g_cbTheirHideArms)
        DVR_WARN("corvobody: CorvoBody.ini has HideBodyArms=1: the body's shoulder vanishes while that hand holds gear, so "
                 "the arm cannot reach the VR hand. Set HideBodyArms=0 in CorvoBody.ini for the shoulder attach.");
    if (!g_cbTheirEnabled) CbWhy("their ini has Enabled=0: no body to attach to");
    return true;
}

static void CbDrop(const char* why)
{
    if (g_cbBody && g_cbHandsHidden && IsLiveObject(g_cbBody)) {
        CbHideBone(g_cbBody, "hand_L_jnt", false);
        CbHideBone(g_cbBody, "hand_R_jnt", false);
    }
    for (int h = 0; h < 2; h++) {
        if (g_cbCtl[h] && IsLiveObject(g_cbCtl[h])) {
            *(float*)(g_cbCtl[h] + kSkcStr) = 0.0f;
            if (g_cbOffStrT) *(float*)(g_cbCtl[h] + g_cbOffStrT) = 0.0f;
        }
        g_cbCtl[h] = NULL; g_cbArmLen[h] = 0.0f;
    }
    if (g_cbBody) Log("corvobody: body %p released (%s)", (void*)g_cbBody, why);
    g_cbBody = NULL; g_cbPawn = NULL; g_cbHandsHidden = false;
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
        // Segment lengths are pose-independent: upper arm + forearm = the reach.
        float ua[3], la[3], hd[3];
        if (CbBoneLocation(body, h ? "upper_arm_R_jnt" : "upper_arm_L_jnt", ua) &&
            CbBoneLocation(body, h ? "lower_arm_R_jnt" : "lower_arm_L_jnt", la) &&
            CbBoneLocation(body, ctlName, hd)) {
            float a = sqrtf((ua[0]-la[0])*(ua[0]-la[0]) + (ua[1]-la[1])*(ua[1]-la[1]) + (ua[2]-la[2])*(ua[2]-la[2]));
            float b = sqrtf((la[0]-hd[0])*(la[0]-hd[0]) + (la[1]-hd[1])*(la[1]-hd[1]) + (la[2]-hd[2])*(la[2]-hd[2]));
            g_cbArmLen[h] = (a > 1.0f && b > 1.0f && a < 200.0f && b < 200.0f) ? a + b : 0.0f;
        } else g_cbArmLen[h] = 0.0f;
    }
    g_cbBody = body; g_cbPawn = pawn;
    if (g_cbHideHands) {
        CbHideBone(body, "hand_L_jnt", true);
        CbHideBone(body, "hand_R_jnt", true);
        g_cbHandsHidden = true;
    }
    Log("corvobody: body %p on pawn %p (shadow twins seen: %d): hand_L_jnt %p hand_R_jnt %p, arm length L %.1f R %.1f uu "
        "(upper arm + forearm; 0 = not measured, no reach clamp), body hands %s. The VR hand point now drives both arms "
        "(strength %.2f, reach clamp %.2f).",
        (void*)body, (void*)pawn, g_cbShadowSeen, (void*)g_cbCtl[0], (void*)g_cbCtl[1], g_cbArmLen[0], g_cbArmLen[1],
        g_cbHandsHidden ? "hidden" : "left visible", g_cbStrength, g_cbReach);
    CbWhy("driving");
    return true;
}

// Called by skelcontrol.cpp at the hand drive's write: the point and the space it wrote.
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
        else if (!IsLiveObject(g_cbBody) || !IsLiveObject(g_cbCtl[0]) || !IsLiveObject(g_cbCtl[1])) {
            g_cbCtl[0] = g_cbCtl[1] = NULL; g_cbHandsHidden = false; g_cbBody = NULL;
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
    // The drive. The point is this dispatch's (ApplyHandToMesh ran just before); a stale
    // or non-world point releases that arm back to CorvoBody's own animation.
    for (int h = 0; h < 2; h++) {
        uint8_t* c = g_cbCtl[h];
        if (!c || !RangeReadable(c, g_cbOffEffSpace + 1)) continue;
        bool fresh = g_cbHandMs[h] > 0.0 && (now - g_cbHandMs[h]) < 500.0;
        if (!fresh || g_cbHandSpace[h] != 0 || dvr::anim::hand_owned(h)) {
            *(float*)(c + kSkcStr) = 0.0f; *(float*)(c + g_cbOffStrT) = 0.0f;
            if (fresh && g_cbHandSpace[h] != 0 && now - g_cbLogMs > 5000.0) {
                g_cbLogMs = now;
                Log("corvobody: the hand drive wrote space %u, not world (0): the body's effector needs a world point "
                    "([Hands] World=1 is the shipped setting); arms released", (unsigned)g_cbHandSpace[h]);
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
                    float k = lim / len;
                    t[0] = sh[0] + d[0]*k; t[1] = sh[1] + d[1]*k; t[2] = sh[2] + d[2]*k;
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
            Log("corvobody: effector write #%ld %s (%.0f %.0f %.0f) strength %.2f (0 writes with the body found = the hand "
                "drive is off or not in world space)", g_cbWrites, h ? "R" : "L", t[0], t[1], t[2], g_cbStrength);
    }
}

static void CorvoSet(bool on, const char* who)
{
    g_cbEnabled = on;
    Log("corvobody: %s (%s)", on ? "ON - the body's arms follow the VR hands" : "OFF - CorvoBody animates its own arms", who);
    if (!on && g_cbBody) CbDrop("switched off");
}

static void CorvoHideHandsSet(bool on, const char* who)
{
    g_cbHideHands = on;
    Log("corvobody: body hands %s (%s)", on ? "hidden" : "visible", who);
    if (g_cbBody && IsLiveObject(g_cbBody) && g_cbHandsHidden != on) {
        CbHideBone(g_cbBody, "hand_L_jnt", on);
        CbHideBone(g_cbBody, "hand_R_jnt", on);
        g_cbHandsHidden = on;
    }
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
    g_cbHideHands = IniFloat(ini, "CorvoBody", "HideBodyHands", 1) != 0.0f;
    g_cbStrength  = IniFloat(ini, "CorvoBody", "ArmStrength", 1.0f);
    if (g_cbStrength < 0.0f) g_cbStrength = 0.0f;
    if (g_cbStrength > 1.0f) g_cbStrength = 1.0f;
    g_cbReach     = IniFloat(ini, "CorvoBody", "ReachClamp", 0.98f);
    if (g_cbReach < 0.0f) g_cbReach = 0.0f;
    if (g_cbReach > 1.2f) g_cbReach = 1.2f;
    char key[32] = ""; GetPrivateProfileStringA("Overlay", "Key", "", key, sizeof(key), ini);
    g_cbOverlayVk = CbParseKey(key);
    if (key[0] && !g_cbOverlayVk) DVR_WARN("config: [Overlay] Key='%s' is not a key name this build knows (F1..F24, Insert, Delete, Home, End, Pause, ScrollLock, PageUp, PageDown, Backspace, Tab) - default used", key);
    Log("config: [CorvoBody] Enabled=%d HideBodyHands=%d ArmStrength=%.2f ReachClamp=%.2f; [Overlay] Key=%s - only acts when "
        "the CorvoBody mod (Nexus 453) is next to the exe; the attach drives its hand_L/R_jnt arm IK from the VR hand point",
        (int)g_cbEnabled, (int)g_cbHideHands, g_cbStrength, g_cbReach, key[0] ? key : "(default: F10, Insert while CorvoBody is present)");
    g_cbDetectMs = 0.0;
    CorvoDetect(true);
}

static void CorvoStatus(dvr::status::Writer& w)
{
    w.kv("present", g_cbPresent);
    w.kv("enabled", g_cbEnabled);
    w.kv("theirRigidCamera", g_cbTheirRigid);
    w.kv("theirHideBodyArms", g_cbTheirHideArms);
    w.kv("body", g_cbBody != NULL);
    w.kv("ctlL", g_cbCtl[0] != NULL);
    w.kv("ctlR", g_cbCtl[1] != NULL);
    w.kv("armLenL", (double)g_cbArmLen[0]);
    w.kv("armLenR", (double)g_cbArmLen[1]);
    w.kv("writes", (unsigned long)g_cbWrites);
    w.kv("clamped", (unsigned long)g_cbClamped);
    w.kv("handsHidden", g_cbHandsHidden);
    w.kv("why", g_cbWhy);
}

static bool CorvoCommand(const char* args)
{
    if (!args || !*args || !strcmp(args, "status")) {
        Log("corvobody: present=%d enabled=%d their(Enabled=%d RigidCamera=%d HideBodyArms=%d) body=%p ctl L=%p R=%p armLen L=%.1f R=%.1f "
            "writes=%ld clamped=%ld handsHidden=%d why='%s'",
            (int)g_cbPresent, (int)g_cbEnabled, (int)g_cbTheirEnabled, (int)g_cbTheirRigid, (int)g_cbTheirHideArms,
            (void*)g_cbBody, (void*)g_cbCtl[0], (void*)g_cbCtl[1], g_cbArmLen[0], g_cbArmLen[1], g_cbWrites, g_cbClamped,
            (int)g_cbHandsHidden, g_cbWhy);
        return true;
    }
    if (!strcmp(args, "on"))  { CorvoSet(true, "seam");  return true; }
    if (!strcmp(args, "off")) { CorvoSet(false, "seam"); return true; }
    if (!strcmp(args, "rescan")) { if (g_cbBody) CbDrop("rescan asked"); g_cbScanMs = 0.0; g_cbDetectMs = 0.0; return true; }
    if (!strcmp(args, "hands on"))  { CorvoHideHandsSet(false, "seam"); return true; }
    if (!strcmp(args, "hands off")) { CorvoHideHandsSet(true, "seam");  return true; }
    Log("corvobody: usage - corvobody on|off|status|rescan|hands on|hands off");
    return true;
}
