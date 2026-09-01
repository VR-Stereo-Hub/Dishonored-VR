// core/vr/openvr_backend.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static bool ActDig(VRActionHandle_t a, bool* activeOut)
{
    InputDigitalActionData_t d;
    if (!g_input || g_input->GetDigitalActionData(a, &d, sizeof(d),
            k_ulInvalidInputValueHandle) != EVRInputError_VRInputError_None)
        return false;
    if (activeOut && d.bActive) *activeOut = true;
    return d.bActive && d.bState;
}

// analog action (x, optional y)
static void ActAna(VRActionHandle_t a, float* x, float* y, bool* activeOut)
{
    *x = 0; if (y) *y = 0;
    InputAnalogActionData_t d;
    if (!g_input || g_input->GetAnalogActionData(a, &d, sizeof(d),
            k_ulInvalidInputValueHandle) != EVRInputError_VRInputError_None)
        return;
    if (!d.bActive) return;
    if (activeOut) *activeOut = true;
    *x = d.x; if (y) *y = d.y;
}


// Stage 7.3: haptic feedback for MotionAim (catch confirmations, key acks).
// Uses the action system's vibration outputs when live, legacy pulse otherwise.
static void MaimHaptic(int hand, float amp, float durSec)
{
    if (!g_padHaptics) return;
    if (g_xrOn) { XrInpHaptic(hand, amp, durSec); return; }   // 38.9
    if (g_useActions && g_input) {
        g_input->TriggerHapticVibrationAction(
            hand ? g_actHapR : g_actHapL, 0.0f, durSec, 160.0f, amp,
            k_ulInvalidInputValueHandle);
    } else if (g_sys && g_ctrlIdx[hand] >= 0) {
        g_sys->TriggerHapticPulse((TrackedDeviceIndex_t)g_ctrlIdx[hand], 0,
                                  (unsigned short)(amp * 3000.0f));
    }
}


// Switch from legacy (Vive-wand emulation) to the SteamVR action system:
// write our manifest + Index bindings next to the DLL, register them, and
// resolve every action handle. Any failure -> stay on legacy, log why.
static void InitActionInput()
{
    if (!g_padEnabled) return;
    if (!GetFnTable(IVRInput_Version, (void**)&g_input) || !g_input) {
        Log("pad: IVRInput unavailable - staying on legacy input");
        g_input = NULL; return;
    }
    char mpath[MAX_PATH], bpath[MAX_PATH];
    _snprintf(mpath, MAX_PATH, "%s\\vr_actions.json", g_dir);
    _snprintf(bpath, MAX_PATH, "%s\\vr_bindings_knuckles.json", g_dir);

    // 37.8: read the controller type SteamVR ACTUALLY reports. The user's
    // find: over Steam Link the Quest pads are not "oculus_touch" - so the
    // 37.6 defaults never attached (movement dead). Register our Touch
    // layout under the reported type, whatever it is - self-adapting, and
    // logged so the string is on record.
    char ctype[64] = "";
    if (g_sys) {
        for (uint32_t i = 1; i < k_unMaxTrackedDeviceCount && !ctype[0]; i++) {
            if (g_sys->GetTrackedDeviceClass(i) !=
                ETrackedDeviceClass_TrackedDeviceClass_Controller) continue;
            g_sys->GetStringTrackedDeviceProperty(i,
                ETrackedDeviceProperty_Prop_ControllerType_String,
                ctype, sizeof(ctype), NULL);
        }
    }
    bool extraType = ctype[0] &&
                     strcmp(ctype, "knuckles") && strcmp(ctype, "oculus_touch");
    Log("pad: controller_type reported = \"%s\"%s",
        ctype[0] ? ctype : "(none tracked yet)",
        extraType ? " -> registering our Touch layout under it" : "");

    static char manifestBuf[8192];
    {
        const char* anchor = "\"binding_url\": \"vr_bindings_touch.json\" }";
        const char* at = extraType ? strstr(kActionManifest, anchor) : NULL;
        if (at) {
            size_t head = (size_t)(at - kActionManifest) + strlen(anchor);
            _snprintf(manifestBuf, sizeof(manifestBuf),
                "%.*s,\n    { \"controller_type\": \"%s\", "
                "\"binding_url\": \"vr_bindings_native.json\" }%s",
                (int)head, kActionManifest, ctype, at + strlen(anchor));
        } else {
            strncpy(manifestBuf, kActionManifest, sizeof(manifestBuf) - 1);
        }
    }
    if (!WriteTextFile(mpath, manifestBuf) ||
        !WriteTextFile(bpath, kBindingsKnuckles)) {
        Log("pad: can't write action manifest - staying on legacy input");
        g_input = NULL; return;
    }
    {   // 37.6: Quest Touch defaults beside the Index ones
        char tpath[MAX_PATH];
        _snprintf(tpath, MAX_PATH, "%s\\vr_bindings_touch.json", g_dir);
        if (!WriteTextFile(tpath, kBindingsTouch))
            Log("pad: could not write Touch bindings (Index unaffected)");
    }
    if (extraType) {   // 37.8: same layout, the reported controller_type
        static char touchBuf[8192];
        const char* tt = strstr(kBindingsTouch, "oculus_touch");
        if (tt) {
            size_t head = (size_t)(tt - kBindingsTouch);
            _snprintf(touchBuf, sizeof(touchBuf), "%.*s%s%s",
                      (int)head, kBindingsTouch, ctype,
                      tt + strlen("oculus_touch"));
            char npath[MAX_PATH];
            _snprintf(npath, MAX_PATH, "%s\\vr_bindings_native.json", g_dir);
            if (!WriteTextFile(npath, touchBuf))
                Log("pad: could not write native-type bindings");
        }
    }
    EVRInputError e = g_input->SetActionManifestPath(mpath);
    if (e != EVRInputError_VRInputError_None) {
        Log("pad: SetActionManifestPath -> %d (manifest: %s)", (int)e, mpath);
        g_useActions = false; return;      // keep g_input so we can retry later
    }
    struct { const char* n; VRActionHandle_t* h; } acts[] = {
        {"/actions/main/in/move", &g_actMove},   {"/actions/main/in/turn", &g_actTurn},
        {"/actions/main/in/hand_r", &g_actHandR},{"/actions/main/in/hand_l", &g_actHandL},
        {"/actions/main/in/jump", &g_actJump},   {"/actions/main/in/stealth", &g_actStealth},
        {"/actions/main/in/interact", &g_actInteract}, {"/actions/main/in/pause", &g_actPause},
        {"/actions/main/in/wheel", &g_actWheel}, {"/actions/main/in/zoom", &g_actZoom},
        {"/actions/main/in/choke", &g_actChoke}, {"/actions/main/in/lean_y", &g_actLeanY},
        {"/actions/main/in/sneak", &g_actSneak}, {"/actions/main/in/recenter", &g_actRecenter},
        {"/actions/main/in/journal", &g_actJournal},
        {"/actions/main/in/health", &g_actHealth},
        {"/actions/main/out/haptic_l", &g_actHapL}, {"/actions/main/out/haptic_r", &g_actHapR},
    };
    bool ok = g_input->GetActionSetHandle((char*)"/actions/main", &g_actSet)
              == EVRInputError_VRInputError_None;
    for (unsigned i = 0; i < sizeof(acts)/sizeof(acts[0]); i++)
        ok = ok && g_input->GetActionHandle((char*)acts[i].n, acts[i].h)
                   == EVRInputError_VRInputError_None;
    if (!ok) {
        Log("pad: action handle resolution failed");
        g_useActions = false; return;      // retryable
    }
    g_useActions = true;
    Log("pad: ACTION input live - A buttons, trackpads, real stick clicks");
}


static bool GetFnTable(const char* version, void** out)
{
    // 38.12: THE ARMING CRASH. Under the OpenXR backend openvr_api is never
    // loaded, so g_VR_GetGenericInterface stays NULL - and this function
    // called it unguarded. Any subsystem that lazily wanted an OpenVR
    // interface (the crash fired at motion-control arm time, from a game
    // thread) jumped straight to address 0, and the game's SEH turned it
    // into "Rendering thread exception: Address = 0x0" plus downstream heap
    // damage (the FaceFX cleanup crash). One guard kills the entire class:
    // every caller already handles false. The log names the caller so the
    // lazy path identifies itself without dying.
    *out = NULL;
    if (!g_VR_GetGenericInterface) {
        static LONG told = 0;
        if (InterlockedIncrement(&told) <= 8)
            Log("GetFnTable(%s) with NO OpenVR loaded (XR mode) - caller "
                "0x%p tid=%lu - refused safely", version,
                _ReturnAddress(),
                (unsigned long)GetCurrentThreadId());
        return false;
    }
    char name[128];
    _snprintf(name, sizeof(name), "FnTable:%s", version);
    EVRInitError err = EVRInitError_VRInitError_None;
    *out = (void*)g_VR_GetGenericInterface(name, &err);
    if (!*out || err != EVRInitError_VRInitError_None) {
        Log("GetGenericInterface(%s) failed: %d", name, (int)err);
        return false;
    }
    return true;
}


static bool TryInitVR()
{
    // 37.1: bench mode runs the game FLAT - the OpenVR path starting SteamVR
    // beside the xrsim bench left the mod waiting on a headset that was
    // never coming (submits=0, growing frame gaps, looked like a crash).
    {
        static int benchMode = -1;
        if (benchMode < 0) {
            char xb[8] = "";
            benchMode = (GetEnvironmentVariableA("DISHONORED_VR_XR_BENCH",
                          xb, sizeof(xb)) && xb[0] == '1') ? 1 : 0;
            if (benchMode)
                Log("vr: XR BENCH mode - OpenVR init suppressed, game runs "
                    "flat while the bench thread reports");
        }
        if (benchMode) return false;
    }
    // 37.3: OpenXR backend selected - OpenVR/SteamVR stays out of the process.
    if (g_xrBackend) return false;
    if (g_vrReady) return true;

    if (!g_openvr) {
        char path[MAX_PATH];
        _snprintf(path, MAX_PATH, "%s\\openvr_api.dll", g_dir);
        g_openvr = LoadLibraryA(path);
        if (!g_openvr) {
            Log("openvr_api.dll not found (err %lu) - VR disabled", GetLastError());
            g_vrFailed = true;
            return false;
        }
        g_VR_InitInternal        = (PFN_VR_InitInternal)       GetProcAddress(g_openvr, "VR_InitInternal");
        g_VR_ShutdownInternal    = (PFN_VR_ShutdownInternal)   GetProcAddress(g_openvr, "VR_ShutdownInternal");
        g_VR_GetGenericInterface = (PFN_VR_GetGenericInterface)GetProcAddress(g_openvr, "VR_GetGenericInterface");
        g_VR_ErrDesc = (PFN_VR_GetVRInitErrorAsEnglishDescription)
                       GetProcAddress(g_openvr, "VR_GetVRInitErrorAsEnglishDescription");
        if (!g_VR_InitInternal || !g_VR_GetGenericInterface) {
            Log("openvr_api.dll missing exports - VR disabled");
            g_vrFailed = true;
            return false;
        }
    }

    EVRApplicationType wantType = g_forceTheater
        ? EVRApplicationType_VRApplication_Overlay
        : EVRApplicationType_VRApplication_Scene;

    EVRInitError err = EVRInitError_VRInitError_None;
    g_VR_InitInternal(&err, wantType);
    if (err != EVRInitError_VRInitError_None) {
        Log("VR_InitInternal(%s) failed: %d (%s) - will retry",
            g_forceTheater ? "overlay" : "scene", (int)err,
            g_VR_ErrDesc ? g_VR_ErrDesc(err) : "?");
        g_vrFailed = true;
        return false;
    }

    if (!g_forceTheater) {
        if (GetFnTable(IVRSystem_Version, (void**)&g_sys) &&
            GetFnTable(IVRCompositor_Version, (void**)&g_comp) &&
            EnsureD3D11() && EnsurePipeline()) {
            g_mode = MODE_SCENE;
            g_vrReady = true; g_vrFailed = false;
            Log("SCENE MODE ready - game view fills the headset, head drives camera");
            {   // 37.6: name the HMD; Quest-family gets the rigid screen
                char hmdMk[128] = "", hmdMd[128] = "";
                g_sys->GetStringTrackedDeviceProperty(0,
                    ETrackedDeviceProperty_Prop_ManufacturerName_String,
                    hmdMk, sizeof(hmdMk), NULL);
                g_sys->GetStringTrackedDeviceProperty(0,
                    ETrackedDeviceProperty_Prop_ModelNumber_String,
                    hmdMd, sizeof(hmdMd), NULL);
                bool questish = strstr(hmdMk, "Oculus") || strstr(hmdMk, "Meta")
                             || strstr(hmdMd, "Oculus") || strstr(hmdMd, "Meta")
                             || strstr(hmdMd, "Quest");
                g_rigidScreen = (g_rigidScreenCfg == 1) ||
                                (g_rigidScreenCfg == -1 && questish);
                Log("hmd: \"%s\" \"%s\" -> screen construction %s",
                    hmdMk, hmdMd,
                    g_rigidScreen ? "RIGID (Quest-family)" : "per-eye (tuned)");
                // 38.1: overlay-scene REJECTED as an architecture (headset
                // verdict: head-locked "less warping but unsmooth", world-
                // anchored "completely fucked"). Code stays dormant for
                // experiments; it never auto-enables again.
                g_ovlScene = (g_ovlSceneCfg == 1);
                // 38.3: explicit opt-in ONLY (38.2's auto measured "worse
                // than before" in the headset - correct but not the product).
                g_worldScreen = (g_worldScreenCfg == 1);
                if (g_worldScreen) {
                    InterlockedExchange(&g_wsReanchor, 1);
                    Log("hmd: WORLD-ANCHORED screen (reprojection-correct; "
                        "recenter re-anchors it in front of you)");
                }
                if (g_ovlScene)
                    Log("hmd: OVERLAY-SCENE mode (reprojection-exempt; panel "
                        "and wrist HUD hidden in this test mode)");
                g_quadAspect = 0.0f;
            }
            InitActionInput();   // modern SteamVR input (A buttons, trackpads)
            // (InitActionInput uses GetFnTableFwd = GetFnTable, defined below)
            return true;
        }
        Log("scene mode setup failed - falling back to theater overlay");
    }

    // theater fallback (stage-1 style)
    if (!GetFnTable(IVROverlay_Version, (void**)&g_ov)) { g_vrFailed = true; return false; }
    EVROverlayError oerr = g_ov->CreateOverlay((char*)"gingasvr.dishonoredvr",
                                               (char*)"Dishonored VR", &g_overlay);
    if (oerr != EVROverlayError_VROverlayError_None) {
        Log("CreateOverlay failed: %d", (int)oerr);
        g_vrFailed = true;
        return false;
    }
    g_ov->SetOverlayWidthInMeters(g_overlay, 4.0f);
    g_ov->SetOverlayCurvature(g_overlay, 0.12f);
    HmdMatrix34_t m;
    memset(&m, 0, sizeof(m));
    m.m[0][0] = 1; m.m[1][1] = 1; m.m[2][2] = 1;
    m.m[1][3] = 1.6f; m.m[2][3] = -2.4f;
    g_ov->SetOverlayTransformAbsolute(g_overlay,
        ETrackingUniverseOrigin_TrackingUniverseStanding, &m);
    g_ov->ShowOverlay(g_overlay);
    g_mode = MODE_THEATER;
    g_vrReady = true; g_vrFailed = false;
    Log("THEATER MODE ready (flat screen)");
    return true;
}


// ----------------------------------------------------------------------------
// Left-hand aim reticle (build 30.5). Lazily creates a 64x64 procedural
// ring-and-dot overlay and anchors it to the MotionAim controller, offset
// along the SAME tilted ray HandRelFull aims with - so the dot marks where
// bolts and bullets will actually go. Shows while weapon tracking is on.
// ----------------------------------------------------------------------------
static void ReticleTick()
{
    if (g_mode != MODE_SCENE) return;
    // 32.28: a Blink aim held within the last 200 ms borrows the reticle and
    // parks it ON the landing spot.
    bool blinkAim = g_blkMarker && g_blkDstOn && g_blkAimDrive &&
                    (MaimNowMs() - g_blkAimSeen) < 200.0 &&
                    g_blkAimDistUU > 20.0f;
    bool want = g_retEnabled && (g_handMesh || blinkAim);
    int dev = (g_maimHand >= 0 && g_maimHand <= 1) ? g_ctrlIdx[g_maimHand] : -1;
    if (dev < 0 || dev >= 16 || !g_devPoseOk[dev]) want = false;
    float uuPerM = (g_skcWorldScale > 10.0f) ? g_skcWorldScale : 100.0f;
    float wantDist = g_retDist, wantSize = g_retSize;
    if (blinkAim) {
        float dUU = g_blkAimDistUU - g_blkMarkerBackUU;   // 32.29: off the surface
        if (dUU < 30.0f) dUU = 30.0f;
        wantDist = dUU / uuPerM;
        if (wantDist < 0.4f) wantDist = 0.4f;
        if (wantDist > 40.0f) wantDist = 40.0f;
        // hold a constant ANGULAR size so it stays readable far away
        wantSize = g_retSize * (wantDist / (g_retDist > 0.2f ? g_retDist : 2.5f));
        if (wantSize < g_retSize) wantSize = g_retSize;
    }

    // 38.13: under the XR backend there are no OpenVR overlays (lazily
    // asking for one here was THE ARMING CRASH) - publish the aim point
    // instead; the eye loop draws the ring as scene geometry.
    if (g_xrOn) {
        g_retXrOn = want;
        if (want) {
            float a2 = g_maimPitchOff * 0.01745329f;
            float sa2 = sinf(a2), ca2 = cosf(a2);
            float lp[3] = { 0.0f, -sa2 * wantDist, -ca2 * wantDist };
            float (*C)[4] = g_devPose[dev];
            for (int k = 0; k < 3; k++)
                g_retXrPos[k] = C[k][0]*lp[0] + C[k][1]*lp[1] +
                                C[k][2]*lp[2] + C[k][3];
            g_retXrSize = wantSize;
        }
        return;
    }

    if (want && !g_retOverlay) {
        if (!g_ov && !GetFnTable(IVROverlay_Version, (void**)&g_ov)) {
            g_retEnabled = false;
            Log("reticle: no overlay interface - disabled");
            return;
        }
        if (g_ov->CreateOverlay((char*)"gingasvr.dishonoredvr.reticle",
                                (char*)"DVR Reticle", &g_retOverlay)
            != EVROverlayError_VROverlayError_None) {
            g_retOverlay = 0;
            g_retEnabled = false;
            Log("reticle: CreateOverlay failed - disabled");
            return;
        }
        // procedural reticle: open white ring + centre dot, dark rims for
        // contrast against bright scenes. RGBA bytes = 0xAABBGGRR words.
        static uint32_t px[64 * 64];
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                float dx = x - 31.5f, dy = y - 31.5f;
                float r = sqrtf(dx*dx + dy*dy);
                float dotA  = 4.5f - r;                       // solid core
                float ringA = 2.2f - fabsf(r - 19.0f);        // open ring
                float rimA  = 1.6f - fabsf(r - 22.6f);        // outer dark rim
                float rim2A = 1.6f - fabsf(r - 15.4f);        // inner dark rim
                float w = dotA > ringA ? dotA : ringA;        // white weight
                if (w < 0) w = 0; if (w > 1) w = 1;
                float d = rimA > rim2A ? rimA : rim2A;        // dark weight
                if (d < 0) d = 0; if (d > 1) d = 1;
                d *= 0.6f * (1.0f - w);
                float a = w + d;
                if (a > 1) a = 1;
                unsigned lum = (unsigned)(a > 0.001f ? 255.0f * (w / a) : 0);
                unsigned al  = (unsigned)(255.0f * a);
                px[y*64 + x] = (al << 24) | (lum << 16) | (lum << 8) | lum;
            }
        }
        g_ov->SetOverlayRaw(g_retOverlay, px, 64, 64, 4);
        g_ov->SetOverlayWidthInMeters(g_retOverlay, g_retSize);
        g_retDevIdx = -1;                    // force the anchor to be set
        Log("reticle: overlay created (%.3fm at %.1fm)", g_retSize, g_retDist);
    }
    if (!g_retOverlay) return;

    static float retLastDist = -1.0f, retLastSize = -1.0f;
    if (want && g_retOverlay && fabsf(wantSize - retLastSize) > 0.002f) {
        retLastSize = wantSize;               // 32.29: size is independent of
        g_ov->SetOverlayWidthInMeters(g_retOverlay, wantSize);   // the transform
    }
    if (want && (dev != g_retDevIdx || g_maimPitchOff != g_retPitch ||
                 fabsf(wantDist - retLastDist) > 0.02f)) {
        g_retDevIdx = dev;
        g_retPitch  = g_maimPitchOff;
        retLastDist = wantDist;
        float a = g_maimPitchOff * 3.14159265f / 180.0f;
        float sa = sinf(a), ca = cosf(a);
        // controller-local: aim ray = (0,-sin a,-cos a); dot sits dist along
        // it, facing back up the ray at the player (overlay front = local +Z)
        HmdMatrix34_t m;
        memset(&m, 0, sizeof(m));
        m.m[0][0] = 1.0f;                            // X = controller X
        m.m[1][1] = ca;  m.m[2][1] = -sa;            // Y
        m.m[1][2] = sa;  m.m[2][2] = ca;             // Z = -ray (faces user)
        m.m[1][3] = -sa * wantDist;                  // position = ray * dist
        m.m[2][3] = -ca * wantDist;
        g_ov->SetOverlayTransformTrackedDeviceRelative(
            g_retOverlay, (TrackedDeviceIndex_t)dev, &m);
    }
    if (want != g_retVisible) {
        g_retVisible = want;
        if (want) g_ov->ShowOverlay(g_retOverlay);
        else      g_ov->HideOverlay(g_retOverlay);
        Log("reticle: %s (hand=%s dev=%d)", want ? "ON" : "off",
            g_maimHand ? "right" : "left", dev);
    }
}
