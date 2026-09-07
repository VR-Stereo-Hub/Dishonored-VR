// game/dishonored/hands/palette_capture.cpp - VR-33 step 2. See state chunk 59.
//
// Capture one qualified hand draw so the palette-to-view conversion can be
// MEASURED offline instead of inferred. Read-only with respect to rendering:
// it copies state, it never changes it, and it runs before the per-hand
// palette modification so it records what the game asked for.


// FNV-1a. Only used to give bytecode a durable identity - a shader pointer can
// be reused after the object it named was destroyed.
static uint32_t PcHash(const void* p, size_t n)
{
    const uint8_t* b = (const uint8_t*)p;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}


static bool PcHashSeen(uint32_t h)
{
    for (int i = 0; i < g_pcHashN; i++) if (g_pcHash[i] == h) return true;
    return false;
}


// ---- the worker -------------------------------------------------------------
//
// Owns bytes, never D3D objects. Hashing, disassembly and file writing all
// happen here so the render thread pays only for a bounded memcpy.

static void PcWriteBytecode(const void* code, size_t n, uint32_t hash)
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\vs_%08X.bin", dvr::paths::dumps_dir(), hash);
    FILE* f = fopen(path, "wb");
    if (!f) {
        Log("pcap: could NOT write %s - the bytecode for this shader is lost "
            "and the offline read cannot proceed for it", path);
        return;
    }
    fwrite(code, 1, n, f);
    fclose(f);
    Log("pcap: bytecode %08X (%u bytes) -> %s", hash, (unsigned)n, path);

    // Disassemble with the shipped compiler rather than decoding tokens by
    // hand. d3dcompiler_47.dll is already a runtime dependency (blit_quad),
    // and a hand-rolled Shader Model 3 decoder is a second thing that can be
    // wrong at exactly the moment the first one is being checked.
    typedef HRESULT (WINAPI *PFN_D3DDisassemble)(LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob**);
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
    if (!dc) {
        Log("pcap: d3dcompiler_47.dll did not load - the .bin is saved, the "
            "disassembly is not. Read it with an external tool.");
        return;
    }
    PFN_D3DDisassemble dis = (PFN_D3DDisassemble)GetProcAddress(dc, "D3DDisassemble");
    if (!dis) {
        Log("pcap: D3DDisassemble not exported by this d3dcompiler_47 - the "
            ".bin is saved, the disassembly is not");
        FreeLibrary(dc);
        return;
    }
    ID3DBlob* out = NULL;
    const HRESULT hr = dis(code, n, 0, NULL, &out);
    if (SUCCEEDED(hr) && out) {
        _snprintf(path, MAX_PATH, "%s\\vs_%08X.asm", dvr::paths::dumps_dir(), hash);
        FILE* g = fopen(path, "wb");
        if (g) {
            fwrite(out->GetBufferPointer(), 1, out->GetBufferSize() - 1, g);
            fclose(g);
            Log("pcap: disassembly -> %s (%u bytes). The skinning-to-position "
                "path is read from THIS, not guessed from register numbers.",
                path, (unsigned)out->GetBufferSize());
        }
    } else {
        // An explicit failure, not a silent one: a shader version this
        // disassembler will not take is a fact about the capture.
        Log("pcap: D3DDisassemble REFUSED this shader (hr 0x%08lx). The .bin "
            "is saved; the version may be outside what this compiler accepts.",
            (unsigned long)hr);
    }
    if (out) out->Release();
    FreeLibrary(dc);
}


static void PcWritePacket(const PcPacket* k)
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\pcap_%04u_%08X.txt", dvr::paths::dumps_dir(),
              k->id, k->bytecodeHash);
    FILE* f = fopen(path, "wb");
    if (!f) {
        InterlockedIncrement(&g_pcFailed);
        Log("pcap: could NOT write packet %u to %s - counted as FAILED, not "
            "as a capture that completed", k->id, path);
        return;
    }
    fprintf(f, "# dishonored-vr palette capture packet\n");
    fprintf(f, "# Game-derived. Local only: dumps/ is gitignored.\n");
    fprintf(f, "id %u\nbytecode %08X\nresetEpoch %u\nframe %u\neye %d\npaletteGen %u\n",
            k->id, k->bytecodeHash, k->resetEpoch, k->frame, k->eye, k->paletteGen);
    fprintf(f, "draw baseVertex %d minIndex %u numVertices %u startIndex %u "
               "primCount %u stream0Off %u stride %u\n",
            k->baseVertex, k->minIndex, k->numVertices, k->startIndex,
            k->primCount, k->stream0Off, k->stride);
    fprintf(f, "viewport %u %u %u %u minZ %.6f maxZ %.6f rt %u %u\n",
            k->viewport.X, k->viewport.Y, k->viewport.Width, k->viewport.Height,
            k->viewport.MinZ, k->viewport.MaxZ, k->rtWidth, k->rtHeight);
    fprintf(f, "config worldScaleUU %.4f posScaleUU %.4f posTrack %d injectHead %d "
               "abs %d drive %d\n",
            k->worldScaleUU, k->posScaleUU, k->posTrackOn, k->injectHeadOn,
            k->absOn, k->driveOn);
    fprintf(f, "yaw hmd %.6f view %.6f camPosC5 %.4f %.4f %.4f\n",
            k->hmdYaw, k->viewYawRad, k->camPosC5[0], k->camPosC5[1], k->camPosC5[2]);
    fprintf(f, "headOk %d handOk %d\n", k->headOk, k->handOk);
    for (int r = 0; r < 3; r++)
        fprintf(f, "headPose %d %.6f %.6f %.6f %.6f\n", r,
                k->headPose[r][0], k->headPose[r][1], k->headPose[r][2], k->headPose[r][3]);
    for (int r = 0; r < 3; r++)
        fprintf(f, "handPose %d %.6f %.6f %.6f %.6f\n", r,
                k->handPose[r][0], k->handPose[r][1], k->handPose[r][2], k->handPose[r][3]);

    fprintf(f, "anchor cls %d n %d ourQ %.6f %.6f %.6f\n",
            k->anchorCls, k->anchorN, k->anchorQ[0], k->anchorQ[1], k->anchorQ[2]);
    for (int a = 0; a < k->anchorN; a++)
        fprintf(f, "anchorV %u pos %.6f %.6f %.6f bi %u %u %u %u bw %.6f %.6f %.6f %.6f\n",
                k->anchorIdx[a], k->anchorPos[a][0], k->anchorPos[a][1], k->anchorPos[a][2],
                k->anchorBi[a][0], k->anchorBi[a][1], k->anchorBi[a][2], k->anchorBi[a][3],
                k->anchorBw[a][0], k->anchorBw[a][1], k->anchorBw[a][2], k->anchorBw[a][3]);

    fprintf(f, "constF %u\n", k->constFCount);
    for (UINT i = 0; i < k->constFCount; i++)
        fprintf(f, "c%u %.6f %.6f %.6f %.6f\n", i,
                k->constF[i][0], k->constF[i][1], k->constF[i][2], k->constF[i][3]);
    fprintf(f, "constI %u\n", k->constICount);
    for (UINT i = 0; i < k->constICount; i++)
        fprintf(f, "i%u %d %d %d %d\n", i,
                k->constI[i][0], k->constI[i][1], k->constI[i][2], k->constI[i][3]);
    fprintf(f, "constB %u\n", k->constBCount);
    for (UINT i = 0; i < k->constBCount; i++)
        fprintf(f, "b%u %d\n", i, (int)k->constB[i]);
    fclose(f);
    InterlockedIncrement(&g_pcDone);
    Log("pcap: packet %u -> %s (c6..c%u carry the palette; the offline read "
        "recomputes the anchor from the bytes above and compares against "
        "ourQ)", k->id, path, 5 + (g_mpPalN ? g_mpPalN : 0));
}


static DWORD WINAPI PcWorker(LPVOID)
{
    while (!g_pcQuit) {
        const LONG tail = g_pcTail, head = g_pcHead;
        if (tail == head) { Sleep(20); continue; }
        PcPacket* k = &g_pcQueue[tail % PC_QUEUE];
        PcWritePacket(k);
        InterlockedExchange(&g_pcTail, tail + 1);
    }
    return 0;
}


// ---- the capture point ------------------------------------------------------

// Called from the draw detour AFTER MsQualify has passed and BEFORE any
// per-hand palette modification. Everything here is a copy; nothing is
// retained. Returns false and counts the attempt if any required read fails -
// a partial capture is worse than none, because it looks like data.
static bool PcCapture(IDirect3DDevice9* dev, const MsContract* con, UINT primCount,
                      int cls, const float* ourQ)
{
    if (!dev || !con) return false;

    const LONG head = g_pcHead;
    if (head - g_pcTail >= PC_QUEUE) {
        InterlockedIncrement(&g_pcDropped);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
            "pcap: queue FULL - packet dropped (%ld so far). A dropped packet "
            "is reported, never counted as a capture.", g_pcDropped);
        return false;
    }
    PcPacket* k = &g_pcQueue[head % PC_QUEUE];
    memset(k, 0, sizeof(*k));

    // --- the shader, and its bytecode if this content is new ---------------
    IDirect3DVertexShader9* vs = NULL;
    if (FAILED(dev->GetVertexShader(&vs)) || !vs) {
        if (vs) vs->Release();
        InterlockedIncrement(&g_pcFailed);
        Log("pcap: FAILED - no vertex shader could be read at a qualified hand "
            "draw. A failed read is not empty state.");
        return false;
    }
    UINT codeLen = 0;
    bool codeOk = SUCCEEDED(vs->GetFunction(NULL, &codeLen)) && codeLen > 0 &&
                  codeLen <= PC_MAX_BYTECODE;
    static uint8_t code[PC_MAX_BYTECODE];
    if (codeOk) codeOk = SUCCEEDED(vs->GetFunction(code, &codeLen));
    vs->Release();                       // released before anything can return
    if (!codeOk) {
        InterlockedIncrement(&g_pcFailed);
        Log("pcap: FAILED - GetFunction refused or the shader is larger than "
            "%d bytes (len %u).", (int)PC_MAX_BYTECODE, codeLen);
        return false;
    }
    k->bytecodeHash = PcHash(code, codeLen);

    // --- the constants actually in force ------------------------------------
    //
    // From the DEVICE, not from our own shadow: hkSetVSConstF modifies c0, so
    // what the game requested is not necessarily what this draw consumed.
    //
    // The whole supported float bank, not a guessed window. 48 matrices from
    // c6 occupy c6..c149, so the c0..c95 this plan originally proposed would
    // have missed 18 of them.
    D3DCAPS9 caps; memset(&caps, 0, sizeof(caps));
    UINT nf = PC_MAX_CONSTF;
    if (SUCCEEDED(dev->GetDeviceCaps(&caps)) && caps.MaxVertexShaderConst)
        nf = (caps.MaxVertexShaderConst < PC_MAX_CONSTF)
             ? caps.MaxVertexShaderConst : PC_MAX_CONSTF;
    if (FAILED(dev->GetVertexShaderConstantF(0, &k->constF[0][0], nf))) {
        InterlockedIncrement(&g_pcFailed);
        Log("pcap: FAILED - GetVertexShaderConstantF(0, %u) refused. Zeroed "
            "constants would look like a valid matrix, so this is not saved.", nf);
        return false;
    }
    k->constFCount = nf;
    // Integer and boolean banks: small, and they can steer branches and
    // addressing. Their absence is recorded rather than assumed benign.
    if (SUCCEEDED(dev->GetVertexShaderConstantI(0, &k->constI[0][0], PC_MAX_CONSTI)))
        k->constICount = PC_MAX_CONSTI;
    if (SUCCEEDED(dev->GetVertexShaderConstantB(0, k->constB, PC_MAX_CONSTB)))
        k->constBCount = PC_MAX_CONSTB;

    // --- viewport and render target ----------------------------------------
    if (FAILED(dev->GetViewport(&k->viewport))) {
        InterlockedIncrement(&g_pcFailed);
        Log("pcap: FAILED - GetViewport refused; clip space could not be "
            "related to pixels.");
        return false;
    }
    IDirect3DSurface9* rt = NULL;
    if (SUCCEEDED(dev->GetRenderTarget(0, &rt)) && rt) {
        D3DSURFACE_DESC sd;
        if (SUCCEEDED(rt->GetDesc(&sd))) { k->rtWidth = sd.Width; k->rtHeight = sd.Height; }
        rt->Release();                   // released inside the detour, always
    }

    // --- the draw's contract and our own inputs ------------------------------
    k->baseVertex = con->baseVertex; k->minIndex = con->minIndex;
    k->numVertices = con->numVertices; k->startIndex = con->startIndex;
    k->primCount = primCount; k->stream0Off = con->stream0Off; k->stride = con->stride;

    k->anchorCls = cls;
    k->anchorN = (cls >= 0 && cls < MS_CLS_N) ? g_mpAnchorN[cls] : 0;
    if (k->anchorN > PC_MAX_ANCHOR) k->anchorN = PC_MAX_ANCHOR;
    for (int a = 0; a < k->anchorN; a++) {
        const uint32_t vi = g_mpAnchorIdx[cls][a];
        k->anchorIdx[a] = vi;
        if ((int)vi < g_msVerts) {
            memcpy(k->anchorPos[a], g_msVert[vi].p, sizeof(float) * 3);
            memcpy(k->anchorBi[a],  g_msVert[vi].bi, 4);
            memcpy(k->anchorBw[a],  g_msVert[vi].bw, sizeof(float) * 4);
        }
    }
    if (ourQ) memcpy(k->anchorQ, ourQ, sizeof(float) * 3);

    // --- the rendering configuration and the pose this draw belongs to -------
    k->id = g_pcNextId++;
    k->resetEpoch = g_pcResetEpoch;
    k->frame = (uint32_t)dvr::frame::count();
    // THE EYE IS NOT IDENTIFIED YET, and says so rather than guessing. The
    // shipped method is 'mono' (one picture in both eyes), so there is no
    // per-draw eye tag to read; on a stereo method there will be, and a
    // capture that cannot name its eye must not be compared against one that
    // can. -2 means "not identified", never "left".
    k->eye = -2;
    k->paletteGen = g_mpCacheGen;
    k->worldScaleUU = g_skcWorldScale; k->posScaleUU = g_posScaleUU;
    k->hmdYaw = g_hmdYaw; k->viewYawRad = g_viewYawRad;
    memcpy(k->camPosC5, g_camPosC5, sizeof(float) * 3);
    k->posTrackOn = g_posTrack ? 1 : 0;
    k->injectHeadOn = g_injectHead ? 1 : 0;
    k->absOn = g_mpAbs ? 1 : 0;
    k->driveOn = g_mpDrive ? 1 : 0;
    k->headOk = g_devPoseOk[0] ? 1 : 0;
    const int hIdx = (cls == MS_CLS_HAND_B) ? 1 : 0;
    k->handOk = g_devPoseOk[3 + hIdx] ? 1 : 0;
    memcpy(k->headPose, g_devPose[0], sizeof(k->headPose));
    memcpy(k->handPose, g_devPose[3 + hIdx], sizeof(k->handPose));

    // Bytecode is written once per content, from here, because the blob is
    // large and the queue holds state packets only.
    if (!PcHashSeen(k->bytecodeHash) && g_pcHashN < PC_MAX_SHADERS) {
        g_pcHash[g_pcHashN++] = k->bytecodeHash;
        PcWriteBytecode(code, codeLen, k->bytecodeHash);
    }

    InterlockedExchange(&g_pcHead, head + 1);
    return true;
}


static void PcTick(void)
{
    if (InterlockedExchange(&g_pcArmReq, 0)) {
        if (!g_pcOn) {
            Log("pcap: capture is not enabled ([Hands] PaletteCapture=0) - the "
                "key did nothing, which is a state worth saying rather than "
                "leaving as silence.");
            return;
        }
        g_pcWant += 4;
        Log("pcap: >>> ARMED for %d packet(s) <<< - the next qualified hand "
            "draws are captured. Move to a new head/controller pose and arm "
            "again: one shader's code is saved once, but its STATE has to be "
            "sampled across poses and both eyes for the conversion to be "
            "solvable from held-out data.", g_pcWant);
    }
    if (!g_pcOn) return;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "pcap: %ld written, %ld dropped (queue full), %ld failed, %d still "
        "requested, %d distinct shader(s) seen. Dropped and failed are counted "
        "separately from written on purpose - a capture that did not complete "
        "must never read as one that did.",
        g_pcDone, g_pcDropped, g_pcFailed, g_pcWant, g_pcHashN);
}


static void PcStart(void)
{
    if (!g_pcOn || g_pcThread) return;
    g_pcQuit = 0;
    g_pcThread = CreateThread(NULL, 0, PcWorker, NULL, 0, NULL);
    Log("pcap: capture ENABLED. F7 arms a batch of 4. Bytecode and packets go "
        "to %s - game-derived content, local only, and nothing derived from it "
        "is committed.", dvr::paths::dumps_dir());
}


// NOT called from DllMain. Waiting on a thread inside DLL_PROCESS_DETACH runs
// under the loader lock and is a textbook deadlock; the worker is a daemon and
// the process tears it down. This exists for the command seam, so a capture
// session can be ended and its queue drained without ending the run.
static void PcStop(void)
{
    if (!g_pcThread) return;
    InterlockedExchange(&g_pcQuit, 1);
    WaitForSingleObject(g_pcThread, 2000);
    CloseHandle(g_pcThread);
    g_pcThread = NULL;
    Log("pcap: worker stopped - %ld written, %ld dropped, %ld failed.",
        g_pcDone, g_pcDropped, g_pcFailed);
}


// `pcap` on the command seam: status, arm <n>, stop. Logs like its siblings
// (dc, ms, bq, handmove) rather than writing into a reply buffer.
static bool PcCommand(const char* args)
{
    if (args && !strncmp(args, "arm", 3)) {
        if (!g_pcOn) {
            Log("pcap: DISABLED ([Hands] PaletteCapture=0) - nothing armed");
            return true;
        }
        int n = args[3] ? atoi(args + 3) : 4;
        if (n < 1) n = 1;
        if (n > 64) n = 64;
        g_pcWant += n;
        Log("pcap: armed for %d more packet(s), %d pending", n, g_pcWant);
        return true;
    }
    if (args && !strncmp(args, "stop", 4)) { PcStop(); return true; }
    Log("pcap: %s | %ld written %ld dropped %ld failed | %d pending | %d "
        "shader(s) | dumps %s",
        g_pcOn ? "on" : "off", g_pcDone, g_pcDropped, g_pcFailed, g_pcWant,
        g_pcHashN, dvr::paths::dumps_dir());
    return true;
}
