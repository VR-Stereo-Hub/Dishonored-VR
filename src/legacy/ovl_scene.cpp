// legacy/ovl_scene.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// 37.9: bring up / feed the head-locked SBS overlay (see the globals note).
static bool OvlSceneEnsure()
{
    if (g_ovlSceneH) return true;
    if (!g_ov && !GetFnTable(IVROverlay_Version, (void**)&g_ov)) return false;
    if (g_ov->CreateOverlay((char*)"gingasvr.dishonored.scene",
                            (char*)"Dishonored VR Screen",
                            &g_ovlSceneH) != EVROverlayError_VROverlayError_None) {
        g_ovlSceneH = 0; return false;
    }
    g_ov->SetOverlayFlag(g_ovlSceneH, VROverlayFlags_SideBySide_Parallel, 1);
    // the SBS halves are 2:1 anamorphic; texel aspect 2 un-squeezes them
    g_ov->SetOverlayTexelAspect(g_ovlSceneH, 2.0f);
    // 38.0: transform is set per-frame (world anchor + lazy follow, or
    // head-locked when OverlayFollowTau=0) - see OvlSceneFrame.
    g_ov->ShowOverlay(g_ovlSceneH);
    Log("ovl-scene: SBS overlay LIVE (follow tau %.2fs, color mode %d) at "
        "%.2f m", g_ovlFollowTau, g_ovlColor, g_screenDist);
    return true;
}


static void OvlSceneFrame()
{
    if (!OvlSceneEnsure()) return;
    // same width math as the eye quads: span the game's rendered FOV
    float fovDeg = (g_liveFovX > 30.0f && g_liveFovX < 150.0f)
                 ? g_liveFovX : g_gameFovDeg;
    if (g_fovLever >= 40.0f) {
        float floorFov = (float)g_fovLever * g_zoomFillFloor;
        if (fovDeg < floorFov) fovDeg = floorFov;
    }
    float fillNow = g_fillScale;
    if ((g_menuOpen || g_inMenu || g_sbsMonoNow) && g_menuFill > 0.1f)
        fillNow = g_menuFill;
    float W = 2.0f * g_screenDist * tanf(fovDeg * 0.5f * 0.0174533f) * fillNow;
    if (fabsf(W - g_ovlSceneW) > 0.01f) {
        g_ovlSceneW = W;
        g_ov->SetOverlayWidthInMeters(g_ovlSceneH, W);
    }

    // ---- 38.0: world anchor with lazy follow -------------------------------
    if (g_ovlFollowTau < 0.005f) {
        // hard head-lock (the 37.9 behavior), kept behind the knob
        HmdMatrix34_t m; memset(&m, 0, sizeof(m));
        m.m[0][0] = m.m[1][1] = m.m[2][2] = 1.0f;
        m.m[2][3] = -g_screenDist;
        g_ov->SetOverlayTransformTrackedDeviceRelative(g_ovlSceneH, 0, &m);
    } else if (g_devPoseOk[0]) {
        static double lastMs = 0.0;
        double nowMs = MaimNowMs();
        float dt = (lastMs > 0.0) ? (float)((nowMs - lastMs) * 0.001) : 0.011f;
        lastMs = nowMs;
        if (dt < 0.0f) dt = 0.0f; if (dt > 0.25f) dt = 0.25f;
        float a = 1.0f - expf(-dt / g_ovlFollowTau);
        float hx = g_devPose[0][0][3], hy = g_devPose[0][1][3],
              hz = g_devPose[0][2][3];
        float ty = g_hmdYaw, tp = g_hmdPitch;
        if (!g_ovlFollowInit) {
            g_ovlFollowInit = true;
            g_ovlYawS = ty; g_ovlPitchS = tp;
            g_ovlPosS[0] = hx; g_ovlPosS[1] = hy; g_ovlPosS[2] = hz;
        }
        float dy = ty - g_ovlYawS;
        while (dy >  3.14159265f) dy -= 6.2831853f;
        while (dy < -3.14159265f) dy += 6.2831853f;
        g_ovlYawS   += a * dy;
        g_ovlPitchS += a * (tp - g_ovlPitchS);
        g_ovlPosS[0] += a * (hx - g_ovlPosS[0]);
        g_ovlPosS[1] += a * (hy - g_ovlPosS[1]);
        g_ovlPosS[2] += a * (hz - g_ovlPosS[2]);
        // R = Ry(yaw) * Rx(pitch); overlay center = pos + R*(0,0,-D)
        float cy = cosf(g_ovlYawS), sy = sinf(g_ovlYawS);
        float cp = cosf(g_ovlPitchS), sp = sinf(g_ovlPitchS);
        HmdMatrix34_t m;
        m.m[0][0] = cy;      m.m[0][1] = sy * sp;  m.m[0][2] = sy * cp;
        m.m[1][0] = 0.0f;    m.m[1][1] = cp;       m.m[1][2] = -sp;
        m.m[2][0] = -sy;     m.m[2][1] = cy * sp;  m.m[2][2] = cy * cp;
        float fwd[3] = { -m.m[0][2], -m.m[1][2], -m.m[2][2] };
        m.m[0][3] = g_ovlPosS[0] + fwd[0] * g_screenDist;
        m.m[1][3] = g_ovlPosS[1] + fwd[1] * g_screenDist;
        m.m[2][3] = g_ovlPosS[2] + fwd[2] * g_screenDist;
        ETrackingUniverseOrigin uni = g_comp
            ? g_comp->GetTrackingSpace()
            : ETrackingUniverseOrigin_TrackingUniverseStanding;
        g_ov->SetOverlayTransformAbsolute(g_ovlSceneH, uni, &m);
    }

    Texture_t t;
    t.handle = (void*)g_texGame[0];          // the SBS pair, both eyes
    t.eType = ETextureType_TextureType_DirectX;
    t.eColorSpace = g_ovlColor == 2 ? EColorSpace_ColorSpace_Linear
                  : g_ovlColor == 0 ? EColorSpace_ColorSpace_Auto
                                    : EColorSpace_ColorSpace_Gamma;
    g_ov->SetOverlayTexture(g_ovlSceneH, &t);
}
