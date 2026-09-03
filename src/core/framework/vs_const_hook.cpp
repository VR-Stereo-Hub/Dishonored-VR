// core/framework/vs_const_hook.cpp - the SetVertexShaderConstantF and
// SetRenderTarget detours (unity build; registered by present_tick.cpp).
// c5 is the render-side camera position (the frame-map ABI), c0 the view-
// projection the positional lean patches, c6.. the skinned bone matrices the
// hand drives rewrite. Bodies verbatim from the 38.92 present hook; line
// numbers in comments refer to src/dllmain.cpp at commit 48766c07.

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
    // c5 = camera world position (frame-map ABI). 41.0: caught inside ANY upload
    // that covers register 5, not only one starting there - after the device
    // Resets a level load brings (the game's AA setting), the engine batched it
    // into a wider block and the seam saw no c5 for a whole run (2026-09-02).
    if (data && startReg <= 5 && startReg + count > 5) {
        const float* c5 = data + (5 - startReg) * 4;
        g_camPosC5[0] = c5[0]; g_camPosC5[1] = c5[1]; g_camPosC5[2] = c5[2];
        g_haveC5 = true;
        dvr::camera::note_render_pos(c5);   // the seam's render-side truth
        if (startReg != 5)
            DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 2,
                            "vsconst: c5 arrives inside a c%u x%u block (%.1f %.1f %.1f)",
                            startReg, count, c5[0], c5[1], c5[2]);
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
                return dvr::frame::orig_set_vs_const(self, startReg, data, count);
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
            return dvr::frame::orig_set_vs_const(self, startReg, buf, count);
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
                    return dvr::frame::orig_set_vs_const(self, startReg, zero, count);
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
                return dvr::frame::orig_set_vs_const(self, startReg, zero4, count);
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
                if (handAll < 0) return dvr::frame::orig_set_vs_const(self, startReg, data, count);
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
                return dvr::frame::orig_set_vs_const(self, startReg, buf, count);
            }
        }
    }

    // --- c0 view-projection: positional lean (the head matrix branch is the
    // retired camera-inject experiment; it stays until the camera seam lands)
    // 41.1: the lean rides this patch only on the vp lane ([PosTrack] Lane=vp,
    // the shipped path); on the camera lane the seam writes it into the camera
    // field with the eye offset and this patch stays out.
    bool wantHead   = g_injectHead && g_haveA;
    bool wantPos    = false;
    if (g_posTrack && dvr::camera::pos_lane() == dvr::camera::PosLane::Vp) {
        float pos[3];
        dvr::camera::position_offset_uu(pos);
        wantPos = pos[0] != 0.0f || pos[1] != 0.0f || pos[2] != 0.0f;
    }
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
            if (wantPos)    { LeanVP(buf); dvr::camera::note_vp_applied(); }
            if (Finite16(buf)) {              // never forward a poisoned matrix
                return dvr::frame::orig_set_vs_const(self, startReg, buf, count);
            }
        }
    }
    return dvr::frame::orig_set_vs_const(self, startReg, data, count);
}


static HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9* self, DWORD idx,
                                           IDirect3DSurface9* rt)
{
    if (idx == 0 && rt) {
        D3DSURFACE_DESC d;
        if (SUCCEEDED(rt->GetDesc(&d))) { g_curRTw = d.Width; g_curRTh = d.Height; }
    }
    return dvr::frame::orig_set_render_target(self, idx, rt);
}
