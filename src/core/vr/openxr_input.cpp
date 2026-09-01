// core/vr/openxr_input.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static XrPath XiPath(const char* s)
{
    XrPath p = XR_NULL_PATH;
    if (g_xrf.stringToPath) g_xrf.stringToPath(g_xriInst, s, &p);
    return p;
}

static bool XiAct(const char* name, const char* loc, XrActionType t,
                  XrAction* out)
{
    XrActionCreateInfo aci; memset(&aci, 0, sizeof(aci));
    aci.type = XR_TYPE_ACTION_CREATE_INFO;
    aci.actionType = t;
    strncpy(aci.actionName, name, sizeof(aci.actionName) - 1);
    strncpy(aci.localizedActionName, loc, sizeof(aci.localizedActionName) - 1);
    if (XR_FAILED(g_xrf.createAction(g_xiSet, &aci, out))) {
        *out = XR_NULL_HANDLE; return false;
    }
    return true;
}


static void XrInpInit(void)      // instance-level: set + actions + bindings
{
    if (g_xiCreated || !g_xrf.createActionSet || !g_xrf.createAction ||
        !g_xrf.suggestBindings || !g_xrf.stringToPath) return;
    XrActionSetCreateInfo asci; memset(&asci, 0, sizeof(asci));
    asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    strcpy(asci.actionSetName, "gameplay");
    strcpy(asci.localizedActionSetName, "Gameplay");
    if (XR_FAILED(g_xrf.createActionSet(g_xriInst, &asci, &g_xiSet))) {
        Log("xr-input: action set failed - controller input off");
        return;
    }
    int n = 0;
    n += XiAct("move",   "Move",              XR_ACTION_TYPE_VECTOR2F_INPUT, &g_xiMove);
    n += XiAct("look",   "Turn",              XR_ACTION_TYPE_VECTOR2F_INPUT, &g_xiLook);
    n += XiAct("trig_l", "Left hand",         XR_ACTION_TYPE_FLOAT_INPUT,    &g_xiTrigL);
    n += XiAct("trig_r", "Right hand",        XR_ACTION_TYPE_FLOAT_INPUT,    &g_xiTrigR);
    n += XiAct("grip_l", "Power wheel",       XR_ACTION_TYPE_FLOAT_INPUT,    &g_xiGripL);
    n += XiAct("grip_r", "Choke",             XR_ACTION_TYPE_FLOAT_INPUT,    &g_xiGripR);
    n += XiAct("btn_a",  "Jump",              XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiA);
    n += XiAct("btn_b",  "Stealth",           XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiB);
    n += XiAct("btn_x",  "Interact",          XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiX);
    n += XiAct("btn_y",  "Pause",             XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiY);
    n += XiAct("clk_l",  "Sprint",            XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiClkL);
    n += XiAct("clk_r",  "Health elixir",     XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiClkR);
    n += XiAct("menu",   "Menu",              XR_ACTION_TYPE_BOOLEAN_INPUT,  &g_xiMenu);
    n += XiAct("pose_l", "Left grip pose",    XR_ACTION_TYPE_POSE_INPUT,     &g_xiPoseL);
    n += XiAct("pose_r", "Right grip pose",   XR_ACTION_TYPE_POSE_INPUT,     &g_xiPoseR);
    n += XiAct("hap_l",  "Left haptic",       XR_ACTION_TYPE_VIBRATION_OUTPUT, &g_xiHapL);
    n += XiAct("hap_r",  "Right haptic",      XR_ACTION_TYPE_VIBRATION_OUTPUT, &g_xiHapR);
    XrActionSuggestedBinding tb[] = {
        { g_xiMove,  XiPath("/user/hand/left/input/thumbstick") },
        { g_xiLook,  XiPath("/user/hand/right/input/thumbstick") },
        { g_xiTrigL, XiPath("/user/hand/left/input/trigger/value") },
        { g_xiTrigR, XiPath("/user/hand/right/input/trigger/value") },
        { g_xiGripL, XiPath("/user/hand/left/input/squeeze/value") },
        { g_xiGripR, XiPath("/user/hand/right/input/squeeze/value") },
        { g_xiA,     XiPath("/user/hand/right/input/a/click") },
        { g_xiB,     XiPath("/user/hand/right/input/b/click") },
        { g_xiX,     XiPath("/user/hand/left/input/x/click") },
        { g_xiY,     XiPath("/user/hand/left/input/y/click") },
        { g_xiClkL,  XiPath("/user/hand/left/input/thumbstick/click") },
        { g_xiClkR,  XiPath("/user/hand/right/input/thumbstick/click") },
        { g_xiMenu,  XiPath("/user/hand/left/input/menu/click") },
        { g_xiPoseL, XiPath("/user/hand/left/input/grip/pose") },
        { g_xiPoseR, XiPath("/user/hand/right/input/grip/pose") },
        { g_xiHapL,  XiPath("/user/hand/left/output/haptic") },
        { g_xiHapR,  XiPath("/user/hand/right/output/haptic") },
    };
    XrInteractionProfileSuggestedBinding sb; memset(&sb, 0, sizeof(sb));
    sb.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
    sb.interactionProfile = XiPath("/interaction_profiles/oculus/touch_controller");
    sb.suggestedBindings = tb;
    sb.countSuggestedBindings = (uint32_t)(sizeof(tb) / sizeof(tb[0]));
    if (XR_FAILED(g_xrf.suggestBindings(g_xriInst, &sb)))
        Log("xr-input: touch binding suggestion FAILED");
    g_xiCreated = true;
    Log("xr-input: action set ready (%d actions, oculus/touch_controller)", n);
}


static void XrInpAttach(void)    // session-level: spaces + attach (one-way)
{
    if (!g_xiCreated || g_xrInpAttached || !g_xrf.attachActionSets ||
        !g_xrf.createActionSpace) return;
    XrActionSpaceCreateInfo asci; memset(&asci, 0, sizeof(asci));
    asci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
    asci.poseInActionSpace.orientation.w = 1.0f;
    asci.action = g_xiPoseL;
    if (XR_FAILED(g_xrf.createActionSpace(g_xriSess, &asci, &g_xiSpaceL)))
        g_xiSpaceL = XR_NULL_HANDLE;
    asci.action = g_xiPoseR;
    if (XR_FAILED(g_xrf.createActionSpace(g_xriSess, &asci, &g_xiSpaceR)))
        g_xiSpaceR = XR_NULL_HANDLE;
    XrSessionActionSetsAttachInfo sai; memset(&sai, 0, sizeof(sai));
    sai.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    sai.countActionSets = 1;
    sai.actionSets = &g_xiSet;
    if (XR_FAILED(g_xrf.attachActionSets(g_xriSess, &sai))) {
        Log("xr-input: attach FAILED - display continues without controllers");
        return;
    }
    g_xrInpAttached = true;
    Log("xr-input: attached - Quest controllers live under OpenXR");
}


static float XiF(XrAction a)
{
    if (a == XR_NULL_HANDLE || !g_xrf.getFloat) return 0.0f;
    XrActionStateGetInfo gi; memset(&gi, 0, sizeof(gi));
    gi.type = XR_TYPE_ACTION_STATE_GET_INFO; gi.action = a;
    XrActionStateFloat st; memset(&st, 0, sizeof(st));
    st.type = XR_TYPE_ACTION_STATE_FLOAT;
    if (XR_FAILED(g_xrf.getFloat(g_xriSess, &gi, &st)) || !st.isActive)
        return 0.0f;
    return st.currentState;
}

static bool XiBl(XrAction a)
{
    if (a == XR_NULL_HANDLE || !g_xrf.getBool) return false;
    XrActionStateGetInfo gi; memset(&gi, 0, sizeof(gi));
    gi.type = XR_TYPE_ACTION_STATE_GET_INFO; gi.action = a;
    XrActionStateBoolean st; memset(&st, 0, sizeof(st));
    st.type = XR_TYPE_ACTION_STATE_BOOLEAN;
    if (XR_FAILED(g_xrf.getBool(g_xriSess, &gi, &st)) || !st.isActive)
        return false;
    return st.currentState != 0;
}

static void XiV2(XrAction a, float* x, float* y)
{
    *x = 0; *y = 0;
    if (a == XR_NULL_HANDLE || !g_xrf.getVec2) return;
    XrActionStateGetInfo gi; memset(&gi, 0, sizeof(gi));
    gi.type = XR_TYPE_ACTION_STATE_GET_INFO; gi.action = a;
    XrActionStateVector2f st; memset(&st, 0, sizeof(st));
    st.type = XR_TYPE_ACTION_STATE_VECTOR2F;
    if (XR_FAILED(g_xrf.getVec2(g_xriSess, &gi, &st)) || !st.isActive) return;
    *x = st.currentState.x; *y = st.currentState.y;
}


static void XrInpSync(XrTime when)   // pace thread, once per XR frame
{
    if (!g_xrInpAttached || !g_xrf.syncActions) return;
    XrInpState s; memset(&s, 0, sizeof(s));
    XrActiveActionSet act = { g_xiSet, XR_NULL_PATH };
    XrActionsSyncInfo si; memset(&si, 0, sizeof(si));
    si.type = XR_TYPE_ACTIONS_SYNC_INFO;
    si.countActiveActionSets = 1;
    si.activeActionSets = &act;
    XrResult r = g_xrf.syncActions(g_xriSess, &si);
    if (!XR_FAILED(r) && r != XR_SESSION_NOT_FOCUSED) {
        s.active = true;
        XiV2(g_xiMove, &s.mv[0], &s.mv[1]);
        XiV2(g_xiLook, &s.lk[0], &s.lk[1]);
        s.trigL = XiF(g_xiTrigL); s.trigR = XiF(g_xiTrigR);
        s.gripL = XiF(g_xiGripL); s.gripR = XiF(g_xiGripR);
        s.a = XiBl(g_xiA); s.b = XiBl(g_xiB);
        s.x = XiBl(g_xiX); s.y = XiBl(g_xiY);
        s.clkL = XiBl(g_xiClkL); s.clkR = XiBl(g_xiClkR);
        s.menu = XiBl(g_xiMenu);
        for (int h = 0; h < 2; h++) {
            XrSpace sp = h ? g_xiSpaceR : g_xiSpaceL;
            if (sp == XR_NULL_HANDLE || !g_xrf.locateSpace) continue;
            XrSpaceLocation sl; memset(&sl, 0, sizeof(sl));
            sl.type = XR_TYPE_SPACE_LOCATION;
            if (XR_FAILED(g_xrf.locateSpace(sp, g_xriSpace, when, &sl)))
                continue;
            if (!(sl.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) ||
                !(sl.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT))
                continue;
            XrQuaternionf q = sl.pose.orientation;
            float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
            float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
            float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
            float (*m)[4] = s.hand[h];
            m[0][0] = 1-2*(yy+zz); m[0][1] = 2*(xy-wz); m[0][2] = 2*(xz+wy);
            m[1][0] = 2*(xy+wz); m[1][1] = 1-2*(xx+zz); m[1][2] = 2*(yz-wx);
            m[2][0] = 2*(xz-wy); m[2][1] = 2*(yz+wx); m[2][2] = 1-2*(xx+yy);
            m[0][3] = sl.pose.position.x;
            m[1][3] = sl.pose.position.y;
            m[2][3] = sl.pose.position.z;
            s.handOk[h] = true;
        }
        static bool told = false;
        if (!told) { told = true;
            Log("xr-input: actions syncing (hands %s%s)",
                s.handOk[0] ? "L" : "-", s.handOk[1] ? "R" : "-");
        }
    }
    EnterCriticalSection(&g_xrCs);
    g_xrInp = s;
    LeaveCriticalSection(&g_xrCs);
}


static void XrInpHaptic(int hand, float amp, float durSec)
{
    // 38.10: QUEUE ONLY - never call the runtime from this (game) thread.
    if (!g_xrInpAttached || !g_xrHaptics) return;
    if (hand < 0 || hand > 1 || !g_xrCsInit) return;
    EnterCriticalSection(&g_xrCs);
    g_xrHapAmp[hand] = amp;
    g_xrHapDur[hand] = durSec;
    LeaveCriticalSection(&g_xrCs);
    InterlockedExchange(&g_xrHapPend[hand], 1);
}


static void XrInpHapticFlush(void)   // PACE THREAD ONLY
{
    if (!g_xrInpAttached || !g_xrf.applyHaptic) return;
    for (int hand = 0; hand < 2; hand++) {
        if (!InterlockedExchange(&g_xrHapPend[hand], 0)) continue;
        float amp, dur;
        EnterCriticalSection(&g_xrCs);
        amp = g_xrHapAmp[hand]; dur = g_xrHapDur[hand];
        LeaveCriticalSection(&g_xrCs);
        XrHapticActionInfo hai; memset(&hai, 0, sizeof(hai));
        hai.type = XR_TYPE_HAPTIC_ACTION_INFO;
        hai.action = hand ? g_xiHapR : g_xiHapL;
        XrHapticVibration v; memset(&v, 0, sizeof(v));
        v.type = XR_TYPE_HAPTIC_VIBRATION;
        v.duration = (XrDuration)(dur * 1e9);
        v.frequency = 160.0f;
        v.amplitude = amp;
        g_xrf.applyHaptic(g_xriSess, &hai, (const XrHapticBaseHeader*)&v);
    }
}
