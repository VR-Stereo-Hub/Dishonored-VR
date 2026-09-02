// game/dishonored/present_tick.cpp - the game side of the frame path (41.0).
// Included by the unity build. core/framework/frame_hooks (a real module) owns
// the D3D9 hooks and the ORDER of the frame path; what the game does per
// present - the seam poll, the head pose into the camera write, the hands, the
// virtual gamepad, the heartbeat and the hotkeys - lives here and is registered
// through DvrInstallFrameHooks(). Bodies are the 38.92 present hook's, kept
// verbatim; line numbers in their comments refer to the original single file
// (src/dllmain.cpp at commit 48766c07).

// Every present, even with VR disabled: the debugging surface.
static void DvrPreTick(IDirect3DDevice9*)
{
    {   // the debugging surface: command.txt (1 Hz), status.json (1 Hz), the
        // "[game] state:" transition line, and the crash filter re-arm
        double nowMs = MaimNowMs();
        dvr::command::poll(nowMs);
        dvr::status::tick(nowMs);
        GameStateTick();
        if ((g_frame & 255) == 0) dvr::crash::rearm();
    }

    // 38.17: the crash fingerprinter now guards BOTH backends. Tonight's
    // Vive load crash produced only the game's bare dialog because the VEH
    // was XR-only; every future fault logs module+offset+thread either way.
    dvr::crash::install();   // fingerprint VEH + minidump filter, idempotent
}

// 41.0: XR pose (meters, quaternion, XR LOCAL space: right +X, up +Y, fwd -Z)
// -> the 3x4 device-to-tracking matrix every consumer of g_devPose reads.
// XR LOCAL space matches the OpenVR standing space these consumers were
// written for (same handedness, same axes), so no axis surgery.
static void DvrPoseTo3x4(const dvr::vr::HeadPose& p, float m[3][4])
{
    float xx = p.qx*p.qx, yy = p.qy*p.qy, zz = p.qz*p.qz;
    float xy = p.qx*p.qy, xz = p.qx*p.qz, yz = p.qy*p.qz;
    float wx = p.qw*p.qx, wy = p.qw*p.qy, wz = p.qw*p.qz;
    m[0][0] = 1 - 2*(yy + zz); m[0][1] = 2*(xy - wz); m[0][2] = 2*(xz + wy);
    m[1][0] = 2*(xy + wz); m[1][1] = 1 - 2*(xx + zz); m[1][2] = 2*(yz - wx);
    m[2][0] = 2*(xz - wy); m[2][1] = 2*(yz + wx); m[2][2] = 1 - 2*(xx + yy);
    m[0][3] = p.px; m[1][3] = p.py; m[2][3] = p.pz;
}

// The runtime layer's poses -> the mod's pose slots, once per present. Head
// -> TrackHead (rotation write, positional, crouch); hands -> slots 3 and 4,
// which is where the XR path always put them (g_ctrlIdx = 3/4).
static void DvrConsumePoses()
{
    dvr::vr::HeadPose hp;
    if (dvr::vr::get_head_pose(hp)) {
        float m[3][4];
        DvrPoseTo3x4(hp, m);
        memcpy(g_devPose[0], m, sizeof(g_devPose[0]));
        g_devPoseOk[0] = true;
        TrackHead(m);
    } else {
        g_devPoseOk[0] = false;
        g_haveLastPose = false;
    }
    for (int h = 0; h < 2; h++) {
        float pos[3], q[4];
        if (dvr::vr::input_get_hand_pose(h, false, pos, q)) {
            dvr::vr::HeadPose hpp = { pos[0], pos[1], pos[2], q[0], q[1], q[2], q[3] };
            float m[3][4];
            DvrPoseTo3x4(hpp, m);
            memcpy(g_devPose[3 + h], m, sizeof(g_devPose[0]));
            g_devPoseOk[3 + h] = true;
        } else g_devPoseOk[3 + h] = false;
    }
    g_ctrlIdx[0] = 3; g_ctrlIdx[1] = 4;
    // per-eye frustum + IPD for the hand pass (symmetric half-angles; the
    // runtime layer claims symmetric fovs too)
    float hh = 0, hv = 0;
    if (dvr::vr::headset_half_fov_deg(&hh, &hv) && hh > 0.0f) {
        float th = tanf(hh * 0.0174533f), tv = tanf(hv * 0.0174533f);
        float sep = 0.0f;
        if (dvr::vr::eye_separation_m(&sep) && sep > 0.04f && sep < 0.08f) g_ipdM = sep;
        for (int eye = 0; eye < 2; eye++) {
            g_eyeFr[eye][0] = -th; g_eyeFr[eye][1] = th;
            g_eyeFr[eye][2] = -tv; g_eyeFr[eye][3] = tv;
            g_eyeOffs[eye][0] = (eye == 0 ? -0.5f : 0.5f) * g_ipdM;
            g_eyeOffs[eye][1] = 0.0f; g_eyeOffs[eye][2] = 0.0f;
        }
        g_eyeFrOk = true;
    }
}

// Every enabled present, after the runtime located the head for this frame
// and before the stereo method captures the game's frame.
static void DvrGameTick(IDirect3DDevice9* self)
{
    (void)self;
    g_xrOn = g_vrReady = dvr::frame::xr_live();   // the session, as of this present
        // 30.24: hitch detector. Any Present-to-Present gap over 80 ms gets
        // logged with what was in flight, so "lag spike on swing" becomes a
        // measured correlation instead of a hunch.
        {
            static double lastPresentMs = 0.0;
            double nowMs = MaimNowMs();
            if (lastPresentMs != 0.0) {
                double gap = nowMs - lastPresentMs;
                if (gap > 80.0)
                    Log("perf: frame gap %.0fms  swingAge=%.0fms aimWin=%d cal=%d gt=%d",
                        gap,
                        g_meleeLastMs ? nowMs - g_meleeLastMs : -1.0,
                        (int)(nowMs < g_maimArmedUntil),
                        g_fpCalPhase, (int)g_gtActive);
            }
            lastPresentMs = nowMs;
        }
        if (!g_presentTid) {
            g_presentTid = GetCurrentThreadId();
            dvr::crash::register_thread("present", g_presentTid);
        }
        g_gameFrames++;
        SbTick();   // 30.83: SpaceBases oracle (legacy stub unless -Legacy)
        {   // 34.7: one-shot block-property hunt, ~30 s in so a level is loaded
            static int bhDone = 0;
            if (!bhDone && MaimNowMs() > 30000.0) { bhDone = 1; BlockPropHunt(); }
        }
        StereoUpdate();   // the live-tuning hotkeys
        DvrConsumePoses();
        // 41.0: the camera seam learns the eye the active method wants next,
        // the IPD and the world scale; the eyetest reads c5 back here.
        dvr::camera::set_eye(dvr::stereo::active() ? dvr::stereo::active()->eye_for_next_frame() : 0);
        dvr::camera::set_ipd_m(g_ipdM);
        dvr::camera::set_world_scale(g_posScaleUU);
        dvr::camera::eyetest_present_tick();
        if (!g_padHookTried) { g_padHookTried = true; InstallPadHook(); }
        UpdateVirtualPad();
        FrameDumpTick();
        // UE3 probe: automatic at ~frame 900 and ~frame 14400, or F9 on demand
        bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        bool f9Edge = f9 && !g_f9WasDown;
        g_f9WasDown = f9;
        // PAUSE (or Shift+F9) = ground truth self-test. Plain F9 does nothing
        // of ours any more: it is the game's own quickload bind, and the old
        // engine probe was firing a hitchy object walk on every quickload.
        bool pk = (GetAsyncKeyState(VK_PAUSE) & 0x8000) != 0;
        static bool pkWas = false;
        bool pkEdge = pk && !pkWas;
        pkWas = pk;
        if (pkEdge || (f9Edge && (GetAsyncKeyState(VK_SHIFT) & 0x8000))) {
            if (g_gtActive) GtStop("cancelled");
            else            GtStart();
        }
        {
            static long long hbQpc = 0;
            LARGE_INTEGER now; QueryPerformanceCounter(&now);
            double el = (g_qpcFreq && hbQpc) ? (double)(now.QuadPart - hbQpc) / (double)g_qpcFreq : 0.0;
            if (el >= 3.0) {
                static uint32_t sbPrev = 0;
                double gfps = g_gameFrames / el;
                double sfps = (double)(dvr::frame::submit_count() - sbPrev) / el;
                sbPrev = dvr::frame::submit_count();
                Log("heartbeat: GAME=%.0ffps  headset(submits)=%.0ffps  pos=%d lean=(%+.1f,%+.1f)uu  pad=%d polls=%ld  headwrites=%ld/3s inject=%d idx=%s  lever=%.0f writes=%ld",
                    gfps, sfps,
                    (int)g_posTrack, (float)g_leanRightUU, (float)g_leanUpUU,
                    (int)g_padActive, (long)g_padPolls,
                    (long)g_pvrHits, (int)g_rotInject,
                    g_idxViewRot != 0xffffffffu ? "found" : "hunting",
                    (float)g_fovLever, (long)g_fovLeverWrites);
                Log("heartbeat: head hits=%ld writes=%ld | menu=%d (script=%d) wheel=%d",
                    (long)g_pvrHits, (long)g_pvrWrites, (int)g_inMenu,
                    (int)g_menuOpen, (int)g_wheelHeld);
                // 30.70: one line that says whether the hand drive is alive and
                // whether it is actually finding the rigs. arms=0 or wpn=0 with
                // live=1 means the upload sizes moved - re-run the sweep.
                if (g_rtdEnable)
                    Log("heartbeat: handrt hands=%d neutrals=%d arms=%s | arms(c6 x%u)=%ld "
                        "wpn(c6 x%u)=%ld /3s | Rtrans=(%.1f,%.1f,%.1f)uu",
                        (int)g_rtdHandOk[0] + (int)g_rtdHandOk[1],
                        (int)g_rtdHaveNeutral[0] + (int)g_rtdHaveNeutral[1],
                        g_rtdSplitHi > g_rtdSplitLo ? "split" : "one",
                        g_rtdSizeArms, (long)g_rtdHitsArms,
                        g_rtdSizeWpn, (long)g_rtdHitsWpn,
                        g_rtdT[1][0], g_rtdT[1][1], g_rtdT[1][2]);
                g_rtdHitsArms = 0; g_rtdHitsWpn = 0;
                // 40.1: WHICH DRIVE OWNS THE HANDS. Both older counters below
                // read 0 forever under the current architecture, and a reader
                // who does not know that concludes "the hands are dead" from a
                // healthy run - which is exactly what happened. Name the owner
                // first, and report the counter that belongs to it.
                //
                //   g_skcRotWrites - retired subsystem, always 0.
                //   g_fpWrites     - the LEGACY component drive, which
                //                    skelcontrol.cpp deliberately stands down
                //                    (returns early) whenever g_skcDrive or
                //                    g_rtdEnable owns the rigs. 0 is CORRECT
                //                    there, not a fault.
                //   g_skcHits      - the SkelControl drive: the one that is
                //                    actually writing bones today.
                {
                    static long skcPrev = 0, fpPrev = 0;
                    long skcNow = (long)g_skcHits, fpNow = g_fpWrites;
                    const char* owner = g_skcDrive  ? "SkelControl (g_skcHits)"
                                      : g_rtdEnable ? "render-time drive"
                                      : g_handMesh  ? "legacy component (g_fpWrites)"
                                                    : "NOBODY - hands are not driven";
                    Log("heartbeat: hands OWNER=%s writes=%ld/3s  handMesh=%d "
                        "armsHidden=%d handSize=%.2f",
                        owner, (g_skcDrive ? skcNow - skcPrev : fpNow - fpPrev),
                        (int)g_handMesh, (int)g_armsHidden, g_skcHandSize);
                    if (g_skcDrive && skcNow == skcPrev)
                        DVR_WARN("hands: SkelControl owns the hands but wrote NOTHING in 3s "
                                 "- the graft is not reaching the rig, so HandSize/pose are "
                                 "not being applied and you will see the game's own arms.");
                    // The legacy counters, kept for continuity, tagged so nobody
                    // reads a designed-in zero as a failure again.
                    DVR_DEBUG("heartbeat: (legacy) rotWrites=%ld fpWrites=%ld/3s sel=%d "
                              "cand=%d fpTarget=%s%s",
                              (long)g_skcRotWrites, fpNow - fpPrev, g_fpSel, g_fpCandN,
                              (g_fpWritten && LooksLikeObj(g_fpWritten)) ? "live" : "STALE/none",
                              (g_skcDrive || g_rtdEnable)
                                ? "  (legacy drive stood down on purpose - 0 is expected)" : "");
                    skcPrev = skcNow; fpPrev = fpNow;
                }
                g_skcRotWrites = 0;
                memcpy(g_rtdCensusSnap, g_rtdCensus, sizeof(g_rtdCensusSnap));
                memset(g_rtdCensus, 0, sizeof(g_rtdCensus));
                g_pvrHits = 0; g_pvrWrites = 0; g_fovLeverWrites = 0;
                g_gameFrames = 0; g_padPolls = 0;
                hbQpc = now.QuadPart;
            } else if (!hbQpc) {
                hbQpc = now.QuadPart;
            }
        }
        HeadInjectTick();
        RotInjectTick();
        SteerTick();

        // ---------------------------------------------------------------
        // Key map after the 30.9 diet - one job each:
        //   F1/F2  image size        F3  bone probe (dump to log)
        //   F4     lean toggle       F5  recentre         F6 hook fallback
        //   F7     stereo            HOME weapon tracking END recalibrate
        //   Arrows trim weapon pos (Shift+L/R switch hand, Shift+U/D pivot)
        //   PAUSE (or Shift+F9)      ground-truth self-test
        // Head tracking is always-on (ini: [HeadTrack] Native=0 to disable).
        // [Debug] Probe= in the ini can also schedule diagnostics key-free.
        // ---------------------------------------------------------------
        // weapon-watch phase machine (buzz marks each transition)
        if (g_wwPhase && MaimNowMs() > g_wwPhaseEnd) {
            if (g_wwPhase == 1) {
                g_wwPhase = 2;
                g_wwPhaseEnd = MaimNowMs() + 15000.0;
                g_wwHistLen = 0;             // histogram covers the window only
                MaimHaptic(g_maimHand, 0.9f, 0.25f);
                Log("weapon: === FIRE NOW === (15 s) - %ld baseline event(s) learned",
                    (long)g_wwBaseN);
            } else {
                g_wwPhase = 0;
                MaimHaptic(g_maimHand, 0.5f, 0.15f);
                Log("weapon: done - %ld new event(s) appeared while firing",
                    (long)g_wwLines);
                LONG hl = g_wwHistLen; if (hl > 256) hl = 256;
                for (int pass = 0; pass < 30; pass++) {
                    int best = -1; uint32_t bc = 0;
                    for (LONG i = 0; i < hl; i++)
                        if (g_wwHistC[i] > bc) { bc = g_wwHistC[i]; best = (int)i; }
                    if (best < 0 || bc == 0) break;
                    const char* nm = RealName(g_wwHistN[best]);
                    const char* cn = RealName(g_wwHistCls[best]);
                    Log("weapon: freq %5u  %-36s on %s",
                        bc, nm ? nm : "?", cn ? cn : "?");
                    g_wwHistC[best] = 0;
                }
            }
        }
        // the script hook IS head tracking now, so bring it up on its own once
        // the game is running (it verifies the prologue before patching)
        if (!g_peInstalled && g_frame == 600 && g_rotInject) InstallProcessEventHook();
        {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

            bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
            static bool f6Was = false;
            if (f6 && !f6Was) {
                MaimHaptic(g_maimHand, 0.7f, 0.10f);
                if (!g_peInstalled) {
                    InstallProcessEventHook();         // start listening
                } else {
                    g_wwLines = 0; g_wwBaseN = 0;
                    g_wwPhase = 1;
                    g_wwPhaseEnd = MaimNowMs() + 6000.0;
                    Log("weapon: BASELINE for 6 s - walk around, do NOT fire");
                }
            }
            f6Was = f6;

            // HOME = first-person mesh follows the hand.  END = dump its matrix.
            bool hom = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
            static bool homWas = false;
            if (hom && !homWas) {
                g_autoHandDone = true;         // manual choice wins from here
                g_handMesh = !g_handMesh;
                g_fpHaveBase = false;
                if (!g_handMesh) FpRestoreRotation(); else FpCaptureNeutral("switched on");
                g_fpWrites = 0; g_fpRestores = 0;
                MaimHaptic(g_maimHand, 0.8f, 0.12f);
                Log("handmesh: %s (%s hand)",
                    g_handMesh ? "ON - mesh follows the hand" : "off",
                    g_maimHand ? "right" : "left");
                if (g_handMesh && !g_peInstalled)
                    Log("handmesh: script hook is not installed - press F6 first");
            }
            homWas = hom;

            // F3 = bone probe (read-only dump for the arms/hands work). The
            // key used to toggle head tracking, which nobody ever needed -
            // that just works by default and stays untouched.
            bool f3k = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
            static bool f3kWas = false;
            if (f3k && !f3kWas) {
                MaimHaptic(g_maimHand, 0.5f, 0.08f);
                Log("arms: F3 pressed - toggle queued for the game thread");
                g_armReq = 1;        // executed by DbgProbeTick, serialized
            }
            f3kWas = f3k;

            // END = the one fix-it key: recapture neutral + full recalibration.
            // (All diagnostics moved to [Debug] Probe= in the ini - no more
            // Shift chords, which numpad keys silently eat anyway.)
            bool endk = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
            static bool endWas = false;
            if (endk && !endWas) {
                MaimHaptic(g_maimHand, 0.5f, 0.08f);
                // 33.1: END now drops EVERY cached pointer first. After a
                // death-reload the collect can rebuild and calibration can
                // pass while writes still target recycled-but-readable
                // objects - all green lights, no power. The user's one
                // fix-it key must not trust anything: forget candidates,
                // selection, bases and neutral, force a fresh collect, then
                // recapture. (And it turns the drive ON if it was off - the
                // "HOME toggled it off and nobody knew" trap.)
                g_fpCandN = 0; g_fpSel = -1;
                g_fpWritten = NULL; g_fpWritten2 = NULL;
                g_fpRef = NULL; g_fpHaveRef = false;
                g_fpHaveBase = false;
                g_fpCollectMs = 0.0;               // collect NOW
                if (!g_handMesh) {
                    g_handMesh = true;
                    g_autoHandDone = true;
                    Log("handmesh: END pressed while OFF - drive switched ON");
                }
                // 33.2: END must reset the drive that actually OWNS the
                // hands - the SkelControl drive - not just the legacy one.
                // (33.1's END rebuilt the wrong subsystem; measured.)
                g_skcStale = 0;
                g_skcPlayerN = 0;
                g_skcProbeFails = 0;
                g_skcCamIdx = -1;
                for (int z2 = 0; z2 < 8; z2++) {
                    g_skcPlayer[z2] = NULL; g_skcObjIdx[z2] = 0;
                    g_skcObjCls[z2] = NULL; g_skcHandOf[z2] = -1;
                }
                g_skcReq = 1;                     // probe NOW
                FpCaptureNeutral("END pressed (full reset)");
                // 33.6: zero the rotation drive here too - whatever the
                // controllers' orientation is at END is the new "weapon
                // points forward"
                // 35.8: factored into SkcRotZeroNeutral (the overlay button
                // needs it too), and the graft drive counts as a live drive.
                if ((g_skcRotDrive || g_graftOn || g_graftWant) && g_injSnapOk)
                    SkcRotZeroNeutral("END pressed");
                Log("handmesh: END = full reset - SkelControl cache dropped, "
                    "immediate re-probe, candidates dropped, fresh collect");
            }
            endWas = endk;

            // (30.8 key diet: PageUp/PageDown/Delete flip keys are gone - the
            // ground-truth-measured math has nothing left for a sign to fix,
            // and pressing one would only mirror a correct answer. INSERT
            // cycling is gone too; collection is automatic. Wrist roll and
            // depth live in [HandTracking] in the ini.)

            // Arrow keys pull the trimmed hand's weapon around in your view:
            // LEFT/RIGHT slide it sideways, UP/DOWN push it away or closer.
            // Shift+LEFT/RIGHT switches which hand you are trimming.
            {
                struct { int vk; int axis; float step; } trim[4] = {
                    { VK_LEFT,  1, -3.0f }, { VK_RIGHT, 1, +3.0f },
                    { VK_UP,    0, +3.0f }, { VK_DOWN,  0, -3.0f },
                };
                static bool was[4] = { false, false, false, false };
                for (int i = 0; i < 4; i++) {
                    bool down = (GetAsyncKeyState(trim[i].vk) & 0x8000) != 0;
                    if (down && !was[i]) {
                        if (shift && trim[i].axis == 1) {
                            g_fpTrimHand = 1 - g_fpTrimHand;
                            Log("handmesh: now trimming the %s hand",
                                g_fpTrimHand ? "RIGHT" : "LEFT");
                        } else if (shift) {
                            float* pm = &g_fpPivotMix2[g_fpTrimHand];
                            *pm += (trim[i].step > 0.0f) ? 0.25f : -0.25f;
                            if (*pm < 0.0f) *pm = 0.0f;
                            if (*pm > 1.5f) *pm = 1.5f;
                            Log("handmesh: %s-hand pivot blend %.2f",
                                g_fpTrimHand ? "RIGHT" : "LEFT", *pm);
                        } else {
                            g_fpBias[g_fpTrimHand][trim[i].axis] += trim[i].step;
                            Log("handmesh: %s-hand trim now fwd=%.0f right=%.0f up=%.0f uu",
                                g_fpTrimHand ? "RIGHT" : "LEFT",
                                g_fpBias[g_fpTrimHand][0], g_fpBias[g_fpTrimHand][1],
                                g_fpBias[g_fpTrimHand][2]);
                        }
                        MaimHaptic(g_maimHand, 0.5f, 0.05f);
                    }
                    was[i] = down;
                }
            }
        }

}

static void DvrBeforeCreateDevice(D3DPRESENT_PARAMETERS* pp) { UncapPresent(pp, "CreateDevice"); }

static void DvrAfterCreateDevice(HRESULT hr, HWND wnd, D3DPRESENT_PARAMETERS* pp)
{
    if (pp) {                                            // 32.9
        g_gameWindowed = pp->Windowed != FALSE;
        if (g_gameWindowed)
            Log("menu: game is WINDOWED - the desktop cursor is always showing, "
                "so the cursor half of the menu test is disabled and script "
                "events are the only menu signal (this is correct, not a "
                "degradation)");
    }
    // The subclass goes on the moment the device exists (38.92): it carries
    // the focus keep-alive, and the overlay needs it before ImGui is up.
    InstallWindowSubclass("CreateDevice");
    if (SUCCEEDED(hr)) g_gameWnd = wnd ? wnd : (pp ? pp->hDeviceWindow : NULL);
}

static void DvrBeforeReset(D3DPRESENT_PARAMETERS* pp)
{
    UncapPresent(pp, "Reset");
    if (pp) g_gameWindowed = pp->Windowed != FALSE;      // 32.9
}

static ID3D11Device* DvrFrameD3D11(ID3D11DeviceContext** ctx)
{
    if (!EnsureD3D11()) return NULL;
    if (ctx) *ctx = g_ctx11;
    return g_dev11;
}

// The F10 overlay, drawn into the stereo method's output texture (ImGui only
// from here - the overlay's own draw callback).
static void DvrOverlayDraw(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, uint32_t w, uint32_t h)
{
    OverlayFrame();
    if (!g_ovlVisible || !g_ovlInit) return;
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
    ctx->RSSetViewports(1, &vp);
    ctx->OMSetRenderTargets(1, &rtv, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

static void DvrInstallFrameHooks()
{
    dvr::frame::Callbacks cb;
    cb.before_create_device = DvrBeforeCreateDevice;
    cb.after_create_device  = DvrAfterCreateDevice;
    cb.before_reset         = DvrBeforeReset;
    cb.pre_tick             = DvrPreTick;
    cb.game_tick            = DvrGameTick;
    cb.set_vs_const         = hkSetVSConstF;
    cb.set_render_target    = hkSetRenderTarget;
    cb.d3d11                = DvrFrameD3D11;
    dvr::frame::set_callbacks(cb);
    dvr::stereo::set_overlay_draw(DvrOverlayDraw);
}
