// legacy/xr_bench.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static DWORD WINAPI XrBenchThread(LPVOID)
{
    Log("xrb: ==== OpenXR bench (XR-1) - 32-bit in-process bring-up ====");
    if (!XrLoadRuntime()) return 0;

    XRB_FN(XR_NULL_HANDLE, xrCreateInstance);
    if (!xrCreateInstance) { Log("xrb: FAIL - no xrCreateInstance"); return 0; }
    const char* extD3D = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
    XrInstanceCreateInfo ici; memset(&ici, 0, sizeof(ici));
    ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
    strcpy(ici.applicationInfo.applicationName, "DishonoredVR");
    ici.applicationInfo.applicationVersion = 370;
    strcpy(ici.applicationInfo.engineName, "dishonored-vr-proxy");
    ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = &extD3D;
    XrInstance inst = XR_NULL_HANDLE;
    XrResult xr = xrCreateInstance(&ici, &inst);
    if (XR_FAILED(xr)) { Log("xrb: FAIL - xrCreateInstance %d", (int)xr); return 0; }

    XRB_FN(inst, xrGetInstanceProperties);
    XRB_FN(inst, xrGetSystem);
    XRB_FN(inst, xrGetSystemProperties);
    XRB_FN(inst, xrEnumerateViewConfigurationViews);
    XRB_FN(inst, xrGetD3D11GraphicsRequirementsKHR);
    XRB_FN(inst, xrCreateSession);
    XRB_FN(inst, xrCreateReferenceSpace);
    XRB_FN(inst, xrEnumerateSwapchainFormats);
    XRB_FN(inst, xrCreateSwapchain);
    XRB_FN(inst, xrEnumerateSwapchainImages);
    XRB_FN(inst, xrPollEvent);
    XRB_FN(inst, xrBeginSession);
    XRB_FN(inst, xrEndSession);
    XRB_FN(inst, xrWaitFrame);
    XRB_FN(inst, xrBeginFrame);
    XRB_FN(inst, xrEndFrame);
    XRB_FN(inst, xrDestroySession);
    XRB_FN(inst, xrDestroyInstance);

    XrInstanceProperties ip; memset(&ip, 0, sizeof(ip));
    ip.type = XR_TYPE_INSTANCE_PROPERTIES;
    if (xrGetInstanceProperties && !XR_FAILED(xrGetInstanceProperties(inst, &ip)))
        Log("xrb: runtime \"%s\" version %u.%u.%u", ip.runtimeName,
            (unsigned)XR_VERSION_MAJOR(ip.runtimeVersion),
            (unsigned)XR_VERSION_MINOR(ip.runtimeVersion),
            (unsigned)XR_VERSION_PATCH(ip.runtimeVersion));

    XrSystemGetInfo sgi; memset(&sgi, 0, sizeof(sgi));
    sgi.type = XR_TYPE_SYSTEM_GET_INFO;
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId sys = XR_NULL_SYSTEM_ID;
    xr = xrGetSystem(inst, &sgi, &sys);
    if (XR_FAILED(xr)) {
        Log("xrb: PARTIAL - runtime up but no HMD system (%d). With xrsim "
            "this is a FAILURE; with VDXR it means the headset is not on.",
            (int)xr);
        xrDestroyInstance(inst); return 0;
    }
    XrSystemProperties sp; memset(&sp, 0, sizeof(sp));
    sp.type = XR_TYPE_SYSTEM_PROPERTIES;
    if (!XR_FAILED(xrGetSystemProperties(inst, sys, &sp)))
        Log("xrb: system \"%s\" (vendor %u)", sp.systemName, sp.vendorId);

    uint32_t nViews = 0;
    XrViewConfigurationView vcv[2];
    memset(vcv, 0, sizeof(vcv));
    vcv[0].type = vcv[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    xrEnumerateViewConfigurationViews(inst, sys,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &nViews, vcv);
    Log("xrb: %u views, recommended %ux%u per eye", nViews,
        vcv[0].recommendedImageRectWidth, vcv[0].recommendedImageRectHeight);

    XrGraphicsRequirementsD3D11KHR gr; memset(&gr, 0, sizeof(gr));
    gr.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
    if (!xrGetD3D11GraphicsRequirementsKHR ||
        XR_FAILED(xrGetD3D11GraphicsRequirementsKHR(inst, sys, &gr))) {
        Log("xrb: FAIL - D3D11 graphics requirements"); xrDestroyInstance(inst);
        return 0;
    }
    Log("xrb: D3D11 min feature level 0x%x, adapter luid %08lx%08lx",
        (unsigned)gr.minFeatureLevel, (unsigned long)gr.adapterLuid.HighPart,
        (unsigned long)gr.adapterLuid.LowPart);

    // own device for the bench (the real backend will share the eye-render
    // device; the bench must not touch live rendering)
    HMODULE d11 = LoadLibraryA("d3d11.dll");
    PFN_D3D11CreateDevice create = d11 ?
        (PFN_D3D11CreateDevice)GetProcAddress(d11, "D3D11CreateDevice") : NULL;
    ID3D11Device* dev = NULL; ID3D11DeviceContext* ctx = NULL;
    D3D_FEATURE_LEVEL fl;
    if (!create || FAILED(create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                                 NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx))) {
        Log("xrb: FAIL - D3D11 device"); xrDestroyInstance(inst); return 0;
    }

    XrGraphicsBindingD3D11KHR gb; memset(&gb, 0, sizeof(gb));
    gb.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
    gb.device = dev;
    XrSessionCreateInfo sci; memset(&sci, 0, sizeof(sci));
    sci.type = XR_TYPE_SESSION_CREATE_INFO;
    sci.next = &gb;
    sci.systemId = sys;
    XrSession sess = XR_NULL_HANDLE;
    xr = xrCreateSession(inst, &sci, &sess);
    if (XR_FAILED(xr)) { Log("xrb: FAIL - xrCreateSession %d", (int)xr);
                         xrDestroyInstance(inst); return 0; }
    Log("xrb: session created on our D3D11 device");

    XrReferenceSpaceCreateInfo rsci; memset(&rsci, 0, sizeof(rsci));
    rsci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.0f;
    XrSpace space = XR_NULL_HANDLE;
    xrCreateReferenceSpace(sess, &rsci, &space);

    int64_t fmts[32]; uint32_t nf = 0;
    xrEnumerateSwapchainFormats(sess, 32, &nf, fmts);
    Log("xrb: %u swapchain formats (first 0x%x)", nf,
        nf ? (unsigned)fmts[0] : 0u);
    XrSwapchainCreateInfo swci; memset(&swci, 0, sizeof(swci));
    swci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    swci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                      XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    swci.format = nf ? fmts[0] : 87 /* DXGI_FORMAT_B8G8R8A8_UNORM */;
    swci.sampleCount = 1;
    swci.width  = vcv[0].recommendedImageRectWidth  ?
                  vcv[0].recommendedImageRectWidth  : 1024;
    swci.height = vcv[0].recommendedImageRectHeight ?
                  vcv[0].recommendedImageRectHeight : 1024;
    swci.faceCount = 1; swci.arraySize = 1; swci.mipCount = 1;
    XrSwapchain swc = XR_NULL_HANDLE;
    xr = xrCreateSwapchain(sess, &swci, &swc);
    if (XR_FAILED(xr)) Log("xrb: FAIL - xrCreateSwapchain %d", (int)xr);
    else {
        uint32_t ni = 0;
        xrEnumerateSwapchainImages(swc, 0, &ni, NULL);
        Log("xrb: swapchain %ux%u created, %u images", swci.width,
            swci.height, ni);
    }

    // pump events to READY, begin, run 30 frames with zero layers
    bool began = false; int frames = 0;
    DWORD until = GetTickCount() + 8000;
    while (GetTickCount() < until && frames < 30) {
        XrEventDataBuffer ev; memset(&ev, 0, sizeof(ev));
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
        while (xrPollEvent(inst, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                XrEventDataSessionStateChanged* sc =
                    (XrEventDataSessionStateChanged*)&ev;
                Log("xrb: session state -> %d", (int)sc->state);
                if (sc->state == XR_SESSION_STATE_READY && !began) {
                    XrSessionBeginInfo bi; memset(&bi, 0, sizeof(bi));
                    bi.type = XR_TYPE_SESSION_BEGIN_INFO;
                    bi.primaryViewConfigurationType =
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (!XR_FAILED(xrBeginSession(sess, &bi))) {
                        began = true; Log("xrb: session BEGUN");
                    }
                }
            }
            memset(&ev, 0, sizeof(ev)); ev.type = XR_TYPE_EVENT_DATA_BUFFER;
        }
        if (began) {
            XrFrameState fs; memset(&fs, 0, sizeof(fs));
            fs.type = XR_TYPE_FRAME_STATE;
            if (XR_FAILED(xrWaitFrame(sess, NULL, &fs))) break;
            xrBeginFrame(sess, NULL);
            XrFrameEndInfo fe; memset(&fe, 0, sizeof(fe));
            fe.type = XR_TYPE_FRAME_END_INFO;
            fe.displayTime = fs.predictedDisplayTime;
            fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            if (XR_FAILED(xrEndFrame(sess, &fe))) break;
            frames++;
        } else Sleep(20);
    }
    Log(frames >= 30 ? "xrb: ==== PASS - %d frames ran on \"%s\" ===="
                     : "xrb: ==== INCOMPLETE - only %d frames (\"%s\") ====",
        frames, ip.runtimeName);
    if (began && xrEndSession) xrEndSession(sess);
    if (xrDestroySession) xrDestroySession(sess);
    if (xrDestroyInstance) xrDestroyInstance(inst);
    if (ctx) ctx->Release();
    if (dev) dev->Release();
    return 0;
}
