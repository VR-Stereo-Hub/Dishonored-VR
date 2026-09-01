// core/gfx/present.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static void RenderEyesAndSubmit()
{
    if (!g_presentTid) g_presentTid = GetCurrentThreadId();   // 38.11
    if (!g_presentNamed) { g_presentNamed = true; dvr::crash::register_thread("present", g_presentTid); dvr::crash::rearm(); }
    OverlayFrame();
    if (g_ovlScene) OvlSceneFrame();         // 37.9: the SBS overlay carries
                                             // the game picture on Quest
    float aspect = (g_capH > 0) ? (float)g_capW / (float)g_capH : (16.0f/9.0f);
    // the menu fill only takes effect when the quad is rebuilt, so a menu
    // opening or closing has to force one - otherwise the setting appears to
    // do nothing until something else changes the aspect
    {
        static bool lastMenu = false;
        bool menuNow = (g_menuOpen || g_inMenu);
        if (menuNow != lastMenu) { lastMenu = menuNow; g_quadAspect = 0.0f; }
    }
    if (g_quadAspect == 0.0f || fabsf(aspect - g_quadAspect) > 0.01f)
        if (!BuildEyeQuads(aspect)) return;
    if (g_worldScreen && !g_xrOn) WsUpdateQuads();   // 38.2: room-locked screen
    if (g_hmEnable && !g_hmReady) HmEnsurePipeline();

    UINT stride = sizeof(QuadVert), offset = 0;
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0; vp.TopLeftY = 0;
    vp.Width = (float)g_eyeW; vp.Height = (float)g_eyeH;
    vp.MinDepth = 0; vp.MaxDepth = 1;

    float black[4] = {0, 0, 0, 1};
    g_ctx11->IASetInputLayout(g_layout);
    g_ctx11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g_ctx11->VSSetShader(g_vs, NULL, 0);
    g_ctx11->PSSetShader(g_ps, NULL, 0);
    g_ctx11->PSSetSamplers(0, 1, &g_sampler);
    g_ctx11->RSSetViewports(1, &vp);

    // 34.7: wrist panel - one upload + one set of billboard corners per
    // submit; drawn into both eyes inside the loop below.
    bool  hudPanelOn = false;
    float hudC[4][3];
    // 38.50: the panel's own gate previously DISAGREED with the redirect
    // gate (no CylTruthLive, no dialogue window) - so whenever the redirect
    // was off but the panel conditions held, the wrist showed the fork RT's
    // FROZEN last frame. Now the panel simply requires the redirect verdict.
    if (g_wristHud && g_hudPixEver && g_handMesh &&        // 35.6: see policy
        InterlockedCompareExchange(&g_hudRedirLive, 0, 0) != 0 &&
        !(g_menuOpen || g_inMenu || g_sbsMonoNow || g_wheelHeld) &&
        EnsureHudPanel11() && HudPanelCorners(hudC)) {
        if (g_hudPixNew) {
            g_hudPixNew = false;
            g_ctx11->UpdateSubresource(g_hudTex11, 0, NULL, g_hudPix,
                                       HUDRB_W * 4, 0);
        }
        hudPanelOn = true;
    }

    // 38.13: XR reticle billboard corners (proj mode; drawn per eye below)
    bool  retOn = false;
    float retC[4][3];
    if (g_xrOn && g_xrLayerMode == 0 && g_retXrOn &&
        XrRetEnsure() && XrRetCorners(retC))
        retOn = true;

    EVRCompositorError cerr = EVRCompositorError_VRCompositorError_None;
    for (int eye = 0; eye < 2; eye++) {
        g_ctx11->OMSetRenderTargets(1, &g_eyeRTV[eye], g_eyeDSV[eye]);
        g_ctx11->ClearRenderTargetView(g_eyeRTV[eye], black);
        if (g_eyeDSV[eye])
            g_ctx11->ClearDepthStencilView(g_eyeDSV[eye], D3D11_CLEAR_DEPTH, 1.0f, 0);
        if (g_ovlScene) goto xOvlSubmitOnly;   // 37.9: black frame keeps the
                                               // compositor fed; the overlay
                                               // carries the picture
        g_ctx11->OMSetDepthStencilState(g_dsOff, 0);   // the flat game image
        // SBS: both eyes sample tex[0] (the stereo pair); AER: per-eye image
        g_ctx11->PSSetShaderResources(0, 1, &g_srvGame[g_sbsMode ? 0 : eye]);
        if (g_xrOn && g_xrQuad && g_xrLayerMode != 0) {
            // 38.3 XR-3: the compositor's QUAD does the screen geometry, so
            // the texture must be the FLAT image (a pre-projected quad here
            // would be projected twice). Fullscreen blit of this eye's SBS
            // half; the 2:1 squeeze undoes itself across the quad's aspect.
            // 38.8: projection mode takes the else branch - the full Vive
            // composition (projected screen + hands + wrist panel).
            static ID3D11Buffer* fsVb[2] = { NULL, NULL };
            if (!fsVb[eye]) {
                D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof(bd));
                bd.ByteWidth = 4 * sizeof(QuadVert);
                bd.Usage = D3D11_USAGE_DYNAMIC;
                bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                g_dev11->CreateBuffer(&bd, NULL, &fsVb[eye]);
            }
            if (fsVb[eye]) {
                float u0 = 0.0f, u1 = 1.0f;
                if (g_sbsMode && !g_sbsMonoNow) {
                    u0 = eye ? 0.5f : 0.0f; u1 = u0 + 0.5f;
                }
                QuadVert fv[4] = {
                    {{-1,  1, 0.5f, 1}, {u0, 0}},   // TL
                    {{ 1,  1, 0.5f, 1}, {u1, 0}},   // TR
                    {{-1, -1, 0.5f, 1}, {u0, 1}},   // BL
                    {{ 1, -1, 0.5f, 1}, {u1, 1}},   // BR
                };
                D3D11_MAPPED_SUBRESOURCE ms;
                if (SUCCEEDED(g_ctx11->Map(fsVb[eye], 0,
                        D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
                    memcpy(ms.pData, fv, sizeof(fv));
                    g_ctx11->Unmap(fsVb[eye], 0);
                }
                g_ctx11->IASetVertexBuffers(0, 1, &fsVb[eye], &stride, &offset);
                g_ctx11->Draw(4, 0);
            }
            // hands + wrist panel are eye-frustum projected 3D content - on
            // a flat quad they'd be geometrically wrong; they return with
            // the projection-layer mode (roadmap: XR true-3D pass).
        } else {
            g_ctx11->IASetVertexBuffers(0, 1, &g_vb[eye], &stride, &offset);
            g_ctx11->Draw(4, 0);

            // 30.77: our own hands/weapons, in VR space, on top of the game image
            HmRenderEye(eye);
            if (g_hmEnable && g_hmReady) {          // restore the quad pipeline
                g_ctx11->IASetInputLayout(g_layout);
                g_ctx11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                g_ctx11->VSSetShader(g_vs, NULL, 0);
                g_ctx11->PSSetShader(g_ps, NULL, 0);
                g_ctx11->PSSetSamplers(0, 1, &g_sampler);
            }

            // 34.7: the wrist HUD panel - additive billboard on the controller
            if (hudPanelOn) HudPanelDrawEye(eye, hudC);

            // 38.13: the aim crosshair, in-scene (XR proj mode)
            if (retOn) XrRetDrawEye(eye, retC);
        }

        // 30.38: settings overlay on top of the game image, in both eyes.
        // 30.48: re-aim the draw data at the EYE texture so the panel sits at
        // eye level (~55% down) instead of the top of the tall eye RT. The
        // ortho maps [DisplayPos, DisplayPos+DisplaySize] across a viewport of
        // DisplaySize, so with DisplaySize = eye dims, pixel = logical -
        // DisplayPos. Mouse input is untouched (it reads io, not draw data).
        if (g_ovlVisible && g_ovlInit) {
            ImDrawData* dd = ImGui::GetDrawData();
            ImVec2 svSize = dd->DisplaySize, svPos = dd->DisplayPos;
            // 38.7: the 38.6 "aspect fix" is REVERTED - the ImGui DX11
            // backend sets its viewport to DisplaySize, so widening the
            // logical span rescales NOTHING and only recentered the panel
            // ~1000 px right, mostly off the 2112-wide texture ("overlay
            // doesn't show up"). Visible-but-stretched beats invisible;
            // the real fix is the overlay on its own XR layer (queued).
            dd->DisplaySize = ImVec2((float)g_eyeW, (float)g_eyeH);
            // 37.7 gave the panel an IPD parallax; 38.2: STILL DOUBLED,
            // because the dominant term was never the IPD - it is the
            // FRUSTUM ASYMMETRY. The measured Quest frustums put each eye's
            // optical center ~24% of the eye width off the texture center in
            // OPPOSITE directions (L (r+l)=-0.54, R +0.54), so "the same
            // pixel in both eyes" is ~28deg divergent. The fix is the same
            // projection the quads use: place the panel center where the
            // head-space point (0, *, -screenDist) actually projects in THIS
            // eye. ndcX = (2x/D - (r+l))/(r-l), x = -ex. On near-symmetric
            // Vive frustums this reduces to (almost exactly) the old math.
            float pcx = 0.5f * (float)g_eyeW;
            // 38.8: XR projection mode needs the same per-eye frustum
            // placement as rigid Steam Link (asymmetric frustums, ~24%
            // per-eye center offset - centered pixels would double).
            if ((g_rigidScreen || (g_xrOn && g_xrLayerMode == 0)) &&
                g_eyeFrOk && g_screenDist > 0.2f) {
                float fl = g_eyeFr[eye][0], fr = g_eyeFr[eye][1];
                float span = fr - fl;
                if (span > 0.1f) {
                    float ndcX = (2.0f * (-g_eyeOffs[eye][0]) / g_screenDist
                                 - (fr + fl)) / span;
                    pcx = (0.5f + 0.5f * ndcX) * (float)g_eyeW;
                }
            }
            dd->DisplayPos  = ImVec2(svSize.x * 0.5f - pcx,
                                     svSize.y * 0.5f - (float)g_eyeH * 0.55f);
            ImGui_ImplDX11_RenderDrawData(dd);
            dd->DisplaySize = svSize; dd->DisplayPos = svPos;
            // the ImGui backend replaced our pipeline state - restore it
            g_ctx11->IASetInputLayout(g_layout);
            g_ctx11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            g_ctx11->VSSetShader(g_vs, NULL, 0);
            g_ctx11->PSSetShader(g_ps, NULL, 0);
            g_ctx11->PSSetSamplers(0, 1, &g_sampler);
            g_ctx11->RSSetViewports(1, &vp);
            g_ctx11->RSSetState(NULL);
            g_ctx11->OMSetBlendState(NULL, NULL, 0xffffffff);
        }

xOvlSubmitOnly:
        if (g_xrOn) {                        // 37.3: deliver to the XR swapchain
            if (!g_xrQuad) XrRtSubmitEye(eye);   // XR-3: publish after the loop
        } else if (g_rigidScreen && !g_worldScreen) {
            // 38.2: world mode takes the PLAIN submit below - the content is
            // rendered at this frame's WaitGetPoses pose, which is exactly
            // what the compositor assumes for an unstamped frame (the normal-
            // VR-game contract). The pose stamp exists to protect HEAD-LOCKED
            // content and is wrong for world-locked geometry.
            // 37.7: stamp the frame with the pose it was RENDERED at. At 45
            // submits against a 90 Hz display the compositor reprojects
            // every other frame - and without the stamp it assumes each
            // frame is freshest-pose, so the reprojection drags our head-
            // locked screen around (the Quest "warp"). With it, only the
            // true delta is reprojected. Quest-family only; the Vive path
            // stays byte-for-byte as it always was.
            VRTextureWithPose_t twp;
            twp.handle = (void*)g_eyeTex[eye];
            twp.eType = ETextureType_TextureType_DirectX;
            twp.eColorSpace = EColorSpace_ColorSpace_Gamma;
            memcpy(twp.mDeviceToAbsoluteTracking.m, g_devPose[0],
                   sizeof(twp.mDeviceToAbsoluteTracking.m));
            EVRCompositorError e = g_comp->Submit((EVREye)eye,
                (Texture_t*)&twp, NULL, EVRSubmitFlags_Submit_TextureWithPose);
            if (e != EVRCompositorError_VRCompositorError_None) cerr = e;
        } else {
            Texture_t t;
            t.handle = (void*)g_eyeTex[eye];
            t.eType = ETextureType_TextureType_DirectX;
            t.eColorSpace = EColorSpace_ColorSpace_Gamma;
            EVRCompositorError e = g_comp->Submit((EVREye)eye, &t, NULL,
                                                  EVRSubmitFlags_Submit_Default);
            if (e != EVRCompositorError_VRCompositorError_None) cerr = e;
        }
    }
    g_ctx11->Flush();
    FrameDumpTick();                         // `dump eyes|capture|hud` from the seam
    if (g_xrOn) {
        if (g_xrQuad) XrRtPublish();         // 38.3: hand off to the pace thread
        else          XrRtFrameEnd();        // 37.3: the frame goes out here
    }

    if ((int)cerr != g_lastSubmitErr) {
        g_lastSubmitErr = (int)cerr;
        Log("compositor submit result changed -> %d (0 = OK) at frame %lu",
            (int)cerr, (unsigned long)g_frame);
    }
}


// ----------------------------------------------------------------------------
// Per-frame: capture, track, render, submit
// ----------------------------------------------------------------------------
static void VRFrame(IDirect3DDevice9* dev)
{
    SbTick();   // 30.83: SpaceBases oracle, written post-tick inside the draw

    // 34.7: one-shot block-property hunt, ~30 s in so a level is loaded
    {
        static int bhDone = 0;
        if (!bhDone && MaimNowMs() > 30000.0) { bhDone = 1; BlockPropHunt(); }
    }

    // 1) capture the game's backbuffer
    IDirect3DSurface9* bb = NULL;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return;
    D3DSURFACE_DESC desc;
    bb->GetDesc(&desc);
    if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) {
        if (!g_warnedFormat) {
            g_warnedFormat = true;
            Log("backbuffer format %d not handled - tell Claude!", (int)desc.Format);
        }
        bb->Release();
        return;
    }
    if (!g_sysmem || desc.Width != g_capW || desc.Height != g_capH || desc.Format != g_capFmt) {
        if (g_sysmem) { g_sysmem->Release(); g_sysmem = NULL; }
        free(g_pixels); g_pixels = NULL;
        if (FAILED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                    D3DPOOL_SYSTEMMEM, &g_sysmem, NULL))) {
            bb->Release();
            return;
        }
        g_capW = desc.Width; g_capH = desc.Height; g_capFmt = desc.Format;
        g_pixels = (uint8_t*)malloc((size_t)g_capW * g_capH * 4);
        Log("capture: %ux%u fmt=%d", g_capW, g_capH, (int)desc.Format);
    }
    HRESULT hr = dev->GetRenderTargetData(bb, g_sysmem);
    bb->Release();
    if (FAILED(hr)) {
        if (!g_warnedRTD) {
            g_warnedRTD = true;
            Log("GetRenderTargetData failed (0x%08lx) - tell Claude! (AA on?)", (unsigned long)hr);
        }
        return;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(g_sysmem->LockRect(&lr, NULL, D3DLOCK_READONLY))) return;
    // straight per-row copy (BGRA -> BGRA), no per-pixel conversion. We copy
    // into a plain cached heap buffer once here, so the (many) UpdateSubresource
    // reads below hit cached memory instead of the write-combined systemmem
    // surface, which is very slow to read back.
    const size_t rowBytes = (size_t)g_capW * 4;
    for (UINT y = 0; y < g_capH; y++)
        memcpy(g_pixels + y * rowBytes,
               (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch, rowBytes);
    g_sysmem->UnlockRect();

    // 34.7: wrist HUD source. GPU-downscale the fork's HUD texture to a small
    // RT and read that back (2.3 MB vs the 36 MB main capture - noise).
    if (g_wristHud && g_dxvkHudTex && !g_hudRbFail) {
        IDirect3DTexture9* ht = *g_dxvkHudTex;
        if (ht) {
            if (!g_hudSmall || !g_hudSys) {
                if (FAILED(dev->CreateRenderTarget(HUDRB_W, HUDRB_H,
                        D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                        &g_hudSmall, NULL)) ||
                    FAILED(dev->CreateOffscreenPlainSurface(HUDRB_W, HUDRB_H,
                        D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_hudSys, NULL))) {
                    g_hudRbFail = true;
                    Log("hud: readback surfaces FAILED - no wrist panel "
                        "(redirect still active; set WristHud=0 to get the "
                        "HUD back in view)");
                }
                else {
                    if (!g_hudPix)
                        g_hudPix = (uint8_t*)malloc((size_t)HUDRB_W * HUDRB_H * 4);
                    Log("hud: readback path ready (%dx%d)", HUDRB_W, HUDRB_H);
                }
            }
            if (g_hudSmall && g_hudSys && g_hudPix) {
                IDirect3DSurface9* hs = NULL;
                if (SUCCEEDED(ht->GetSurfaceLevel(0, &hs)) && hs) {
                    // 38.55: clear the fork's HUD RT right AFTER reading it,
                    // from OUR thread, with a plain ColorFill. This replaces
                    // fork M8.4's PresentEx-time clear (fork back to exact
                    // M8.2): same staleness cure - an element that stops
                    // being drawn is black by the next readback - without
                    // any state churn inside the fork's present path, which
                    // is a suspect for the chain artifact. Fill AFTER the
                    // StretchRect below so this frame's content still shows.
                    if (SUCCEEDED(dev->StretchRect(hs, NULL, g_hudSmall, NULL,
                                                   D3DTEXF_LINEAR)) &&
                        SUCCEEDED(dev->GetRenderTargetData(g_hudSmall, g_hudSys))) {
                        D3DLOCKED_RECT hl;
                        if (SUCCEEDED(g_hudSys->LockRect(&hl, NULL,
                                                         D3DLOCK_READONLY))) {
                            for (UINT y = 0; y < HUDRB_H; y++)
                                memcpy(g_hudPix + (size_t)y * HUDRB_W * 4,
                                       (const uint8_t*)hl.pBits + (size_t)y * hl.Pitch,
                                       (size_t)HUDRB_W * 4);
                            g_hudSys->UnlockRect();
                            g_hudPixNew = true;
                            g_hudPixEver = true;
                            dev->ColorFill(hs, NULL, D3DCOLOR_ARGB(0,0,0,0));
                        }
                    }
                    hs->Release();
                }
            }
        }
    }

    // 32.80: SEE the captured frame instead of inferring it.
    // Right eye black + the pair sitting off-centre is the exact signature of
    // a CORNER IMAGE - the game drawing 1920x1080 of content into a 3200x1800
    // backbuffer, so the left eye samples mostly-content and the right eye
    // samples mostly-nothing. It is also the signature of several other
    // things, and build 32.57 was lost to guessing between them. So dump the
    // actual pixels once, a few seconds in, and look. A BMP is four lines of
    // header and needs no library.
    {
        static int dumpAt = 0;
        if (g_capW && g_capH && ++dumpAt == 400) {
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "capture_dump_%ux%u.bmp", g_capW, g_capH);
            FILE* f = fopen(path, "wb");
            if (f) {
                const uint32_t rowB = g_capW * 4, imgB = rowB * g_capH;
                uint8_t fh[14] = { 'B','M' }, ih[40] = {};
                uint32_t fsz = 14 + 40 + imgB, off = 54;
                memcpy(fh + 2, &fsz, 4); memcpy(fh + 10, &off, 4);
                uint32_t v40 = 40; memcpy(ih, &v40, 4);
                int32_t w = (int32_t)g_capW, h = -(int32_t)g_capH;  // top-down
                memcpy(ih + 4, &w, 4); memcpy(ih + 8, &h, 4);
                uint16_t planes = 1, bpp = 32;
                memcpy(ih + 12, &planes, 2); memcpy(ih + 14, &bpp, 2);
                memcpy(ih + 20, &imgB, 4);
                fwrite(fh, 1, 14, f); fwrite(ih, 1, 40, f);
                fwrite(g_pixels, 1, imgB, f);
                fclose(f);
                Log("diag: wrote %s - this is exactly what the headset is fed, "
                    "before it is split into eyes", path);
            } else {
                Log("diag: could not open the capture dump for writing");
            }
        }
    }

    // 37.3: OpenXR backend bring-up (with its own backoff). On success it
    // sets g_xrOn and enters MODE_SCENE - the same pipeline as OpenVR from
    // here on, with the XR frame loop as pose source and delivery.
    if (g_xrBackend && !g_xrOn) XrRtTryInit();

    if (g_mode == MODE_SCENE) {
        StereoUpdate(); // hotkeys
        g_gameFrames++;

        // 30.30: SBS mono fallback. While a menu is open, the powers wheel is
        // held, or the DXVK fork reports a frame with (nearly) no stereo
        // splices - loading screens, videos, flat scenes - both eyes get the
        // full frame instead of a half each, so 2D UI stays readable.
        if (g_sbsMode || g_seqMode) {
            if (!g_dxvkSplicesTried) {
                g_dxvkSplicesTried = true;
                HMODULE hFork = GetModuleHandleA("dxvk_d3d9.dll");
                if (hFork) {
                    g_dxvkSeq = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_seq");
                    g_dxvkEyeSign = (volatile float*)
                        GetProcAddress(hFork, "dxvk_vr_eyesign");
                    if (g_seqMode && !g_dxvkEyeSign)
                        Log("seq: FORK IS TOO OLD - dxvk_vr_eyesign missing. "
                            "Both eyes will get the same image (mono). Update "
                            "dxvk_d3d9.dll or remove seq=1 from dxvk_stereo.txt.");
                    g_dxvkSplices = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_splices");
                    g_dxvkProj = (volatile float*)
                        GetProcAddress(hFork, "dxvk_vr_proj");
                    g_dxvkView = (volatile float*)
                        GetProcAddress(hFork, "dxvk_vr_view");   // p53
                    g_dxvkSep = (volatile float*)
                        GetProcAddress(hFork, "dxvk_vr_sep");
                    g_dxvkConv = (volatile float*)
                        GetProcAddress(hFork, "dxvk_vr_conv");
                    g_dxvkDumpReq = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_dumpreq");
                    g_dxvkMark = (volatile float*)
                        GetProcAddress(hFork, "dxvk_vr_mark");
                    g_dxvkMarkKill = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_markkill");
                    g_dxvkUvFix = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_uvfix");
                    g_dxvkHudWrist = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_hudwrist");
                    g_dxvkHudTex = (IDirect3DTexture9* volatile*)
                        GetProcAddress(hFork, "dxvk_vr_hudtex");
                    g_dxvkShadowFix = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_shadowfix");
                    g_dxvkReflect = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_reflect");
                    g_dxvkKillMask = (volatile unsigned int*)
                        GetProcAddress(hFork, "dxvk_vr_killmask");
                    Log("hud: wrist exports %s",
                        (g_dxvkHudWrist && g_dxvkHudTex)
                        ? "resolved" : "NOT FOUND (fork older than M6.0)");
                }
                Log("sbs: fork splice counter %s, proj export %s, frame dump %s,"
                    " marker locator %s",
                    g_dxvkSplices ? "resolved" : "NOT FOUND (mono only on menus)",
                    g_dxvkProj ? "resolved" : "NOT FOUND (using GameFOVDeg)",
                    g_dxvkDumpReq ? "on Scroll Lock" : "NOT FOUND (old fork build)",
                    g_dxvkMark ? "resolved" : "NOT FOUND (old fork build)");
            }
            // 30.36: live FOV from the fork's projection export. The screen
            // quad is sized from the FOV the game renders THIS frame - a
            // cutscene's zoomed-in FOV presents at true 1:1 angles instead
            // of stretching, and GameFOVDeg misconfig can no longer warp.
            if (g_dxvkProj) {
                float xs = g_dxvkProj[0];
                if (xs > 0.05f && xs < 20.0f) {
                    float fov = 2.0f * atanf(1.0f / xs) * 57.29578f;
                    if (fov > 30.0f && fov < 150.0f &&
                        fabsf(fov - g_liveFovX) > 0.5f) {
                        g_liveFovX = fov;
                        g_quadAspect = 0.0f;   // rebuild quads at new scale
                        Log("screen: live FOV %.1f deg (from render matrix)", fov);
                    }
                    // 30.37: drive the fork's stereo separation from real
                    // IPD x world scale x live projection, so stereo depth
                    // and positional parallax agree on how big a meter is:
                    //   eyeOffsetUU = IPD_m * uuPerMeter
                    //   sep = xs * eyeOffsetUU / (2 * conv)
                    // (sep*conv is the true translation term; sep alone is
                    // the frustum shear; zero parallax stays at w = conv.)
                    if (g_dxvkSep && g_dxvkConv && *g_dxvkConv > 1.0f) {
                        *g_dxvkSep = xs * (g_ipdM * g_posScaleUU) * 0.5f
                                   / (*g_dxvkConv);
                    }
                }
            }
            // 30.34: the fork (M3.5) duplicates HUD/menu/wheel UI into both
            // halves, so those frames are stereo-CORRECT - forcing mono on
            // them shows the squeezed pair (unreadable). Trust the fork's
            // per-frame stereo-content counter; menu events are only the
            // fallback for an old fork without the counter. This also kills
            // the mono-stuck-at-level-start bug: the game's save-slot polls
            // (Req_SaveSlotInfos) re-open the menu flag right after
            // auto-start, but with the counter rule a high splice count wins.
            bool wantMono = g_dxvkSplices
                          ? (*g_dxvkSplices < 8)
                          : (g_menuOpen || g_wheelHeld);
            if (g_seqMode) {
                // Nothing to sample differently in seq - "mono" here just
                // means stop shifting and give both eyes the same frame, so
                // a menu or loading screen cannot land in one eye only.
                if (wantMono != g_seqMonoNow) {
                    g_seqMonoNow = wantMono;
                    Log("seq: %s (splices=%u)", wantMono ? "MONO" : "stereo",
                        g_dxvkSplices ? *g_dxvkSplices : 0u);
                }
                wantMono = false;   // fall through; SBS bookkeeping below is
                                    // about half-frame UVs and does not apply
            }
            if (wantMono != g_sbsMonoNow) {
                g_sbsMonoNow = wantMono;
                g_quadAspect = 0.0f;   // force per-eye quad UV rebuild
                Log("sbs: %s (menu=%d wheel=%d splices=%u)",
                    wantMono ? "MONO" : "stereo", (int)g_menuOpen,
                    (int)g_wheelHeld,
                    g_dxvkSplices ? *g_dxvkSplices : 0u);
            }
            // 34.6: wrist-HUD policy. Redirect UI draws to the fork's HUD
            // texture only during stereo gameplay; any menu/mono frame draws
            // its UI in view as before.
            // 38.49: push the ini bisector value once it can be seen to stick
            if (g_dxvkKillMask && g_killMaskIni != 0) {
                if (*g_dxvkKillMask != (unsigned int)g_killMaskIni) {
                    *g_dxvkKillMask = (unsigned int)g_killMaskIni;
                    Log("killmask: [Debug] KillMask=%d applied - if the "
                        "artifact vanishes, that class draws it",
                        g_killMaskIni);
                }
            }
            // 38.49: push the ini bisector value (the overlay checkboxes
            // still win - this only sets a non-zero starting mask).
            if (g_dxvkKillMask && g_killMaskIni != 0) {
                static bool kmTold = false;
                if (!kmTold) { kmTold = true;
                    *g_dxvkKillMask = (unsigned int)g_killMaskIni;
                    Log("killmask: [Debug] KillMask=%d applied - if the "
                        "artifact VANISHES, that class draws it",
                        g_killMaskIni);
                }
            }
            if (g_dxvkHudWrist) {
                // 34.7: the power WHEEL is also a kUpDup UI draw, and it was
                // redirected right off the screen. While the wheel is held
                // the redirect pauses so the wheel draws in view.
                // 35.6: AND the hands must actually be driving (g_handMesh).
                // Idling at the MAIN menu, the attract scene's camera kept
                // gameplay dispatches flowing, the stale-flag ghost test
                // cleared the menu flag, splices were high (live 3D
                // background) - and the redirect swept the MAIN MENU onto
                // the wrist panel. "The sword follows your hand = you are
                // playing" (the 30.23 lesson) is the discriminator that
                // cannot false-positive at a menu.
                // 38.46 MAIN-MENU FIX (the only HUD change kept from the
                // split experiment). Measured by the user: "if I was in the
                // main menu after my arms armed (e.g. I feel haptics) the
                // main menu would get tiny on my hand". g_handMesh latches
                // true once the hands arm and STAYS true at the menu, and the
                // menu flags miss the main menu in windowed mode - script
                // events are the only signal there. A possessed pawn cannot
                // exist at the main menu, so requiring one closes the hole
                // without touching anything the wrist HUD does in gameplay.
                bool dlgNow = (g_dlgHudOff && MaimNowMs() < g_dlgUntilMs);
                unsigned int want = (g_wristHud && g_handMesh &&
                                     CylTruthLive() && !dlgNow &&
                                     !g_menuOpen && !g_inMenu &&
                                     !g_sbsMonoNow && !g_wheelHeld) ? 1u : 0u;
                InterlockedExchange(&g_hudRedirLive, want ? 1 : 0);
                {   // say when the conversation window closes, so the log
                    // shows both edges and not just the entry
                    static bool dlgWas = false;
                    if (dlgWas && !dlgNow) Log("dialog: over - wrist HUD back");
                    dlgWas = dlgNow;
                }
                if (*g_dxvkHudWrist != want) {
                    *g_dxvkHudWrist = want;
                    Log("hud: wrist redirect %s", want ? "ON" : "off");
                }
            }
        }

        // DECOUPLED AER: let the game render as fast as it can (uncapped). Each
        // game frame is sheared for one eye and stored in that eye's texture;
        // eyes alternate. We only SUBMIT to the compositor on a ~90 Hz clock,
        // using the freshest image for each eye. So the game can run 150-200 fps
        // -> each eye refreshes ~75-100 fps and the two eyes are only one game
        // frame apart -> the AER blur shrinks toward nothing, while the headset
        // still always gets a solid 90 Hz (no judder even if fps dips).
        if (EnsureGameTex(g_capW, g_capH)) {
            if (g_seqMode) {
                // The frame we just captured is ONE eye, whole and full-size.
                // g_drawEye names the eye the fork was told to draw, so it is
                // the eye this frame belongs to; store it, flip, and tell the
                // fork which eye to draw next. The write happens here, in
                // Present, before any draw of the next frame can read it.
                if (g_seqMonoNow) {
                    g_ctx11->UpdateSubresource(g_texGame[0], 0, NULL, g_pixels, g_capW * 4, 0);
                    g_ctx11->UpdateSubresource(g_texGame[1], 0, NULL, g_pixels, g_capW * 4, 0);
                    if (g_dxvkEyeSign) *g_dxvkEyeSign = 0.0f;
                } else {
                    g_ctx11->UpdateSubresource(g_texGame[g_drawEye], 0, NULL, g_pixels, g_capW * 4, 0);
                    static bool seqPrimed = false;
                    if (!seqPrimed) {   // don't show a black eye on frame one
                        seqPrimed = true;
                        g_ctx11->UpdateSubresource(g_texGame[!g_drawEye], 0, NULL, g_pixels, g_capW * 4, 0);
                    }
                    g_drawEye ^= 1;
                    if (g_dxvkEyeSign) *g_dxvkEyeSign = g_drawEye ? 1.0f : -1.0f;
                }
            } else if (g_sbsMode) {
                // 30.29: the frame is already a stereo pair (left|right).
                // One upload; both eye quads sample their half of tex[0].
                g_ctx11->UpdateSubresource(g_texGame[0], 0, NULL, g_pixels, g_capW * 4, 0);
            } else if (g_stereoEnabled) {
                g_ctx11->UpdateSubresource(g_texGame[g_drawEye], 0, NULL, g_pixels, g_capW * 4, 0);
                static bool primed = false;
                if (!primed) {
                    g_ctx11->UpdateSubresource(g_texGame[!g_drawEye], 0, NULL, g_pixels, g_capW * 4, 0);
                    primed = true;
                }
                g_drawEye ^= 1;
            } else {
                g_ctx11->UpdateSubresource(g_texGame[0], 0, NULL, g_pixels, g_capW * 4, 0);
                g_ctx11->UpdateSubresource(g_texGame[1], 0, NULL, g_pixels, g_capW * 4, 0);
            }
        }

        // submit on a ~90 Hz wall-clock, regardless of game fps
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double msSince = g_qpcFreq ? (double)(now.QuadPart - g_lastSubmitQpc) * 1000.0 / (double)g_qpcFreq : 999.0;
        if (!g_haveSubmitted || msSince >= 10.9) {   // ~90.9 Hz
            bool poseOk = true;
            if (g_xrOn) {
                // 37.3: pose + pacing from the OpenXR frame loop. FrameBegin
                // pumps events, waits the frame, locates the views, and
                // feeds g_devPose[0]/TrackHead exactly like the branch below.
                poseOk = XrRtFrameBegin();
            } else {
                TrackedDevicePose_t poses[16];
                EVRCompositorError we = g_comp->WaitGetPoses(poses, 16, NULL, 0);
                if ((int)we != g_lastWaitErr) {
                    g_lastWaitErr = (int)we;
                    Log("WaitGetPoses result changed -> %d (0 = OK)", (int)we);
                }
                // Stage 7.3: stash raw device poses for MotionAim
                for (int di = 0; di < 16; di++) {
                    g_devPoseOk[di] = poses[di].bPoseIsValid != 0;
                    if (g_devPoseOk[di])
                        memcpy(g_devPose[di], poses[di].mDeviceToAbsoluteTracking.m,
                               sizeof(g_devPose[di]));
                }
                TrackHead(&poses[k_unTrackedDeviceIndex_Hmd]);
            }
            if (poseOk) {
                RtDriveUpdate();   // 30.70: render-time hand transform
                if (!g_padHookTried) { g_padHookTried = true; InstallPadHook(); }
                UpdateVirtualPad();
                ReticleTick();
                RtdMarkerTick();   // 30.71: controller rings for alignment
                RenderEyesAndSubmit();
                g_submits++;
                g_lastSubmitQpc = now.QuadPart;
                g_haveSubmitted = true;
            }
        }
    } else if (g_mode == MODE_THEATER && g_ov) {
        if (EnsureD3D11() && EnsureGameTex(g_capW, g_capH)) {
            g_ctx11->UpdateSubresource(g_texGame[0], 0, NULL, g_pixels, g_capW * 4, 0);
            g_ctx11->Flush();
            Texture_t t;
            t.handle = (void*)g_texGame[0];
            t.eType = ETextureType_TextureType_DirectX;
            t.eColorSpace = EColorSpace_ColorSpace_Gamma;
            EVROverlayError oe = g_ov->SetOverlayTexture(g_overlay, &t);
            if ((int)oe != g_lastSubmitErr) {
                g_lastSubmitErr = (int)oe;
                Log("overlay submit result changed -> %d", (int)oe);
            }
        }
    }
}
