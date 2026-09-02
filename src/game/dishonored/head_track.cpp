// game/dishonored/head_track.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

static inline bool CamAlive()
{
    if (!g_camObj || !g_camObjIdx) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || g_camObjIdx >= onum) return false;
    return (uint8_t*)objs[g_camObjIdx] == g_camObj;
}


// Verify a cached camera pointer still is a live DishonoredPlayerCamera.
static bool CamStillValid()
{
    if (!g_camObj) return false;
    if (!CamAlive()) return false;
    if (!RangeReadable(g_camObj, kCamLoc2 + 12)) return false;
    if (*(uint32_t*)(g_camObj + kNameOff) != g_camNameIdx) return false;
    const char* cn = ObjClassName(g_camObj);
    return cn && strstr(cn, "PlayerCamera");
}


// Scan GObjects for a live instance whose class contains "PlayerCamera".
static bool FindLiveCamera()
{
    if (!RangeReadable((void*)kGNamesData, 8)) return false;
    const char* n0 = NameFromIndex(0);
    if (!n0 || strcmp(n0, "None") != 0) return false;         // GNames sane?
    if (!RangeReadable((void*)kGObjHdr, 12)) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return false;
    if (!RangeReadable(objs, 64 * sizeof(void*))) return false;

    for (uint32_t i = 1; i < onum; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        uint32_t nidx = *(uint32_t*)(o + kNameOff);
        uint32_t nnum = *(uint32_t*)(o + kNameOff + 4);
        if (nnum == 0) continue;                              // want an instance
        const char* nm = RealName(nidx);
        if (!nm || strncmp(nm, "Default__", 9) == 0) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "PlayerCamera")) continue;
        g_camObj = o; g_camNameIdx = nidx; g_camObjIdx = i;
        Log("stereo: live camera found obj[%u] '%s_%u' class '%s' @ %p",
            i, nm, nnum - 1, cn, (void*)o);
        return true;
    }
    return false;
}


static void RecenterHead()
{
    g_refHmdYaw = g_hmdYaw;
    Log("headinject: recentered  hmdYaw=%.1f", g_refHmdYaw * 57.2958f);
}


// Build g_viewA = rotate the world about the camera by your head's ABSOLUTE yaw
// (relative to the last recenter). Mouse head-tracking is off while this runs,
// so there's no second rotation to fight - the view turns exactly with your
// head (1:1). Applied to c0, which the render actually uses.
static void UpdateHeadInject()
{
    g_haveA = false;
    if (!g_injectHead || !g_camObj) return;
    if (!RangeReadable(g_camObj + 0x50, 0x40)) return;
    float* pos = (float*)(g_camObj + 0x80);

    float resYaw = g_flipYaw * (g_hmdYaw - g_refHmdYaw);
    while (resYaw >  3.14159265f) resYaw -= 6.2831853f;
    while (resYaw < -3.14159265f) resYaw += 6.2831853f;

    float ct = cosf(resYaw), st = sinf(resYaw);
    float Cx = pos[0], Cy = pos[1], Cz = pos[2];
    // A (row-major, p' = p*A): rotate about world Z by resYaw, about point C
    float A[16] = {
        ct,  st,  0, 0,
       -st,  ct,  0, 0,
        0,   0,   1, 0,
        0,   0,   0, 1
    };
    float CRx = Cx*ct - Cy*st, CRy = Cx*st + Cy*ct, CRz = Cz;
    A[12] = Cx - CRx; A[13] = Cy - CRy; A[14] = Cz - CRz; A[15] = 1;
    memcpy(g_viewA, A, sizeof(A));
    g_haveA = true;
}


// Find every FRotator on the camera object that agrees with its forward vector.
static void FindPovRotators()
{
    g_povN = 0;
    if (!CamStillValid() || !RangeReadable(g_camObj + 0x50, 12)) return;
    float* bx = (float*)(g_camObj + 0x50);
    float f[3] = { bx[0], bx[1], bx[2] };
    if (V3Norm(f) < 0.5f) return;

    for (uint32_t off = 0x40; off + 12 <= 0x400; off += 4) {
        if (!RangeReadable(g_camObj + off, 12)) break;
        int32_t* r = (int32_t*)(g_camObj + off);
        if (r[0] < -0x20000 || r[0] > 0x20000) continue;
        if (r[1] < -0x20000 || r[1] > 0x20000) continue;
        if (r[2] < -0x20000 || r[2] > 0x20000) continue;
        if (!r[0] && !r[1]) continue;
        float pitch = (float)r[0] / kUEPerRad, yaw = (float)r[1] / kUEPerRad;
        float g[3] = { cosf(pitch)*cosf(yaw), cosf(pitch)*sinf(yaw), sinf(pitch) };
        if (V3Dot(g, f) > 0.995f) {                 // reproduces the real forward
            if (g_povN < 8) g_povOff[g_povN++] = off;
            Log("headinj: POV rotator candidate @+0x%03x = (%d,%d,%d)",
                (unsigned)off, r[0], r[1], r[2]);
        }
    }
    Log("headinj: %d candidate(s) found on the camera object", g_povN);
}


static void VpFindObjects()
{
    g_vpN = 0;
    // the live camera is already known - always watch it
    if (CamStillValid() && RangeReadable(g_camObj, VP_BYTES)) {
        g_vpObj[g_vpN] = g_camObj;
        g_vpName[g_vpN] = ObjClassName(g_camObj);
        g_vpN++;
        Log("viewprobe: watching the live camera @ %p", (void*)g_camObj);
    }
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;

    // The camera's POV is a CACHE - the last build proved writing it does
    // nothing, because the engine recomputes it every tick from the
    // PlayerController. So find the controller: walk the camera's pointer
    // fields until one resolves to an object whose class is a Controller.
    if (CamStillValid()) {
        for (uint32_t off = 0x30; off + 4 <= 0x400 && g_vpN < VP_OBJS; off += 4) {
            if (!RangeReadable(g_camObj + off, 4)) break;
            uint8_t* p = *(uint8_t**)(g_camObj + off);
            if (!p || ((uintptr_t)p & 3)) continue;
            if (!RangeReadable(p, kClassOff + 4)) continue;
            const char* pc = ObjClassName(p);
            if (!pc || !strstr(pc, "Controller")) continue;
            if (!RangeReadable(p, VP_BYTES)) continue;
            bool dup = false;
            for (int k = 0; k < g_vpN; k++) if (g_vpObj[k] == p) dup = true;
            if (dup) continue;
            g_vpObj[g_vpN] = p; g_vpName[g_vpN] = pc; g_vpN++;
            Log("viewprobe: camera +0x%03x points at the controller '%s' @ %p",
                (unsigned)off, pc, (void*)p);
        }
    }

    int listed = 0;
    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        if (*(uint32_t*)(o + kNameOff + 4) == 0) continue;
        const char* cn = ObjClassName(o);
        if (!cn) continue;
        const char* nm = RealName(*(uint32_t*)(o + kNameOff));
        if (nm && !strncmp(nm, "Default__", 9)) continue;
        // 'Tweaks' classes are config archetypes, not live actors - last run
        // filled up on those, which is why nothing moved.
        if (strstr(cn, "Tweaks") || strstr(cn, "Notify") || strstr(cn, "AnimNode"))
            continue;
        // NB: plain "Controller" also matches DisDarkVisionPpController and
        // friends - post-process controllers that flooded the watch list and
        // pushed out everything that mattered.
        bool live = strstr(cn, "PlayerController") || strstr(cn, "PlayerCamera") ||
                    strstr(cn, "PlayerInput") || strstr(cn, "PlayerPawn") ||
                    (strstr(cn, "Corvo") != NULL);
        if (!live) {
            if (listed < 14 && strstr(cn, "Player")) {   // inventory, for next time
                Log("viewprobe:   (also present: '%s')", cn);
                listed++;
            }
            continue;
        }
        if (!RangeReadable(o, VP_BYTES)) continue;
        bool dup = false;
        for (int k = 0; k < g_vpN; k++) if (g_vpObj[k] == o) dup = true;
        if (dup) continue;
        if (g_vpN >= VP_OBJS) continue;
        g_vpObj[g_vpN] = o; g_vpName[g_vpN] = cn; g_vpN++;
        Log("viewprobe: watching obj[%u] '%s' class '%s' @ %p",
            i, nm ? nm : "?", cn, (void*)o);
    }
    if (!g_vpN) Log("viewprobe: no live player objects found - are you in gameplay?");
}


static void VpSnap(uint8_t dst[VP_OBJS][VP_BYTES])
{
    for (int k = 0; k < g_vpN; k++)
        if (RangeReadable(g_vpObj[k], VP_BYTES))
            memcpy(dst[k], g_vpObj[k], VP_BYTES);
}


static void VpNudgeMouse(int dx)
{
    INPUT in; memset(&in, 0, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(INPUT));
}


static void HeadInjectTick()
{
    // (30.8b: F3 retired too - native head tracking is on by default, the
    // hook self-installs, and [HeadTrack] Native=0 in the ini is the off
    // switch for the rare case that wants one)
    // (30.8 key diet: the Shift+F4 view-discovery probe is retired from the
    // keyboard; schedule it with [Debug] Probe=view in the ini instead)
    if (!g_vpPhase) return;
    if (--g_vpTimer > 0) return;

    if (g_vpPhase == 1) {                     // control done: record how noisy
        VpSnap(g_vpSnapB);
        int noisy = 0;
        for (int k = 0; k < g_vpN; k++)
            for (int off = 0; off + 4 <= VP_BYTES; off += 4) {
                int32_t a = *(int32_t*)(g_vpSnapA[k]+off);
                int32_t b = *(int32_t*)(g_vpSnapB[k]+off);
                uint32_t d = (uint32_t)(a > b ? a - b : b - a);
                g_vpNoise[k][off/4] = d;
                if (d) noisy++;
            }
        Log("viewprobe: %d field(s) drift on their own - measuring against that", noisy);
        VpSnap(g_vpSnapA);
        VpNudgeMouse(1200);                   // a stimulus far above the drift
        g_vpPhase = 2; g_vpTimer = 10;
        Log("viewprobe: injected a +1200 mouse delta, measuring...");
        return;
    }

    // inject pass finished: whatever moved now, and not during control, is the
    // field the engine really turns the view with
    VpSnap(g_vpSnapB);
    int hits = 0;
    g_rotN = 0;
    for (int k = 0; k < g_vpN; k++) {
        for (int off = 0; off + 4 <= VP_BYTES; off += 4) {
            int32_t a = *(int32_t*)(g_vpSnapA[k]+off);
            int32_t b = *(int32_t*)(g_vpSnapB[k]+off);
            if (a == b) continue;
            int32_t d = b - a;
            // must move much more than this field's own drift - a moving view
            // used to mark the rotation itself as "noise" and filter it out
            uint32_t mag = (uint32_t)(d < 0 ? -d : d);
            if (mag < g_vpNoise[k][off/4] * 3 + 60) continue;
            float fa = *(float*)(g_vpSnapA[k]+off), fb = *(float*)(g_vpSnapB[k]+off);
            bool rotRange = (a > -0x20000 && a < 0x20000 && b > -0x20000 && b < 0x20000);
            bool fltRange = (fa == fa && fb == fb &&
                             fabsf(fa) < 1.0e5f && fabsf(fb) < 1.0e5f &&
                             fabsf(fb - fa) > 1.0e-4f);
            Log("viewprobe:   %s +0x%03x : int %d -> %d (d=%d) | flt %.4f -> %.4f%s",
                g_vpName[k] ? g_vpName[k] : "?", (unsigned)off, a, b, d, fa, fb,
                rotRange ? "   <== ROTATOR-SHAPED (likely the view yaw)"
                         : (fltRange ? "   <== float angle?" : ""));
            // a rotator is pitch,yaw,roll - if this field and the next both
            // moved and both sit in rotator range, this is the base
            if (rotRange && off + 8 <= VP_BYTES && g_rotN < 16) {
                int32_t a2 = *(int32_t*)(g_vpSnapA[k]+off+4);
                int32_t b2 = *(int32_t*)(g_vpSnapB[k]+off+4);
                if (a2 != b2 && a2 > -0x20000 && a2 < 0x20000 &&
                    b2 > -0x20000 && b2 < 0x20000) {
                    g_rot[g_rotN].obj = g_vpObj[k];
                    g_rot[g_rotN].off = (uint32_t)off;
                    g_rot[g_rotN].cls = g_vpName[k];
                    g_rotN++;
                }
            }
            if (++hits >= 40) break;
        }
        if (hits >= 40) break;
    }
    if (!hits) Log("viewprobe: nothing moved beyond its own drift");
    VpNudgeMouse(-1200);                      // put the view back
    g_vpProbing = false;
    Log("viewprobe: done - %d changed field(s), %d writable rotator(s) found",
        hits, g_rotN);
    for (int k = 0; k < g_rotN; k++)
        Log("viewprobe:   rotator[%d] = %s +0x%03x", k,
            g_rot[k].cls ? g_rot[k].cls : "?", g_rot[k].off);
    if (g_rotN) Log("viewprobe: press F3 again to DRIVE these with your head");
    g_vpPhase = 0;
}


static bool FindPlayerController()
{
    if (!CamStillValid()) { g_pcObj = NULL; g_pcOff = 0; return false; }

    if (g_pcOff && RangeReadable(g_camObj + g_pcOff, 4)) {
        uint8_t* p = *(uint8_t**)(g_camObj + g_pcOff);
        if (p && p == g_pcObj) return true;          // unchanged and still ours
    }

    double now = MaimNowMs();
    if (now < g_pcNextScanMs) return false;          // don't rescan every frame
    g_pcNextScanMs = now + 500.0;

    g_pcObj = NULL; g_pcOff = 0;
    if (!BuildLiveSet()) return false;               // engine's own object table
    for (uint32_t off = 0x30; off + 4 <= 0x400; off += 4) {
        if (!RangeReadable(g_camObj + off, 4)) break;
        uint8_t* p = *(uint8_t**)(g_camObj + off);
        if (!IsLiveObject(p)) continue;              // must be a REAL live object
        const char* cn = ObjClassName(p);
        if (!cn || !strstr(cn, "PlayerController")) continue;
        if (!RangeReadable(p, 0x120)) continue;
        g_pcObj = p; g_pcOff = off;
        static void* lastLogged = NULL;
        if (lastLogged != (void*)p) {
            lastLogged = (void*)p;
            Log("viewinject: player controller '%s' @ %p (via camera +0x%03x)",
                cn, (void*)p, (unsigned)off);
        }
        return true;
    }
    return false;
}


// Write the head pose into every rotator the probe implicated. Stick turning
// survives: read what the game changed since our last write and fold it in.
static void RotInjectTick()
{
    {   // F5 = recentre: whichever way you're facing becomes straight ahead
        bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        static bool f5Was = false;
        if (f5 && !f5Was && g_rotInject) {
            g_rotHaveRef = false; g_rotHaveLast = false;
            Log("viewinject: recentred");
        }
        f5Was = f5;
    }
    if (!g_rotInject) return;
    // 38.68: scripted-camera manners. Quiet script writes are not always an
    // emergency - a keyhole seat-in or a cutscene mutes them ON PURPOSE, and
    // grabbing the controller there is what broke the intro boat (see the
    // globals note). Hold off while the engine has announced a cinematic,
    // while the script path has never claimed the current pawn, and for a
    // grace window after any pawn latch. Normal head tracking (the script
    // path itself) is untouched by every one of these.
    {
        static int fbHoldWas = 0;
        int fbHold = 0;
        // 38.76: hold 2 expires 60 s after a spawn - a machine whose script
        // path never fires must still get the fallback (self-heal, no log
        // needed); the seat-in window it protected is over long before that.
        double sinceLatch = MaimNowMs() - g_fbPawnMs;
        if (CineActive())                                fbHold = 1;
        else if (!g_fbPvrSince && sinceLatch < 60000.0)  fbHold = 2;
        else if (sinceLatch < 15000.0)                   fbHold = 3;
        if (fbHold) {
            if (fbHold != fbHoldWas)
                Log("viewinject: direct fallback HOLDING OFF (%s) - the "
                    "game's scripted camera keeps the controller",
                    fbHold == 1 ? "cinematic announced" :
                    fbHold == 2 ? "script path has not claimed this pawn yet"
                                : "fresh pawn grace");
            fbHoldWas = fbHold;
            return;
        }
        fbHoldWas = 0;
    }
    if (MaimNowMs() < g_fbBackoffMs) return;   // 38.75: watchdog back-off
    // the script hook drives the view only while its writes are FRESH; a
    // stale claim (save-load killed the event) hands control back here
    if (g_scriptHeadOK && (MaimNowMs() - g_scriptHeadMs) < 750.0) return;
    if (g_scriptHeadOK) {
        g_scriptHeadOK = false;
        Log("viewinject: script camera writes went stale (save-load?) - the "
            "direct fallback is taking the camera back");
    }
    // Menus drive their own camera work - stay clear. But "menu" here is only a
    // guess from cursor visibility, and after a load the cursor reads visible
    // until real mouse movement arrives. That is exactly the "doesn't work till
    // I wiggle the mouse" problem, so require the camera to be settled too.
    if (g_inMenu && !CamStillValid()) { g_rotHaveLast = false; return; }
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!FindPlayerController()) return;
    if (!g_pcObj || !RangeReadable(g_pcObj, 0x120)) { g_pcObj = NULL; g_pcOff = 0; return; }
    {   // cheap identity check before writing: still a PlayerController?
        const char* cn = ObjClassName(g_pcObj);
        if (!cn || !strstr(cn, "PlayerController")) { g_pcObj = NULL; g_pcOff = 0; return; }
    }
    if (!RangeReadable(g_pcObj + kPcRotBase[0], 12)) return;
    int32_t* r0 = (int32_t*)(g_pcObj + kPcRotBase[0]);   // authoritative rotation

    if (g_rotHaveLast) {
        float d = (float)(r0[1] - g_rotLastYaw);
        while (d >  32768.0f) d -= 65536.0f;
        while (d < -32768.0f) d += 65536.0f;
        // A single frame can't legitimately turn you 180 degrees; if it looks
        // like it did, the engine re-based the rotation (load, cutscene,
        // teleport) - resync instead of folding a bogus delta in forever.
        if (d > 32000.0f || d < -32000.0f) {
            g_rotBodyYaw = (float)r0[1];
            g_rotHaveRef = false;
        } else {
            g_rotBodyYaw += d;
        }
    } else {
        g_rotBodyYaw = (float)r0[1];
    }
    while (g_rotBodyYaw >  1.0e7f) g_rotBodyYaw -= 65536.0f;
    while (g_rotBodyYaw < -1.0e7f) g_rotBodyYaw += 65536.0f;
    if (!g_rotHaveRef) { g_rotRefYaw = g_hmdYaw; g_rotHaveRef = true; }

    float dy = g_hmdYaw - g_rotRefYaw;
    while (dy >  3.14159265f) dy -= 6.2831853f;
    while (dy < -3.14159265f) dy += 6.2831853f;

    float yawU   = g_rotBodyYaw + dy * kUEPerRad * (float)g_flipYaw;
    float pitchU = g_hmdPitch * kUEPerRad * (float)g_flipPitch;
    float rollU  = g_rotRoll ? (g_hmdRoll * kUEPerRad * (float)g_flipRoll) : 0.0f;
    if (pitchU >  16000.0f) pitchU =  16000.0f;
    if (pitchU < -16000.0f) pitchU = -16000.0f;

    int32_t iy = (int32_t)yawU, ip = (int32_t)pitchU, ir = (int32_t)rollU;
    for (int k = 0; k < (int)(sizeof(kPcRotBase)/sizeof(kPcRotBase[0])); k++) {
        if (!RangeReadable(g_pcObj + kPcRotBase[k], 12)) continue;
        int32_t* r = (int32_t*)(g_pcObj + kPcRotBase[k]);
        r[0] = ip; r[1] = iy; r[2] = ir;
    }
    if (CamStillValid())
        for (int k = 0; k < (int)(sizeof(kCamRotBase)/sizeof(kCamRotBase[0])); k++) {
            if (!RangeReadable(g_camObj + kCamRotBase[k], 12)) continue;
            int32_t* r = (int32_t*)(g_camObj + kCamRotBase[k]);
            r[0] = ip; r[1] = iy; r[2] = ir;
        }
    g_rotLastYaw = iy; g_rotHaveLast = true;

    // 32.92: publish the frame we just wrote, exactly like the script path
    // does. Matched pair: this rotation was computed from THIS g_hmdYaw.
    g_viewPitchRad  = (float)ip / kUEPerRad;
    g_viewYawRad    = (float)iy / kUEPerRad;
    g_injHmdYawSnap = g_hmdYaw;
    g_injHmdPitchSnap = g_hmdPitch;
    g_injSnapOk = true;

    // Watchdog: last time this "worked then froze". If our writes stop moving
    // the view - the head turns but the engine's yaw sits still - hand control
    // back to the mouse instead of leaving you stuck.
    static int   stuck = 0;
    static int32_t prevSeen = 0;
    static float prevHead = 0;
    float headMove = g_hmdYaw - prevHead; prevHead = g_hmdYaw;
    if (headMove < 0) headMove = -headMove;
    if (headMove > 0.02f && r0[1] == prevSeen) stuck++; else stuck = 0;
    prevSeen = r0[1];
    // 38.75: THE QUEST "CAN'T LOOK UP OR DOWN" REPORTS. This used to set
    // g_rotInject=false - native injection OFF for the whole session, the
    // SCRIPT path included, "press F3 to try again" - and drop to mouse
    // emulation (yaw survives, pitch is eaten in pad mode, the fast path
    // that drives the hands stops, and on the Quest the frustum-fill quad
    // no longer matches the render = "zoomed in"). It trips whenever the
    // engine ignores direct yaw writes while frames flow - a script-owned
    // camera (the prison intro cutscene right after the skip, loads) with
    // the player looking around - and at the low frame rates of a fresh
    // install (shader compilation) 90 frames is a few seconds of casual
    // head motion. Now: native injection is never touched; the direct
    // fallback alone backs off for 3 s, then resumes.
    if (stuck > 90) {
        stuck = 0;
        g_fbBackoffMs = MaimNowMs() + 3000.0;
        static double lastSaid = 0.0;
        if (MaimNowMs() - lastSaid > 30000.0) {
            lastSaid = MaimNowMs();
            Log("viewinject: engine ignored 90 frames of direct yaw writes "
                "(script-owned camera?) - direct fallback backing off 3 s; "
                "native head injection stays ON");
        }
    }
}


static void ApplyHeadToViewRotation(void* parms)
{
    if (!parms || !RangeReadable(parms, 40)) return;

    // Two different classes declare this event with different signatures:
    //   PlayerController: (float DeltaTime, out Rotator View, out Rotator Delta)
    //   CameraModifier:   (Actor ViewTarget, float DeltaTime, out Rotator View, ...)
    // so the rotator sits at +4 in one and +8 in the other. Find DeltaTime - 
    // a small positive float, ~0.016 - and the rotator follows it. Guessing +4
    // is why turning your head moved the view up and down: that write was
    // landing on the pitch field of the other layout.
    // DeltaTime must look like a REAL frame time. "> 0 and < 0.5" was not
    // enough: a pointer reinterpreted as a float is a tiny positive number
    // (0x1D3D3000 reads as ~1.7e-21) and sailed straight through, so a
    // ViewTarget pointer got mistaken for DeltaTime. Writing an int into the
    // actual DeltaTime float is what threw the view at the sky and crashed it.
    float f0 = *(float*)((uint8_t*)parms + 0);
    float f4 = *(float*)((uint8_t*)parms + 4);
    const float kMinDT = 0.0005f, kMaxDT = 0.2f;    // 0.5 ms .. 200 ms
    uint32_t rotOff;
    if (f0 > kMinDT && f0 < kMaxDT)      rotOff = 4;
    else if (f4 > kMinDT && f4 < kMaxDT) rotOff = 8;
    else return;                                     // unknown shape - hands off
    int32_t* rot = (int32_t*)((uint8_t*)parms + rotOff);

    // second gate: it must actually look like a rotator before we touch it
    if (rot[0] < -0x30000 || rot[0] > 0x30000) return;   // pitch out of range
    if (rot[2] < -0x30000 || rot[2] > 0x30000) return;   // roll out of range

    static uint32_t loggedOff = 0xffffffffu;
    if (loggedOff != rotOff) {
        loggedOff = rotOff;
        Log("script: view rotation is at Parms+%u (pitch=%d yaw=%d)",
            rotOff, rot[0], rot[1]);
    }

    // one DELTA application per rendered frame - but the engine calls this
    // once per CAMERA MODIFIER, and only the modifier whose out-value
    // survives the chain reaches the screen. Which one that is depends on
    // the modifier list order REBUILT AT EVERY LEVEL LOAD - the per-load
    // coin flip behind a week of "pitch dead on some machines, F9 flips it"
    // (Beardo's discovery). 38.86: the first dispatch of a frame computes
    // the target rotation (yaw delta folded in ONCE); every further
    // dispatch in the same frame gets the SAME absolute values stamped, so
    // whichever modifier the chain listens to is carrying our numbers.
    // 38.87: grouping by PRESENTED frame stuttered on rigs where the game
    // ticks more than once per present - the second tick's dispatches were
    // overwritten with the first tick's remembered rotation, so the camera
    // alternated fresh/stale = judder (measured on the dev rig the hour
    // 38.86 shipped). The chain pass we must blanket completes in
    // MICROSECONDS; a real new tick is milliseconds later. Group by TIME:
    // re-stamp the same absolutes only within 2 ms of the primary write;
    // anything later recomputes (safe: each application folds only the head
    // movement since the last one).
    static double  frWriteMs = -1.0e9;
    static int32_t frP = 0, frY = 0, frR = 0;
    static bool    frHave = false;
    double frNow = MaimNowMs();
    if (!g_chainStamp) {
        // 38.88: ChainStamp=0 - the exact pre-38.86 path. One write to the
        // first dispatch per presented frame; every later dispatch of the
        // chain is left alone.
        static uint32_t lastFrameOld = 0xffffffffu;
        if (g_frame == lastFrameOld) return;
        lastFrameOld = g_frame;
    } else if (frNow - frWriteMs < 2.0) {
        if (frHave) {
            rot[0] = frP; rot[1] = frY;
            if (g_rotRoll) rot[2] = frR;
            InterlockedIncrement(&g_pvrWrites);
            g_scriptHeadOK = true;
            g_scriptHeadMs = frNow;
            g_fbPvrSince   = 1;
        }
        return;
    }
    frHave = false;

    static float prevYaw = 0, prevPitch = 0;
    static bool  havePrev = false;
    if (!havePrev) { prevYaw = g_hmdYaw; prevPitch = g_hmdPitch; havePrev = true; return; }

    float dy = g_hmdYaw - prevYaw;
    while (dy >  3.14159265f) dy -= 6.2831853f;
    while (dy < -3.14159265f) dy += 6.2831853f;
    float dp = g_hmdPitch - prevPitch;
    prevYaw = g_hmdYaw; prevPitch = g_hmdPitch;
    // a tracking glitch must never become a giant one-frame swing
    if (dy >  0.5f) dy =  0.5f;  if (dy < -0.5f) dy = -0.5f;
    if (dp >  0.5f) dp =  0.5f;  if (dp < -0.5f) dp = -0.5f;

    int32_t before0 = rot[0], before1 = rot[1];

    // YAW stays relative: it has to compose with stick turning, which also
    // moves this value.
    rot[1] += (int32_t)(dy * kUEPerRad * (float)g_flipYaw);

    // PITCH is ABSOLUTE. Your head's pitch simply IS the view pitch - there is
    // no body pitch to compose with - so accumulating deltas was wrong: any
    // per-frame rounding just piles up, which is the slow climb into the sky.
    int32_t wantPitch = (int32_t)(g_hmdPitch * kUEPerRad * (float)g_flipPitch);
    if (wantPitch >  16000) wantPitch =  16000;
    if (wantPitch < -16000) wantPitch = -16000;
    rot[0] = wantPitch;

    if (g_rotRoll) rot[2] = (int32_t)(g_hmdRoll * kUEPerRad * (float)g_flipRoll);
    frP = rot[0]; frY = rot[1];                       // 38.86/87: this chain
    frR = g_rotRoll ? rot[2] : 0; frHave = true;      // pass's values
    frWriteMs = MaimNowMs();
    InterlockedIncrement(&g_pvrWrites);   // 30.57: writes that actually landed
    g_scriptHeadOK = true;
    g_scriptHeadMs = MaimNowMs();
    g_fbPvrSince   = 1;   // 38.68: script path has claimed this pawn's camera
    g_viewPitchRad = (float)rot[0] / kUEPerRad;
    g_viewYawRad   = (float)rot[1] / kUEPerRad;
    // the head values THIS camera write was computed from - matched pair
    g_injHmdYawSnap = g_hmdYaw; g_injHmdPitchSnap = g_hmdPitch;
    g_injSnapOk = true;
    (void)dp;

    static int hb = 0;
    if (++hb >= 150) {
        hb = 0;
        Log("headtrack: hmd pitch=%.1f yaw=%.1f deg | view pitch %d->%d yaw %d->%d",
            g_hmdPitch * 57.2958f, g_hmdYaw * 57.2958f,
            before0, rot[0], before1, rot[1]);
    }
}


static void TrackHead(const float (*m)[4])
{
    // (30.8 key diet: F8 mouse-look toggle retired - F3 owns head tracking,
    // menus auto-pause the head-mouse, [Tracking] Enabled covers the rest)


    // 30.23: slow body anchor for the hand-position neutral (~90 Hz cadence)
    {
        if (!g_bodyAnchorOk) {
            g_bodyAnchor[0] = m[0][3];
            g_bodyAnchor[1] = m[1][3];
            g_bodyAnchor[2] = m[2][3];
            g_bodyAnchorOk = true;
        } else {
            float al = 0.011f / (g_anchorTau > 0.1f ? g_anchorTau : 0.1f);
            g_bodyAnchor[0] += al * (m[0][3] - g_bodyAnchor[0]);
            g_bodyAnchor[1] += al * (m[1][3] - g_bodyAnchor[1]);
            g_bodyAnchor[2] += al * (m[2][3] - g_bodyAnchor[2]);
        }
    }

    // Always capture absolute HMD yaw/pitch/roll for the camera-matrix injection.
    float fx = -m[0][2], fy = -m[1][2], fz = -m[2][2]; // forward
    float yaw   = atan2f(fx, -fz);
    float fyc   = fy < -1.f ? -1.f : (fy > 1.f ? 1.f : fy);
    float pitch = asinf(fyc);
    g_hmdYaw = yaw; g_hmdPitch = pitch;
    g_hmdRoll = atan2f(m[1][0], m[1][1]); // right.up vs up.up

    // 31.8: physical crouch moved OUT of the positional-tracking block. It only
    // needs your head height against the standing reference, and burying it
    // inside "if (g_posTrack)" meant turning lean off silently disabled
    // crouching too - a dependency nothing in the UI hinted at.
    if (g_crouchOn) {
        float py2 = m[1][3];
        // 32.15: CONFIRMED working once the reference was right - the log shows
        // "crouch: DOWN (head 0.23 m below standing)" after an F5. The only bug
        // was that the standing reference was captured on the very first frame,
        // usually with the headset still on a desk, which put it near table
        // height and made the threshold unreachable.
        // Self-heal instead of requiring the ritual: standing is the HIGHEST
        // your head has been. A desk-height capture corrects itself the moment
        // you put the headset on, and crouching only ever lowers py2, so it
        // cannot drag the baseline down with it.
        if (!g_crouchRefOk) { g_crouchRef = py2; g_crouchRefOk = true; }
        if (py2 > g_crouchRef) g_crouchRef = py2;
        float drop = g_crouchRef - py2;
        bool want = g_crouchHeld ? (drop > g_crouchReleaseM) : (drop > g_crouchDropM);
        if (want != g_crouchHeld) {
            g_crouchHeld = want;
            Log("crouch: %s (head %.2f m below standing)", want ? "DOWN" : "up", drop);
        }
    } else if (g_crouchHeld) {
        g_crouchHeld = false;
    }

    // --- Stage 5.0: positional head tracking (lean/peek/crouch) --------------
    // F4 toggles; F5 re-centers the reference to wherever your head is now.
    {
        bool f4 = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (f4 && !g_f4Was) {
            g_posTrack = !g_posTrack;
            g_posHaveRef = false;                       // fresh origin on enable
            if (!g_posTrack) { g_leanRightUU = 0; g_leanUpUU = 0; g_leanFwdUU = 0; }
            Log("postrack: %s (F4)", g_posTrack ? "ON" : "off");
        }
        g_f4Was = f4;
        bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f5 && !g_f5Was) {
            g_posHaveRef = false;
            g_crouchRefOk = false;
            // 32.11: recapture the HAND neutrals too. This is why the trim
            // needed a nudge at every launch: the neutral is the controller
            // offset from your head at the instant the poses first went valid,
            // which is wherever the controllers happened to be sitting when
            // the game came up. Every hand position is measured from it, so a
            // different resting pose = a different neutral = the whole rig
            // shifted, and the only knob left to fix it with was the trim.
            // 32.15: F5 does NOT recalibrate the hands. I added that in 32.13
            // because it was asked for, without thinking through what it does
            // to the trim - and one session's log settles it. Three F5 presses
            // produced three different neutrals:
            //     L=(-0.429,-0.280,0.395)
            //     L=(-0.358,-0.183,0.423)
            //     L=(-0.447,-0.337,0.163)
            // Hand position is (pose - neutral) * scale + trim, so moving the
            // neutral translates every hand position and the tuned trim stops
            // meaning what it meant. Re-tuning the trim after every recentre is
            // the exact chore the saved neutral was built to end.
            // Worse, F5 is now also how you set the crouch reference, so a key
            // you need to press while standing was silently re-randomising the
            // hand alignment. Calibration belongs on its own button, pressed
            // once at setup.
            // 32.16: SHIFT+F5 is the full setup reset - view, position, crouch
            // AND a fresh hand calibration. Plain F5 stays the everyday key and
            // still leaves the hands alone.
            // Worth being clear about why these are separate at all: the hand
            // neutral is the controller offset from your HEAD, expressed in
            // head-relative axes. Turning around or stepping sideways does not
            // change it - your hands travel with you. Nothing about moving in
            // the play space invalidates it, which is why it is a one-time
            // setup value and not a recentre.
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                g_skcCalReq = 1;
                Log("postrack: FULL reset (Shift+F5) - view, position, crouch "
                    "AND hands. Hold your controllers where you want them and "
                    "keep still for 3 s. The trim will need re-tuning after.");
            } else {
                Log("postrack: re-centered (F5) - view, position and crouch "
                    "reference; hands untouched");
            }
        }
        g_f5Was = f5;

        if (g_posTrack) {
            float px = m[0][3], py = m[1][3], pz = m[2][3]; // meters
            if (!g_posHaveRef) {
                g_posRefX = px; g_posRefY = py; g_posRefZ = pz;
                g_posHaveRef = true;
            }
            // 32.5: the old crouch block is GONE. Disabling it with "if (false)"
            // left its else-branch live, so it cleared g_crouchHeld every single
            // frame while the new block set it - a flip-flop that logged 5,137
            // times and never held the button long enough for the game to see a
            // crouch. Dead code has to be deleted, not switched off.
            float dx = px - g_posRefX, dy = py - g_posRefY + g_heightOffsetM,
                  dz = pz - g_posRefZ;
            // 38.46: publish the RAW horizontal offset in the head's own yaw
            // frame - before the safety clamp, which exists to stop the camera
            // leaving the body and must not shrink what we hand the stick -
            // and bleed the reference toward the head while past the deadzone.
            // That bleed IS the auto-recenter.
            if (g_roomScaleCfg) {
                float cy0 = cosf(yaw), sy0 = sinf(yaw);
                g_roomRightM = dx * cy0 + dz * sy0;
                g_roomFwdM   = dx * sy0 - dz * cy0;
                float hl = sqrtf(g_roomFwdM * g_roomFwdM +
                                 g_roomRightM * g_roomRightM);
                static double rsLast = 0.0;
                double rsNow = MaimNowMs();
                float rsDt = (rsLast > 0.0)
                           ? (float)((rsNow - rsLast) * 0.001) : 0.0f;
                rsLast = rsNow;
                if (rsDt > 0.25f) rsDt = 0.25f;      // a hitch must not lurch
                if (hl > g_roomDeadM && rsDt > 0.0f) {
                    float step = g_roomBleedMS * rsDt;
                    float room = hl - g_roomDeadM;
                    if (step > room) step = room;
                    float k = step / hl;
                    g_posRefX += dx * k;
                    g_posRefZ += dz * k;
                    dx = px - g_posRefX; dz = pz - g_posRefZ;
                }
            } else { g_roomFwdM = 0.0f; g_roomRightM = 0.0f; }
            float r = sqrtf(dx*dx + dy*dy + dz*dz);
            if (r > g_posMaxM && r > 0.0001f) {          // safety clamp
                float k = g_posMaxM / r; dx *= k; dy *= k; dz *= k;
            }
            // 38.15: stance eye-drop, AFTER the clamp (a 0.5-1.2 m drop must
            // never be eaten by the 0.8 m lean limiter). Cylinder is the
            // authority; unknown reads decay the drop toward zero.
            {
                float ch = PawnCollisionHeight();
                // 38.16: deep crouch - shrink the capsule while the GAME
                // says crouched, never fight its standing or vent writes.
                if (g_deepCrouchCfg && ch > 0.0f) {
                    if (ch > 76.0f) {
                        if (g_dcOurs != 0.0f) {
                            g_dcOurs = 0.0f;
                            Log("deep-crouch: standing - cylinder back to "
                                "the game's %.1fuu", ch);
                        }
                    } else if (ch >= 50.0f) {          // game's own crouch
                        if (PawnSetCollisionHeight(g_deepCrouchUU)) {
                            if (g_dcOurs == 0.0f)
                                Log("deep-crouch: cylinder %.1f -> %.1fuu "
                                    "(fits under more)", ch, g_deepCrouchUU);
                            g_dcOurs = g_deepCrouchUU;
                        }
                    } else if (g_dcOurs != 0.0f &&
                               fabsf(ch - g_dcOurs) > 0.6f) {
                        g_dcOurs = 0.0f;               // vent (game's 33)
                    }
                }
                float targetUU = 0.0f;
                if (g_crouchEyeCfg) {
                    // 38.20: PLAIN crouch only (50..86). Slide and crawl are
                    // SCRIPTED states where the game animates its own camera
                    // down - stacking our drop on top slammed the view into
                    // the ground ("sliding pushed my view to the floor").
                    // Plain crouch (65) is the one state the game leaves the
                    // camera alone, and the only one that needs us.
                    if (ch > 50.0f && ch < 86.0f)
                        targetUU = (87.5f - ch) * g_crouchEyeScale;
                }
                g_crouchDropUU += (targetUU - g_crouchDropUU) * 0.05f;
                if (fabsf(targetUU - g_crouchDropUU) < 0.05f)
                    g_crouchDropUU = targetUU;
                static float dropTold = 0.0f;
                if (fabsf(targetUU - dropTold) > 5.0f) {
                    dropTold = targetUU;
                    Log("crouch-eye: drop -> %.1fuu (stance change)", targetUU);
                }
                if (g_crouchDropUU > 0.01f && g_posScaleUU > 1.0f)
                    dy -= g_crouchDropUU / g_posScaleUU;
            }
            // Project onto the HMD's horizontal frame. Tracking space is y-up,
            // RH; forward_h = (sin yaw, 0, -cos yaw), so right = (cos yaw, 0,
            // sin yaw). The game camera follows HMD yaw (mouse-look), so HMD
            // right == view right. Vertical is just world y.
            float cyw = cosf(yaw), syw = sinf(yaw);
            g_leanRightUU = (dx*cyw + dz*syw) * g_posScaleUU;
            g_leanUpUU    =  dy               * g_posScaleUU;
            // 30.35: forward component (tracking space is y-up RH;
            // forward_h = (sin yaw, 0, -cos yaw)). Roll does not affect the
            // forward axis, so no counter-rotation needed below.
            g_leanFwdUU   = (dx*syw - dz*cyw) * g_posScaleUU;
            // The lean offset is measured in a roll-free head frame, but it is
            // applied in the engine's view space - and native tracking can put
            // ROLL in that view space. Counter-rotate, or leaning while your
            // head is tilted slides the world diagonally.
            if (g_rotInject && g_rotRoll) {
                float rr = g_hmdRoll * (float)g_flipRoll;
                float cr = cosf(rr), sr = sinf(rr);
                float R = g_leanRightUU, U = g_leanUpUU;
                g_leanRightUU =  R*cr + U*sr;
                g_leanUpUU    = -R*sr + U*cr;
            }
        }
        // 41.1: the camera seam owns the offset from here; both lanes (the c0
        // patch, the camera write) read it there. Off = zero, so a disabled
        // tracker never leaves a stale lean behind on either lane.
        dvr::camera::set_position_offset_uu(g_posTrack ? (float)g_leanRightUU : 0.0f,
                                            g_posTrack ? (float)g_leanUpUU : 0.0f,
                                            g_posTrack ? (float)g_leanFwdUU : 0.0f);
    }

    // Menus show the Windows cursor; gameplay hides it. Track that state
    // ALWAYS (the virtual pad uses it too), and while a menu is up, pause the
    // head-mouse so it doesn't fight gamepad/mouse menu navigation.
    {
        // 30.57: cursor visibility ALONE was the menu test, and it goes stale
        // (reads "visible" long after a load) - the root cause of the whole
        // nudge workaround and of tonight's head-tracking outage. The game's
        // own script events are authoritative and we already track them for
        // the stereo/mono switch, so require BOTH: a stale cursor can no
        // longer park anything, while a real menu still registers.
        // 32.9: the cursor half of this test is FULLSCREEN-ONLY. Measured:
        // after the game was flipped to windowed, CURSOR_SHOWING reads true
        // for the entire session - the desktop cursor exists over a windowed
        // app whether or not a menu is up. That made `menu` permanently true,
        // and one stuck flag took out three things at once:
        //  - the eye quads used MenuFillScale (0.72) forever -> black border
        //  - the SkelControl probe is gated on !menu -> hands never found,
        //     so the controllers drove nothing
        //  - the head-mouse stayed parked
        // The script events are authoritative and work in both modes; the
        // cursor only ever covered the cases they miss (the boot main menu).
        // Losing that in windowed mode is a far smaller price than this.
        CURSORINFO ci; ci.cbSize = sizeof(ci);
        bool cursorVis = !g_gameWindowed
                      && GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) != 0;
        // 30.58, from the 30.57 telemetry: the pause menu logged
        // "menu=0 (script=1)" - Dishonored HIDES the cursor in its own menus
        // when navigating with a pad, so the AND gate concluded "gameplay" and
        // left the head-mouse running. Head movement then dragged the mouse and
        // the menu highlight chased it: the "skipping around". The script event
        // is the authority; the cursor only adds the cases script events miss
        // (the boot main menu).
        // 30.75: head tracking sometimes takes a second to come back after a
        // load or at startup. §10 of the handoff already names the cause - the
        // game's save-slot polls (Req_SaveSlotInfos / CanSaveGame) re-open the
        // menu flag right after auto-start closes it - and nothing was ever
        // clearing it, so the head-mouse stayed parked until a real menu event
        // arrived. A script "open" with no cursor and no follow-up is a stale
        // flag, not a menu: Dishonored hides the cursor in its own menus but a
        // REAL menu keeps re-dispatching, so the timeout only trips on the
        // ghost. Anything that reopens a genuine menu sets the flag again.
        {
            static double menuSince = 0.0;
            static bool   wasOpen = false;
            // 32.5: Dishonored HIDES the cursor in its own menus when you
            // navigate with a pad - that is the 30.58 lesson, already in the
            // handoff - so "no cursor" alone is not evidence of a stale flag.
            // I reintroduced the exact bug this project had already fixed, and
            // it was clearing the menu flag inside real pause menus (which is
            // why the menu size snapped back to gameplay fill).
            // The honest discriminator: the engine stops dispatching the view
            // rotation while paused, so gameplay dispatches STILL FLOWING means
            // the flag is a ghost. A real menu shows no dispatches at all.
            static LONG hitsAtStale = 0;
            // 38.17: the ghost-clear needs a LIVE PAWN. At the MAIN MENU the
            // 3D pub background keeps dispatching view rotations, and in
            // windowed mode the cursor half of the test is disabled - so the
            // clearer fired at a REAL menu ("stale flag cleared ... 21
            // gameplay dispatches" measured at the main menu) and swept the
            // menu UI onto the wrist panel, tiny. The stance oracle only
            // reads while a gameplay pawn exists - the exact discriminator:
            // ghost menus happen in gameplay (pawn live), the main menu has
            // no pawn.
            if (g_menuOpen && !cursorVis && CylTruthLive()) {
                double now = MaimNowMs();
                if (!wasOpen) { wasOpen = true; menuSince = now; hitsAtStale = g_pvrHits; }
                else if (now - menuSince > 1500.0 && (g_pvrHits - hitsAtStale) > 20) {
                    g_menuOpen = false; wasOpen = false;
                    Log("menu: stale flag cleared after %.0f ms - no cursor AND "
                        "%ld gameplay dispatches still flowing, so this was a "
                        "ghost, not a menu", now - menuSince,
                        (long)(g_pvrHits - hitsAtStale));
                }
            } else {
                wasOpen = false;
            }
        }
        // 32.47: THE SAME GHOST TEST, APPLIED TO THE CURSOR.
        // "My head stops controlling the camera after loading in and only a
        // restart fixes it." The stale-flag rescue above only ever handled one
        // half of the gate - a stuck SCRIPT flag with no cursor. If the CURSOR
        // is the thing stuck showing (a load screen, focus bouncing to another
        // window, an overlay), nothing clears it, `menu` stays true forever and
        // the head-mouse is parked for the rest of the session. That is exactly
        // the reported symptom, and the fix is the discriminator this code
        // already trusts: the engine stops dispatching view rotation while
        // paused, so if gameplay dispatches are still flowing the cursor is a
        // ghost rather than a menu.
        {
            static double curSince = 0.0;
            static bool   curWas   = false;
            static LONG   curHits  = 0;
            if (cursorVis && !g_menuOpen) {
                double now = MaimNowMs();
                if (!curWas) { curWas = true; curSince = now; curHits = g_pvrHits; }
                else if (now - curSince > 1500.0 && (g_pvrHits - curHits) > 20) {
                    g_cursorGhost = true;
                    Log("menu: stale CURSOR ignored after %.0f ms - %ld gameplay "
                        "dispatches still flowing, so head tracking stays live",
                        now - curSince, (long)(g_pvrHits - curHits));
                }
            } else {
                curWas = false;
                if (g_cursorGhost) {
                    g_cursorGhost = false;
                    Log("menu: cursor is behaving again - ghost override off");
                }
            }
            if (g_cursorGhost) cursorVis = false;
        }
        // An escape hatch that does not need a restart. If both automatic
        // rescues somehow miss, F9 forces gameplay mode.
        {
            bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (f9 && !g_f9Was) {
                g_menuOpen = false; g_cursorGhost = true; cursorVis = false;
                Log("menu: FORCED to gameplay mode (F9) - head tracking live");
            }
            g_f9Was = f9;
        }
        bool menu = g_menuOpen || cursorVis;
        if (menu != g_inMenu) {
            g_inMenu = menu;
            Log("pad: %s (cursor %s, script menu %s)",
                menu ? "MENU mode - head-mouse paused"
                     : "game mode - head-mouse active",
                cursorVis ? "visible" : "hidden",
                g_menuOpen ? "open" : "closed");
        }
        if (menu) { g_haveLastPose = false; return; }
    }

    // While the power wheel is held open the game freezes look input, so any
    // head-mouse deltas we send are EATEN - head and game camera would come
    // back misaligned. Pause emulation and re-sync from the current pose when
    // the wheel closes (no snap, no accumulated offset).
    if (g_wheelHeld) { g_haveLastPose = false; return; }

    // While injecting head-look into c0, DON'T also emulate mouse - one clean
    // absolute rotation, nothing to fight. (Turn your body with the controller.)
    if (g_injectHead) { g_haveLastPose = false; return; }
    if (g_rotInject)   { g_haveLastPose = false; return; }
    if (g_vpProbing)   { g_haveLastPose = false; return; }
    if (!g_trackingEnabled) { g_haveLastPose = false; return; }
    if (g_gameWnd && GetForegroundWindow() != g_gameWnd) { g_haveLastPose = false; return; }

    if (g_haveLastPose) {
        float dyaw = yaw - g_lastYaw;
        if (dyaw >  3.14159265f) dyaw -= 6.2831853f;
        if (dyaw < -3.14159265f) dyaw += 6.2831853f;
        float dpitch = pitch - g_lastPitch;

        const float R2D = 57.29578f;
        float mx = dyaw   * R2D * g_yawCounts   + g_carryX;
        float my = -dpitch * R2D * g_pitchCounts * (g_invertPitch ? -1.f : 1.f) + g_carryY;
        int ix = (int)mx, iy = (int)my;
        g_carryX = mx - ix; g_carryY = my - iy;

        if (ix || iy) {
            INPUT in;
            memset(&in, 0, sizeof(in));
            in.type = INPUT_MOUSE;
            in.mi.dx = ix; in.mi.dy = iy;
            in.mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(1, &in, sizeof(INPUT));
        }
    }
    g_lastYaw = yaw; g_lastPitch = pitch;
    g_haveLastPose = true;
}
