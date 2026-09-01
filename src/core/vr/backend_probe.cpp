// core/vr/backend_probe.cpp - which VR backend serves this machine.
// Included by the unity build (it uses the OpenXR loader-negotiation code and
// the config globals).
//
// The original 38.5 "auto" answered by taking a process snapshot: OpenXR only
// when VirtualDesktop.Streamer.exe was running and vrserver.exe was not. That
// is why a Quest on Link, Air Link or Steam Link, and every other OpenXR
// headset, fell through to the OpenVR path. Auto now asks the runtimes:
//
//   1. env DISHONORED_VR_BACKEND = openvr | openxr | auto, else [VR] Backend
//   2. auto: OpenXR - load the runtime the way the backend will (env
//      XR_RUNTIME_JSON, then [VR] XrRuntimeJson, then the 32-bit registry),
//      xrCreateInstance with XR_KHR_D3D11_enable, xrGetSystem(HMD). A system
//      = an OpenXR headset is on and reachable from a 32-bit process. The
//      probe instance is destroyed again so the backend creates its own with
//      the extensions it wants.
//   3. else OpenVR - openvr_api.dll next to the proxy, VR_IsRuntimeInstalled()
//      and VR_IsHmdPresent(). Neither launches SteamVR.
//   4. neither answered: OpenVR, which is what every earlier build did when
//      no VD streamer was running (the SteamVR rig's path; TryInitVR keeps
//      retrying until SteamVR is up).
// SteamVR registers no 32-bit OpenXR runtime, so a SteamVR-only machine still
// lands on OpenVR; a VDXR machine lands on OpenXR even with SteamVR running.

static bool BackendProbeOpenXr()
{
    if (!g_xrGipa && !XrLoadRuntime()) {
        Log("probe: no 32-bit OpenXR runtime could be negotiated");
        return false;
    }
    XRB_FN(XR_NULL_HANDLE, xrCreateInstance);
    if (!xrCreateInstance) return false;
    const char* exts[1] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
    XrInstanceCreateInfo ici; memset(&ici, 0, sizeof(ici));
    ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
    strncpy(ici.applicationInfo.applicationName, "DishonoredVR-probe", XR_MAX_APPLICATION_NAME_SIZE - 1);
    strncpy(ici.applicationInfo.engineName, "dishonored-vr-proxy", XR_MAX_ENGINE_NAME_SIZE - 1);
    ici.applicationInfo.applicationVersion = 1;
    ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = exts;
    XrInstance inst = XR_NULL_HANDLE;
    XrResult r = xrCreateInstance(&ici, &inst);
    if (XR_FAILED(r) || inst == XR_NULL_HANDLE) {
        Log("probe: xrCreateInstance failed (%d) - no usable OpenXR runtime", (int)r);
        return false;
    }
    char runtime[XR_MAX_RUNTIME_NAME_SIZE] = "?";
    XRB_FN(inst, xrGetInstanceProperties);
    if (xrGetInstanceProperties) {
        XrInstanceProperties ip; memset(&ip, 0, sizeof(ip));
        ip.type = XR_TYPE_INSTANCE_PROPERTIES;
        if (!XR_FAILED(xrGetInstanceProperties(inst, &ip)))
            strncpy(runtime, ip.runtimeName, sizeof(runtime) - 1);
    }
    XRB_FN(inst, xrGetSystem);
    XRB_FN(inst, xrDestroyInstance);
    bool ok = false;
    if (xrGetSystem) {
        XrSystemGetInfo sgi; memset(&sgi, 0, sizeof(sgi));
        sgi.type = XR_TYPE_SYSTEM_GET_INFO;
        sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId sys = XR_NULL_SYSTEM_ID;
        r = xrGetSystem(inst, &sgi, &sys);
        ok = !XR_FAILED(r) && sys != XR_NULL_SYSTEM_ID;
        Log("probe: OpenXR runtime \"%s\": %s", runtime,
            ok ? "HMD system available" : "no HMD system (headset off / not streaming?)");
    }
    if (xrDestroyInstance) xrDestroyInstance(inst);
    return ok;
}

static bool BackendProbeOpenVr()
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\openvr_api.dll", g_dir);
    HMODULE m = LoadLibraryA(path);
    if (!m) { Log("probe: openvr_api.dll not found next to the proxy"); return false; }
    typedef bool (*PFN_b)(void);
    PFN_b installed = (PFN_b)GetProcAddress(m, "VR_IsRuntimeInstalled");
    PFN_b present = (PFN_b)GetProcAddress(m, "VR_IsHmdPresent");
    bool inst = installed && installed();
    bool hmd = present && present();
    Log("probe: OpenVR runtime %s, HMD %s", inst ? "installed" : "NOT installed", hmd ? "present" : "not present");
    return inst && hmd;
}

// Sets g_xrBackend. Called from LoadConfig in place of the process snapshot.
static void BackendSelect(const char* ini)
{
    char be[16] = "";
    if (!GetEnvironmentVariableA("DISHONORED_VR_BACKEND", be, sizeof(be)))
        GetPrivateProfileStringA("VR", "Backend", "auto", be, sizeof(be), ini);
    for (char* p = be; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    if (be[0] == 'a') {
        if (BackendProbeOpenXr()) {
            g_xrBackend = true;
            Log("config: VR backend AUTO -> OPENXR (a 32-bit OpenXR runtime has an HMD)");
        } else if (BackendProbeOpenVr()) {
            g_xrBackend = false;
            Log("config: VR backend AUTO -> OPENVR (SteamVR runtime + HMD present)");
        } else {
            g_xrBackend = false;
            Log("config: VR backend AUTO -> OPENVR by default (no runtime answered; "
                "TryInitVR will keep trying SteamVR - set [VR] Backend=openxr for a Quest)");
        }
    } else {
        g_xrBackend = (be[0] == 'o' && be[1] == 'p') || be[0] == 'x';   // "openxr" / "xr"
        Log("config: VR backend = %s (%s)", g_xrBackend ? "OPENXR" : "OPENVR",
            GetEnvironmentVariableA("DISHONORED_VR_BACKEND", NULL, 0) ? "env" : "ini");
    }
    if (g_xrBackend)
        Log("config: VR backend = OPENXR (OpenVR/SteamVR will not start)");
}
