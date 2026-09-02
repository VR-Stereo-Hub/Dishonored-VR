// legacy/cam_seam.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static uintptr_t CsModuleEnd()
{
    uint8_t* base = (uint8_t*)0x400000;
    if (!RangeReadable(base, 0x1000)) return 0;
    uint32_t peOff = *(uint32_t*)(base + 0x3C);
    if (peOff > 0x800) return 0;
    if (!RangeReadable(base + peOff, 0x100)) return 0;
    return 0x400000u + *(uint32_t*)(base + peOff + 0x50);   // SizeOfImage
}


static bool CsLooksRet8(uint8_t* fn)
{
    if (!RangeReadable(fn, 0x200)) return false;
    for (int i = 0; i < 0x200 - 3; i++)
        if (fn[i] == 0xC2 && fn[i+1] == 0x08 && fn[i+2] == 0x00) return true;
    return false;
}


extern "C" void __cdecl CsHit(int slot, void* ecx, void* a1, void* a2)
{
    if (slot < 0 || slot >= 24) return;
    CsHook* h = &g_cs[slot];
    InterlockedIncrement(&h->calls);
    // 30.42: pointers only. Contents are read later (CsReadDeferred), after
    // the function has returned and filled its out-params. Keeping this stub
    // minimal also keeps it off the hot path - slot 59 alone fires ~200/s.
    h->lastEcx = ecx; h->lastA1 = a1; h->lastA2 = a2;
}


static bool CsInstallOne(void** vt, int slot, const char* tag)
{
    if (g_csCount >= 24) return false;
    int idx = g_csCount;
    CsHook* h = &g_cs[idx];
    h->slot = slot; h->target = (uint8_t*)vt[slot];
    h->calls = 0; h->lastEcx = h->lastA1 = h->lastA2 = NULL;
    memset(h->a1s, 0, sizeof(h->a1s)); memset(h->a2s, 0, sizeof(h->a2s));
    h->tag = tag;
    uint8_t* s = (uint8_t*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE,
                                        PAGE_EXECUTE_READWRITE);
    if (!s) return false;
    int n = 0;
    s[n++] = 0x9C;                                            // pushfd
    s[n++] = 0x60;                                            // pushad
    // stack now: [+0..1F] pushad regs (ecx at +0x18), [+0x20] eflags,
    //            [+0x24] ret, [+0x28] arg1, [+0x2C] arg2
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x2C;  // push [esp+2C] = a2
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x2C;  // push [esp+2C] = a1 (shifted)
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x20;  // push [esp+20] = ecx (shifted x2)
    s[n++] = 0x68; memcpy(s + n, &idx, 4); n += 4;               // push slot index
    s[n++] = 0xE8;                                               // call CsHit (cdecl)
    { int32_t rel = (int32_t)((uintptr_t)&CsHit - ((uintptr_t)s + n + 4));
      memcpy(s + n, &rel, 4); n += 4; }
    s[n++] = 0x83; s[n++] = 0xC4; s[n++] = 0x10;                 // add esp,16
    s[n++] = 0x61;                                               // popad
    s[n++] = 0x9D;                                               // popfd
    s[n++] = 0xE9;                                               // jmp original
    { int32_t rel = (int32_t)((uintptr_t)h->target - ((uintptr_t)s + n + 4));
      memcpy(s + n, &rel, 4); n += 4; }

    DWORD op;
    if (!VirtualProtect(&vt[slot], 4, PAGE_READWRITE, &op)) return false;
    vt[slot] = s;
    VirtualProtect(&vt[slot], 4, op, &op);
    g_csCount++;
    return true;
}


static void CamSeamInstall(uint8_t* obj, const char* tag)
{
    if (!obj || ((uintptr_t)obj & 3) || !RangeReadable(obj, 4)) return;
    void** vt = *(void***)obj;
    uintptr_t modEnd = CsModuleEnd();
    if (!vt || !modEnd || !RangeReadable(vt, 4)) return;
    int shortlisted = 0, walked = 0;
    for (int i = 0; i < 400 && g_csCount < 24; i++) {
        if (!RangeReadable(&vt[i], 4)) break;
        uint8_t* fn = (uint8_t*)vt[i];
        if ((uintptr_t)fn < 0x401000 || (uintptr_t)fn >= modEnd) break;
        walked = i + 1;
        if (!CsLooksRet8(fn)) continue;
        bool dup = false;
        for (int k = 0; k < g_csCount; k++) if (g_cs[k].target == fn) dup = true;
        if (dup) continue;
        if (CsInstallOne(vt, i, tag)) {
            Log("camseam: %s slot %d rva 0x%06x hooked", tag, i,
                (unsigned)((uintptr_t)fn - 0x400000));
            shortlisted++;
        }
    }
    Log("camseam: %s - vtable %p, %d entries walked, %d candidates hooked",
        tag, (void*)vt, walked, shortlisted);
}


// ---- 30.42: DEFERRED output sampling (replaces the 30.41 deep wrap) --------
// 30.41 wrapped the candidate in a C function typed thiscall/ret-8 and CRASHED
// the game: CsLooksRet8 only proves a `ret 8` byte pattern exists SOMEWHERE in
// the first 0x200 bytes, which is not proof of the signature. Returning with
// the wrong stack cleanup corrupts the frame (crash EIP was a heap address).
//
// The safe equivalent: the counting stub already tail-jumps (stack-neutral,
// signature-agnostic) and records the ARGUMENT POINTERS. Out-params are empty
// at entry but FILLED by the time the function returns - so we simply read
// those pointers LATER, from the reporting tick on the game thread. Same
// information, zero calling-convention assumptions, no crash surface.
static void CsReadDeferred(CsHook* h)
{
    void* a1 = h->lastA1;
    void* a2 = h->lastA2;
    if (a1 && !((uintptr_t)a1 & 3) && RangeReadable(a1, 12)) memcpy(h->a1s, a1, 12);
    if (a2 && !((uintptr_t)a2 & 3) && RangeReadable(a2, 12)) memcpy(h->a2s, a2, 12);
}


static void CamPovProbe()
{
    if (!g_povProbe) return;
    double now = MaimNowMs();
    if (now < g_povNext) return;
    g_povNext = now + 2000.0;
    if (g_povRuns >= 12) return;              // 24 s of evidence is plenty

    // 30.44: find the camera OURSELVES. 30.43 depended on g_camObj being
    // populated by the retired camera-write path, so the probe never ran once
    // all session (zero campov lines in the log). FindLiveCamera is the proven
    // scan - it is the one GObjects walk that works, because the camera is a
    // long-lived named instance, not a transient runtime actor.
    if (!CamStillValid()) {
        g_camObj = NULL;
        if (!FindLiveCamera()) {
            if (g_povRuns == 1 || g_povRuns == 6)
                Log("campov: no live camera yet (load into a level first)");
            g_povRuns++;
            return;
        }
    }
    g_povRuns++;

    if (!LooksLikeObj(g_camObj)) return;
    Log("campov: ---- scan %d on %s @%p (view yaw=%.1f pitch=%.1f deg) ----",
        g_povRuns, ObjClassName(g_camObj), (void*)g_camObj,
        g_hmdYaw * 57.2958f, g_hmdPitch * 57.2958f);

    int hits = 0;
    for (uint32_t off = 0x40; off + 28 <= 0x800 && hits < 10; off += 4) {
        uint8_t* p = g_camObj + off;
        if (!RangeReadable(p, 28)) break;
        const float*   f = (const float*)p;
        const int32_t* r = (const int32_t*)(p + 12);
        const float    fov = *(const float*)(p + 24);
        // location: finite, plausible world magnitude, not all tiny
        bool locOk = true; float mx = 0;
        for (int i = 0; i < 3; i++) {
            float v = f[i];
            if (v != v || v > 1e7f || v < -1e7f) { locOk = false; break; }
            float a = v < 0 ? -v : v; if (a > mx) mx = a;
        }
        if (!locOk || mx < 50.0f) continue;
        // rotator: three int32 inside a plausible UE3 rotation range
        bool rotOk = true;
        for (int i = 0; i < 3; i++)
            if (r[i] < -0x30000 || r[i] > 0x30000) { rotOk = false; break; }
        if (!rotOk) continue;
        // FOV: a sane camera field of view in degrees
        if (!(fov > 20.0f && fov < 170.0f)) continue;

        hits++;
        Log("campov:   +0x%03x loc=(%.1f,%.1f,%.1f) rot=(%d,%d,%d) fov=%.1f",
            off, f[0], f[1], f[2], r[0], r[1], r[2], fov);
    }
    if (!hits) Log("campov:   no POV-shaped block found in +0x40..+0x800");
}


static void CamPovWiggle()
{
    // 30.47: proven no-effect (all three windows fired, view never moved) -
    // dormant unless explicitly re-armed via [CamSeam] Wiggle=1.
    if (!g_povWiggle || g_wigPhase >= 6) return;
    if (!g_handMesh) return;                       // in gameplay only
    if (!CamStillValid()) { g_camObj = NULL; if (!FindLiveCamera()) return; }
    double now = MaimNowMs();
    if (g_wigPhase == -1) {
        g_wigPhase = 0; g_wigNext = now + 3000.0;
        Log("campov: WIGGLE begin - watch for three ~22deg view turns");
        Log("campov: WIGGLE window 1 (+0x330) NOW");
    }
    if (now >= g_wigNext) {
        g_wigPhase++;
        g_wigNext = now + (g_wigPhase % 2 ? 2000.0 : 3000.0);  // rest 2s, test 3s
        if (g_wigPhase == 2) Log("campov: WIGGLE window 2 (+0x350) NOW");
        if (g_wigPhase == 4) Log("campov: WIGGLE window 3 (+0x374) NOW");
        if (g_wigPhase >= 6) { Log("campov: WIGGLE done"); return; }
    }
    if (g_wigPhase % 2) return;                    // resting between windows
    uint32_t off = kPovOffs[g_wigPhase / 2];
    uint8_t* p = g_camObj + off;
    if (!RangeReadable(p, 28)) return;
    int32_t* rot = (int32_t*)(p + 12);
    rot[1] += 4000;                                // ~22 deg yaw, reasserted per tick
}


static void CamFovWiggle()
{
    // on-demand via the overlay button; re-armable between presses.
    if (!g_fwigGo) { g_fwigPhase = -1; return; }
    if (g_fwigPhase >= 8) { g_fwigGo = false; g_fwigPhase = -1; return; }
    if (!CamStillValid()) { g_camObj = NULL; if (!FindLiveCamera()) return; }
    double now = MaimNowMs();
    if (g_fwigPhase == -1) {
        g_fwigPhase = 0; g_fwigNext = now + 3000.0;
        Log("campov: FOVWIGGLE begin - four 3s windows, watch for ZOOM IN");
        Log("campov: FOVWIGGLE window 1 (+0x53c) NOW");
        g_fwigSaved = 0.0f;
    }
    if (now >= g_fwigNext) {
        // leaving a test window: restore the engine's value once
        if (!(g_fwigPhase % 2) && g_fwigSaved > 20.0f) {
            uint8_t* pr = g_camObj + kFovCands[g_fwigPhase / 2];
            if (RangeReadable(pr, 4)) *(float*)pr = g_fwigSaved;
        }
        g_fwigPhase++;
        g_fwigNext = now + (g_fwigPhase % 2 ? 2000.0 : 3000.0);
        g_fwigSaved = 0.0f;
        if (g_fwigPhase == 2) Log("campov: FOVWIGGLE window 2 (+0x540) NOW");
        if (g_fwigPhase == 4) Log("campov: FOVWIGGLE window 3 (+0x564) NOW");
        if (g_fwigPhase == 6) Log("campov: FOVWIGGLE window 4 (+0x254) NOW");
        if (g_fwigPhase >= 8) { Log("campov: FOVWIGGLE done"); return; }
    }
    if (g_fwigPhase % 2) return;                   // resting between windows
    uint8_t* p = g_camObj + kFovCands[g_fwigPhase / 2];
    if (!RangeReadable(p, 4)) return;
    float* fov = (float*)p;
    if (g_fwigSaved == 0.0f && *fov > 20.0f && *fov < 170.0f) g_fwigSaved = *fov;
    *fov = 70.0f;                                  // reassert per tick
}


static void CamFovHuntScan(uint8_t* obj, const char* tag, uint32_t lim)
{
    if (!obj || ((uintptr_t)obj & 3) || !RangeReadable(obj, 4)) return;
    char line[420]; int len = 0; int found = 0;
    len = _snprintf(line, sizeof(line), "fovhunt:  %s", tag);
    for (uint32_t off = 0x40; off + 4 <= lim && found < 14; off += 4) {
        if (!RangeReadable(obj + off, 4)) break;
        float v = *(float*)(obj + off);
        if (!(v >= 55.0f && v <= 135.0f)) continue;
        if (v != v) continue;
        found++;
        len += _snprintf(line + len, sizeof(line) - len - 1, " +0x%03x=%.2f", off, v);
        if (len > (int)sizeof(line) - 24) break;
    }
    if (found) Log("%s", line);
}


static void CamFovHunt()
{
    if (!g_fovHuntGo) { g_fhRuns = 0; return; }
    double now = MaimNowMs();
    if (now < g_fhNext) return;
    g_fhNext = now + 2000.0;
    if (g_fhRuns >= 6) { g_fovHuntGo = false; g_fhRuns = 0; Log("fovhunt: done"); return; }
    g_fhRuns++;
    float rendered = 0.0f;   // 41.0: no measured render FOV source yet
    Log("fovhunt: ---- scan %d (rendered FOV %.2f) ----", g_fhRuns, rendered);
    if (g_peCtrl) CamFovHuntScan(g_peCtrl, "ctrl", 0xA00);
    if (g_peCtrl && RangeReadable(g_peCtrl + 0x248, 4)) {
        uint8_t* pawn = *(uint8_t**)(g_peCtrl + 0x248);
        if (pawn) CamFovHuntScan(pawn, "pawn", 0xA00);
    }
    if (CamStillValid()) CamFovHuntScan(g_camObj, "cam ", 0x800);
}


static void CamSeamTick()
{
    if (!g_csRecon) return;
    double now = MaimNowMs();
    if (!g_csDone) {
        if (!g_peCtrl) return;
        if (g_csArmAt == 0.0) { g_csArmAt = now + 8000.0; return; }
        if (now < g_csArmAt) return;
        g_csDone = true;
        CamSeamInstall(g_peCtrl, "ctrl");
        g_csNextRep = now + 5000.0;
        return;
    }
    if (g_csCount == 0 || now < g_csNextRep) return;
    g_csNextRep = now + 5000.0;
    for (int i = 0; i < g_csCount; i++) {
        CsHook* h = &g_cs[i];
        LONG c = InterlockedExchange(&h->calls, 0);
        if (!c) continue;
        CsReadDeferred(h);      // 30.42: read the OUT-params, post-return
        Log("camseam: %s slot %2d rva 0x%06x calls=%ld/5s ecx=%p a1=%p(%.1f,%.1f,%.1f) a2=%p(%d,%d,%d)",
            h->tag, h->slot, (unsigned)((uintptr_t)h->target - 0x400000), (long)c,
            h->lastEcx, h->lastA1, h->a1s[0], h->a1s[1], h->a1s[2],
            h->lastA2, h->a2s[0], h->a2s[1], h->a2s[2]);
    }
}
