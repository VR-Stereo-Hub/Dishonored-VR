// core/vr/openxr_pace.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ---- XR-3: the pace thread ---------------------------------------------
// Owns every call that can block on the runtime. The game thread cannot be
// stalled by VDXR no matter what the session state does - which is the
// XR-2 crash class, removed by construction.
static DWORD WINAPI XrPaceThread(LPVOID)
{
    g_xrPaceTid = GetCurrentThreadId();     // 38.11
    dvr::crash::register_thread("xr-pace", g_xrPaceTid);
    Log("xr: pace thread up (detached frame loop; the game thread never "
        "waits on the runtime) tid=%lu", (unsigned long)g_xrPaceTid);
    while (g_xrRun) {
        // session lifecycle
        if (g_xrf.pollEvent) {
            XrEventDataBuffer ev; memset(&ev, 0, sizeof(ev));
            ev.type = XR_TYPE_EVENT_DATA_BUFFER;
            // 40.2: g_xrRun in the condition. A runtime with a backlog of
            // events can hold this inner loop past a stop request, and the
            // whole point of the stop is that it completes in bounded time.
            while (g_xrRun && g_xrf.pollEvent(g_xriInst, &ev) == XR_SUCCESS) {
                if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    XrEventDataSessionStateChanged* sc =
                        (XrEventDataSessionStateChanged*)&ev;
                    if (sc->state == XR_SESSION_STATE_READY && !g_xriBegun) {
                        XrSessionBeginInfo bi; memset(&bi, 0, sizeof(bi));
                        bi.type = XR_TYPE_SESSION_BEGIN_INFO;
                        bi.primaryViewConfigurationType =
                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        if (!XR_FAILED(g_xrf.beginSession(g_xriSess, &bi))) {
                            g_xriBegun = true;
                            Log("xr: session BEGUN (pace thread)");
                        }
                    } else if (sc->state == XR_SESSION_STATE_STOPPING &&
                               g_xriBegun) {
                        g_xrf.endSession(g_xriSess);
                        g_xriBegun = false;
                        Log("xr: session ended (headset idle/removed) - waiting");
                    }
                }
                memset(&ev, 0, sizeof(ev)); ev.type = XR_TYPE_EVENT_DATA_BUFFER;
            }
        }
        if (!g_xriBegun) { Sleep(100); continue; }

        XrFrameState fs; memset(&fs, 0, sizeof(fs));
        fs.type = XR_TYPE_FRAME_STATE;
        if (XR_FAILED(g_xrf.waitFrame(g_xriSess, NULL, &fs))) {
            Sleep(50); continue;
        }
        if (XR_FAILED(g_xrf.beginFrame(g_xriSess, NULL))) continue;
        XrTime disp = fs.predictedDisplayTime;

        // freshest pose for the game camera (published to the game thread)
        XrViewLocateInfo vli; memset(&vli, 0, sizeof(vli));
        vli.type = XR_TYPE_VIEW_LOCATE_INFO;
        vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        vli.displayTime = disp;
        vli.space = g_xriSpace;
        XrViewState vs; memset(&vs, 0, sizeof(vs));
        vs.type = XR_TYPE_VIEW_STATE;
        uint32_t nv = 0;
        XrView views[2]; memset(views, 0, sizeof(views));
        views[0].type = views[1].type = XR_TYPE_VIEW;
        bool poseOk = !XR_FAILED(g_xrf.locateViews(g_xriSess, &vli, &vs, 2,
                                                   &nv, views)) && nv >= 2 &&
                      (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
        if (poseOk) {
            XrQuaternionf q = views[0].pose.orientation;
            float px = 0.5f * (views[0].pose.position.x + views[1].pose.position.x);
            float py = 0.5f * (views[0].pose.position.y + views[1].pose.position.y);
            float pz = 0.5f * (views[0].pose.position.z + views[1].pose.position.z);
            float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
            float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
            float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
            float dx = views[1].pose.position.x - views[0].pose.position.x;
            float dy = views[1].pose.position.y - views[0].pose.position.y;
            float dz = views[1].pose.position.z - views[0].pose.position.z;
            EnterCriticalSection(&g_xrCs);
            g_xrpM[0][0] = 1 - 2*(yy + zz); g_xrpM[0][1] = 2*(xy - wz); g_xrpM[0][2] = 2*(xz + wy);
            g_xrpM[1][0] = 2*(xy + wz); g_xrpM[1][1] = 1 - 2*(xx + zz); g_xrpM[1][2] = 2*(yz - wx);
            g_xrpM[2][0] = 2*(xz - wy); g_xrpM[2][1] = 2*(yz + wx); g_xrpM[2][2] = 1 - 2*(xx + yy);
            g_xrpM[0][3] = px; g_xrpM[1][3] = py; g_xrpM[2][3] = pz;
            for (int eye = 0; eye < 2; eye++) {
                // 38.6: no negation - see the sync-path adapter comment
                g_xrpFr[eye][0] = tanf(views[eye].fov.angleLeft);
                g_xrpFr[eye][1] = tanf(views[eye].fov.angleRight);
                g_xrpFr[eye][2] = tanf(views[eye].fov.angleDown);
                g_xrpFr[eye][3] = tanf(views[eye].fov.angleUp);
            }
            g_xrpIpd = sqrtf(dx*dx + dy*dy + dz*dz);
            g_xrpValid = true;
            memcpy(g_xrpLocViews, views, sizeof(g_xrpLocViews));   // 38.8
            g_xrpLocOk = true;
            LeaveCriticalSection(&g_xrCs);
        }
        XrInpSync(disp);                    // 38.9: controllers, every frame
        XrInpHapticFlush();                 // 38.10: queued haptics, THIS thread

        // new content? copy it into the swapchains (multithread-protected
        // context; the copy is device-side and quick)
        LONG seq = g_xrpSeq;
        float qw, qh, qcx, qcy, qd;
        EnterCriticalSection(&g_xrCs);
        qw = g_xrpQuadW; qh = g_xrpQuadH;
        qcx = g_xrpQuadCx; qcy = g_xrpQuadCy; qd = g_xrpQuadD;
        LeaveCriticalSection(&g_xrCs);
        if (seq != g_xrpShown && g_xrpTex[0] && g_xrpTex[1]) {
            // 40.2 A TIMEOUT IS A SUCCESS CODE. XrResult is negative for
            // failure only: XR_TIMEOUT_EXPIRED is +1, so XR_FAILED() is FALSE
            // for it and the old `!XR_FAILED(wait)` copied into an image the
            // compositor had NOT yet finished reading. That is a data race
            // with the runtime on the one resource the headset displays, and
            // it is invisible - every call returns a success code.
            //
            // The published content is also no longer marked shown until both
            // eyes actually received it. g_xrpShown used to advance before the
            // copies, so a frame lost to a timeout was dropped permanently
            // instead of being retried on the next pass.
            bool copied[2] = { false, false };
            for (int eye = 0; eye < 2; eye++) {
                if (g_xriSwc[eye] == XR_NULL_HANDLE) continue;
                uint32_t idx = 0;
                if (XR_FAILED(g_xrf.acquire(g_xriSwc[eye], NULL, &idx)))
                    continue;
                XrSwapchainImageWaitInfo wi; memset(&wi, 0, sizeof(wi));
                wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
                wi.timeout = 100000000;      // 100 ms; the pace thread owns waits
                XrResult wr = g_xrf.wait(g_xriSwc[eye], &wi);
                if (wr == XR_SUCCESS) {
                    if (idx < g_xriImgN[eye] && g_xriImg[eye][idx]) {
                        EnterCriticalSection(&g_xrCs);
                        g_ctx11->CopyResource(g_xriImg[eye][idx], g_xrpTex[eye]);
                        LeaveCriticalSection(&g_xrCs);
                        g_xrShownOnce[eye] = true;
                        copied[eye] = true;
                    } else {
                        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                            "xr: swapchain image %u for eye %d is out of range or "
                            "null (have %u images) - nothing copied this frame",
                            idx, eye, g_xriImgN[eye]);
                    }
                } else {
                    // Released anyway to keep acquire/release paired: an
                    // unreleased image starves the swapchain within a few
                    // frames, which is a hard stall rather than one stale
                    // frame. The image keeps its previous contents, and the
                    // retry above re-copies as soon as the runtime is ready.
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                        "xr: xrWaitSwapchainImage eye %d returned %d (%s) after "
                        "100 ms - NOT copying into an image the compositor may "
                        "still be reading; this frame is retried next pass",
                        eye, (int)wr,
                        wr == XR_TIMEOUT_EXPIRED ? "XR_TIMEOUT_EXPIRED"
                                                 : "failure");
                }
                XrSwapchainImageReleaseInfo ri; memset(&ri, 0, sizeof(ri));
                ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
                g_xrf.release(g_xriSwc[eye], &ri);
            }
            if (copied[0] && copied[1]) g_xrpShown = seq;
        }

        // compose: two head-locked layers (VIEW space, one per eye) - a
        // CYLINDER when the runtime offers it (constant angular scale, so
        // 130-deg content doesn't edge-stretch and sweep with head turns),
        // else the flat quad. Until content exists, an empty frame keeps
        // the session alive and earns FOCUSED (bioshock-vr session-28 rule).
        XrCompositionLayerQuad ql[2]; memset(ql, 0, sizeof(ql));
        XrCompositionLayerCylinderKHR cl[2]; memset(cl, 0, sizeof(cl));
        XrCompositionLayerProjectionView ppv[2]; memset(ppv, 0, sizeof(ppv));
        XrCompositionLayerProjection pl; memset(&pl, 0, sizeof(pl));
        const XrCompositionLayerBaseHeader* layers[2];
        uint32_t nlayers = 0;
        // 38.8: mode 0 = projection (Vive-parity), stamped with the views
        // the published content was rendered from; falls back to quad until
        // those views exist. cyl needs the extension.
        bool cyl = (g_xrLayerMode == 1) && g_xrCylOn;
        bool proj = false;
        XrView pv2[2];
        double pubAgeMs = 0.0;
        if (g_xrLayerMode == 0) {
            EnterCriticalSection(&g_xrCs);
            proj = g_xrpPubOk;
            if (proj) {
                memcpy(pv2, g_xrpPubViews, sizeof(pv2));
                pubAgeMs = MaimNowMs() - g_xrpPubMs;
            }
            LeaveCriticalSection(&g_xrCs);
        }
        // 38.83: a frozen stamp must never reach the compositor. If the
        // published views are stale (the locate->ring->publish chain
        // stalled somewhere), stamp THIS frame's freshly located views -
        // already in hand from the top of this loop - instead.
        if (proj && poseOk && (g_stampLive || pubAgeMs > 250.0)) {
            memcpy(pv2, views, sizeof(pv2));
            static bool liveSaid = false;
            if (g_stampLive && !liveSaid) {
                liveSaid = true;
                Log("xr: layer stamped with the LIVE head pose (StampLive=1) - "
                    "the compositor cannot cancel the head motion the render "
                    "already applied");
            }
            static double staleSaidMs = 0.0;
            double ssn = MaimNowMs();
            if (!g_stampLive && ssn - staleSaidMs > 10000.0) {
                staleSaidMs = ssn;
                Log("xr: published view stamp STALE (%.0f ms) - layer stamped "
                    "with live views instead (frozen-image guard)", pubAgeMs);
            }
        }
        // 38.84 STAMP WHAT YOU RENDERED (see the g_stampFix note). The
        // render's true pitch comes from the fork; the stamp's pitch from
        // its own quaternion (XR: Y up, -Z forward). If they diverge, the
        // stamp is rotated about its LOCAL X by the delta so the compositor
        // places the image where the game actually drew it - head pitch
        // then works at compositor level even when the engine ignored the
        // rotator. Gated: mode on, fork export FRESH (menus stop updating
        // it), delta between 2 and 60 degrees.
        if (proj && g_stampFix && g_dxvkView) {
            static float  sfSeq = -1.0f;
            static double sfSeqMs = 0.0;
            float seqNow = g_dxvkView[3];
            double sfNow = MaimNowMs();
            if (seqNow != sfSeq) { sfSeq = seqNow; sfSeqMs = sfNow; }
            if (sfNow - sfSeqMs < 500.0) {           // export is live
                float fz = g_dxvkView[2];
                if (fz >  1.0f) fz =  1.0f;
                if (fz < -1.0f) fz = -1.0f;
                float renderPitch = asinf(fz);       // UE3 world: Z is up
                XrQuaternionf hq = pv2[0].pose.orientation;
                float fy = 2.0f * (hq.w * hq.x - hq.y * hq.z);
                if (fy >  1.0f) fy =  1.0f;
                if (fy < -1.0f) fy = -1.0f;
                float headPitch = asinf(fy);         // XR: fwd=-Z, up=+Y
                float delta = renderPitch - headPitch;
                if (g_stampFix == 2) delta = -delta;
                float ad = delta < 0 ? -delta : delta;
                if (ad > 0.035f && ad < 1.05f) {     // 2..60 deg
                    float ha = 0.5f * delta;
                    XrQuaternionf r; r.x = sinf(ha); r.y = 0; r.z = 0;
                    r.w = cosf(ha);
                    for (int se = 0; se < 2; se++) {
                        XrQuaternionf a = pv2[se].pose.orientation;
                        XrQuaternionf o;
                        o.x = a.w*r.x + a.x*r.w + a.y*r.z - a.z*r.y;
                        o.y = a.w*r.y - a.x*r.z + a.y*r.w + a.z*r.x;
                        o.z = a.w*r.z + a.x*r.y - a.y*r.x + a.z*r.w;
                        o.w = a.w*r.w - a.x*r.x - a.y*r.y - a.z*r.z;
                        pv2[se].pose.orientation = o;
                    }
                    static double sfSaidMs = 0.0;
                    if (sfNow - sfSaidMs > 10000.0) {
                        sfSaidMs = sfNow;
                        Log("xr: stampfix %d ACTIVE render=%.1f deg head=%.1f "
                            "deg delta=%.1f deg", g_stampFix,
                            renderPitch * 57.2958f, headPitch * 57.2958f,
                            delta * 57.2958f);
                    }
                }
            }
        }
        if (proj && g_xrShownOnce[0] && g_xrShownOnce[1]) {
            for (int eye = 0; eye < 2; eye++) {
                ppv[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                ppv[eye].pose = pv2[eye].pose;
                ppv[eye].fov  = pv2[eye].fov;
                ppv[eye].subImage.swapchain = g_xriSwc[eye];
                ppv[eye].subImage.imageRect.extent.width  = (int32_t)g_xrEyeW;
                ppv[eye].subImage.imageRect.extent.height = (int32_t)g_xrEyeH;
            }
            pl.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            pl.space = g_xriSpace;
            pl.viewCount = 2;
            pl.views = ppv;
            layers[nlayers++] = (const XrCompositionLayerBaseHeader*)&pl;
            static bool ptold = false;
            if (!ptold) { ptold = true;
                Log("xr: PROJECTION layer live (Vive-parity screen; stamped "
                    "with render-time views)");
            }
        } else if (g_xrShownOnce[0] && g_xrShownOnce[1] && qw > 0.01f) {
            for (int eye = 0; eye < 2; eye++) {
                if (cyl) {
                    cl[eye].type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
                    cl[eye].space = g_xriViewSpace;
                    cl[eye].eyeVisibility = eye ? XR_EYE_VISIBILITY_RIGHT
                                                : XR_EYE_VISIBILITY_LEFT;
                    cl[eye].subImage.swapchain = g_xriSwc[eye];
                    cl[eye].subImage.imageRect.extent.width  = (int32_t)g_xrEyeW;
                    cl[eye].subImage.imageRect.extent.height = (int32_t)g_xrEyeH;
                    cl[eye].pose.orientation.w = 1.0f;
                    cl[eye].pose.position.x = qcx;
                    cl[eye].pose.position.y = qcy;
                    cl[eye].pose.position.z = 0.0f;   // cylinder axis at the head
                    cl[eye].radius = qd;
                    // the arc subtends exactly what the flat screen would:
                    // the same 1:1 angular-scale law the whole rig obeys
                    cl[eye].centralAngle = 2.0f * atanf(0.5f * qw / qd);
                    cl[eye].aspectRatio  = (cl[eye].centralAngle * qd) / qh;
                    layers[nlayers++] =
                        (const XrCompositionLayerBaseHeader*)&cl[eye];
                } else {
                    ql[eye].type = XR_TYPE_COMPOSITION_LAYER_QUAD;
                    ql[eye].space = g_xriViewSpace;
                    ql[eye].eyeVisibility = eye ? XR_EYE_VISIBILITY_RIGHT
                                                : XR_EYE_VISIBILITY_LEFT;
                    ql[eye].subImage.swapchain = g_xriSwc[eye];
                    ql[eye].subImage.imageRect.extent.width  = (int32_t)g_xrEyeW;
                    ql[eye].subImage.imageRect.extent.height = (int32_t)g_xrEyeH;
                    ql[eye].pose.orientation.w = 1.0f;
                    ql[eye].pose.position.x = qcx;
                    ql[eye].pose.position.y = qcy;
                    ql[eye].pose.position.z = -qd;
                    ql[eye].size.width  = qw;
                    ql[eye].size.height = qh;
                    layers[nlayers++] =
                        (const XrCompositionLayerBaseHeader*)&ql[eye];
                }
            }
            static bool told = false;
            if (!told) { told = true;
                if (cyl)
                    Log("xr: CYLINDER layers live - r=%.2fm arc=%.1fdeg "
                        "h=%.2fm at (%.2f %.2f), VIEW space",
                        qd, cl[0].centralAngle * 57.29578f, qh, qcx, qcy);
                else
                    Log("xr: quad layers live - %.2fx%.2fm at (%.2f %.2f %.2f), "
                        "VIEW space (compositor-held, reprojection-exempt)",
                        qw, qh, qcx, qcy, -qd);
            }
        }
        XrFrameEndInfo fe; memset(&fe, 0, sizeof(fe));
        fe.type = XR_TYPE_FRAME_END_INFO;
        fe.displayTime = disp;
        fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fe.layerCount = nlayers;
        fe.layers = nlayers ? layers : NULL;
        XrResult xr = g_xrf.endFrame(g_xriSess, &fe);
        static int lastErr = 0;
        if ((int)xr != lastErr) {
            lastErr = (int)xr;
            Log("xr: pace xrEndFrame result changed -> %d (0 = OK)", (int)xr);
        }
    }
    Log("xr: pace thread leaving the frame loop cleanly (tid=%lu) - no further "
        "runtime or D3D11 call will be made from this lane",
        (unsigned long)g_xrPaceTid);
    LogFlush();
    return 0;
}


// ---- 40.2: stopping the pace thread is a JOIN, not a request --------------
// 38.79 stood the GAME thread down at PreExit and left this one running: it
// set g_xrRun = 0 and returned immediately. The pace thread can be up to 100 ms
// inside xrWaitFrame, another 100 ms inside xrWaitSwapchainImage, or mid
// CopyResource into g_xriImg[eye][idx] - swapchain textures owned by the
// RUNTIME, which we never AddRef and which stop existing when it tears its
// session down. So the exact fault 38.79 set out to prevent, and which the
// author recorded as "EIP dededede after PreExit" (a call through freed
// memory), still had an open path. Nobody was waiting for the thread to leave.
//
// Bounded wait, and on expiry the thread is LEFT RUNNING on purpose.
// TerminateThread here would be worse than the race it is trying to close: it
// abandons g_xrCs held (deadlocking any later publish) and abandons an
// acquired swapchain image the runtime is still tracking. Fail soft, and say
// so in the log - the presence of the error line is what tells the next reader
// which of the two cases they are looking at.
static void XrPaceStop(const char* why)
{
    InterlockedExchange(&g_xrRun, 0);
    HANDLE t = g_xrThread;
    if (!t) return;
    g_xrThread = NULL;
    const double t0 = MaimNowMs();
    const DWORD  w  = WaitForSingleObject(t, 750);
    const double dt = MaimNowMs() - t0;
    if (w == WAIT_OBJECT_0) {
        Log("shutdown: pace thread JOINED in %.0f ms (%s) - no runtime or D3D11 "
            "call can outlive teardown from this lane now", dt, why);
    } else {
        DVR_ERROR("shutdown: pace thread did NOT exit within 750 ms "
                  "(WaitForSingleObject -> %lu after %.0f ms, %s). It is left "
                  "running deliberately; TerminateThread would orphan g_xrCs and "
                  "abandon an acquired swapchain image, which is worse than the "
                  "race. If an exit fault follows THIS line, the pace lane is "
                  "still the one to chase - if it happens WITHOUT this line, the "
                  "thread was already gone and the pace lane is not the cause.",
                  (unsigned long)w, dt, why);
    }
    CloseHandle(t);
    LogFlush();
}
