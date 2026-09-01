// legacy/fire_tracer.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static LONG CALLBACK FireVeh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        CONTEXT* c = ep->ContextRecord;
        DWORD hit = (DWORD)(c->Dr6 & 0xF);        // any of DR0..DR3
        if (hit) {
            uint32_t eip = (uint32_t)c->Eip;
            LONG n = g_capN; if (n > 64) n = 64;
            int found = -1;
            for (LONG i = 0; i < n; i++)
                if (g_capEips[i] == eip) { found = (int)i; break; }
            if (found >= 0) g_capCount[found]++;
            else if (n < 64) {
                LONG idx = InterlockedIncrement(&g_capN) - 1;
                if (idx < 64) { g_capEips[idx] = eip; g_capCount[idx] = 1; }
            }
            c->Dr6 = 0;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


// set/clear 4 read/write watches (DR0..DR3, 4 bytes each) on every thread
static void SetFireWatchAllThreads(bool enable)
{
    auto apply = [&](CONTEXT& c){
        if (enable) {
            c.Dr0 = (DWORD)g_fireWatch[0]; c.Dr1 = (DWORD)g_fireWatch[1];
            c.Dr2 = (DWORD)g_fireWatch[2]; c.Dr3 = (DWORD)g_fireWatch[3];
            c.Dr7 = (c.Dr7 & ~0xFFuLL) | 0x55;         // L0..L3 enabled
            // RW=11 (r/w) LEN=11 (4B) for all four slots (bits 16..31)
            c.Dr7 = (c.Dr7 & ~(0xFFFFuLL << 16)) | (0xFFFFuLL << 16);
        } else {
            c.Dr0 = c.Dr1 = c.Dr2 = c.Dr3 = 0; c.Dr7 &= ~0xFFuLL;
        }
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te; te.dwSize = sizeof(te);
        DWORD myPid = GetCurrentProcessId(), myTid = GetCurrentThreadId();
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID != myPid) continue;
                if (te.th32ThreadID == myTid) continue;   // never suspend ourselves
                HANDLE h = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT |
                                      THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (!h) continue;
                SuspendThread(h);
                CONTEXT c; memset(&c, 0, sizeof(c));
                c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(h, &c)) { apply(c); SetThreadContext(h, &c); }
                ResumeThread(h);
                CloseHandle(h);
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }
    CONTEXT c; memset(&c, 0, sizeof(c));
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HANDLE me = GetCurrentThread();
    if (GetThreadContext(me, &c)) { apply(c); SetThreadContext(me, &c); }
}


static bool FireArmSample(bool isShot)
{
    if (g_fireArmed || !g_fireTraceEnabled || g_inMenu) return false;
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!g_camObj) return false;
    g_fireWatch[0] = (uintptr_t)(g_camObj + 0x50);       // basis X row (orient)
    g_fireWatch[1] = (uintptr_t)(g_camObj + kCamRight);  // +0x60 right row (orient)
    g_fireWatch[2] = (uintptr_t)(g_camObj + kCamLoc1);   // +0x90 POV origin
    g_fireWatch[3] = (uintptr_t)(g_camObj + kCamLoc2);   // +0xC4 POV origin 2
    g_capN = 0;
    if (!g_fireVeh) g_fireVeh = AddVectoredExceptionHandler(1, FireVeh);
    SetFireWatchAllThreads(true);
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    g_fireArmQpc = q.QuadPart;
    g_fireArmed = true; g_fireIsShot = isShot;
    return true;
}


static bool BgHas(uint32_t eip)
{
    for (int i = 0; i < g_bgN; i++) if (g_bgEips[i] == eip) return true;
    return false;
}


static void FireTraceTick(bool triggerEdge)
{
    // close an open sample after its window
    if (g_fireArmed) {
        LARGE_INTEGER q; QueryPerformanceCounter(&q);
        double ms = g_qpcFreq ? (double)(q.QuadPart - g_fireArmQpc) * 1000.0 / (double)g_qpcFreq : 999.0;
        double win = g_fireIsShot ? 60.0 : 120.0;
        if (ms >= win) {
            SetFireWatchAllThreads(false);
            g_fireArmed = false;
            LONG n = g_capN; if (n > 64) n = 64;
            if (!g_fireIsShot) {
                // fold into background
                for (LONG i = 0; i < n; i++)
                    if (g_bgN < 256 && !BgHas(g_capEips[i]))
                        g_bgEips[g_bgN++] = g_capEips[i];
            } else {
                g_fireRounds++;
                int novel = 0;
                Log("firetrace: SHOT %d — %ld readers, background=%d, NOVEL:",
                    g_fireRounds, (long)n, g_bgN);
                for (LONG i = 0; i < n; i++)
                    if (!BgHas(g_capEips[i])) {
                        novel++;
                        Log("firetrace:   *** eip=0x%08x n=%u  (not in background)",
                            g_capEips[i], g_capCount[i]);
                    }
                if (!novel)
                    Log("firetrace:   (none novel — fire reads same code as camera; widening next)");
                if (g_fireRounds >= 20) {
                    g_fireTraceEnabled = false;
                    Log("firetrace: 20 shots captured — tracer off until next launch");
                }
            }
        }
        return; // one sample at a time
    }

    // not sampling: a trigger edge starts a SHOT sample; otherwise, between
    // shots, periodically take a short BACKGROUND sample to learn the per-frame
    // readers (only once we have some baseline do shot-novelty become meaningful)
    if (triggerEdge) { FireArmSample(true); return; }
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    double sinceBg = g_qpcFreq && g_lastBgQpc
        ? (double)(q.QuadPart - g_lastBgQpc) * 1000.0 / (double)g_qpcFreq : 9999.0;
    if (sinceBg >= 400.0) { if (FireArmSample(false)) g_lastBgQpc = q.QuadPart; }
}


static bool SpawnAlreadyLogged(uint8_t* o)
{
    for (int i = 0; i < g_spawnLogN; i++) if (g_spawnLogged[i] == o) return true;
    if (g_spawnLogN < 32) g_spawnLogged[g_spawnLogN++] = o;
    else g_spawnLogged[g_frame % 32] = o;
    return false;
}


// dump FVector (unit-direction & world-scale) and FRotator candidates in an
// object, flagging any unit vector that matches the camera's forward (= aim)
static void DumpAimFields(uint8_t* o, const float* camFwd)
{
    for (uint32_t off = 0x0c; off + 12 <= 0x220; off += 4) {
        if (!RangeReadable(o + off, 12)) break;
        float* f = (float*)(o + off);
        // finite check
        bool fin = true; float mag = 0;
        for (int k = 0; k < 3; k++) {
            float v = f[k];
            if (v != v || v > 3.0e38f || v < -3.0e38f) { fin = false; break; }
            mag += v < 0 ? -v : v;
        }
        if (!fin) continue;
        float len = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
        // unit-direction candidate (a normalized aim vector)?
        if (len > 0.85f && len < 1.15f && mag > 0.3f) {
            float dot = camFwd ? (f[0]*camFwd[0] + f[1]*camFwd[1] + f[2]*camFwd[2]) : 0;
            Log("spawn:   dir? +0x%03x = (% .3f,% .3f,% .3f) |v|=%.2f  dot(camFwd)=%.2f%s",
                (unsigned)off, f[0], f[1], f[2], len, dot,
                (dot > 0.98f) ? "  <== MATCHES VIEW AIM" : "");
        } else if (mag > 100.0f && mag < 3.0e6f) {
            // world-scale FVector (location or velocity)
            Log("spawn:   vec  +0x%03x = (% .1f,% .1f,% .1f) |sum|=%.0f",
                (unsigned)off, f[0], f[1], f[2], mag);
        }
        // FRotator candidate: 3 int32 in rotator range, not all zero
        int32_t* r = (int32_t*)(o + off);
        bool rot = true; int nz = 0;
        for (int k = 0; k < 3; k++) {
            if (r[k] < -0x20000 || r[k] > 0x20000) { rot = false; break; }
            if (r[k]) nz++;
        }
        if (rot && nz >= 2 && off >= 0x40)
            Log("spawn:   rot? +0x%03x = (%d,%d,%d) [pitch,yaw,roll?]",
                (unsigned)off, r[0], r[1], r[2]);
    }
}


static void SpawnScanOnce()
{
    // 7.2 bug fix: nothing in this path ever LOOKED for the camera, so
    // camFwd logged as (0,0,0). Find it lazily here.
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;

    // camera forward = basis X row @ +0x50 (for aim comparison)
    float camFwd[3] = {0,0,0}; bool haveFwd = false;
    if (CamStillValid() && RangeReadable(g_camObj + 0x50, 12)) {
        float* bx = (float*)(g_camObj + 0x50);
        float l = sqrtf(bx[0]*bx[0] + bx[1]*bx[1] + bx[2]*bx[2]);
        if (l > 0.1f) { camFwd[0]=bx[0]/l; camFwd[1]=bx[1]/l; camFwd[2]=bx[2]/l; haveFwd = true; }
    }

    int found = 0;
    for (uint32_t i = 1; i < num && found < 8; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        uint32_t nnum = *(uint32_t*)(o + kNameOff + 4);
        if (nnum == 0) continue;                       // skip class/CDO, want instances
        const char* cn = ObjClassName(o);
        if (!cn) continue;
        if (!(strstr(cn,"Projectile") || strstr(cn,"Bolt") || strstr(cn,"Bullet") ||
              strstr(cn,"Dart") || strstr(cn,"Arrow") || strstr(cn,"Grenade") ||
              strstr(cn,"Bullet") || strstr(cn,"Ammo") || strstr(cn,"Spring") ||
              strstr(cn,"Sticky") || strstr(cn,"Nail")))
            continue;
        if (SpawnAlreadyLogged(o)) continue;
        const char* nm = RealName(*(uint32_t*)(o + kNameOff));
        Log("spawn: NEW obj[%u] '%s_%u' class '%s'  camFwd=(%.3f,%.3f,%.3f)",
            i, nm ? nm : "?", nnum ? nnum - 1 : 0, cn,
            camFwd[0], camFwd[1], camFwd[2]);
        DumpAimFields(o, haveFwd ? camFwd : NULL);
        found++;
    }
}


static void SpawnTraceTick(bool triggerEdge)
{
    if (triggerEdge && !g_spawnActive) {
        g_spawnActive = true;
        g_spawnDelay = 2;    // let the projectile spawn & get its aim set
        return;
    }
    if (!g_spawnActive) return;
    if (g_spawnDelay-- > 0) return;
    SpawnScanOnce();
    // scan a couple of frames to be safe, then stop
    static int extra = 0;
    if (g_spawnActive && g_spawnDelay == -1) extra = 3;
    if (extra-- <= 0) {
        g_spawnActive = false;
        g_spawnRounds++;
        if (g_spawnRounds >= 30) {
            g_fireTraceEnabled = false;
            Log("spawn: 30 shots scanned — tracer off until next launch");
        }
    }
}
