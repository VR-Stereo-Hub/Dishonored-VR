// core/vr/openxr_loader.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static bool XrLoadRuntime(void)
{
    char json[2 * MAX_PATH] = "";
    if (!GetEnvironmentVariableA("XR_RUNTIME_JSON", json, sizeof(json)))
        if (g_xrJsonIni[0]) strcpy(json, g_xrJsonIni);   // 38.4: ini next
    if (!json[0]) {
        HKEY k;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1",
                          0, KEY_READ, &k) == ERROR_SUCCESS) {
            DWORD t = 0, sz = sizeof(json);
            if (RegQueryValueExA(k, "ActiveRuntime", NULL, &t,
                                 (BYTE*)json, &sz) != ERROR_SUCCESS)
                json[0] = 0;
            RegCloseKey(k);
        }
    }
    if (!json[0]) { Log("xrb: FAIL - no XR_RUNTIME_JSON env and no 32-bit "
                        "ActiveRuntime registered"); return false; }
    Log("xrb: runtime manifest: %s", json);

    // pull "library_path" out of the manifest (crude scan, ample for the
    // well-formed manifests runtimes ship)
    char dll[2 * MAX_PATH] = "";
    {
        FILE* f = fopen(json, "rb");
        if (!f) { Log("xrb: FAIL - manifest unreadable"); return false; }
        char buf[8192]; size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f); buf[n] = 0;
        const char* p = strstr(buf, "library_path");
        if (p) p = strchr(p + 12, ':');
        if (p) p = strchr(p, '"');
        if (p) {
            const char* q = strchr(++p, '"');
            if (q && (size_t)(q - p) < sizeof(dll) - 1) {
                memcpy(dll, p, q - p); dll[q - p] = 0;
            }
        }
        if (!dll[0]) { Log("xrb: FAIL - no library_path in manifest"); return false; }
        for (char* c = dll; *c; c++) if (*c == '/') *c = '\\';
        if (dll[1] != ':') {                 // relative to the manifest's dir
            char full[2 * MAX_PATH];
            strcpy(full, json);
            char* bs = strrchr(full, '\\');
            if (bs) { strcpy(bs + 1, dll); strcpy(dll, full); }
        }
    }
    Log("xrb: runtime dll: %s", dll);
    HMODULE m = LoadLibraryA(dll);
    if (!m) { Log("xrb: FAIL - LoadLibrary error %lu (wrong bitness? missing "
                  "deps?)", GetLastError()); return false; }
    PFN_xrNegotiateLoaderRuntimeInterface nego =
        (PFN_xrNegotiateLoaderRuntimeInterface)GetProcAddress(
            m, "xrNegotiateLoaderRuntimeInterface");
    if (!nego) { Log("xrb: FAIL - no xrNegotiateLoaderRuntimeInterface export");
                 return false; }
    XrNegotiateLoaderInfo li; memset(&li, 0, sizeof(li));
    li.structType = XR_LOADER_INTERFACE_STRUCT_LOADER_INFO;
    li.structVersion = XR_LOADER_INFO_STRUCT_VERSION;
    li.structSize = sizeof(li);
    li.minInterfaceVersion = 1;
    li.maxInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    li.minApiVersion = XR_MAKE_VERSION(1, 0, 0);
    li.maxApiVersion = XR_MAKE_VERSION(1, 0x3ff, 0xfff);
    XrNegotiateRuntimeRequest rr; memset(&rr, 0, sizeof(rr));
    rr.structType = XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST;
    rr.structVersion = XR_RUNTIME_INFO_STRUCT_VERSION;
    rr.structSize = sizeof(rr);
    XrResult xr = nego(&li, &rr);
    if (XR_FAILED(xr) || !rr.getInstanceProcAddr) {
        Log("xrb: FAIL - negotiation returned %d", (int)xr); return false;
    }
    g_xrGipa = rr.getInstanceProcAddr;
    Log("xrb: negotiated (runtime api %u.%u, interface v%u)",
        (unsigned)XR_VERSION_MAJOR(rr.runtimeApiVersion),
        (unsigned)XR_VERSION_MINOR(rr.runtimeApiVersion),
        (unsigned)rr.runtimeInterfaceVersion);
    return true;
}
