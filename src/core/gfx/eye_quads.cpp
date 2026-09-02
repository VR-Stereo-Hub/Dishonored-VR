// core/gfx/eye_quads.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).



// Build per-eye quad vertex buffers: corners of a head-locked virtual surface,
// pre-projected into clip space with the eye's real asymmetric frustum
// (Valve's documented GetProjectionRaw composition).
static bool BuildEyeQuads(float aspect)
{
    // 36.9: fetch both eyes' geometry first (for the cache + the frustum
    // log), then build the quads with the per-eye centering the whole rig
    // was tuned around. (36.8's shared-center Quest theory measured wrong
    // on both headsets and is gone; Quest goes through the OpenXR backend.)
    // 37.3: under OpenXR the cache is maintained by the XR frame loop
    // (xrLocateViews fovs + eye offsets) - skip the OpenVR fetch entirely.
    for (int eye = 0; eye < 2 && !g_xrOn; eye++) {
        float l, r, t, b;
        g_sys->GetProjectionRaw((EVREye)eye, &l, &r, &t, &b);

        // eye-to-head transform (struct-by-value across the C FnTable).
        // 38.1: validity = ORTHONORMAL rotation + small translation, instead
        // of "looks like identity". The old check silently ACCEPTED up to
        // ~36 deg of eye cant while discarding the rotation - which, if the
        // runtime reports canted eyes, skews every scene frame per eye
        // (the Quest/Steam Link warp suspect). Now the full 3x3 is kept.
        float ex = (eye == 0) ? -0.032f : 0.032f, ey = 0, ez = 0;
        HmdMatrix34_t e2h = g_sys->GetEyeToHeadTransform((EVREye)eye);
        {
            float rn0 = e2h.m[0][0]*e2h.m[0][0] + e2h.m[0][1]*e2h.m[0][1] + e2h.m[0][2]*e2h.m[0][2];
            float rn1 = e2h.m[1][0]*e2h.m[1][0] + e2h.m[1][1]*e2h.m[1][1] + e2h.m[1][2]*e2h.m[1][2];
            float rn2 = e2h.m[2][0]*e2h.m[2][0] + e2h.m[2][1]*e2h.m[2][1] + e2h.m[2][2]*e2h.m[2][2];
            bool ok = fabsf(rn0 - 1.0f) < 0.1f && fabsf(rn1 - 1.0f) < 0.1f &&
                      fabsf(rn2 - 1.0f) < 0.1f && fabsf(e2h.m[0][3]) < 0.2f &&
                      fabsf(e2h.m[1][3]) < 0.2f && fabsf(e2h.m[2][3]) < 0.2f;
            if (ok) {
                ex = e2h.m[0][3]; ey = e2h.m[1][3]; ez = e2h.m[2][3];
                for (int rr = 0; rr < 3; rr++)
                    for (int cc = 0; cc < 3; cc++)
                        g_eyeRot[eye][rr*3 + cc] = e2h.m[rr][cc];
                // cant angle from the trace: cos(a) = (tr-1)/2
                float tr = e2h.m[0][0] + e2h.m[1][1] + e2h.m[2][2];
                float ca = 0.5f * (tr - 1.0f);
                if (ca > 1.0f) ca = 1.0f; if (ca < -1.0f) ca = -1.0f;
                if (acosf(ca) > 0.005f) g_eyeCanted = true;   // > ~0.3 deg
                // 30.37: remember the real IPD - it drives the stereo separation
                // (with world scale) so the world renders at true 1:1 size.
                if (fabsf(ex) > 0.02f && fabsf(ex) < 0.05f) g_ipdM = 2.0f * fabsf(ex);
            } else {
                Log("eye-to-head transform looked wrong, using nominal IPD");
            }
            static bool e2hTold[2] = { false, false };   // 38.1: full-matrix evidence
            if (!e2hTold[eye]) { e2hTold[eye] = true;
                float tr = e2h.m[0][0] + e2h.m[1][1] + e2h.m[2][2];
                float ca = 0.5f * (tr - 1.0f);
                if (ca > 1.0f) ca = 1.0f; if (ca < -1.0f) ca = -1.0f;
                Log("e2h[%d]: [%+.4f %+.4f %+.4f | %+.4f] [%+.4f %+.4f %+.4f | %+.4f] "
                    "[%+.4f %+.4f %+.4f | %+.4f] cant=%.2fdeg %s",
                    eye,
                    e2h.m[0][0], e2h.m[0][1], e2h.m[0][2], e2h.m[0][3],
                    e2h.m[1][0], e2h.m[1][1], e2h.m[1][2], e2h.m[1][3],
                    e2h.m[2][0], e2h.m[2][1], e2h.m[2][2], e2h.m[2][3],
                    acosf(ca) * 57.29578f,
                    ok ? (g_eyeCantCfg ? "(rotation APPLIED)" : "(rotation ignored: EyeCant=0)")
                       : "(REJECTED - nominal fallback)");
            }
        }
        // 34.7: cache the frustum + eye offset for the wrist-panel billboard
        g_eyeFr[eye][0] = l; g_eyeFr[eye][1] = r;
        g_eyeFr[eye][2] = t; g_eyeFr[eye][3] = b;
        g_eyeOffs[eye][0] = ex; g_eyeOffs[eye][1] = ey; g_eyeOffs[eye][2] = ez;
    }
    g_eyeFrOk = true;
    {   // one line of evidence per session: the asymmetry that broke Quest
        static bool frTold = false;
        if (!frTold) { frTold = true;
            Log("eye frustums: L[%.3f %.3f %.3f %.3f] ex=%.4f | "
                "R[%.3f %.3f %.3f %.3f] ex=%.4f",
                g_eyeFr[0][0], g_eyeFr[0][1], g_eyeFr[0][2], g_eyeFr[0][3],
                g_eyeOffs[0][0],
                g_eyeFr[1][0], g_eyeFr[1][1], g_eyeFr[1][2], g_eyeFr[1][3],
                g_eyeOffs[1][0]);
        }
    }
    for (int eye = 0; eye < 2; eye++) {
        float l = g_eyeFr[eye][0], r = g_eyeFr[eye][1];
        float t = g_eyeFr[eye][2], b = g_eyeFr[eye][3];
        float ex = g_eyeOffs[eye][0], ey = g_eyeOffs[eye][1],
              ez = g_eyeOffs[eye][2];

        float D = g_screenDist;
        float idx = 1.0f / (r - l), idy = 1.0f / (b - t);

        // FILL THE VERTICAL FOV: size the surface so its height exactly spans
        // the eye's frustum top-to-bottom at distance D (no black bars above/
        // below). Width follows the game's aspect, so the wide 16:9 sides run
        // off past your peripheral vision instead of leaving borders. The
        // frustum's vertical center is (t+b)/2 * D, horizontal center (l+r)/2*D.
        // 1:1 ANGULAR SCALE: the surface subtends exactly the game's rendered
        // horizontal FOV, so world geometry through the lens matches what the
        // game drew - no zoom, no shrink. (Stretching wider than the rendered
        // FOV = the "super zoomed" bug.) To shrink the border, raise the FOV
        // the GAME renders (in-game slider / ini) and set GameFOVDeg to match.
        float H, W;
        if (g_fillView) {
            // 30.36: prefer the live rendered FOV (fork export); the ini's
            // GameFOVDeg is only the fallback when the export is absent.
            float fovDeg = (g_liveFovX > 30.0f && g_liveFovX < 150.0f)
                         ? g_liveFovX : g_gameFovDeg;
            // 30.53: a ZOOM must magnify, not shrink the image. Sizing the
            // quad from the instantaneous rendered FOV is right for gentle
            // cutscene framing, but the spyglass narrows the render to ~44 deg
            // and that turned the view into a tiny window floating in black.
            // With the lever armed we know the steady-state FOV, so hold a
            // floor under the presentation size: below it, the same lens area
            // shows a narrower slice of world - which IS magnification.
            if (g_fovLever >= 40.0f) {
                float floorFov = (float)g_fovLever * g_zoomFillFloor;
                if (fovDeg < floorFov) fovDeg = floorFov;
            }
            // 32.4: the pause menu is authored to the full frame, so at a fill
            // that crops for gameplay comfort its edges fall outside the quad.
            // Menus get their own fill - the image shrinks, the whole UI is
            // visible, and gameplay framing is untouched.
            // 33.9: the LOAD screens (save-slot browser + level loading) never
            // trip the script menu flag (LoadGameClicked even CLOSES it), so
            // they rendered at gameplay fill and their edges were cropped.
            // g_sbsMonoNow is already the trusted "this frame is UI, not
            // world" signal (fork splice count < 8 - the same rule that
            // switches those frames to mono), so any mono frame gets the
            // menu fill too.
            float fillNow = g_fillScale;
            if ((g_menuOpen || g_inMenu || g_sbsMonoNow) && g_menuFill > 0.1f)
                fillNow = g_menuFill;
            W = 2.0f * D * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f) * fillNow;
            H = W / aspect;
            // 40.1 WORLD SCALE, the number behind "everything looks too big".
            // W is pinned to the rendered horizontal FOV, so the only free
            // variable is H, and H comes from the FULL-FRAME aspect. Push the
            // render toward square (a tall ResY next to a modest ResX) and the
            // virtual screen grows taller than the lenses can show; the player
            // then sees the MIDDLE of a tall image, which reads as zoom, not as
            // crop. Log the subtended angles so that is arithmetic, not a
            // matter of opinion. Rebuild-only, so this costs nothing per frame.
            {
                const float kR2D = 57.29578f;
                float subH = 2.0f * atanf((W * 0.5f) / D) * kR2D;
                float subV = 2.0f * atanf((H * 0.5f) / D) * kR2D;
                Log("quad: fov=%.1f deg fill=%.2f frameAspect=%.3f perEyeAspect=%.3f "
                    "W=%.3f H=%.3f D=%.2f m -> subtends %.1f x %.1f deg",
                    fovDeg, fillNow, aspect, aspect * 0.5f, W, H, D, subH, subV);
                if (subV > 110.0f)
                    DVR_WARN("quad: the virtual screen subtends %.0f deg VERTICALLY, which is "
                             "far more than any headset shows (~90-110). You will see only "
                             "the middle of the image and everything will look magnified. "
                             "This follows from the render aspect %ux%u: a taller frame "
                             "makes a taller screen. A 16:9-ish frame keeps it near 70 deg.",
                             subV, g_capW, g_capH);
            }
        } else {
            W = g_screenWidth; H = W / aspect;  // legacy fixed-size screen
        }
        // Per-eye frustum centering: what the Vive/Index rig is tuned around.
        // 37.4: under the XR backend the screen is ONE rigid object instead -
        // on Quest's strongly asymmetric frustums, per-eye centering hands
        // each eye a differently-placed screen (the Steam Link non-fusion,
        // reproduced identically on the first XR flight). This branch never
        // runs for OpenVR headsets, so the 36.8 lesson (it feels wrong on
        // the Vive) stays honored.
        float ccx, ccy;
        if (g_xrOn || g_rigidScreen) {
            ccx = D * 0.25f * (g_eyeFr[0][0] + g_eyeFr[0][1]
                             + g_eyeFr[1][0] + g_eyeFr[1][1]);
            ccy = D * 0.25f * (g_eyeFr[0][2] + g_eyeFr[0][3]
                             + g_eyeFr[1][2] + g_eyeFr[1][3]);
        } else {
            ccx = D * (l + r) * 0.5f;
            ccy = D * (t + b) * 0.5f;
        }

        // 38.3: export the authored screen geometry for the XR quad layer
        g_bqW = W; g_bqH = H; g_bqCx = ccx; g_bqCy = ccy;

        // corners in head space (order TL, TR, BL, BR for a triangle strip)
        float cx[4] = { ccx - W/2, ccx + W/2, ccx - W/2, ccx + W/2 };
        float cy[4] = { ccy + H/2, ccy + H/2, ccy - H/2, ccy - H/2 };
        // 30.29 SBS: the backbuffer holds a left|right stereo pair; each eye
        // samples its own half (the 2:1 horizontal squeeze undoes itself when
        // half the texels stretch across the full-aspect quad).
        float u0 = 0.0f, u1 = 1.0f;
        float u [4] = {  u0,   u1,   u0,   u1  };
        float v [4] = {  0,    0,    1,    1  };

        // 38.13 XR proj mode: FILL THE FRUSTUM at exact 1:1 angular scale.
        // A visible screen border is what reprojection DRAGS during head
        // motion (the residual "warping"); edge-to-edge content leaves it
        // nothing to drag, and the tan-linear uv mapping is exact for
        // rectilinear content (no fill-scale distortion of cutscene or
        // spyglass framing - the zoom floor keeps magnifying by design).
        // Gameplay frames only; menus/mono keep the authored screen quad.
        if (g_xrOn && g_xrLayerMode == 0 && g_xrFrustumFill &&
            g_fillView && !(g_menuOpen || g_inMenu || g_sbsMonoNow)) {
            float fovDeg = (g_liveFovX > 30.0f && g_liveFovX < 150.0f)
                         ? g_liveFovX : g_gameFovDeg;
            // 40.2 SAY WHICH NUMBER IS SETTING THE SCALE. This is the single
            // value that decides how big the world looks: the UV window is
            // built as if the texture spans exactly fovDeg, so if the game is
            // rendering something else, everything magnifies by the ratio of
            // the half-angle tangents. g_liveFovX is the MEASURED render FOV
            // from the fork's dxvk_vr_proj export; g_gameFovDeg is an ini
            // constant nobody verified. The fallback between them was silent,
            // and in the 2026-09-01 headset run the export produced nothing at
            // all - its publish is gated on a landscape viewport too - so an
            // assumed 100 deg set world scale for a whole session in silence.
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 10000,
                "quad/fill: world scale is set by %s = %.1f deg%s",
                (g_liveFovX > 30.0f && g_liveFovX < 150.0f)
                    ? "the MEASURED render FOV (fork dxvk_vr_proj)"
                    : "the ini constant [Screen] GameFOVDeg",
                fovDeg,
                (g_liveFovX > 30.0f && g_liveFovX < 150.0f) ? ""
                    : " - the fork's projection export has produced NOTHING this "
                      "session, so this number is ASSUMED, not measured. If the "
                      "world looks magnified, the game is rendering a different "
                      "FOV and the ratio of the half-angle tangents is exactly "
                      "how much too big it looks.");
            if (g_fovLever >= 40.0f) {
                float floorFov = (float)g_fovLever * g_zoomFillFloor;
                if (fovDeg < floorFov) fovDeg = floorFov;
            }
            float tanC  = tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
            float tanCv = tanC / aspect;
            if (tanC > 0.05f) {
                // frustum edges in HEAD space include the eye offset (the
                // frustum belongs to the eye, the quad lives at the head) -
                // without ex the temple side keeps a ~1 deg draggable border
                float xL = l * D + ex; if (xL < -tanC  * D) xL = -tanC  * D;
                float xR = r * D + ex; if (xR >  tanC  * D) xR =  tanC  * D;
                float yB = t * D + ey; if (yB < -tanCv * D) yB = -tanCv * D;
                float yT = b * D + ey; if (yT >  tanCv * D) yT =  tanCv * D;
                cx[0] = xL; cx[1] = xR; cx[2] = xL; cx[3] = xR;
                cy[0] = yT; cy[1] = yT; cy[2] = yB; cy[3] = yB;
                float us = (u1 - u0);
                for (int i = 0; i < 4; i++) {
                    float un = 0.5f + 0.5f * (cx[i] / (D * tanC));
                    float vn = 0.5f - 0.5f * (cy[i] / (D * tanCv));
                    u[i] = u0 + un * us;
                    v[i] = vn;
                }
                // 40.2 FILLSCALE WAS INERT IN THIS BRANCH, so there was no
                // working lever for apparent size anywhere. The frustum-fill
                // path is 1:1 BY CONSTRUCTION - it presents exactly the angle
                // the content subtends - and it discards the authored W/H that
                // [Screen] FillScale multiplies. So the "screen fill" slider
                // did nothing whenever XrFrustumFill was on, which is the
                // default, and the world-scale knob only moves stereo depth.
                // A tester with a correctly 1:1 view that still feels enormous
                // had nothing to turn.
                //
                // The UVs above are already computed from the UNSCALED corners,
                // so scaling the corners here presents the SAME sampled content
                // across a smaller or larger angle - which is exactly what
                // apparent size means. Guarded so 1.0 stays byte-identical to
                // the previous behaviour.
                //
                // Cost, stated because it is the reason this branch existed:
                // below 1.0 the screen border comes back, and the border is
                // what reprojection drags during head motion. Comfort lever,
                // not a correctness fix - 1:1 is the geometrically right answer
                // and this deliberately leaves it.
                if (g_fillScale > 0.01f && fabsf(g_fillScale - 1.0f) > 0.001f) {
                    for (int i = 0; i < 4; i++) {
                        cx[i] *= g_fillScale;
                        cy[i] *= g_fillScale;
                    }
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 10000,
                        "quad/fill: FillScale %.2f - presenting the frame across "
                        "%.0f%% of the angle it actually subtends, so the world "
                        "looks %s than life size. This is deliberately NOT 1:1.",
                        g_fillScale, g_fillScale * 100.0f,
                        g_fillScale < 1.0f ? "SMALLER" : "BIGGER");
                }
            }
        }

        QuadVert verts[4];
        const float* R = g_eyeRot[eye];
        for (int i = 0; i < 4; i++) {
            // 38.2: remember the authored corner (anchor space) + uv so the
            // world-anchored path can re-project them every frame.
            g_wsCorn[eye][i][0] = cx[i];
            g_wsCorn[eye][i][1] = cy[i];
            g_wsCorn[eye][i][2] = -D;
            g_wsUv[eye][i][0] = u[i];
            g_wsUv[eye][i][1] = v[i];
            // eye space: p_eye = R^T (p_head - t). e2h maps eye->head
            // (p_head = R p_eye + t), so the inverse uses the transpose.
            // R identity (Vive/Index, or EyeCant=0) reduces this to the
            // 37.8 translation-only math exactly.
            float hx = cx[i] - ex, hy = cy[i] - ey, hz = -D - ez;
            float x = hx, y = hy, z = hz;
            if (g_eyeCantCfg) {
                x = R[0]*hx + R[3]*hy + R[6]*hz;
                y = R[1]*hx + R[4]*hy + R[7]*hz;
                z = R[2]*hx + R[5]*hy + R[8]*hz;
            }
            float clipX = 2*idx*x + (r + l)*idx*z;
            float clipY = 2*idy*y + (b + t)*idy*z;
            float clipW = -z;
            verts[i].pos[0] = clipX;
            verts[i].pos[1] = clipY;
            verts[i].pos[2] = 0.5f * clipW; // mid-depth; no depth testing
            verts[i].pos[3] = clipW;
            verts[i].uv[0] = u[i];
            verts[i].uv[1] = v[i];
        }

        if (g_vb[eye]) { g_vb[eye]->Release(); g_vb[eye] = NULL; }
        D3D11_BUFFER_DESC bd;
        memset(&bd, 0, sizeof(bd));
        bd.ByteWidth = sizeof(verts);
        // 38.2: world mode rewrites the quad every frame - dynamic buffer.
        if (g_worldScreen) {
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        } else {
            bd.Usage = D3D11_USAGE_IMMUTABLE;
        }
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof(sd));
        sd.pSysMem = verts;
        if (FAILED(g_dev11->CreateBuffer(&bd, &sd, &g_vb[eye]))) {
            Log("eye %d quad VB failed", eye);
            return false;
        }
    }
    g_wsCornOk = true;
    g_quadAspect = aspect;
    Log("eye quads built (aspect %.3f)", aspect);
    return true;
}


// 38.2: world-anchored screen - re-project the authored quad corners through
// the CURRENT head pose every frame. The screen is a fixed object in room
// space (like every VR cinema screen and like an OpenXR quad layer in LOCAL
// space), so SteamVR's reprojection and the Quest streaming client's timewarp
// are both CORRECT for it by construction. p_head = H^T (A p_anchor + a - h),
// then the usual per-eye offset/rotation/frustum.
static void WsUpdateQuads()
{
    if (!g_wsCornOk || !g_devPoseOk[0] || !g_ctx11) return;
    float (*hm)[4] = g_devPose[0];

    if (InterlockedExchange(&g_wsReanchor, 0)) {
        // yaw-level anchor at the current head position: forward = head -Z
        // flattened to the horizon (looking straight up/down keeps the old yaw)
        float fx = -hm[0][2], fz = -hm[2][2];
        float fl = sqrtf(fx*fx + fz*fz);
        if (fl > 0.2f) {
            fx /= fl; fz /= fl;
            // anchor basis, room<-anchor, columns X,Y,Z: Za=-fwd, Ya=up, Xa=Ya x Za
            g_wsA[0] =  fz; g_wsA[1] = 0; g_wsA[2] = -fx;
            g_wsA[3] =  0;  g_wsA[4] = 1; g_wsA[5] =  0;
            g_wsA[6] =  fx; g_wsA[7] = 0; g_wsA[8] =  fz;
        }
        g_wsPos[0] = hm[0][3]; g_wsPos[1] = hm[1][3]; g_wsPos[2] = hm[2][3];
        Log("worldscreen: anchored at (%.2f %.2f %.2f) yaw=%.1fdeg",
            g_wsPos[0], g_wsPos[1], g_wsPos[2],
            atan2f(-g_wsA[2], g_wsA[8]) * 57.29578f);
    }

    for (int eye = 0; eye < 2; eye++) {
        if (!g_vb[eye]) continue;
        float l = g_eyeFr[eye][0], r = g_eyeFr[eye][1];
        float t = g_eyeFr[eye][2], b = g_eyeFr[eye][3];
        float idx = 1.0f / (r - l), idy = 1.0f / (b - t);
        float ex = g_eyeOffs[eye][0], ey = g_eyeOffs[eye][1],
              ez = g_eyeOffs[eye][2];
        const float* R = g_eyeRot[eye];
        QuadVert verts[4];
        for (int i = 0; i < 4; i++) {
            const float* c = g_wsCorn[eye][i];
            // anchor -> room
            float rx = g_wsA[0]*c[0] + g_wsA[1]*c[1] + g_wsA[2]*c[2] + g_wsPos[0];
            float ry = g_wsA[3]*c[0] + g_wsA[4]*c[1] + g_wsA[5]*c[2] + g_wsPos[1];
            float rz = g_wsA[6]*c[0] + g_wsA[7]*c[1] + g_wsA[8]*c[2] + g_wsPos[2];
            // room -> head (H is room<-head, so transpose)
            float dx = rx - hm[0][3], dy = ry - hm[1][3], dz = rz - hm[2][3];
            float hx = hm[0][0]*dx + hm[1][0]*dy + hm[2][0]*dz;
            float hy = hm[0][1]*dx + hm[1][1]*dy + hm[2][1]*dz;
            float hz = hm[0][2]*dx + hm[1][2]*dy + hm[2][2]*dz;
            // head -> eye
            float px = hx - ex, py = hy - ey, pz = hz - ez;
            float x = px, y = py, z = pz;
            if (g_eyeCantCfg) {
                x = R[0]*px + R[3]*py + R[6]*pz;
                y = R[1]*px + R[4]*py + R[7]*pz;
                z = R[2]*px + R[5]*py + R[8]*pz;
            }
            if (z > -0.05f) z = -0.05f;       // behind the eye: clamp, never div0
            verts[i].pos[0] = 2*idx*x + (r + l)*idx*z;
            verts[i].pos[1] = 2*idy*y + (b + t)*idy*z;
            verts[i].pos[3] = -z;
            verts[i].pos[2] = 0.5f * (-z);
            verts[i].uv[0] = g_wsUv[eye][i][0];
            verts[i].uv[1] = g_wsUv[eye][i][1];
        }
        D3D11_MAPPED_SUBRESOURCE ms;
        if (SUCCEEDED(g_ctx11->Map(g_vb[eye], 0, D3D11_MAP_WRITE_DISCARD,
                                   0, &ms))) {
            memcpy(ms.pData, verts, sizeof(verts));
            g_ctx11->Unmap(g_vb[eye], 0);
        }
    }
}
