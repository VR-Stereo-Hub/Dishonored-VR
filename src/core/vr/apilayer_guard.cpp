// core/vr/apilayer_guard.cpp - included by src/mod/dishonoredvr.cpp (unity
// build).
//
// ===========================================================================
// A 64-BIT IMPLICIT API LAYER KILLS VR IN THIS 32-BIT PROCESS. FIND IT FIRST.
//
// Measured on the dev machine, 2026-09-06. An OBS mirror capture layer had
// registered itself as an IMPLICIT OpenXR API layer:
//
//   HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit
//     C:\Users\<user>\AppData\Local\OpenXR-OBSMirror\
//       XR_APILAYER_NOVENDOR_OBSMirror.json = 0        (0 means ENABLED)
//
// Its library is x64 only. HKCU\SOFTWARE is NOT subject to WOW64 registry
// redirection, so this 32-bit process sees the same key a 64-bit one does, the
// loader tries to load a 64-bit DLL into it, and xrCreateInstance fails with
// XR_ERROR_FILE_ACCESS_ERROR (-32).
//
// The failure is at INSTANCE creation, which is before any runtime is chosen,
// so it took down the native VDXR runtime AND the SteamVR shim with the same
// error - and the shim's "is SteamVR installed?" message sent the diagnosis in
// entirely the wrong direction. An implicit layer is opt-out by design: nobody
// asked for it, and nothing in the mod's own configuration mentions it.
//
// WHAT THIS DOES, AND WHAT IT DELIBERATELY DOES NOT.
//
// It reads the implicit-layer registry keys, reads each enabled layer's
// manifest, resolves its library, and reads the PE header's machine type. A
// layer whose library is not x86 cannot possibly load here, so its manifest's
// own `disable_environment` variable is set in THIS PROCESS before the loader
// runs. That is the documented opt-out and the layer's author chose the name.
//
// It does NOT write the registry. The layer stays installed and keeps working
// for every 64-bit application on the machine; only this process skips it. A
// mod that quietly disabled someone's capture software system-wide to fix its
// own launch would be worse than the bug.
//
// A layer with no `disable_environment` cannot be opted out of, and that is
// reported as the actionable thing it is rather than swallowed.
//
// LANE: called from EnsureConfig, on the first Direct3DCreate9. That is well
// before XR bring-up and safely past the loader lock, where advapi32 and the
// file APIs are usable.
// ===========================================================================

// The PE machine type of a DLL, without loading it. 0 if it cannot be read.
static WORD AlgMachine(const char* path)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    WORD machine = 0;
    DWORD got = 0;
    LONG peOff = 0;
    BYTE mz[2] = { 0, 0 };
    if (ReadFile(f, mz, 2, &got, NULL) && got == 2 && mz[0] == 'M' && mz[1] == 'Z' &&
        SetFilePointer(f, 0x3C, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
        ReadFile(f, &peOff, 4, &got, NULL) && got == 4 && peOff > 0 &&
        SetFilePointer(f, peOff, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
        DWORD sig = 0;
        if (ReadFile(f, &sig, 4, &got, NULL) && got == 4 && sig == 0x00004550)
            ReadFile(f, &machine, 2, &got, NULL);
    }
    CloseHandle(f);
    return machine;
}


// Pull one string value out of a flat JSON manifest. These files are small,
// hand-written and shallow, so a scan for "key" followed by its string is
// enough - and a parser that can be wrong in a new way is not wanted in a path
// whose whole job is to stop a launch failing.
static bool AlgJsonStr(const char* json, const char* key, char* out, size_t cap)
{
    out[0] = 0;
    char pat[64];
    _snprintf(pat, sizeof(pat), "\"%s\"", key);
    pat[sizeof(pat) - 1] = 0;
    const char* p = strstr(json, pat);
    if (!p) return false;
    p = strchr(p + strlen(pat), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '"') return false;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) {
        if (*p == '\\' && p[1]) {          // the manifests escape backslashes
            p++;
            out[n++] = *p++;
            continue;
        }
        out[n++] = *p++;
    }
    out[n] = 0;
    return n > 0;
}


static bool AlgReadFile(const char* path, char* buf, size_t cap)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD got = 0;
    const BOOL ok = ReadFile(f, buf, (DWORD)(cap - 1), &got, NULL);
    CloseHandle(f);
    if (!ok) return false;
    buf[got] = 0;
    return got > 0;
}


// One implicit layer: is it loadable here, and if not, can it be switched off?
static void AlgCheckLayer(const char* manifest, DWORD disabled, const char* where,
                          int* bad, int* stuck)
{
    if (disabled) {
        DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Info,
                "apilayer: %s '%s' is already disabled by its registry value "
                "(%lu; 0 would mean enabled)", where, manifest, (unsigned long)disabled);
        return;
    }
    char json[8192];
    if (!AlgReadFile(manifest, json, sizeof(json))) {
        DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Warn,
                "apilayer: %s '%s' is registered ENABLED but its manifest will "
                "not read. The loader will hit the same wall and xrCreateInstance "
                "can fail with XR_ERROR_FILE_ACCESS_ERROR (-32) for every runtime, "
                "native and shim alike.", where, manifest);
        (*stuck)++;
        return;
    }

    char lib[MAX_PATH] = "", name[128] = "", dis[128] = "";
    AlgJsonStr(json, "library_path", lib, sizeof(lib));
    AlgJsonStr(json, "name", name, sizeof(name));
    AlgJsonStr(json, "disable_environment", dis, sizeof(dis));
    if (!lib[0]) return;

    // A relative library_path is relative to the manifest's own directory.
    char full[MAX_PATH * 2];
    if (lib[1] == ':' || (lib[0] == '\\' && lib[1] == '\\')) {
        _snprintf(full, sizeof(full), "%s", lib);
    } else {
        char dir[MAX_PATH];
        _snprintf(dir, MAX_PATH, "%s", manifest);
        dir[MAX_PATH - 1] = 0;
        char* slash = strrchr(dir, '\\');
        if (slash) *slash = 0; else dir[0] = 0;
        const char* rel = lib;
        if (rel[0] == '.' && rel[1] == '\\') rel += 2;
        _snprintf(full, sizeof(full), "%s\\%s", dir, rel);
    }
    full[sizeof(full) - 1] = 0;

    const WORD m = AlgMachine(full);
    const bool loadable = (m == IMAGE_FILE_MACHINE_I386);
    DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Info,
            "apilayer: %s '%s' -> %s (%s)%s", where,
            name[0] ? name : manifest, full,
            m == IMAGE_FILE_MACHINE_I386  ? "x86, loadable here" :
            m == IMAGE_FILE_MACHINE_AMD64 ? "x64 - CANNOT load in this 32-bit process" :
            m == 0 ? "the library could not be read" : "an unexpected machine type",
            loadable ? "" : " <-- this is enough to fail xrCreateInstance");
    if (loadable) return;

    (*bad)++;
    if (!dis[0]) {
        DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Error,
                "apilayer: '%s' cannot load here and its manifest declares no "
                "disable_environment, so there is no way to opt this process out "
                "of it. VR will not start while it is enabled. Set its registry "
                "value under %s to 1 to disable it, or uninstall the layer - "
                "this mod will not edit the registry for you.",
                name[0] ? name : manifest, where);
        (*stuck)++;
        return;
    }
    if (!g_algGuard) {
        DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Warn,
                "apilayer: '%s' cannot load in a 32-bit process and [VR] "
                "DisableBadApiLayers=0 says to leave it alone. Expect "
                "xrCreateInstance to fail with XrResult(-32) for every runtime "
                "and the game to run flat. Set the key back to 1 to have this "
                "handled.", name[0] ? name : manifest);
        (*stuck)++;
        return;
    }
    SetEnvironmentVariableA(dis, "1");
    DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Warn,
            "apilayer: DISABLED '%s' for this process by setting %s=1, the "
            "opt-out its own manifest declares. Nothing was written to the "
            "registry: the layer stays installed and keeps working for every "
            "64-bit application. If VR still refuses, this was not the cause "
            "and the xr: lines below say what is.",
            name[0] ? name : manifest, dis);
    DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Warn,
            "apilayer:   THE COST, so it is not a mystery later: a capture or "
            "overlay layer that is switched off cannot see this game's OpenXR "
            "session, and tools built on one will report that no OpenXR "
            "application is running - some of them guess at a different VR API "
            "rather than saying they cannot see it. That is not fixable by "
            "configuration: a 64-bit layer can never load into a 32-bit "
            "process. Record the game window, or capture from the headset "
            "runtime's own mirror, instead.");
}


static void AlgScanKey(HKEY root, const char* subkey, REGSAM view, const char* where,
                       int* seen, int* bad, int* stuck)
{
    HKEY k = NULL;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ | view, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0; ; i++) {
        char name[MAX_PATH * 2];
        DWORD nlen = sizeof(name), type = 0, data = 0, dlen = sizeof(data);
        const LONG r = RegEnumValueA(k, i, name, &nlen, NULL, &type,
                                     (LPBYTE)&data, &dlen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (type != REG_DWORD) continue;
        (*seen)++;
        AlgCheckLayer(name, data, where, bad, stuck);
    }
    RegCloseKey(k);
}


// Every place the OpenXR loader looks for implicit layers. HKCU\SOFTWARE is not
// WOW64-redirected, so the plain key and the WOW6432Node one are both read for
// HKLM and the same list is walked for HKCU - a layer registered in any of them
// reaches this process.
static void ApiLayerGuard()
{
    static const char* kPath = "SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit";
    // The scan always runs and always reports - knowing which layers are
    // registered and which of them cannot load here is worth having in every
    // log, and it costs three registry reads. Only the ACT of disabling one is
    // behind the key.
    // The guard runs BEFORE LoadConfig - it has to, since the loader must not
    // see a bad layer - so it reads its own key rather than waiting for the
    // config pass to set the global.
    {
        char ini[MAX_PATH];
        _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
        g_algGuard = GetPrivateProfileIntA("VR", "DisableBadApiLayers", 1, ini) != 0;
    }
    if (!g_algGuard)
        DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Info,
                "apilayer: [VR] DisableBadApiLayers=0 - layers will be reported "
                "but not switched off");
    int seen = 0, bad = 0, stuck = 0;
    AlgScanKey(HKEY_CURRENT_USER,  kPath, 0,               "HKCU", &seen, &bad, &stuck);
    AlgScanKey(HKEY_LOCAL_MACHINE, kPath, KEY_WOW64_32KEY, "HKLM(32)", &seen, &bad, &stuck);
    AlgScanKey(HKEY_LOCAL_MACHINE, kPath, KEY_WOW64_64KEY, "HKLM(64)", &seen, &bad, &stuck);
    if (!seen) {
        DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Info,
                "apilayer: no implicit OpenXR API layers are registered, so none "
                "can be the reason VR fails to start");
        return;
    }
    DVR_LOG(dvr::log::Cat::openxr, dvr::log::Level::Info,
            "apilayer: %d implicit layer(s) registered, %d unloadable in a "
            "32-bit process, %d of those with no way to opt out. An implicit "
            "layer is opt-out by design - nothing in this mod's configuration "
            "asks for one - and a single unloadable layer fails xrCreateInstance "
            "for EVERY runtime, which reads as 'no VR at all' rather than as a "
            "layer problem.", seen, bad, stuck);
}
