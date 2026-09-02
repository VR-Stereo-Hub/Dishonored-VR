// core/vr/openxr_backend.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static void XrRtTryInit(void)
{
    static DWORD nextTry = 0;
    static int   tries = 0;
    if (g_xrOn || tries >= 10) return;
    DWORD now = GetTickCount();
    if (now < nextTry) return;
    nextTry = now + 5000;
    tries++;
    Log("xr: backend bring-up attempt %d", tries);
    dvr::crash::install();   // fingerprint VEH + minidump filter, idempotent

    if (!g_xrGipa && !XrLoadRuntime()) return;
    if (g_xriInst == XR_NULL_HANDLE) {
        XRB_FN(XR_NULL_HANDLE, xrCreateInstance);
        if (!xrCreateInstance) { Log("xr: no xrCreateInstance"); return; }
        // 38.7: ask for the cylinder-screen extension when the runtime has
        // it (enumerated pre-instance; a missing extension must never fail
        // instance creation, so it is only requested when advertised).
        const char* exts[2] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME, NULL };
        uint32_t nExts = 1;
        {
            XRB_FN(XR_NULL_HANDLE, xrEnumerateInstanceExtensionProperties);
            if (xrEnumerateInstanceExtensionProperties && g_xrCylCfg != 0) {
                static XrExtensionProperties eps[64];
                uint32_t ne = 0;
                for (int i = 0; i < 64; i++) {
                    memset(&eps[i], 0, sizeof(eps[i]));
                    eps[i].type = XR_TYPE_EXTENSION_PROPERTIES;
                }
                if (!XR_FAILED(xrEnumerateInstanceExtensionProperties(
                        NULL, 64, &ne, eps))) {
                    if (ne > 64) ne = 64;
                    for (uint32_t i = 0; i < ne; i++)
                        if (!strcmp(eps[i].extensionName,
                                XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME)) {
                            exts[nExts++] =
                                XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME;
                            g_xrCylOn = true;
                            break;
                        }
                }
            }
            Log("xr: cylinder-screen extension %s",
                g_xrCylOn ? "available - CYLINDER presentation" :
                (g_xrCylCfg == 0 ? "disabled by ini (flat quad)"
                                 : "not offered by runtime (flat quad)"));
        }
        XrInstanceCreateInfo ici; memset(&ici, 0, sizeof(ici));
        ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
        strcpy(ici.applicationInfo.applicationName, "DishonoredVR");
        strcpy(ici.applicationInfo.engineName, "dishonored-vr-proxy");
        ici.applicationInfo.applicationVersion = 373;
        ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
        ici.enabledExtensionCount = nExts;
        ici.enabledExtensionNames = exts;
        if (XR_FAILED(xrCreateInstance(&ici, &g_xriInst))) {
            Log("xr: xrCreateInstance failed"); g_xriInst = XR_NULL_HANDLE;
            return;
        }
        XRB_FN(g_xriInst, xrGetInstanceProperties);
        if (xrGetInstanceProperties) {
            XrInstanceProperties ip; memset(&ip, 0, sizeof(ip));
            ip.type = XR_TYPE_INSTANCE_PROPERTIES;
            if (!XR_FAILED(xrGetInstanceProperties(g_xriInst, &ip))) {
                strncpy(g_xriRuntimeName, ip.runtimeName,
                        sizeof(g_xriRuntimeName) - 1);
                Log("xr: runtime \"%s\"", g_xriRuntimeName);
                {   // 40.2: the crash file's run header must name the runtime.
                    // dvr-xrsim and VirtualDesktopXR fault into the same
                    // d3d11.dll and produce identical fingerprint text.
                    char cx[128];
                    _snprintf(cx, sizeof(cx) - 1, "backend=openxr runtime=\"%s\"",
                              g_xriRuntimeName);
                    cx[sizeof(cx) - 1] = 0;
                    dvr::crash::set_context(cx);
                }
            }
        }
    }
    XRB_FN(g_xriInst, xrGetSystem);
    XRB_FN(g_xriInst, xrEnumerateViewConfigurationViews);
    XRB_FN(g_xriInst, xrGetD3D11GraphicsRequirementsKHR);
    XRB_FN(g_xriInst, xrCreateSession);
    XRB_FN(g_xriInst, xrCreateReferenceSpace);
    XRB_FN(g_xriInst, xrEnumerateSwapchainFormats);
    XRB_FN(g_xriInst, xrCreateSwapchain);
    XRB_FN(g_xriInst, xrEnumerateSwapchainImages);
    g_xrf.pollEvent = NULL;
    { XRB_FN(g_xriInst, xrPollEvent);      g_xrf.pollEvent = xrPollEvent; }
    { XRB_FN(g_xriInst, xrBeginSession);   g_xrf.beginSession = xrBeginSession; }
    { XRB_FN(g_xriInst, xrEndSession);     g_xrf.endSession = xrEndSession; }
    { XRB_FN(g_xriInst, xrWaitFrame);      g_xrf.waitFrame = xrWaitFrame; }
    { XRB_FN(g_xriInst, xrBeginFrame);     g_xrf.beginFrame = xrBeginFrame; }
    { XRB_FN(g_xriInst, xrEndFrame);       g_xrf.endFrame = xrEndFrame; }
    { XRB_FN(g_xriInst, xrLocateViews);    g_xrf.locateViews = xrLocateViews; }
    { XRB_FN(g_xriInst, xrAcquireSwapchainImage); g_xrf.acquire = xrAcquireSwapchainImage; }
    { XRB_FN(g_xriInst, xrWaitSwapchainImage);    g_xrf.wait = xrWaitSwapchainImage; }
    { XRB_FN(g_xriInst, xrReleaseSwapchainImage); g_xrf.release = xrReleaseSwapchainImage; }
    // 38.9 input
    { XRB_FN(g_xriInst, xrStringToPath);          g_xrf.stringToPath = xrStringToPath; }
    { XRB_FN(g_xriInst, xrCreateActionSet);       g_xrf.createActionSet = xrCreateActionSet; }
    { XRB_FN(g_xriInst, xrCreateAction);          g_xrf.createAction = xrCreateAction; }
    { XRB_FN(g_xriInst, xrSuggestInteractionProfileBindings); g_xrf.suggestBindings = xrSuggestInteractionProfileBindings; }
    { XRB_FN(g_xriInst, xrCreateActionSpace);     g_xrf.createActionSpace = xrCreateActionSpace; }
    { XRB_FN(g_xriInst, xrAttachSessionActionSets); g_xrf.attachActionSets = xrAttachSessionActionSets; }
    { XRB_FN(g_xriInst, xrSyncActions);           g_xrf.syncActions = xrSyncActions; }
    { XRB_FN(g_xriInst, xrGetActionStateFloat);   g_xrf.getFloat = xrGetActionStateFloat; }
    { XRB_FN(g_xriInst, xrGetActionStateBoolean); g_xrf.getBool = xrGetActionStateBoolean; }
    { XRB_FN(g_xriInst, xrGetActionStateVector2f); g_xrf.getVec2 = xrGetActionStateVector2f; }
    { XRB_FN(g_xriInst, xrLocateSpace);           g_xrf.locateSpace = xrLocateSpace; }
    { XRB_FN(g_xriInst, xrApplyHapticFeedback);   g_xrf.applyHaptic = xrApplyHapticFeedback; }
    if (!xrGetSystem || !g_xrf.waitFrame || !g_xrf.endFrame ||
        !g_xrf.locateViews || !g_xrf.acquire) {
        Log("xr: core functions missing"); return;
    }

    XrSystemGetInfo sgi; memset(&sgi, 0, sizeof(sgi));
    sgi.type = XR_TYPE_SYSTEM_GET_INFO;
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(g_xriInst, &sgi, &g_xriSys))) {
        Log("xr: no HMD yet (headset off / VD not streaming?) - will retry");
        return;
    }
    XrViewConfigurationView vcv[2]; memset(vcv, 0, sizeof(vcv));
    vcv[0].type = vcv[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    uint32_t nv = 0;
    xrEnumerateViewConfigurationViews(g_xriInst, g_xriSys,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &nv, vcv);
    if (nv < 2 || !vcv[0].recommendedImageRectWidth) {
        Log("xr: bad view configuration"); return;
    }
    g_xrEyeW = vcv[0].recommendedImageRectWidth;
    g_xrEyeH = vcv[0].recommendedImageRectHeight;
    Log("xr: %ux%u per eye", g_xrEyeW, g_xrEyeH);

    XrGraphicsRequirementsD3D11KHR gr; memset(&gr, 0, sizeof(gr));
    gr.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
    XrResult grRes = xrGetD3D11GraphicsRequirementsKHR(g_xriInst, g_xriSys, &gr);  // spec: must call

    // 40.1 (the author's 39.3 fix): this LUID used to be fetched and dropped on
    // the floor - the spec says the call must happen, so it happened, and the
    // answer went nowhere. Publish it so EnsureD3D11 creates the device on the
    // adapter the runtime can actually read from.
    if (XR_SUCCEEDED(grRes)) {
        g_wantAdapterLuid.LowPart  = (DWORD)gr.adapterLuid.LowPart;
        g_wantAdapterLuid.HighPart = (LONG)gr.adapterLuid.HighPart;
        g_wantAdapterLuidOk = (gr.adapterLuid.LowPart || gr.adapterLuid.HighPart);
        Log("xr: runtime graphics requirements: adapter luid %08lX-%08lX, "
            "min feature level 0x%04X%s",
            (unsigned long)gr.adapterLuid.HighPart,
            (unsigned long)gr.adapterLuid.LowPart,
            (unsigned)gr.minFeatureLevel,
            g_wantAdapterLuidOk ? "" : " (zero luid - runtime expressed no preference)");
    } else {
        DVR_WARN("xr: xrGetD3D11GraphicsRequirementsKHR failed (%d) - the adapter the "
                 "runtime needs is unknown; the device will use the default adapter",
                 (int)grRes);
    }

    if (!EnsureD3D11() || !g_dev11) { Log("xr: no D3D11 device yet"); return; }
    // If something else built the device before XR bring-up, it never saw the
    // LUID. Say so out loud rather than leaving a silent mismatch.
    if (g_wantAdapterLuidOk && g_gotAdapterLuidOk &&
        !LuidEq(g_gotAdapterLuid, g_wantAdapterLuid)) {
        DVR_ERROR("xr: the D3D11 device predates XR bring-up and sits on the WRONG "
                  "adapter (\"%s\"). Restarting the game usually re-orders this; if it "
                  "persists the capture path is creating the device too early.",
                  g_gotAdapterName);
    }
    // pipeline (shaders + eye RTs) - with g_xrEyeW already known, the eye
    // RTs come out at the runtime's recommended size, so CopyResource into
    // the swapchain images is dimension-exact
    if (!EnsurePipeline()) { Log("xr: pipeline init failed"); return; }

    if (g_xriSess == XR_NULL_HANDLE) {
        XrGraphicsBindingD3D11KHR gb; memset(&gb, 0, sizeof(gb));
        gb.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
        gb.device = g_dev11;
        XrSessionCreateInfo sci; memset(&sci, 0, sizeof(sci));
        sci.type = XR_TYPE_SESSION_CREATE_INFO;
        sci.next = &gb;
        sci.systemId = g_xriSys;
        if (XR_FAILED(xrCreateSession(g_xriInst, &sci, &g_xriSess))) {
            Log("xr: xrCreateSession failed"); g_xriSess = XR_NULL_HANDLE;
            return;
        }
        XrReferenceSpaceCreateInfo rsci; memset(&rsci, 0, sizeof(rsci));
        rsci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        rsci.poseInReferenceSpace.orientation.w = 1.0f;
        xrCreateReferenceSpace(g_xriSess, &rsci, &g_xriSpace);
    }

    // swapchains: R8G8B8A8 family required (CopyResource from the UNORM eye
    // textures is family-legal; sRGB tells the compositor the truth about
    // the gamma-encoded game image)
    int64_t fmts[32]; uint32_t nf = 0;
    xrEnumerateSwapchainFormats(g_xriSess, 32, &nf, fmts);
    int64_t pick = 0;
    for (uint32_t i = 0; i < nf && !pick; i++)
        if (fmts[i] == 29 /*R8G8B8A8_UNORM_SRGB*/) pick = fmts[i];
    for (uint32_t i = 0; i < nf && !pick; i++)
        if (fmts[i] == 28 /*R8G8B8A8_UNORM*/) pick = fmts[i];
    if (!pick) { Log("xr: no R8G8B8A8-family swapchain format"); return; }
    for (int eye = 0; eye < 2; eye++) {
        if (g_xriSwc[eye] != XR_NULL_HANDLE) continue;
        XrSwapchainCreateInfo swci; memset(&swci, 0, sizeof(swci));
        swci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                          XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swci.format = pick;
        swci.sampleCount = 1;
        swci.width = g_xrEyeW; swci.height = g_xrEyeH;
        swci.faceCount = 1; swci.arraySize = 1; swci.mipCount = 1;
        if (XR_FAILED(xrCreateSwapchain(g_xriSess, &swci, &g_xriSwc[eye]))) {
            Log("xr: swapchain %d failed", eye); return;
        }
        XrSwapchainImageD3D11KHR imgs[8];
        memset(imgs, 0, sizeof(imgs));
        for (int i = 0; i < 8; i++) imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        uint32_t ni = 0;
        xrEnumerateSwapchainImages(g_xriSwc[eye], 8, &ni,
            (XrSwapchainImageBaseHeader*)imgs);
        if (ni > 8) ni = 8;
        g_xriImgN[eye] = ni;
        for (uint32_t i = 0; i < ni; i++) g_xriImg[eye][i] = imgs[i].texture;
    }
    // ---- 38.3 XR-3 bring-up: VIEW space, publish textures, shared-context
    // protection, and the pace thread ------------------------------------
    if (g_xrQuad) {
        if (g_xriViewSpace == XR_NULL_HANDLE) {
            XrReferenceSpaceCreateInfo rsci; memset(&rsci, 0, sizeof(rsci));
            rsci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
            rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            rsci.poseInReferenceSpace.orientation.w = 1.0f;
            if (XR_FAILED(xrCreateReferenceSpace(g_xriSess, &rsci,
                                                 &g_xriViewSpace))) {
                Log("xr: VIEW space failed"); return;
            }
        }
        for (int eye = 0; eye < 2; eye++) {
            if (g_xrpTex[eye]) continue;
            D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
            td.Width = g_xrEyeW; td.Height = g_xrEyeH;
            td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            if (FAILED(g_dev11->CreateTexture2D(&td, NULL, &g_xrpTex[eye]))) {
                Log("xr: publish tex %d failed", eye); return;
            }
        }
        {   // the pace thread shares the immediate context for CopyResource
            XrMtItf* mt = NULL;
            if (SUCCEEDED(g_ctx11->QueryInterface(kIID_D3D10Mt, (void**)&mt))
                && mt) {
                mt->SetMultithreadProtected(TRUE);
                mt->Release();
                Log("xr: D3D11 context multithread protection ON");
            } else {
                Log("xr: WARNING - no ID3D10Multithread; context unguarded");
            }
        }
        if (!g_xrCsInit) { InitializeCriticalSection(&g_xrCs); g_xrCsInit = true; }
        // 38.9: input - actions + bindings before attach, attach before the
        // pace thread starts syncing
        XrInpInit();
        XrInpAttach();
        if (!g_xrThread) {
            InterlockedExchange(&g_xrRun, 1);
            g_xrThread = CreateThread(NULL, 0, XrPaceThread, NULL, 0, NULL);
        }
    }
    g_xrOn = true;
    g_mode = MODE_SCENE;
    g_quadAspect = 0.0f;                 // rebuild quads with XR frustums
    Log("xr: BACKEND LIVE on \"%s\" - %ux%u/eye, fmt 0x%x, %u+%u images%s",
        g_xriRuntimeName, g_xrEyeW, g_xrEyeH, (unsigned)pick,
        g_xriImgN[0], g_xriImgN[1],
        g_xrQuad ? " (XR-3: quad layers, detached pacing)" : "");
}


static bool XrRtFrameBeginSync(void)   // XR-2 path, verbatim ([VR] XrQuads)
{
    if (!g_xrOn) return false;
    // event pump
    if (g_xrf.pollEvent) {
        XrEventDataBuffer ev; memset(&ev, 0, sizeof(ev));
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
        while (g_xrf.pollEvent(g_xriInst, &ev) == XR_SUCCESS) {
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
                        Log("xr: session BEGUN");
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
    if (!g_xriBegun) return false;

    XrFrameState fs; memset(&fs, 0, sizeof(fs));
    fs.type = XR_TYPE_FRAME_STATE;
    if (XR_FAILED(g_xrf.waitFrame(g_xriSess, NULL, &fs))) return false;
    if (XR_FAILED(g_xrf.beginFrame(g_xriSess, NULL))) return false;
    g_xriFrameOpen = true;
    g_xriDispTime = fs.predictedDisplayTime;

    XrViewLocateInfo vli; memset(&vli, 0, sizeof(vli));
    vli.type = XR_TYPE_VIEW_LOCATE_INFO;
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = g_xriDispTime;
    vli.space = g_xriSpace;
    XrViewState vs; memset(&vs, 0, sizeof(vs));
    vs.type = XR_TYPE_VIEW_STATE;
    uint32_t nv = 0;
    memset(g_xriViews, 0, sizeof(g_xriViews));
    g_xriViews[0].type = g_xriViews[1].type = XR_TYPE_VIEW;
    if (XR_FAILED(g_xrf.locateViews(g_xriSess, &vli, &vs, 2, &nv, g_xriViews))
        || nv < 2)
        return true;   // frame is open; submit black rather than stall

    // ---- pose adapter: XR LOCAL space == OpenVR standing space -------------
    if (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
        XrQuaternionf q = g_xriViews[0].pose.orientation;
        float px = 0.5f * (g_xriViews[0].pose.position.x +
                           g_xriViews[1].pose.position.x);
        float py = 0.5f * (g_xriViews[0].pose.position.y +
                           g_xriViews[1].pose.position.y);
        float pz = 0.5f * (g_xriViews[0].pose.position.z +
                           g_xriViews[1].pose.position.z);
        TrackedDevicePose_t hp; memset(&hp, 0, sizeof(hp));
        float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
        float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
        float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
        float (*m)[4] = hp.mDeviceToAbsoluteTracking.m;
        m[0][0] = 1 - 2*(yy + zz); m[0][1] = 2*(xy - wz); m[0][2] = 2*(xz + wy);
        m[1][0] = 2*(xy + wz); m[1][1] = 1 - 2*(xx + zz); m[1][2] = 2*(yz - wx);
        m[2][0] = 2*(xz - wy); m[2][1] = 2*(yz + wx); m[2][2] = 1 - 2*(xx + yy);
        m[0][3] = px; m[1][3] = py; m[2][3] = pz;
        hp.bPoseIsValid = 1; hp.bDeviceIsConnected = 1;
        hp.eTrackingResult = ETrackingResult_TrackingResult_Running_OK;
        memcpy(g_devPose[0], m, sizeof(g_devPose[0]));
        g_devPoseOk[0] = true;
        TrackHead(&hp);
    }
    // ---- frustum adapter: XrFovf angles -> GetProjectionRaw tangents -------
    // (derived from BuildEyeQuads' own math: t = -tan(up), b = -tan(down),
    // l = tan(left), r = tan(right); left/down angles are negative in XR)
    for (int eye = 0; eye < 2; eye++) {
        // 38.6: the raw tangents ARE the XR tangents - no negation. The old
        // -tan(up)/-tan(down) swap put the up-fov in the bottom slot, so the
        // screen centered 13 deg UP on the Quest's down-biased frustum
        // ("screen too high, black border at the bottom" - measured: Steam
        // Link reports t=-1.428 b=+0.966 for the same lens; now we agree).
        g_eyeFr[eye][0] = tanf(g_xriViews[eye].fov.angleLeft);
        g_eyeFr[eye][1] = tanf(g_xriViews[eye].fov.angleRight);
        g_eyeFr[eye][2] = tanf(g_xriViews[eye].fov.angleDown);
        g_eyeFr[eye][3] = tanf(g_xriViews[eye].fov.angleUp);
    }
    {
        float dx = g_xriViews[1].pose.position.x - g_xriViews[0].pose.position.x;
        float dy = g_xriViews[1].pose.position.y - g_xriViews[0].pose.position.y;
        float dz = g_xriViews[1].pose.position.z - g_xriViews[0].pose.position.z;
        float ipd = sqrtf(dx*dx + dy*dy + dz*dz);
        if (ipd > 0.04f && ipd < 0.08f) g_ipdM = ipd;
        for (int eye = 0; eye < 2; eye++) {
            g_eyeOffs[eye][0] = (eye == 0 ? -0.5f : 0.5f) * ipd;
            g_eyeOffs[eye][1] = 0.0f; g_eyeOffs[eye][2] = 0.0f;
        }
    }
    if (!g_eyeFrOk) { g_eyeFrOk = true; g_quadAspect = 0.0f; }
    return true;
}


static void XrRtSubmitEye(int eye)
{
    if (!g_xriFrameOpen || g_xriSwc[eye] == XR_NULL_HANDLE) return;
    uint32_t idx = 0;
    if (XR_FAILED(g_xrf.acquire(g_xriSwc[eye], NULL, &idx))) return;
    XrSwapchainImageWaitInfo wi; memset(&wi, 0, sizeof(wi));
    wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    wi.timeout = 1000000000;             // 1 s - never wedge the render thread
    if (!XR_FAILED(g_xrf.wait(g_xriSwc[eye], &wi))) {
        if (idx < g_xriImgN[eye] && g_xriImg[eye][idx] && g_eyeTex[eye])
            g_ctx11->CopyResource(g_xriImg[eye][idx], g_eyeTex[eye]);
    }
    XrSwapchainImageReleaseInfo ri; memset(&ri, 0, sizeof(ri));
    ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    g_xrf.release(g_xriSwc[eye], &ri);
}


static void XrRtFrameEnd(void)
{
    if (!g_xriFrameOpen) return;
    g_xriFrameOpen = false;
    XrCompositionLayerProjectionView pv[2];
    memset(pv, 0, sizeof(pv));
    for (int eye = 0; eye < 2; eye++) {
        pv[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        pv[eye].pose = g_xriViews[eye].pose;
        pv[eye].fov  = g_xriViews[eye].fov;
        pv[eye].subImage.swapchain = g_xriSwc[eye];
        pv[eye].subImage.imageRect.extent.width  = (int32_t)g_xrEyeW;
        pv[eye].subImage.imageRect.extent.height = (int32_t)g_xrEyeH;
    }
    XrCompositionLayerProjection layer; memset(&layer, 0, sizeof(layer));
    layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    layer.space = g_xriSpace;
    layer.viewCount = 2;
    layer.views = pv;
    const XrCompositionLayerBaseHeader* layers[1] =
        { (const XrCompositionLayerBaseHeader*)&layer };
    XrFrameEndInfo fe; memset(&fe, 0, sizeof(fe));
    fe.type = XR_TYPE_FRAME_END_INFO;
    fe.displayTime = g_xriDispTime;
    fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fe.layerCount = 1;
    fe.layers = layers;
    XrResult xr = g_xrf.endFrame(g_xriSess, &fe);
    static int lastEndErr = 0;
    if ((int)xr != lastEndErr) {
        lastEndErr = (int)xr;
        Log("xr: xrEndFrame result changed -> %d (0 = OK)", (int)xr);
    }
}


// ---- XR-3: game-thread side --------------------------------------------
// Non-blocking pose consume: TrackHead and the game-state writers stay on
// the thread that has always run them; only the DATA comes from the pace
// thread. Returns false until the pace thread has published a valid pose.
static bool XrRtFrameBegin(void)
{
    if (!g_xrOn) return false;
    if (!g_xrQuad) return XrRtFrameBeginSync();
    float m[3][4]; float fr[2][4]; float ipd; bool valid;
    EnterCriticalSection(&g_xrCs);
    valid = g_xrpValid;
    memcpy(m, g_xrpM, sizeof(m));
    memcpy(fr, g_xrpFr, sizeof(fr));
    ipd = g_xrpIpd;
    if (g_xrpLocOk) {                      // 38.8/38.9: raw views -> ring
        memcpy(g_xrViewRing[g_xrRingHead & 3], g_xrpLocViews,
               sizeof(g_xrViewRing[0]));
        g_xrRingHead++;
        g_xrConsOk = true;
        g_xrRingMs = MaimNowMs();          // 38.83: freshness follows the views
    }
    // 38.9: hand poses -> the tracked-device slots every subsystem reads
    bool hOk[2] = { false, false };
    float hm2[2][3][4];
    if (g_xrInpAttached) {
        for (int h = 0; h < 2; h++) {
            hOk[h] = g_xrInp.handOk[h];
            if (hOk[h]) memcpy(hm2[h], g_xrInp.hand[h], sizeof(hm2[h]));
        }
    }
    LeaveCriticalSection(&g_xrCs);
    if (g_xrInpAttached) {
        for (int h = 0; h < 2; h++) {
            if (hOk[h]) {
                memcpy(g_devPose[3 + h], hm2[h], sizeof(g_devPose[0]));
                g_devPoseOk[3 + h] = true;
            } else g_devPoseOk[3 + h] = false;
        }
        g_ctrlIdx[0] = 3; g_ctrlIdx[1] = 4;
    }
    if (!valid) return false;
    TrackedDevicePose_t hp; memset(&hp, 0, sizeof(hp));
    memcpy(hp.mDeviceToAbsoluteTracking.m, m, sizeof(m));
    hp.bPoseIsValid = 1; hp.bDeviceIsConnected = 1;
    hp.eTrackingResult = ETrackingResult_TrackingResult_Running_OK;
    memcpy(g_devPose[0], m, sizeof(g_devPose[0]));
    g_devPoseOk[0] = true;
    TrackHead(&hp);
    memcpy(g_eyeFr, fr, sizeof(g_eyeFr));
    if (ipd > 0.04f && ipd < 0.08f) g_ipdM = ipd;
    for (int eye = 0; eye < 2; eye++) {
        g_eyeOffs[eye][0] = (eye == 0 ? -0.5f : 0.5f) * ipd;
        g_eyeOffs[eye][1] = 0.0f; g_eyeOffs[eye][2] = 0.0f;
    }
    if (!g_eyeFrOk) { g_eyeFrOk = true; g_quadAspect = 0.0f; }
    return true;
}


// Publish the finished eye textures + current screen geometry to the pace
// thread. Cheap: two device-side copies and a few floats, no XR calls.
static void XrRtPublish(void)
{
    if (!g_xrpTex[0] || !g_xrpTex[1]) return;
    EnterCriticalSection(&g_xrCs);
    g_ctx11->CopyResource(g_xrpTex[0], g_eyeTex[0]);
    g_ctx11->CopyResource(g_xrpTex[1], g_eyeTex[1]);
    g_xrpQuadW  = g_bqW;  g_xrpQuadH  = g_bqH;
    g_xrpQuadCx = g_bqCx; g_xrpQuadCy = g_bqCy + g_xrScreenY;   // 38.6 knob
    g_xrpQuadD  = g_screenDist;
    if (g_xrConsOk) {                      // 38.8/38.9: age-matched stamp
        int back = g_xrPoseDelay;
        if (back < 0) back = 0; if (back > 3) back = 3;
        int idx = g_xrRingHead - 1 - back;
        if (idx < 0) idx = 0;
        memcpy(g_xrpPubViews, g_xrViewRing[idx & 3], sizeof(g_xrpPubViews));
        g_xrpPubOk = true;
        g_xrpPubMs = g_xrRingMs;           // 38.83: stamp age = views' age
    }
    LeaveCriticalSection(&g_xrCs);
    InterlockedIncrement(&g_xrpSeq);
}
