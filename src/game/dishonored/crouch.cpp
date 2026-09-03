// game/dishonored/crouch.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// The pawn the player is actually possessing. PlayerController+kPcPawn first,
// the event latch second - the same preference crouch and fp_mesh have used
// since 32.40, hoisted here because everything that asks "is there a gameplay
// pawn" ends up in PawnCollisionHeight().
//
// 41.1: THE EVENT LATCH NEVER FIRES IN THIS GAME. PeLatch names a pawn only
// when ProcessEvent dispatches ON an object whose class contains "PlayerPawn",
// and DishonoredPlayerPawn is native - measured over a 5.5 minute gameplay run
// (2026-09-02, simulator lane, build alpha-186): `handmesh: latched controller
// 'DishonoredPlayerController'` fired, `latched pawn` never did, the script
// census carried DishonoredNPCPawn and no player pawn, and g_pePawn was NULL
// the whole run. Everything downstream of CylTruthLive() then read "no
// gameplay" for the entire session (ENGINE_NOTES, "the pawn oracle").
static uint8_t* PossessedPawn()
{
    const char* src = "none";
    uint8_t* pawn = NULL;
    if (g_peCtrl && LooksLikeObj(g_peCtrl) && RangeReadable(g_peCtrl + kPcPawn, 4)) {
        uint8_t* pp = *(uint8_t**)(g_peCtrl + kPcPawn);
        if (LooksLikeObj(pp)) { pawn = pp; src = "ctrl+0x248"; }
    }
    if (!pawn && g_pePawn && LooksLikeObj(g_pePawn)) { pawn = g_pePawn; src = "event-latch"; }
    // Name the OWNER before the result, and log the CHANGE only: this one
    // signal decides the game state, the stale-menu clear and the runtime's
    // gameplay verdict, so a healthy run and a dead one must not read alike.
    static const char* was = NULL;
    if (src != was) {
        was = src;
        Log("pawn oracle: source now %s%s (controller %s, event latch %s)", src,
            pawn ? "" : "  <-- NO GAMEPLAY PAWN: the game state, the stale-menu clear and "
                        "the runtime's gameplay verdict all read 'menu' from here",
            g_peCtrl ? "latched" : "NOT latched", g_pePawn ? "latched" : "NOT latched");
    }
    return pawn;
}

static float PawnCollisionHeight()              // < 0 = unknown
{
    if (!g_cylTried) {
        g_cylTried = 1;
        g_cylHOff   = FindPropOffset("CylinderComponent", "CollisionHeight");
        g_cylCompOff= FindPropOffset("Actor", "CollisionComponent");
        Log("cyl: CollisionHeight at CylinderComponent+0x%x, "
            "CollisionComponent at Actor+0x%x%s", g_cylHOff, g_cylCompOff,
            (g_cylHOff && g_cylCompOff) ? "" : "  <-- NOT FOUND, no measurement");
    }
    if (!g_cylHOff || !g_cylCompOff) return -1.0f;
    uint8_t* pawn = PossessedPawn();
    if (!pawn) return -1.0f;
    if (!RangeReadable(pawn + g_cylCompOff, 4)) return -1.0f;
    uint8_t* comp = *(uint8_t**)(pawn + g_cylCompOff);
    if (!comp || ((uintptr_t)comp & 3) || !RangeReadable(comp, g_cylHOff + 4))
        return -1.0f;
    float h = *(float*)(comp + g_cylHOff);
    if (h > 1.0f && h < 500.0f) { g_cylOkMs = MaimNowMs(); g_cylLast = h; return h; }
    return -1.0f;
}


// 38.19 CRAWL TUCK. Measured A/B (user): under-table crawling is flawless
// with the hand drive OFF (HOME) and hit-or-miss with it on, even after the
// 38.18 height fix - the world-space arm posing collides with the crawl
// system's clearance probes. So while the game itself is in forced-crawl
// (cylinder < 50 - the vents/under-furniture state), the drive stands down
// and the arms return to the game's own tucked animation, exactly what the
// HOME test proved works; hands come back the moment you emerge.
// [Hands] CrawlTuck=0 disables.
static bool CrawlTuckNow()
{
    // 38.21: tuck for the WHOLE crouch, with hysteresis - the shape the
    // user actually measured as flawless (HOME pressed before approaching,
    // standing included). The 38.19/38.20 crawl-only trigger (<50) engaged
    // mid-entry, exactly where the cylinder flaps 65<->33, so the release/
    // restore cycle thrashed at the boundary and blocked entry outright.
    // Engage below 76 (any crouch), release only above 80 (real stand) -
    // the flap zone lies entirely inside the tucked band, so transitions
    // happen only on true stance changes. Functional hands (blink aim,
    // crosshair, melee, wheel) read controller poses and stay live; only
    // the visual arm-follow yields to the game's own crouch animation.
    static bool on = false;
    if (!g_crawlTuckCfg || g_cylLast <= 0.0f ||
        (MaimNowMs() - g_cylOkMs) > 1500.0) { on = false; return false; }
    if (on)  { if (g_cylLast > 80.0f) on = false; }
    else     { if (g_cylLast < 76.0f) on = true;  }
    return on;
}


// 38.16: the matching writer (deep crouch). Same pointer discipline as the
// read; only ever called with values in [33, 64].
static bool PawnSetCollisionHeight(float v)
{
    if (!g_cylHOff || !g_cylCompOff) return false;
    uint8_t* pawn = g_pePawn;
    if (!pawn || !LooksLikeObj(pawn)) return false;
    if (!RangeReadable(pawn + g_cylCompOff, 4)) return false;
    uint8_t* comp = *(uint8_t**)(pawn + g_cylCompOff);
    if (!comp || ((uintptr_t)comp & 3) || !RangeReadable(comp, g_cylHOff + 4))
        return false;
    *(float*)(comp + g_cylHOff) = v;
    return true;
}


// Find Pawn.bIsCrouched by reflection: a BoolProperty UObject named
// "bIsCrouched" carries its byte offset at +0x5c and its bitmask at +0x6c -
// the same two fields the SkelControl probe read in 30.94.
static void CrouchPropFind()
{
    if (g_bIsCrouchFound && g_bWantsCrFound) return;
    double now = MaimNowMs();
    if (now < g_crouchPropNext) return;
    g_crouchPropNext = now + 5000.0;
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || strcmp(cn, "BoolProperty")) continue;
        const char* pn = RealName(*(uint32_t*)(o + kNameOff));
        bool wants = (pn && !strcmp(pn, "bWantsToCrouch"));
        if (!pn || (strcmp(pn, "bIsCrouched") && !wants)) continue;
        uint32_t off  = *(uint32_t*)(o + 0x5c);
        uint32_t mask = *(uint32_t*)(o + 0x6c);
        if (!mask || off < 0x20 || off > 0x2000) continue;
        if (wants) {
            if (!g_bWantsCrFound) {
                uint8_t* ow2 = RangeReadable(o + kOuterOff, 4)
                             ? *(uint8_t**)(o + kOuterOff) : NULL;
                const char* oc2 = (ow2 && LooksLikeObj(ow2))
                                ? RealName(*(uint32_t*)(ow2 + kNameOff)) : "?";
                g_bWantsCrOff = off; g_bWantsCrMask = mask;
                g_bWantsCrFound = true;
                Log("crouch/prop: bWantsToCrouch on '%s' at +0x%03x mask 0x%08x",
                    oc2 ? oc2 : "?", (unsigned)off, (unsigned)mask);
            }
            continue;
        }
        // 32.37: there is MORE THAN ONE bIsCrouched in GObjects - UE3 declares
        // one per class that redeclares it, and DLC packages add their own. The
        // first hit won, and it was the wrong one: the flag read true for
        // 109-719 ms bursts while the user held a crouch for seconds, which is
        // a bit that means something else. Log every candidate with its owning
        // class and prefer the one declared on a Pawn.
        uint8_t* owner = RangeReadable(o + kOuterOff, 4)
                       ? *(uint8_t**)(o + kOuterOff) : NULL;
        const char* ocls = (owner && LooksLikeObj(owner))
                         ? RealName(*(uint32_t*)(owner + kNameOff)) : "?";
        bool isPawn = ocls && strstr(ocls, "Pawn") != NULL;
        Log("crouch/prop: bIsCrouched on '%s' at +0x%03x mask 0x%08x%s",
            ocls ? ocls : "?", (unsigned)off, (unsigned)mask,
            isPawn ? "   <-- PAWN, using this" : "");
        if (!isPawn) continue;                 // keep looking for the Pawn one
        g_bIsCrouchOff = off; g_bIsCrouchMask = mask; g_bIsCrouchFound = true;
        if (g_bWantsCrFound) return;      // both found - stop sweeping
    }
}


// 32.38: Actor.Location's byte offset, by reflection. UE3 declares Location as
// a StructProperty on class Actor; its offset lives at +0x5c exactly like the
// BoolProperty above. Never guessed - a guessed layout is what crashed 30.41.
static void LocPropFind()
{
    if (g_actorLocFound) return;
    double now = MaimNowMs();
    if (now < g_locPropNext) return;
    g_locPropNext = now + 5000.0;
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || strcmp(cn, "StructProperty")) continue;
        const char* pn = RealName(*(uint32_t*)(o + kNameOff));
        if (!pn || strcmp(pn, "Location")) continue;
        uint8_t* owner = RangeReadable(o + kOuterOff, 4)
                       ? *(uint8_t**)(o + kOuterOff) : NULL;
        const char* ocls = (owner && LooksLikeObj(owner))
                         ? RealName(*(uint32_t*)(owner + kNameOff)) : "?";
        uint32_t off = *(uint32_t*)(o + 0x5c);
        if (off < 0x20 || off > 0x2000) continue;
        bool isActor = ocls && !strcmp(ocls, "Actor");
        Log("crouch/prop: Location on '%s' at +0x%03x%s",
            ocls ? ocls : "?", (unsigned)off,
            isActor ? "   <-- Actor, using this" : "");
        if (!isActor) continue;
        g_actorLocOff = off; g_actorLocFound = true;
        return;
    }
}


static void CrouchStateTick()
{
    // 32.36: say out loud which trim set is live and what is in it. "The
    // crouch numbers do nothing" has to be checkable from the log rather than
    // reasoned about - the detection provably works, so the question is
    // whether the values reach the placement.
    {
        static double nextTell = 0.0;
        double nw = MaimNowMs();
        if (nw >= nextTell && g_skcDrive) {
            nextTell = nw + 3000.0;
            bool useC = g_skcCrouchTrimOn && g_pawnCrouched;
            float t[3];
            for (int k2 = 0; k2 < 3; k2++)
                t[k2] = g_skcTrim[1][k2] + (useC ? g_skcTrimCrouch[1][k2] : 0.0f);
            Log("trim: %s (perStance=%d crouched=%d) R=(%.0f,%.0f,%.0f)"
                " = stand(%.0f,%.0f,%.0f)%s",
                useC ? "stand+CROUCH offset" : "stand", (int)g_skcCrouchTrimOn,
                (int)g_pawnCrouched, t[0], t[1], t[2],
                g_skcTrim[1][0], g_skcTrim[1][1], g_skcTrim[1][2],
                useC ? " + offset" : "");
        }
    }
    CrouchPropFind();
    LocPropFind();

    // 32.40: the possessed pawn, not whatever the event stream touched last.
    // PlayerController+0x248 is the Pawn reference the census already proved
    // out; g_pePawn stays as the fallback so nothing regresses if the layout
    // differs on some build.
    uint8_t* pawn = NULL;
    const char* pawnSrc = "ctrl+0x248";
    if (g_peCtrl && LooksLikeObj(g_peCtrl) && RangeReadable(g_peCtrl + 0x248, 4)) {
        uint8_t* pp = *(uint8_t**)(g_peCtrl + 0x248);
        if (LooksLikeObj(pp)) pawn = pp;
    }
    if (!pawn) { pawn = g_pePawn; pawnSrc = "event-latch"; }
    if (!pawn || !LooksLikeObj(pawn)) return;
    if (pawn != g_crouchPawnWas) {
        Log("crouch/pawn: now %p (%s) via %s%s", (void*)pawn, ObjClassName(pawn),
            pawnSrc, g_crouchPawnWas ? "   <-- POINTER CHANGED" : "");
        g_crouchPawnWas = pawn;
    }

    // --- signal A: the engine's flag (kept, but only logged unless selected)
    bool flagSays = g_pawnCrouched;
    if (g_bIsCrouchFound && RangeReadable(pawn + g_bIsCrouchOff, 4))
        flagSays = (*(uint32_t*)(pawn + g_bIsCrouchOff) & g_bIsCrouchMask) != 0;
    bool wantsSays = false;
    if (g_bWantsCrFound && RangeReadable(pawn + g_bWantsCrOff, 4))
        wantsSays = (*(uint32_t*)(pawn + g_bWantsCrOff) & g_bWantsCrMask) != 0;

    // --- signal B: measured eye height above the pawn's own origin ----------
    bool eyeOk = false;
    if (g_actorLocFound && RangeReadable(pawn + g_actorLocOff, 12) &&
        CamStillValid() && RangeReadable(g_camObj + 0x80, 12)) {
        const float* cp = (const float*)(g_camObj + 0x80);
        const float* pl = (const float*)(pawn + g_actorLocOff);
        float eye = cp[2] - pl[2];
        // A teleport, a level load or a cutscene camera can put the camera
        // anywhere; anything outside a human range is not a stance reading.
        if (eye > 10.0f && eye < 400.0f) {
            g_eyeNowUU = eye;
            eyeOk = true;
            // 32.39: "standing = the highest eye height ever seen" was wrong in
            // the same way a peak meter is wrong. The log shows it: the true
            // standing eye was ~100 uu and the crouched one ~80, but a handful
            // of transients (jumps, stairs, a camera cut) ratcheted the
            // baseline to 105 and it can never come back down - so 90 of 104
            // samples read CROUCHED while the user was standing, the crouch
            // trim set went live, and the standing calibration vanished.
            // Track BOTH stances instead and classify against the midpoint.
            // A spike now pulls the standing estimate a few percent and decays
            // out; it cannot latch. Nothing is ever a permanent reference.
            double nowMs = MaimNowMs();
            double dt = (g_eyeLastMs > 0.0) ? (nowMs - g_eyeLastMs) : 0.0;
            g_eyeLastMs = nowMs;
            if (!g_eyeHaveRef) {
                g_eyeStandUU  = eye;
                g_eyeCrouchUU = eye - g_eyeDropUU;
                g_eyeHaveRef  = true;
            }
            float a = (float)(dt / 3000.0);           // ~3 s time constant
            if (a < 0.0f)   a = 0.0f;
            if (a > 0.15f)  a = 0.15f;
            float mid = 0.5f * (g_eyeStandUU + g_eyeCrouchUU);
            if (eye >= mid) g_eyeStandUU  += a * (eye - g_eyeStandUU);
            else            g_eyeCrouchUU += a * (eye - g_eyeCrouchUU);
            if (g_eyeStandUU - g_eyeCrouchUU < 4.0f)
                g_eyeCrouchUU = g_eyeStandUU - 4.0f;   // keep them ordered
            float sep = g_eyeStandUU - g_eyeCrouchUU;
            mid = 0.5f * (g_eyeStandUU + g_eyeCrouchUU);
            // Until the two clusters are genuinely apart we have not seen a
            // crouch yet and must not invent one.
            bool want = g_eyeCrouched;
            if (sep >= g_eyeDropUU * 0.5f) {
                float h = 0.15f * sep;                 // hysteresis band
                if (eye < mid - h) want = true;
                if (eye > mid + h) want = false;
            } else {
                want = false;
            }
            if (want != g_eyeCrouched) {
                g_eyeCrouched = want;
                Log("crouch/eye: %s (eye %.0f uu, stand %.0f, crouch %.0f,"
                    " sep %.0f)", want ? "CROUCHED" : "standing", eye,
                    g_eyeStandUU, g_eyeCrouchUU, sep);
            }
        }
    }

    // Heartbeat so the two signals can be compared from the log rather than
    // argued about. bIsCrouched has now been wrong twice; this is what settles
    // whether the replacement is right before any more of the trim is blamed.
    // 32.40: every input to the decision, 5 Hz, one line. Two rounds have now
    // been spent reasoning about which flag is "the" crouch state from
    // transition logs alone; this prints the raw numbers so the answer comes
    // out of the data. pawnZ is in the line because if the pointer is what has
    // been blipping, pawnZ jumps by metres between samples and that is
    // instantly visible.
    if (g_crouchDiag) {
        double nw2 = MaimNowMs();
        if (nw2 >= g_crouchDiagNext) {
            g_crouchDiagNext = nw2 + 200.0;
            float pz = (g_actorLocFound && RangeReadable(pawn + g_actorLocOff, 12))
                     ? ((const float*)(pawn + g_actorLocOff))[2] : 0.0f;
            float cz = (CamStillValid() && RangeReadable(g_camObj + 0x80, 12))
                     ? ((const float*)(g_camObj + 0x80))[2] : 0.0f;
            // 38.25 crawlbox: film the stuck moment. stick raw (what the
            // controller sends) vs delivered (what the game receives after
            // menu shaping), pawn planar speed, and the menu flags. The
            // three possible stuck shapes separate instantly: stick high +
            // speed 0 = physics block; raw high + delivered chopped = OUR
            // input shaping (ghost menu = MenuStep pulses); speed high +
            // view stuck = camera lag.
            float px2 = 0, py2 = 0, spd = 0;
            if (g_actorLocFound && RangeReadable(pawn + g_actorLocOff, 12)) {
                px2 = ((const float*)(pawn + g_actorLocOff))[0];
                py2 = ((const float*)(pawn + g_actorLocOff))[1];
                static float lpx = 0, lpy = 0; static double lts = 0;
                if (lts > 0 && nw2 > lts)
                    spd = sqrtf((px2-lpx)*(px2-lpx) + (py2-lpy)*(py2-lpy)) /
                          (float)((nw2 - lts) * 0.001);
                lpx = px2; lpy = py2; lts = nw2;
            }
            SHORT dlx = 0, dly = 0; dvr::pad::delivered_stick(&dlx, &dly);
            float rawMx = 0.0f, rawMy = 0.0f; dvr::pad::raw_move(&rawMx, &rawMy);
            Log("crouch/raw: camZ=%.1f pawnZ=%.1f eye=%.1f  bIsCrouched=%d"
                " bWants=%d eyeSays=%d  pad=0x%04x mask=0x%04x"
                " btn=%d btnSays=%d (%s%s)"
                " | stickRaw=(%.2f,%.2f) stickOut=(%.2f,%.2f) spd=%.0fuu/s"
                " pos=(%.0f,%.0f)"          // 38.67: pawn XY - the boat trace
                " menu=%d/%d wheel=%d",
                cz, pz, cz - pz, (int)flagSays, (int)wantsSays,
                (int)g_eyeCrouched,
                (unsigned)dvr::pad::buttons(),
                (unsigned)g_crouchBtnMask,
                (int)dvr::pad::crouch_down(),
                (int)g_crouchBtnSt, g_crouchToggle ? "toggle" : "hold",
                g_crouchTogLock ? ", measured" : ", assumed",
                rawMx, rawMy,
                dlx / 32767.0f, dly / 32767.0f, spd,
                px2, py2,
                (int)g_menuOpen, (int)g_inMenu, (int)g_wheelHeld);
        }
    }

    // --- signal C: the crouch button we send the game ----------------------
    {
        uint32_t btns = (uint32_t)dvr::pad::buttons();
        bool held = (btns & g_crouchBtnMask) != 0;
        bool camOk = CamStillValid() && RangeReadable(g_camObj + 0x80, 12);
        const float* cpv = camOk ? (const float*)(g_camObj + 0x80) : NULL;
        float cz = camOk ? cpv[2] : 0.0f;
        float cx = camOk ? cpv[0] : 0.0f;
        float cy = camOk ? cpv[1] : 0.0f;
        double tms = MaimNowMs();
        if (camOk) {
            float d = cz - g_camZLast;
            if (d < 0.0f) d = -d;
            if (d > 2.0f || g_camMoveMs == 0.0) g_camMoveMs = tms;
            g_camZLast = cz;
        }

        // Which button IS crouch? Press a button, wait 400 ms, look at the
        // camera. Down => that button crouches you. Nothing else in the game
        // lowers the view and holds it there. Two agreements adopt the bit.
        uint32_t rise = btns & ~g_crouchBtnPrev;
        for (int b = 0; b < 16; b++) {
            uint32_t bit = 1u << b;
            if ((rise & bit) && cz != 0.0f) {
                g_crouchBitPend[b] = true;
                g_crouchBitMs[b]   = tms;
                g_crouchBitZ[b]    = cz;
                g_crouchBitX[b]    = cx;
                g_crouchBitY[b]    = cy;
                g_crouchBitQuiet[b] = (tms - g_camMoveMs) > 400.0;
                if (g_crouchBindArm) {
                    g_crouchBindArm = false;
                    g_crouchBtnMask = bit;
                    g_crouchTogLock = false; g_crouchTogVote = 0;
                    g_crouchBtnSt   = false;
                    Log("crouch/btn: BOUND to 0x%04x by hand", (unsigned)bit);
                }
            }
            if (g_crouchBitPend[b] && (tms - g_crouchBitMs[b]) > 400.0) {
                g_crouchBitPend[b] = false;
                if (cz == 0.0f) continue;
                // 32.44: only judge a press when the evidence is clean.
                // - the camera has to have been STILL before the press
                // - it must not have travelled horizontally (blink, walking)
                // - the vertical move has to be crouch-sized, not a fall
                float dx = cx - g_crouchBitX[b], dy = cy - g_crouchBitY[b];
                float horiz = sqrtf(dx*dx + dy*dy);
                float dz = g_crouchBitZ[b] - cz;
                if (dz < 0.0f) dz = -dz;
                if (!g_crouchBitQuiet[b] || horiz > 40.0f ||
                    (dz > 4.0f && (dz < 10.0f || dz > 45.0f))) {
                    Log("crouch/learn: ignoring 0x%04x (dz %.1f, moved %.0f uu,"
                        " %s) - not a clean crouch", (unsigned)bit, dz, horiz,
                        g_crouchBitQuiet[b] ? "camera was still" : "camera was"
                        " already moving");
                    continue;
                }
                // 32.43: score on the MAGNITUDE of the camera move, not its
                // sign. A toggle crouch alternates - press one puts you down,
                // press two stands you up - so "did the camera go DOWN" scored
                // +1,-1,+1,-1 forever and never reached the threshold. The log
                // shows exactly that: eight cycles of 0x2000, all correctly
                // detected, none ever adopted. A button that moves the view
                // and holds it there is a crouch button in either direction;
                // one that does nothing to the view is not.
                float ch = dz;
                if (ch >= 10.0f)    g_crouchBitScore[b]++;
                else if (ch < 3.0f) g_crouchBitScore[b]--;
                else continue;
                Log("crouch/learn: button 0x%04x -> camera moved %.1f uu"
                    " (score %d)", (unsigned)bit, ch, g_crouchBitScore[b]);
                if (g_crouchBitScore[b] >= 3 && !(g_crouchBtnMask & bit)) {
                    g_crouchBtnMask |= bit;
                    Log("crouch/learn: ADOPTED 0x%04x as a crouch button"
                        " (mask now 0x%04x)", (unsigned)bit,
                        (unsigned)g_crouchBtnMask);
                }
                // And the reverse: a bit in the mask that keeps doing nothing
                // to the camera does not belong there. Without this the
                // default mask could only ever grow, so a player whose crouch
                // is bound elsewhere would end up toggling their stance on
                // some unrelated button forever.
                if (g_crouchBitScore[b] <= -3 && (g_crouchBtnMask & bit) &&
                    (g_crouchBtnMask & ~bit) != 0) {
                    g_crouchBtnMask &= ~bit;
                    Log("crouch/learn: DROPPED 0x%04x - it never moves the"
                        " camera (mask now 0x%04x)", (unsigned)bit,
                        (unsigned)g_crouchBtnMask);
                }
            }
        }
        g_crouchBtnPrev = btns;
        if (held && !g_sneakWas) {                    // press
            g_crouchZPress = cz;
            g_crouchXPress = cx; g_crouchYPress = cy;
            g_crouchQuietP = (tms - g_camMoveMs) > 400.0;
            g_crouchChkPend = true; g_crouchChkMs = tms;
            g_crouchRelPend = false;
            if (g_crouchToggle && !CylTruthLive()) g_crouchBtnSt = !g_crouchBtnSt;
        } else if (!held && g_sneakWas) {             // release
            g_crouchRelMs = tms;
            g_crouchRelPend = true;
        }
        if (!g_crouchToggle && !CylTruthLive()) g_crouchBtnSt = held;
        g_sneakWas = held;

        // 32.96: the cylinder is the referee now. 65.0 = crouched, 87.5 =
        // standing, measured; the log even caught our label inverted against
        // it. Reading the truth beats inferring it, so when the cylinder
        // speaks, it overrides the press-counting - which stays only for the
        // moments the pawn pointer is not readable.
        // 32.97: the referee no longer rules DURING the play. The cylinder
        // takes a beat to resize after the toggle, so the 32.96 sync flipped
        // the stance to standing mid-crouch and back again once the resize
        // landed - a double-flip on every legitimate transition, which is
        // exactly the trim wobble and "inconsistent" feel reported. Two
        // rules: stay silent for 800 ms after any press or release (the
        // transition window belongs to the button), and only override after
        // THREE consecutive agreeing readings (~750 ms of steady cylinder).
        // Truth that is a moment late beats truth that argues mid-motion.
        {
            static double cylNext = 0.0;
            static int    cylAgree = 0;
            static bool   cylLast  = false;
            if (tms >= cylNext) {
                cylNext = tms + 250.0;
                double lastEdge = g_crouchChkMs > g_crouchRelMs
                                ? g_crouchChkMs : g_crouchRelMs;
                float ch2 = PawnCollisionHeight();
                if (ch2 > 1.0f && (tms - lastEdge) > 800.0) {
                    bool cylSays = ch2 < 76.0f;      // 65 vs 87.5, split it
                    if (cylSays == cylLast) {
                        if (cylAgree < 3) cylAgree++;
                    } else {
                        cylLast = cylSays; cylAgree = 1;
                    }
                    if (cylAgree >= 3 && cylSays != g_crouchBtnSt) {
                        g_crouchBtnSt = cylSays;
                        Log("crouch/cyl: stance corrected from the collision "
                            "cylinder (%.1f uu, steady -> %s)", ch2,
                            cylSays ? "CROUCHED" : "standing");
                    }
                } else {
                    cylAgree = 0;    // transition window: no opinion
                }
            }
        }

        // 32.44: SELF-CORRECTION. A toggle we track by counting presses is
        // one missed or spurious event away from being inverted for the rest
        // of the session - which is exactly what a bad learner adoption did.
        // So check our own answer against the world: 400 ms after a crouch
        // press the view should have gone DOWN if we now believe we are
        // crouched, and UP if we believe we stood. If it went the other way we
        // are inverted, and the honest thing is to flip rather than to keep
        // insisting. Only judged on clean evidence, same rules as the learner.
        if (g_crouchChkPend && (tms - g_crouchChkMs) > 400.0) {
            g_crouchChkPend = false;
            float dx = cx - g_crouchXPress, dy = cy - g_crouchYPress;
            float horiz = sqrtf(dx*dx + dy*dy);
            float d = cz - g_crouchZPress;            // + = the view rose
            if (camOk && g_crouchQuietP && horiz < 40.0f &&
                (d > 8.0f || d < -8.0f)) {
                bool worldSaysCrouched = (d < 0.0f);
                if (worldSaysCrouched != g_crouchBtnSt && !CylTruthLive()) {
                    g_crouchBtnSt = worldSaysCrouched;
                    Log("crouch/btn: RESYNC - view moved %+.1f uu, so we are"
                        " %s (we had it backwards)", d,
                        worldSaysCrouched ? "CROUCHED" : "standing");
                }
            }
        }

        // Hold-style or toggle-style? Measure it. A quarter second after the
        // button comes up, is the view still lower than it was at the press?
        // If yes the game latched the crouch and the button is a toggle. Two
        // agreeing cycles lock it in; assuming either way has cost two rounds
        // already.
        if (g_crouchRelPend && !g_crouchTogLock &&
            (tms - g_crouchRelMs) > 400.0) {
            g_crouchRelPend = false;
            float rdx = cx - g_crouchXPress, rdy = cy - g_crouchYPress;
            bool clean = camOk && g_crouchQuietP &&
                         sqrtf(rdx*rdx + rdy*rdy) < 40.0f;
            if (cz != 0.0f && g_crouchZPress != 0.0f && clean) {
                // 32.43: same sign bug as the learner. Hold-style means the
                // view RETURNS to where it was at the press once you let go;
                // toggle-style means it stays moved - up or down, depending on
                // which half of the toggle you just pressed. So the test is
                // magnitude, not direction. The old version voted +1/-1 in
                // strict alternation on a toggle button and could never lock.
                float drop = g_crouchZPress - cz;
                if (drop < 0.0f) drop = -drop;
                int vote = (drop > 8.0f) ? 1 : -1;    // stayed moved => toggle
                if ((vote > 0) == (g_crouchTogVote > 0)) g_crouchTogVote += vote;
                else g_crouchTogVote = vote;
                Log("crouch/btn: view settled %.1f uu from the press (vote %d)",
                    drop, g_crouchTogVote);
                if (g_crouchTogVote >= 2 || g_crouchTogVote <= -2) {
                    bool tog = (g_crouchTogVote > 0);
                    // Do NOT re-seed g_crouchBtnSt here. The toggle flip has
                    // been tracking correctly since the first press; guessing
                    // a fresh value from one camera sample can only desync it.
                    g_crouchToggle = tog;
                    g_crouchTogLock = true;
                    Log("crouch/btn: MEASURED - the crouch button is %s-style",
                        tog ? "TOGGLE" : "hold");
                }
            }
        }
    }

    // Pick the source, then DEBOUNCE it. Every candidate signal so far has
    // been noisy at the 30-500 ms scale while a real crouch lasts seconds, so
    // a state must hold for CrouchHoldMs before the hands are allowed to move.
    // Even a correct signal wants this: nobody wants the arms snapping pose
    // because a flag flickered for two frames.
    bool raw;
    if (g_crouchSrc == 3)                         raw = g_crouchBtnSt;
    else if (g_crouchSrc == 2)                    raw = wantsSays;
    else if (g_crouchSrc == 1 && g_actorLocFound) raw = g_eyeCrouched;
    else                                          raw = flagSays;
    if (g_crouchSrc == 2 && !g_bWantsCrFound)     raw = flagSays;

    double tnow = MaimNowMs();
    if (raw != g_crouchPend) { g_crouchPend = raw; g_crouchPendMs = tnow; }
    bool settled = g_pawnCrouched;
    if (g_crouchPend != g_pawnCrouched &&
        (tnow - g_crouchPendMs) >= (double)g_crouchHoldMs)
        settled = g_crouchPend;

    bool now = g_crouchForce ? true : settled;    // overlay override for tuning
    if (now != g_pawnCrouched) {
        g_pawnCrouched = now;
        float cylH = PawnCollisionHeight();
        Log("crouch: pawn is %s - hand trim '%s' now active%s | collision "
            "cylinder height %.1f uu (the number that decides tables)",
            now ? "CROUCHED" : "standing",
            now ? "stand + crouch offset" : "stand",
            g_crouchForce ? " (FORCED by the overlay)" : "", cylH);
    }
}


// ----------------------------------------------------------------------------
// The physical-crouch pulse (moved out of the pad path, 41.1)
// ----------------------------------------------------------------------------
// This used to live inside UpdateVirtualPad, which made "the gamepad is dead"
// and "physical crouch misfired" the same eighty lines. It is motion crouch,
// so it belongs here; the pad calls it through Callbacks::shape_buttons and
// ORs XINPUT_GAMEPAD_B while it says true.
//
// Under [Mode] GamepadOnly=1 motion crouch is OFF, g_crouchHeld never moves,
// and this returns false for the whole run. That zero is BY DESIGN.
//
// 32.87: physical crouch presses THE CROUCH BUTTON. It used to hold
// LEFT_THUMB, which 32.42-32.44 measured is NOT crouch - the game's crouch is
// B (0x2000), and it is a TOGGLE, so holding anything is wrong twice over.
// Pulse B for ~120 ms on each stance transition.
// 32.95: THE REFEREE IS WITHDRAWN. Making the player's real-world stance the
// authority over the toggle is exactly backwards for a SEATED player; every
// button crouch was cancelled within a second by "physical up". Back to
// EDGE-triggered: a sustained real-world stance CHANGE sends one press, and
// otherwise the button owns the toggle completely.
// 33.5: WHO owns the current crouch. A seated player's ordinary chair posture
// crosses the duck/rise thresholds all day, and a "rise" while button-crouched
// was a legal un-crouch pulse: Corvo stood up under the boat the moment the
// user sat back. The rule: PHYSICAL CROUCH MAY ONLY UNDO ITS OWN CROUCHES.
static bool CrouchPulseTick(bool userStealth)
{
    static bool   crPulsed      = false;  // stance we last SENT
    static bool   crSeen        = false;  // stance we last saw
    static double crStableSince = 0.0;
    static double crLastPulse   = 0.0;
    static double crUntil       = 0.0;
    static bool   crPhysOwned   = false;
    double crNow = MaimNowMs();
    if (g_crouchHeld != crSeen) {
        crSeen = g_crouchHeld;
        crStableSince = crNow;
    }
    {   // any button press claims the stance for the button
        static bool stealthWas2 = false;
        if (userStealth && !stealthWas2) crPhysOwned = false;
        stealthWas2 = userStealth;
    }
    bool inGameplay = !g_inMenu;
    bool aiming = dvr::pad::wheel_held() || (crNow - g_blkAimSeen) < 800.0;
    if (crSeen != crPulsed &&
        (crNow - crStableSince) > 300.0 &&
        (crNow - crLastPulse)  > 1200.0 &&
        !aiming && inGameplay && !userStealth) {
        // 32.98: THE PULSE ASKS THE CYLINDER FIRST. Ducking your real head
        // while already game-crouched - what every human does going under a
        // boat hull - used to fire a pulse at a toggle and STAND CORVO UP
        // under the boat. A duck-pulse only fires if the cylinder says
        // standing, a rise-pulse only if it says crouched.
        // 33.0: THREE stances, not two. 87.5 standing, 65 crouched - and 33
        // inside vents, a forced crawl the game controls. Below normal crouch
        // height the stance is not ours to change.
        float chP = PawnCollisionHeight();
        bool sendIt = true;
        if (chP > 1.0f) {
            bool gameCrouched = chP < 76.0f;
            bool forcedLow    = chP < 50.0f;     // vents/crawls
            sendIt = !forcedLow &&
                   ( (crSeen && !gameCrouched)   // duck: need standing
                  || (!crSeen && gameCrouched    // rise: need crouch
                      && crPhysOwned));          //   ...that WE made
        }
        crPulsed = crSeen;             // stance acknowledged either way
        if (sendIt) {
            crPhysOwned = crSeen;      // our crouch now / undone
            crLastPulse = crNow;
            crUntil     = crNow + 120.0;
            Log("crouch: physical %s (sustained) -> pulsing B (cylinder %.1f)%s",
                crSeen ? "DOWN" : "up", chP,
                crSeen ? " [physical owns this crouch]" : "");
        } else {
            Log("crouch: physical %s but the game is already there "
                "(cylinder %.1f) - no pulse", crSeen ? "DOWN" : "up", chP);
        }
    }
    return crNow < crUntil;
}
