// game/dishonored/motion_aim.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// Game-world direction from a shot's original view basis (yaw/pitch, from the
// projectile's own spawn forward - the ground truth of "where the game aims")
// plus the head-relative hand components. No camera object involved.
static void MaimDirFromView(float viewYaw, float viewPitch, const float* rel,
                            float* out)
{
    float cp = cosf(viewPitch), sp = sinf(viewPitch);
    float cy = cosf(viewYaw),   sy = sinf(viewYaw);
    float F[3] = {  cp*cy,  cp*sy,  sp };
    float R[3] = { -sy,     cy,     0  };
    float U[3] = { -sp*cy, -sp*sy,  cp };
    out[0] = F[0]*rel[2] + R[0]*rel[0] + U[0]*rel[1];
    out[1] = F[1]*rel[2] + R[1]*rel[0] + U[1]*rel[1];
    out[2] = F[2]*rel[2] + R[2]*rel[0] + U[2]*rel[1];
    V3Norm(out);
}


// Rewrite one projectile's aim to `dir`. refDir = the direction its velocity
// should currently be parallel to (spawn forward for a fresh catch, our last
// write for repeats).
static void MaimWriteAim(uint8_t* o, const float* dir, const float* refDir,
                         int* velWrites)
{
    float pitch = asinf(dir[2] < -1.f ? -1.f : (dir[2] > 1.f ? 1.f : dir[2]));
    float yaw   = atan2f(dir[1], dir[0]);
    float cp = cosf(pitch), sp = sinf(pitch), cy = cosf(yaw), sy = sinf(yaw);

    float* mx = (float*)(o + 0x50);
    mx[0] = cp*cy;   mx[1] = cp*sy;   mx[2] = sp;
    float* my = (float*)(o + 0x60);
    my[0] = -sy;     my[1] = cy;      my[2] = 0.0f;
    float* mz = (float*)(o + 0x70);
    mz[0] = -sp*cy;  mz[1] = -sp*sy;  mz[2] = cp;

    int32_t rp = (int32_t)(pitch * 10430.378f);
    int32_t ry = (int32_t)(yaw   * 10430.378f);
    int32_t* r1 = (int32_t*)(o + 0x9c);  r1[0] = rp; r1[1] = ry; r1[2] = 0;
    int32_t* r2 = (int32_t*)(o + 0xd0);  r2[0] = rp; r2[1] = ry; r2[2] = 0;

    // world-scale FVectors parallel to refDir = velocity: rotate, keep speed
    int nvel = 0;
    uint32_t sweepEnd = RangeReadable(o, 0x2c0) ? 0x2c0 : 0x220;
    for (uint32_t off = 0x140; off + 12 <= sweepEnd; off += 4) {
        float* f = (float*)(o + off);
        float v[3] = { f[0], f[1], f[2] };
        bool fin = true;
        for (int k = 0; k < 3; k++)
            if (v[k] != v[k] || v[k] > 3.0e38f || v[k] < -3.0e38f) { fin = false; break; }
        if (!fin) continue;
        float mag = sqrtf(V3Dot(v, v));
        if (mag < 500.0f || mag > 3.0e6f) continue;
        float n[3] = { v[0]/mag, v[1]/mag, v[2]/mag };
        if (V3Dot(n, refDir) < 0.995f) continue;
        f[0] = dir[0]*mag; f[1] = dir[1]*mag; f[2] = dir[2]*mag;
        nvel++;
        off += 8;
    }
    if (velWrites) *velWrites = nvel;
}


static void SteerAdd(uint8_t* o, const float* dir, double now)
{
    int slot = -1;
    for (int i = 0; i < g_steerN; i++) if (g_steer[i].ptr == o) slot = i;
    if (slot < 0) { slot = (g_steerN < 8) ? g_steerN++ : (int)(((uint32_t)now) % 8); }
    g_steer[slot].ptr = o;
    g_steer[slot].until = now + 400.0;
    g_steer[slot].dir[0] = dir[0]; g_steer[slot].dir[1] = dir[1]; g_steer[slot].dir[2] = dir[2];
    g_steer[slot].probed = 0;
    g_steer[slot].steered = false;
}


// Runs every frame: re-point anything we caught recently.
static void SteerTick()
{
    double now = MaimNowMs();
    for (int i = 0; i < g_steerN; i++) {
        SteerObj* s = &g_steer[i];
        if (!s->ptr || now > s->until) { s->ptr = NULL; continue; }
        uint8_t* o = s->ptr;
        if (!RangeReadable(o, 0x400)) { s->ptr = NULL; continue; }

        // one-time: report every world-scale vector, so we can SEE where this
        // projectile actually keeps its velocity
        int stage = g_aimProbe ? ((now > s->until - 330.0) ? 2 :
                    ((now > s->until - 380.0) ? 1 : 0)) : 0;
        if (stage && s->probed < stage) {
            s->probed = stage;
            Log("aimvec: --- %s pass %d ---", "projectile", stage);
            for (uint32_t off = 0x40; off + 12 <= 0x400; off += 4) {
                float* f = (float*)(o + off);
                bool fin = true;
                for (int k = 0; k < 3; k++)
                    if (f[k] != f[k] || fabsf(f[k]) > 3.0e38f) { fin = false; break; }
                if (!fin) continue;
                float mag = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
                if (mag < 300.0f || mag > 3.0e6f) continue;
                Log("aimvec: +0x%03x = (%.0f,%.0f,%.0f) |v|=%.0f", (unsigned)off,
                    f[0], f[1], f[2], mag);
            }
        }

        // ONE address. The probe found the arrow's velocity at +0x1b4 - 
        // (-8817, 38926, -2644), magnitude exactly 40000, a speed constant - 
        // and that is the same slot the pistol bullet used. So we rotate that
        // vector onto the hand ray, preserving its speed, and touch nothing
        // else. We keep doing it for a few frames because the engine fills
        // velocity in AFTER it announces the projectile.
        float* v = (float*)(o + 0x1b4);
        float mag = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (mag > 500.0f && mag < 3.0e6f) {
            float d0 = v[0]/mag, d1 = v[1]/mag, d2 = v[2]/mag;
            float dot = d0*s->dir[0] + d1*s->dir[1] + d2*s->dir[2];
            if (dot < 0.999f) {                       // not already ours
                v[0] = s->dir[0]*mag; v[1] = s->dir[1]*mag; v[2] = s->dir[2]*mag;
                if (!s->steered) {
                    s->steered = true;
                    Log("aim: velocity steered onto the hand ray (speed %.0f)", mag);
                }
            }
        }
    }
}


static void MaimCatch(uint8_t* o, const char* cn, const float* rel, double now)
{
    if (!RangeReadable(o, 0x220)) return;

    // the shot's true view aim = the projectile's own spawn forward; fall
    // back to its rotator if the matrix looks unset
    float oldF[3] = { *(float*)(o+0x50), *(float*)(o+0x54), *(float*)(o+0x58) };
    if (V3Norm(oldF) < 0.5f) {
        int32_t* r = (int32_t*)(o + 0x9c);
        float p = (float)r[0] / 10430.378f, y = (float)r[1] / 10430.378f;
        oldF[0] = cosf(p)*cosf(y); oldF[1] = cosf(p)*sinf(y); oldF[2] = sinf(p);
        if (V3Norm(oldF) < 0.5f) return;
    }
    float viewPitch = asinf(oldF[2] < -1.f ? -1.f : (oldF[2] > 1.f ? 1.f : oldF[2]));
    float viewYaw   = atan2f(oldF[1], oldF[0]);

    float dir[3];
    MaimDirFromView(viewYaw, viewPitch, rel, dir);

    int slot = -1;
    for (int k = 0; k < g_maimHitN; k++)
        if (g_maimHits[k].ptr == o) { slot = k; break; }
    if (slot < 0) {
        if (g_maimHitN < 16) slot = g_maimHitN++;
        else slot = (int)(((uint32_t)now) % 16);
    }
    MaimHit* h = &g_maimHits[slot];
    h->ptr = o; h->firstMs = now;
    h->cls = *(void**)(o + kClassOff);
    h->nameIdx = *(uint32_t*)(o + kNameOff);
    h->viewYaw = viewYaw; h->viewPitch = viewPitch;
    h->viewF[0] = oldF[0]; h->viewF[1] = oldF[1]; h->viewF[2] = oldF[2];
    {
        float* p = (float*)(o + 0x80);
        h->lastPos[0] = p[0]; h->lastPos[1] = p[1]; h->lastPos[2] = p[2];
    }
    h->trackN = 0;

    // if a fire-writer watch was pending on this (or any) pooled object,
    // report what wrote the rotation during THIS shot
    if (g_awAddr) AimWatchReport("shot fired");

    int nvel = 0;
    MaimWriteAim(o, dir, oldF, &nvel);
    h->lastDir[0] = dir[0]; h->lastDir[1] = dir[1]; h->lastDir[2] = dir[2];

    g_maimWindowCaught = true;
    SteerAdd(o, dir, now);
    MaimHaptic(g_maimHand, 0.6f, 0.06f);
    Log("aim: %s redirected  view=(%.2f,%.2f,%.2f) -> hand=(%.2f,%.2f,%.2f)  vel-rewrites=%d",
        cn ? cn : "?", oldF[0], oldF[1], oldF[2], dir[0], dir[1], dir[2], nvel);
}


// Is this class name a throwable/shootable projectile?
static bool MaimClassMatches(const char* cn)
{
    if (!cn) return false;
    return strncmp(cn, "DisProjectile", 13) == 0 ||
           strcmp (cn, "DisBullet")         == 0 ||
           strncmp(cn, "DisGrenade", 10)    == 0 ||
           strncmp(cn, "DisSticky", 9)      == 0;
}


static void MaimWatchAdd(uint8_t* o)
{
    for (int k = 0; k < g_maimWatchN; k++)
        if (g_maimWatch[k].ptr == o) return;
    int slot;
    if (g_maimWatchN < 64) slot = g_maimWatchN++;
    else { static int rr = 0; slot = rr; rr = (rr + 1) % 64; }
    MaimWatch* w = &g_maimWatch[slot];
    w->ptr = o;
    w->cls = *(void**)(o + kClassOff);
    w->nameIdx = *(uint32_t*)(o + kNameOff);
    if (RangeReadable(o + 0x80, 12)) {
        float* p = (float*)(o + 0x80);
        w->pos[0] = p[0]; w->pos[1] = p[1]; w->pos[2] = p[2];
    } else { w->pos[0] = w->pos[1] = w->pos[2] = 1e30f; }
}


// One full string-name scan: collect projectile-class instances (the pool)
// and PlayerCamera instances. Expensive-ish (like one 7.2 tracer scan) - 
// runs at most once per 5 s and only when armed.
static void MaimDiscover(void** objs, uint32_t num)
{
    g_maimCamN = 0;
    int nw = 0;
    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        if (*(uint32_t*)(o + kNameOff + 4) == 0) continue;   // instances only
        const char* cn = ObjClassName(o);
        if (!cn) continue;
        if (MaimClassMatches(cn)) { MaimWatchAdd(o); nw++; }
        else if (strstr(cn, "PlayerCamera") && g_maimCamN < 4) {
            g_maimCams[g_maimCamN].ptr = o;
            g_maimCams[g_maimCamN].nameIdx = *(uint32_t*)(o + kNameOff);
            g_maimCamN++;
        }
    }
    Log("aim: discover - %d projectile pool object(s), %d camera(s), watching %d",
        nw, g_maimCamN, g_maimWatchN);
}


// Distance filter: within MaxDist of ANY PlayerCamera instance (one of them
// is the live one; stale ones only widen the accept region harmlessly).
static bool MaimNearCamera(const float* pos)
{
    bool any = false;
    for (int k = 0; k < g_maimCamN; k++) {
        uint8_t* c = g_maimCams[k].ptr;
        if (!c || !RangeReadable(c, 0x8c) ||
            *(uint32_t*)(c + kNameOff) != g_maimCams[k].nameIdx) continue;
        any = true;
        float* p = (float*)(c + 0x80);
        float dx = pos[0]-p[0], dy = pos[1]-p[1], dz = pos[2]-p[2];
        if (dx*dx + dy*dy + dz*dz <= g_maimMaxDist * g_maimMaxDist) return true;
    }
    if (!any) g_maimNeedDiscover = true;   // all cameras went stale
    return false;
}


static bool MaimTakeSnapshot(void** objs, uint32_t num)
{
    if (!RangeReadable(objs, (size_t)num * sizeof(void*))) return false;
    if (num > g_maimSnapCap) {
        uint32_t cap = num + 8192;
        void** p = (void**)realloc(g_maimSnap, (size_t)cap * sizeof(void*));
        if (!p) return false;
        g_maimSnap = p; g_maimSnapCap = cap;
    }
    memcpy(g_maimSnap, objs, (size_t)num * sizeof(void*));
    g_maimSnapN = num;
    return true;
}


// Per-tick (~90 Hz): find a fresh projectile and steer it along the hand ray.
static void MotionAimTick(float hl, float hr)
{
    // (live keys moved to the mesh-control cluster; PitchOffsetDeg is ini-only)
    if (!g_maimEnabled) return;

    // watchpoint flood guard: freed/reused memory can storm the VEH - the VEH
    // itself must not suspend threads, so disarm from here (safe context)
    if (g_awAddr && g_awTotal > 2000) AimWatchReport("flood - object likely freed");

    double now = MaimNowMs();

    // arm a shot window on the aim hand's trigger edge
    {
        static bool trigWas = false;
        float t = g_maimHand ? hr : hl;
        bool trigNow = t > 0.55f;
        if (trigNow && !trigWas) {
            g_maimArmedUntil = now + g_maimWindowMs;
            g_maimArmMs = now;
            g_maimNeedSnap = true;
            g_maimWindowCaught = false;
            g_maimWindowLogged = false;
            float rel[3];
            bool haveHand = MaimHandRel(rel);
            Log("aim: trigger pulled (L=%.2f R=%.2f, watching '%s' hand) - hand ray %s",
                hl, hr, g_maimHand ? "right" : "left",
                haveHand ? "OK" : "UNAVAILABLE (no controller pose!)");
        }
        trigWas = trigNow;
        // if a trigger is being squeezed but never crosses the threshold, say so
        {
            static int loud = 0;
            float t2 = g_maimHand ? hr : hl;
            if (t2 > 0.05f && t2 <= 0.55f && ++loud > 120) {
                loud = 0;
                Log("aim: aim-hand trigger reads %.2f - below the 0.55 fire threshold", t2);
            }
        }
    }
    if (now > g_maimArmedUntil) {
        if (!g_maimWindowLogged) {
            g_maimWindowLogged = true;
            if (!g_maimWindowCaught)
                Log("aim: shot window closed - nothing caught (power/hitscan, or a miss: check discover counts)");
            // arm the fire-writer watch on the freshest caught pooled object,
            // so the NEXT shot reveals who writes its rotation
            if (!g_awAddr) {
                uint8_t* best = NULL; double bestMs = -1;
                for (int k = 0; k < g_maimHitN; k++)
                    if (g_maimHits[k].ptr && g_maimHits[k].firstMs > bestMs) {
                        best = g_maimHits[k].ptr; bestMs = g_maimHits[k].firstMs;
                    }
                if (best && RangeReadable(best, 0xa4)) AimWatchArm(best);
            }
        }
        return;
    }

    float rel[3];
    if (!MaimHandRel(rel)) return;

    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    if (!RangeReadable(objs, (size_t)num * sizeof(void*))) return;

    // trigger-time baseline + (rate-limited) pool/camera discovery
    if (g_maimNeedSnap) {
        g_maimNeedSnap = false;
        if (!MaimTakeSnapshot(objs, num)) { g_maimArmedUntil = 0; return; }
        if ((g_maimNeedDiscover || !g_maimWatchN || !g_maimCamN) &&
            now - g_maimLastDiscover > 5000.0) {
            g_maimNeedDiscover = false; g_maimLastDiscover = now;
            MaimDiscover(objs, num);
        }
        return;
    }
    if (!g_maimSnapN) return;

    // (a) keep steering this window's catches (150 ms), following the hand
    for (int k = 0; k < g_maimHitN; k++) {
        MaimHit* h = &g_maimHits[k];
        if (!h->ptr || h->firstMs < g_maimArmMs || now - h->firstMs >= 150.0)
            continue;
        uint8_t* o = h->ptr;
        if (!RangeReadable(o, kClassOff + 4) ||
            *(void**)(o + kClassOff) != h->cls ||
            *(uint32_t*)(o + kNameOff) != h->nameIdx) { h->ptr = NULL; continue; }
        if (!RangeReadable(o, 0x220)) { h->ptr = NULL; continue; }
        float dir[3];
        MaimDirFromView(h->viewYaw, h->viewPitch, rel, dir);
        // flight tracker: which way is it ACTUALLY moving?
        if (h->trackN < 6 && RangeReadable(o + 0x80, 12)) {
            float* p = (float*)(o + 0x80);
            float dp[3] = { p[0]-h->lastPos[0], p[1]-h->lastPos[1], p[2]-h->lastPos[2] };
            float dl = sqrtf(V3Dot(dp, dp));
            if (dl > 25.0f) {
                float nd[3] = { dp[0]/dl, dp[1]/dl, dp[2]/dl };
                Log("aim: track[%d] |dp|=%.0fuu  dot(hand)=%+.2f dot(view)=%+.2f",
                    h->trackN, dl, V3Dot(nd, h->lastDir), V3Dot(nd, h->viewF));
                h->lastPos[0] = p[0]; h->lastPos[1] = p[1]; h->lastPos[2] = p[2];
                h->trackN++;
            }
        }
        MaimWriteAim(o, dir, h->lastDir, NULL);
        h->lastDir[0] = dir[0]; h->lastDir[1] = dir[1]; h->lastDir[2] = dir[2];
    }

    // (b) NEW objects: diff the list against the trigger-time snapshot.
    // 30.25: this loop was the combat hitch - a VirtualQuery-gated read and
    // name lookup for EVERY slot that churned (thousands per frame when
    // particles fly). Now: RAW reads inside the fault boundary (a dying
    // object costs one aborted tick, not a crash), and a class-pointer ->
    // verdict cache so each CLASS pays for its name exactly once.
    static void*   sClsK[2048];
    static uint8_t sClsV[2048];
    g_walkTid2 = GetCurrentThreadId();
    if (setjmp(g_walkJmp2)) {
        g_walkTid2 = 0;
        static int aborted = 0;
        if (++aborted <= 3) Log("aim: pool diff aborted on a dying object");
    } else {
        uint32_t lim = num < g_maimSnapN ? num : g_maimSnapN;
        for (uint32_t i = 1; i < num; i++) {
            uint8_t* o = (uint8_t*)objs[i];
            if (!o) continue;
            if (i < lim) { if ((void*)o == g_maimSnap[i]) continue; g_maimSnap[i] = o; }
            else if (i < g_maimSnapCap) { g_maimSnap[i] = o; }
            if ((uintptr_t)o & 3) continue;
            void* cls = *(void**)(o + kClassOff);          // raw, guarded
            if (!cls || ((uintptr_t)cls & 3)) continue;
            if (*(uint32_t*)(o + kNameOff + 4) == 0) continue;
            unsigned hh = (unsigned)(((uintptr_t)cls) >> 4) & 2047;
            bool match;
            if (sClsK[hh] == cls) match = sClsV[hh] != 0;
            else {
                const char* cn2 = ObjClassName(o);         // slow path, once
                match = MaimClassMatches(cn2);
                sClsK[hh] = cls; sClsV[hh] = match ? 1 : 0;
            }
            if (!match) continue;
            const char* cn = ObjClassName(o);
            if (!MaimClassMatches(cn)) continue;           // re-verify for real
            MaimWatchAdd(o);                   // future shots reuse this object
            if (!RangeReadable(o + 0x80, 12)) continue;
            float* p = (float*)(o + 0x80);
            if (!MaimNearCamera(p)) continue;
            MaimCatch(o, cn, rel, now);
        }
        if (num > g_maimSnapN && num <= g_maimSnapCap) g_maimSnapN = num;
        else if (num < g_maimSnapN) g_maimSnapN = num;
        g_walkTid2 = 0;
    }

    // (c) POOLED objects: watch-list position-teleport catch (a reused
    // projectile snaps from wherever it died to the muzzle)
    for (int k = 0; k < g_maimWatchN; k++) {
        MaimWatch* w = &g_maimWatch[k];
        uint8_t* o = w->ptr;
        if (!o) continue;
        if (!RangeReadable(o, kClassOff + 4) ||
            *(void**)(o + kClassOff) != w->cls ||
            *(uint32_t*)(o + kNameOff) != w->nameIdx) { w->ptr = NULL; continue; }
        if (!RangeReadable(o + 0x80, 12)) continue;
        float* p = (float*)(o + 0x80);
        float dx = p[0]-w->pos[0], dy = p[1]-w->pos[1], dz = p[2]-w->pos[2];
        float moved2 = dx*dx + dy*dy + dz*dz;
        w->pos[0] = p[0]; w->pos[1] = p[1]; w->pos[2] = p[2];
        if (moved2 < 100.0f * 100.0f) continue;      // idle (or slow drift)
        if (!MaimNearCamera(p)) continue;
        bool caughtThisWindow = false;
        for (int j = 0; j < g_maimHitN; j++)
            if (g_maimHits[j].ptr == o && g_maimHits[j].firstMs >= g_maimArmMs)
                { caughtThisWindow = true; break; }
        if (caughtThisWindow) continue;
        MaimCatch(o, ObjClassName(o), rel, now);
    }
}
