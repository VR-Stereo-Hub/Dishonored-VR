// core/framework/frame_hooks.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ============================================================================
// 30.66 - RENDER-TIME BONE TEST. Writing SpaceBases/LocalAtoms in memory does
// nothing (30.62-65: no effect even at 25uu), so the renderer keeps its own
// copy of the pose. But the frame map showed where the FINAL pose lands: the
// skinned bone matrices are uploaded as vertex constants starting at c6 (3
// float4 rows per bone, 30..159 registers). Nothing can overwrite those - the
// draw consumes them immediately - and we already hook the upload.
//
// This test offsets the first-person rig at that upload. The FP rig is told
// apart from world characters by distance: with camera-relative rendering the
// camera position rides in c5, and only the player's own arms/weapon sit
// within a couple of feet of it.
// ============================================================================
static HRESULT __stdcall hkSetVSConstF(IDirect3DDevice9* self, UINT startReg,
                                       const float* data, UINT count)
{
    // c5 = camera world position (frame-map ABI)
    if (startReg == 5 && count >= 1 && data) {
        g_camPosC5[0] = data[0]; g_camPosC5[1] = data[1]; g_camPosC5[2] = data[2];
        g_haveC5 = true;
    }

    // ---- 30.70: live rig census + the stepped identifier -------------------
    // One unguarded increment per skinned upload (render thread only, no lock,
    // no logging) - this is the cheap kind of instrumentation, and it is what
    // lets the overlay offer the sizes that actually exist instead of a list
    // guessed months ago.
    // ---- 30.74: constant-map capture --------------------------------------
    if (data && count >= 1 && count <= 4 && startReg < 260) {
        VcEnt e; e.reg = startReg; e.count = count;
        memcpy(e.v, data, sizeof(float) * 4 * count);
        if (g_vcAfter > 0) {
            if (InterlockedDecrement(&g_vcAfter) >= 0) {
                if (e.count == 4)
                    Log("vcmap:  AFTER  c%-3u x4  t=(%8.1f %8.1f %8.1f) "
                        "r0=(%6.2f %6.2f %6.2f) r1=(%6.2f %6.2f %6.2f)",
                        e.reg, e.v[3], e.v[7], e.v[11],
                        e.v[0], e.v[1], e.v[2], e.v[4], e.v[5], e.v[6]);
                else
                    Log("vcmap:  AFTER  c%-3u x%u  [%8.3f %8.3f %8.3f %8.3f]",
                        e.reg, e.count, e.v[0], e.v[1], e.v[2], e.v[3]);
            } else g_vcAfter = 0;
        } else {
            g_vcRing[g_vcRingN % 48] = e; g_vcRingN++;
        }
    }
    if (g_vcArmed && data && startReg == 6 && count == g_rtdSizeArms) {
        g_vcArmed = 0;
        Log("vcmap: ==== constants in effect at the ARMS draw (c6 x%u) ====", count);
        int n = g_vcRingN < 48 ? g_vcRingN : 48;
        int base = g_vcRingN < 48 ? 0 : (g_vcRingN % 48);
        for (int q = 0; q < n; q++) {
            VcEnt* e = &g_vcRing[(base + q) % 48];
            if (e->count == 4)
                Log("vcmap:  before c%-3u x4  [%8.2f %8.2f %8.2f %8.2f] "
                    "[%8.2f %8.2f %8.2f %8.2f] [%8.2f %8.2f %8.2f %8.2f] "
                    "[%8.2f %8.2f %8.2f %8.2f]", e->reg,
                    e->v[0],e->v[1],e->v[2],e->v[3], e->v[4],e->v[5],e->v[6],e->v[7],
                    e->v[8],e->v[9],e->v[10],e->v[11], e->v[12],e->v[13],e->v[14],e->v[15]);
            else
                Log("vcmap:  before c%-3u x%u  [%8.3f %8.3f %8.3f %8.3f]",
                    e->reg, e->count, e->v[0], e->v[1], e->v[2], e->v[3]);
        }
        Log("vcmap:  (camera c5 = %.1f %.1f %.1f)", g_camPosC5[0], g_camPosC5[1], g_camPosC5[2]);
        g_vcAfter = 24;
        g_vcRingN = 0;
    }

    // 30.72: a count of 4 at c6 is a plain per-draw LocalToWorld for a STATIC
    // mesh - which is what the unmoving half of the crossbow is. Its
    // translation is a real world position, so a camera-distance filter works
    // here even though it was hopeless for rig-local bone data.
    if (data && startReg == 6 && count == 4 && g_haveC5 && g_rtdStaticLog > 0) {
        float dx = data[3] - g_camPosC5[0];
        float dy = data[7] - g_camPosC5[1];
        float dz = data[11] - g_camPosC5[2];
        float d = sqrtf(dx*dx + dy*dy + dz*dz);
        if (d < g_rtdStaticRadius && InterlockedDecrement(&g_rtdStaticLog) >= 0)
            Log("handrt/static: c6 x4 pos=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f,%.1f) "
                "dist=%.1f  row0=(%.3f,%.3f,%.3f)", data[3], data[7], data[11],
                g_camPosC5[0], g_camPosC5[1], g_camPosC5[2], d,
                data[0], data[1], data[2]);
    }

    if (g_sbGo && data && startReg == 6 && count >= 3 && count <= 255) {
        int qi = count / 3;
        float v = data[3];
        if (v < g_sbMin[qi]) g_sbMin[qi] = v;
        if (v > g_sbMax[qi]) g_sbMax[qi] = v;
        g_sbSeen[qi]++;
    }
    if (g_sbGo && data && startReg == 6) {
        if (count == g_rtdSizeArms) {
            g_sbC6[0] = data[3]; g_sbC6[1] = data[7]; g_sbC6[2] = data[11];
        } else if (count == g_rtdSizeWpn) {
            g_sbC6w[0] = data[3]; g_sbC6w[1] = data[7]; g_sbC6w[2] = data[11];
        }
    }
    UINT myOrd = 0xffffffffu;
    if (data && startReg == 6 && count >= 3 && count <= 255) {
        g_rtdCensus[count / 3]++;
        // number the uploads of the ARMS size within this frame: the two arms
        // are separate draws, so the ordinal is the only handle on which is which
        static uint32_t ordFrame = 0xffffffffu;
        static UINT     ordN = 0;
        if (g_frame != ordFrame) {
            ordFrame = g_frame;
            if (ordN) g_rtdOrdSeen = ordN;
            ordN = 0;
        }
        if (count == g_rtdSizeArms) myOrd = ordN++;

        // 30.72 SPACE PROBE. The one measurement that settles everything: if
        // the bone data is camera-ALIGNED, a bone's translation is unchanged as
        // you turn on the spot; if it is camera-relative WORLD, the same
        // translation swings round with your yaw. Sample it against hmdYaw for
        // a few seconds and the answer falls straight out of the numbers.
        if (g_rtdProbeOn && count == g_rtdIdSize) {
            double now = MaimNowMs();
            if (now >= g_rtdProbeNext) {
                g_rtdProbeNext = now + 150.0;
                float cx = 0, cy = 0, cz = 0; int nb = 0;
                for (UINT q = 0; q + 3 <= count; q += 3) {
                    cx += data[(q+0)*4+3]; cy += data[(q+1)*4+3]; cz += data[(q+2)*4+3];
                    nb++;
                }
                if (nb) { cx /= nb; cy /= nb; cz /= nb; }
                Log("handrt/space: c6 x%u ord=%d hmdYaw=%+7.1f hmdPitch=%+6.1f "
                    "bone0=(%8.2f,%8.2f,%8.2f) centroid=(%8.2f,%8.2f,%8.2f) "
                    "cam=(%.0f,%.0f,%.0f)", count,
                    (myOrd == 0xffffffffu) ? -1 : (int)myOrd,
                    g_hmdYaw * 57.2958f, g_hmdPitch * 57.2958f,
                    data[3], data[7], data[11], cx, cy, cz,
                    g_camPosC5[0], g_camPosC5[1], g_camPosC5[2]);
            }
            if (now >= g_rtdProbeUntil) {
                g_rtdProbeOn = false;
                Log("handrt/space: probe done - if bone0 stayed put the rig is "
                    "CAMERA-aligned (follow 1); if it swung with hmdYaw it is "
                    "WORLD-aligned (follow 0)");
            }
        }

        if (g_rtdIdGo && count == g_rtdIdSize) {
            if (g_rtdIdOrd >= 0 && count == g_rtdSizeArms &&
                myOrd != (UINT)g_rtdIdOrd) {
                return g_origSetVSConstF(self, startReg, data, count);
            }
            float per = (g_rtdIdPeriodMs > 100.0f) ? g_rtdIdPeriodMs : 100.0f;
            float off = g_rtdIdAmp * sinf((float)(MaimNowMs() * (6.2831853 / per)));
            float buf[4 * 256];
            memcpy(buf, data, sizeof(float) * 4 * count);
            bool range = (g_rtdIdHi > g_rtdIdLo);
            for (UINT b = 0; b + 3 <= count; b += 3) {
                if (range) {                    // only the bones under inspection
                    int bone = (int)(b / 3);
                    if (bone < g_rtdIdLo || bone >= g_rtdIdHi) continue;
                }
                buf[b * 4 + 3] += off;          // rig X (forward): slides toward/away
            }
            return g_origSetVSConstF(self, startReg, buf, count);
        }
    }

    // ---- 30.77: hide the game's view model ---------------------------------
    // We could never PLACE these rigs, but we can certainly collapse them: zero
    // the bone matrices and every vertex lands on one point, so the mesh has no
    // area and draws nothing. Cheaper and far more reliable than the draw-
    // distance cull, and it needs no game state at all.
    if (g_hmEnable && g_hmHideGame && data && startReg == 6 && !g_rtdIdGo) {
        if (count >= 3 && count <= 250) {
            for (int q = 0; q < 12 && g_hmHideSize[q]; q++)
                if (count == g_hmHideSize[q]) {
                    static float zero[4 * 256] = { 0 };
                    g_hmStaticWindow = g_hmStaticDraws;   // its attachments follow
                    return g_origSetVSConstF(self, startReg, zero, count);
                }
        }
        // static attachments (the crossbow's body): world position in .w, and
        // ONLY while we are still inside the view-model cluster
        if (g_hmHideStatic && count == 4 && g_haveC5 && g_hmStaticWindow > 0) {
            InterlockedDecrement(&g_hmStaticWindow);
            float dx = data[3] - g_camPosC5[0];
            float dy = data[7] - g_camPosC5[1];
            float dz = data[11] - g_camPosC5[2];
            if (dx*dx + dy*dy + dz*dz < g_hmHideStaticUU * g_hmHideStaticUU) {
                static float zero4[16] = { 0 };
                return g_origSetVSConstF(self, startReg, zero4, count);
            }
        }
    }

    // ---- 30.70/71: THE HAND/WEAPON DRIVE -----------------------------------
    // The measured write point. 3 registers per bone from c6, rig-local, and
    // the draw consumes them immediately - nothing downstream can undo this,
    // which is the entire reason the old component drive could not win.
    //
    // Routing: each rig is assigned to a controller, and the ARMS rig can be
    // split by bone index so the right arm follows the right hand and the left
    // arm the left. Held off while a diagnostic owns the same uploads.
    if (data && startReg == 6 && g_rtdOn && !g_boneRtGo && !g_rtdIdGo &&
        count >= 3 && count <= 250) {
        int  handAll = -1;
        bool weapon = false, split = false;
        if (count == g_rtdSizeArms && g_rtdDoArms) {
            // 30.72: prefer the ordinal, because the arms are separate draws.
            // The bone-index split stays available for a rig that really does
            // hold both arms, but it demonstrably is not this one.
            if (g_rtdUseOrdinals && myOrd < 8) {
                handAll = g_rtdOrdHand[myOrd];
                if (handAll < 0) return g_origSetVSConstF(self, startReg, data, count);
            } else {
                handAll = g_rtdArmsHand;
                split   = (g_rtdSplitHi > g_rtdSplitLo);
            }
        } else if (count == g_rtdSizeWpn && g_rtdDoWpn) {
            handAll = g_rtdWpnHand;  weapon = true;
        } else if (g_rtdSizeWpn2 && count == g_rtdSizeWpn2 && g_rtdDoWpn) {
            handAll = g_rtdWpn2Hand; weapon = true;
        }
        if (handAll >= 0) {
            float R[2][9], T[2][3];
            bool ok[2] = { false, false };
            if (split) {                       // both hands drive this one rig
                ok[0] = RtdSnapshot(R[0], T[0], 0, weapon);
                ok[1] = RtdSnapshot(R[1], T[1], 1, weapon);
            } else {
                ok[handAll] = RtdSnapshot(R[handAll], T[handAll], handAll, weapon);
            }
            if (ok[0] || ok[1]) {
                float buf[4 * 256];
                memcpy(buf, data, sizeof(float) * 4 * count);   // untouched bones pass through
                for (UINT b = 0; b + 3 <= count; b += 3) {
                    int bone = (int)(b / 3);
                    int h = split ? ((bone >= g_rtdSplitLo && bone < g_rtdSplitHi) ? 1 : 0)
                                  : handAll;
                    if (!ok[h]) continue;                       // that hand is asleep
                    const float* r0 = data + (b + 0) * 4;
                    const float* r1 = data + (b + 1) * 4;
                    const float* r2 = data + (b + 2) * 4;
                    for (int i = 0; i < 3; i++) {
                        float a0 = R[h][i*3+0], a1 = R[h][i*3+1], a2 = R[h][i*3+2];
                        float* o = buf + (b + i) * 4;
                        o[0] = a0*r0[0] + a1*r1[0] + a2*r2[0];
                        o[1] = a0*r0[1] + a1*r1[1] + a2*r2[1];
                        o[2] = a0*r0[2] + a1*r1[2] + a2*r2[2];
                        o[3] = a0*r0[3] + a1*r1[3] + a2*r2[3] + T[h][i];  // translation rides .w
                    }
                }
                InterlockedIncrement(weapon ? &g_rtdHitsWpn : &g_rtdHitsArms);
                if (InterlockedDecrement(&g_rtdLogReq) >= 0) {
                    // The CENTROID of the bone translations locates the rig's
                    // own origin relative to its bones - that is the number the
                    // pivot needs, measurable here instead of guessed.
                    float cx = 0, cy = 0, cz = 0; int nb = 0;
                    for (UINT b = 0; b + 3 <= count; b += 3) {
                        cx += data[(b+0)*4+3]; cy += data[(b+1)*4+3]; cz += data[(b+2)*4+3];
                        nb++;
                    }
                    if (nb) { cx /= nb; cy /= nb; cz /= nb; }
                    Log("handrt: %s c6 x%u (%d bones) ord=%d hand=%s  frame=%lu  "
                        "bone0 t=(%.1f,%.1f,%.1f)  centroid=(%.1f,%.1f,%.1f)  "
                        "hmdYaw=%.1f cam=(%.0f,%.0f,%.0f)",
                        weapon ? "WPN " : "ARMS", count, nb,
                        (myOrd == 0xffffffffu) ? -1 : (int)myOrd,
                        split ? "split" : (handAll ? "R" : "L"),
                        (unsigned long)g_frame, data[3], data[7], data[11],
                        cx, cy, cz, g_hmdYaw * 57.2958f,
                        g_camPosC5[0], g_camPosC5[1], g_camPosC5[2]);
                } else {
                    g_rtdLogReq = 0;
                }
                return g_origSetVSConstF(self, startReg, buf, count);
            }
        }
    }

    // --- c0 view-projection: positional lean (the head matrix branch is the
    // retired camera-inject experiment; it stays until the camera seam lands)
    bool wantHead   = g_injectHead && g_haveA;
    bool wantPos    = g_posTrack && (g_leanRightUU != 0.0f || g_leanUpUU != 0.0f || g_leanFwdUU != 0.0f);
    if ((wantHead || wantPos) && data && count >= 4 && count <= 240 &&
        startReg == 0) {
        const float* m = data;
        bool affine = IsAffineRowMajor(m) || IsAffineColMajor(m);
        if (!affine && !IsMirrored(m) && IsMainScenePass() && Finite16(m)) {
            float buf[4 * 240];
            memcpy(buf, data, sizeof(float) * 4 * count);
            if (wantHead) {
                // first 4 registers = the view-projection; M = A * VP
                float M[16]; const float* VP = buf;
                for (int i = 0; i < 4; i++)
                    for (int j = 0; j < 4; j++) {
                        float s = 0;
                        for (int k = 0; k < 4; k++) s += g_viewA[i*4+k] * VP[k*4+j];
                        M[i*4+j] = s;
                    }
                memcpy(buf, M, sizeof(float) * 16);
            }
            if (wantPos)    LeanVP(buf);
            if (Finite16(buf)) {              // never forward a poisoned matrix
                return g_origSetVSConstF(self, startReg, buf, count);
            }
        }
    }
    return g_origSetVSConstF(self, startReg, data, count);
}


static HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9* self, DWORD idx,
                                           IDirect3DSurface9* rt)
{
    if (idx == 0 && rt) {
        D3DSURFACE_DESC d;
        if (SUCCEEDED(rt->GetDesc(&d))) { g_curRTw = d.Width; g_curRTh = d.Height; }
    }
    return g_origSetRT(self, idx, rt);
}


static HRESULT __stdcall hkPresent(IDirect3DDevice9* self, const RECT* src,
                                   const RECT* dst, HWND wnd, const RGNDATA* dirty)
{
    // 38.79: once the game has announced shutdown, our VR work stands down
    // completely - an affected user's log ended in "xr: EXCEPTION 0xc0000005
    // at dededede" (a call through freed memory) AFTER GameSessionEnded/
    // PreExit, i.e. a crash dialog on every quit. Nothing of ours may touch
    // the dying device or the XR session past that point.
    if (InterlockedCompareExchange(&g_gameExiting, 0, 0))
        return g_origPresent(self, src, dst, wnd, dirty);
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

    // 38.14: [VR] FpsCap - hold the game at a ROCK-STEADY rate. The measured
    // stutter cause on Quest: game fps wandering 66-80 against a fixed
    // 72/90 Hz display = uneven frame cadence (some vsyncs get one new game
    // frame, some get two). SSW's synthesized frames "feel horrible" (user),
    // so the honest fix is an even cadence: pin the game to the display rate
    // (72 with VD@72) or exactly half (45 with VD@90) and let plain
    // reprojection keep head motion perfectly smooth. Sleep-then-spin for
    // sub-millisecond accuracy; 0 = off (default; Vive path untouched).
    if (g_fpsCap > 0.0f && g_qpcFreq && g_xrOn) {   // XR sessions only: the
        // shared ini must never re-cadence the Vive rig (auto -> OPENVR
        // ignores the cap entirely)
        static LONGLONG nextDue = 0;
        LARGE_INTEGER fc; QueryPerformanceCounter(&fc);
        LONGLONG period = (LONGLONG)((double)g_qpcFreq / g_fpsCap);
        if (nextDue == 0 || fc.QuadPart > nextDue + 4 * period)
            nextDue = fc.QuadPart;           // (re)sync after a real hitch
        LONGLONG wait = nextDue - fc.QuadPart;
        if (wait > 0) {
            double ms = (double)wait * 1000.0 / (double)g_qpcFreq;
            if (ms > 2.0) Sleep((DWORD)(ms - 1.5));
            do { QueryPerformanceCounter(&fc); }
            while (fc.QuadPart < nextDue);
        }
        nextDue += period;
    }

    g_frame++;
    if (!g_disabled) {
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
        // 37.3: the OpenXR backend boots itself (5 s backoff inside);
        // readiness comes from the XR bring-up.
        if (g_xrBackend && !g_xrOn && (g_frame == 1 || g_frame % 60 == 0))
            XrRtTryInit();
        if (g_xrOn && !g_vrReady) {
            g_vrReady = true;
            Log("xr: pipeline READY - frames flow to the headset from here");
        }
        // 41.0: the side-by-side present pipeline is gone. The present path is
        // rebuilt on the stereo seam (docs/ARCHITECTURE.md); until it lands,
        // only the pose and gamepad lanes run here and nothing reaches a
        // headset. Fail soft: the game runs flat.
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
        if (g_vrReady) {
            if (!g_padHookTried) { g_padHookTried = true; InstallPadHook(); }
            UpdateVirtualPad();
        }
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
                double gfps = g_gameFrames / el, sfps = g_submits / el;
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
                g_gameFrames = 0; g_submits = 0; g_padPolls = 0;
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
    return g_origPresent(self, src, dst, wnd, dirty);
}


static HRESULT __stdcall hkReset(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* pp)
{
    UncapPresent(pp, "Reset");
    if (pp) g_gameWindowed = pp->Windowed != FALSE;      // 32.9
    Log("device Reset (%ux%u windowed=%d)",
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->Windowed : -1);
    if (g_sysmem) { g_sysmem->Release(); g_sysmem = NULL; }
    g_capW = g_capH = 0; g_capFmt = D3DFMT_UNKNOWN;
    // Every default-pool resource this proxy creates must appear on this list
    // (38.63: a forgotten one made the game's Reset fail forever).
    HRESULT rhr = g_origReset(self, pp);
    return rhr;
}


static HRESULT __stdcall hkCreateDevice(IDirect3D9* self, UINT adapter,
                                        D3DDEVTYPE type, HWND wnd, DWORD flags,
                                        D3DPRESENT_PARAMETERS* pp,
                                        IDirect3DDevice9** outDev)
{
    UncapPresent(pp, "CreateDevice");
    HRESULT hr = g_origCreateDevice(self, adapter, type, wnd, flags, pp, outDev);
    if (pp) {                                            // 32.9
        g_gameWindowed = pp->Windowed != FALSE;
        if (g_gameWindowed)
            Log("menu: game is WINDOWED - the desktop cursor is always showing, "
                "so the cursor half of the menu test is disabled and script "
                "events are the only menu signal (this is correct, not a "
                "degradation)");
    }
    Log("CreateDevice -> 0x%08lx (%ux%u windowed=%d)",
        (unsigned long)hr,
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->Windowed : -1);
    // The subclass goes on the moment the device exists (38.92): it carries
    // the focus keep-alive, and the overlay needs it before ImGui is up.
    InstallWindowSubclass("CreateDevice");
    if (SUCCEEDED(hr) && outDev && *outDev) {
        g_gameWnd = wnd ? wnd : (pp ? pp->hDeviceWindow : NULL);
        void* oldPresent = PatchVtable(*outDev, 17, (void*)hkPresent);
        if (oldPresent && !g_origPresent) g_origPresent = (PFN_Present)oldPresent;
        void* oldReset = PatchVtable(*outDev, 16, (void*)hkReset);
        if (oldReset && !g_origReset) g_origReset = (PFN_Reset)oldReset;
        void* oldVSC = PatchVtable(*outDev, 94, (void*)hkSetVSConstF); // SetVertexShaderConstantF
        if (oldVSC && !g_origSetVSConstF) g_origSetVSConstF = (PFN_SetVSConstF)oldVSC;
        void* oldSRT = PatchVtable(*outDev, 37, (void*)hkSetRenderTarget); // SetRenderTarget
        if (oldSRT && !g_origSetRT) g_origSetRT = (PFN_SetRenderTarget)oldSRT;
        Log("device hooks installed (Present/Reset/SetVSConstF/SetRenderTarget)");
    }
    return hr;
}
