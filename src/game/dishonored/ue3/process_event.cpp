// game/dishonored/ue3/process_event.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// Called for every ProcessEvent. Must stay cheap: once a class is known it is
// a single pointer compare, and unknown classes are string-checked once each.
static void PeLatch(void* obj)
{
    if (!obj || ((uintptr_t)obj & 3)) return;
    if (!RangeReadable(obj, kClassOff + 4)) return;
    void* cls = *(void**)((uint8_t*)obj + kClassOff);
    if (!cls) return;
    if (cls == g_clsCtrl) { g_peCtrl = (uint8_t*)obj; return; }
    if (cls == g_clsPawn) {
        if (g_pePawn != (uint8_t*)obj) {          // 38.68: NEW pawn = fresh
            g_pePawn = (uint8_t*)obj;             // spawn/load - restart the
            g_fbPawnMs = MaimNowMs();             // fallback's hold-off
            g_fbPvrSince = 0;
        }
        return;
    }

    static void* rejected[64];
    static int   rejN = 0;
    for (int i = 0; i < rejN; i++) if (rejected[i] == cls) return;
    if (rejN < 64) rejected[rejN++] = cls; else rejN = 0;   // wrap: re-check later

    const char* cn = ObjClassName((uint8_t*)obj);
    if (!cn) return;
    if (strstr(cn, "PlayerController")) {
        g_clsCtrl = cls; g_peCtrl = (uint8_t*)obj;
        Log("handmesh: latched controller '%s' @ %p (from the event stream)", cn, obj);
    } else if (strstr(cn, "PlayerPawn") && !strstr(cn, "Proxy") &&
               !strstr(cn, "Tweaks") && !strstr(cn, "Specific")) {
        g_clsPawn = cls; g_pePawn = (uint8_t*)obj;
        g_fbPawnMs = MaimNowMs(); g_fbPvrSince = 0;   // 38.68: fresh pawn
        Log("handmesh: latched pawn '%s' @ %p (from the event stream)", cn, obj);
    }
}


extern "C" void __cdecl PeHandler(void* obj, void* a1, void* a2, void* a3)
{
    InterlockedIncrement(&g_peCalls);

    PeLatch(obj);   // the engine tells us who the real actors are
    // 32.8: while the blink window is open, note which script events fire ON a
    // candidate. Pointer compares only - no class-name lookup on this path.
    if (g_bpGo && obj) {
        for (int i = 0; i < g_bpN; i++) {
            if (g_bpObj[i] != obj) continue;
            uint8_t* f = (uint8_t*)a1;
            if (!f || ((uintptr_t)f & 3) || !RangeReadable(f, kNameOff + 8)) break;
            uint32_t nidx = *(uint32_t*)(f + kNameOff);
            int slot = -1;
            for (int e = 0; e < g_bpEvtN; e++) if (g_bpEvt[e] == nidx) { slot = e; break; }
            if (slot >= 0) { g_bpEvtC[slot]++; break; }
            // 32.10: the pawn and controller are candidates now, and they
            // dispatch hundreds of unrelated events a second. Only names that
            // could plausibly be the aim earn one of the 48 slots.
            const char* fn = RealName(nidx);
            if (!fn) break;
            if (!strstr(fn, "Blink") && !strstr(fn, "Power") &&
                !strstr(fn, "Teleport") && !strstr(fn, "Reach") &&
                !strstr(fn, "Aim") && !strstr(fn, "Target") &&
                !strstr(fn, "Trace") && !strstr(fn, "Reticle") &&
                !strstr(fn, "Traversal")) break;
            if (g_bpEvtN < 48) { g_bpEvt[g_bpEvtN] = nidx; g_bpEvtC[g_bpEvtN] = 1; g_bpEvtN++; }
            break;
        }
    }
    IntroSkipApply();  // 38.69: jump past the broken boat arrival, once
    DvrConsoleApply(); // the seam's `console <text>` runs here, on the script lane
    FovLeverApply();   // 30.50: outrun the engine's per-tick FOV recompute
    // 41.0: the per-eye camera seam, same lane and cadence as the lever. The
    // lever only revalidates the camera object while it is armed, so the seam
    // keeps its own liveness check while it has something to write (the first
    // eyetest ran with a null camera for all six candidates - lever off).
    if (dvr::camera::eyetest_active() || dvr::camera::eye() != 0) {
        static LONG camReval = 0;
        if (g_camObj && !CamAlive()) g_camObj = NULL;
        if (!g_camObj && (InterlockedIncrement(&camReval) & 31) == 0) FindLiveCamera();
    }
    dvr::camera::eyetest_script_tick(g_camObj);   // the write-point instrument
    dvr::camera::apply_eye_offset(g_camObj);      // the eye offset (aer/reentry)
    BlinkTestApply();  // 32.14: same lane, same reason
    SkcRotApply();     // 32.1: same trick for the hand rotators
    BoneWigApply();    // 30.62: which bone bank does the renderer read
    if (g_sbWritePoint == 0) SbApply("script");   // 30.83 oracle, tick-time lane

    // fast path: the view-rotation event, every frame
    // 38.77: the HAND DRIVE lives on this lane and used to sit behind the
    // same g_rotInject gate as head injection - so anything that switched
    // native head tracking off (the old watchdog; [HeadTrack] Native=0)
    // silently stopped driving the hands too ("hands weren't working").
    // The event match no longer depends on the flag; only the head write does.
    if (g_idxViewRot != 0xffffffffu) {
        uint8_t* f = (uint8_t*)a1;
        if (f && !((uintptr_t)f & 3) && RangeReadable(f, kNameOff + 8) &&
            *(uint32_t*)(f + kNameOff) == g_idxViewRot) {
            InterlockedIncrement(&g_pvrHits);   // 30.37b: head-write telemetry
            // 38.85 THE PITCH COIN FLIP, finally. ProcessViewRotation is
            // dispatched by SEVERAL objects each frame: the PlayerController
            // (whose rotator the engine USES) and its camera modifiers
            // (CameraShake et al - their out-rotator is discarded while the
            // modifier is inactive). The one-write-per-frame gate in
            // ApplyHeadToViewRotation bound our head write to WHICHEVER
            // fired FIRST, and that order is set by the modifier list
            // rebuilt at every level load - a per-load coin flip. Machines
            // that ordered controller-first always worked (this dev rig);
            // machines that shuffled got "pitch dead, writes landing"
            // (rotator values track the head IN THE DISCARDED PARMS), and
            // a quickload flipped the state - measured by a user: F9
            // toggled "look works" <-> on every reload. The write now goes
            // ONLY to the latched PlayerController's own dispatch; if no
            // controller is latched yet, the old first-dispatch behavior
            // carries until it is.
            // 38.86: the 38.85 controller gate was WRONG - the engine
            // dispatches this event only through the camera MODIFIER chain
            // (every vocab ever logged says CameraModifier_CameraShake), so
            // gating on the controller silenced head writes outright
            // (measured: "inverted and stays in the middle" = a static
            // camera under the world-locking layer). Ungated again; the
            // order-proofing lives inside ApplyHeadToViewRotation now.
            if (g_rotInject) ApplyHeadToViewRotation(a2);
            ApplyHandToMesh();
            return;
        }
    }
    // ---- HAND AIM, driven by the engine's own events ----
    // The differential watch showed two things: the fire input arrives as a
    // script event on the controller, and the arrow itself raises script events
    // the instant it exists. So we no longer scan GObjects hoping to catch a
    // projectile - the engine hands us the object, on the game thread, at the
    // moment it spawns. The redirect maths is the one that measured
    // dot(hand)=+1.00 back in 7.3; only the catching was ever unreliable.
    if (g_maimEnabled && obj) {
        uint8_t* f = (uint8_t*)a1;
        if (f && !((uintptr_t)f & 3) && RangeReadable(f, kNameOff + 8)) {
            uint32_t nidx = *(uint32_t*)(f + kNameOff);
            const char* nm = RealName(nidx);
            double now = MaimNowMs();

            // a fire input opens the window
            // 30.25: sword events (UsePrimaryItem / Attack) no longer arm the
            // window - a sword cannot spawn a projectile, and the armed
            // window's pool diff was the 120 ms/frame hitch behind "massive
            // lag spikes every time I swing". Left-hand fire still arms via
            // UseSecondaryItem/Fire here plus the trigger edge in
            // MotionAimTick.
            if (nm && (strstr(nm, "UseSecondaryItem") || strstr(nm, "Fire"))) {
                g_maimArmedUntil = now + 1500.0;
                g_maimArmMs = now;
            }
            // menu / dialog activity - mute melee injection (30.26)
            if (nm && (strstr(nm, "PauseMenu") || strstr(nm, "PauseGame") ||
                       strstr(nm, "CanLoadGame") || strstr(nm, "CanSaveGame") ||
                       strstr(nm, "SaveSlotInfos") || strstr(nm, "LoadGameClicked") ||
                       strstr(nm, "MessageBox") || strstr(nm, "BackToWindows") ||
                       strstr(nm, "Wheel_Open")))
                g_uiEventMs = now;
            // 30.30: authoritative menu open/close for SBS mono fallback.
            // Close checks run FIRST: "OnResumeGameClicked" contains neither
            // open keyword, but keep the order defensive anyway.
            if (nm) {
                // 34.0: OnLoadGameClicked was on the CLOSE list on the theory
                // that it meant "load confirmed, menu going away". The log
                // says otherwise: it fires when the SAVE-SLOT BROWSER opens
                // (click "Load Game" in the pause menu), so the browser was
                // rendered at gameplay fill and cut off. It belongs on the
                // OPEN list; if the flag lingers into the actual load, the
                // auto-start close and the stale-flag ghost test already
                // clean it up.
                if (strstr(nm, "MenuClosed") || strstr(nm, "ResumeGameClicked") ||
                    strstr(nm, "NewGameClicked")) {
                    if (g_menuOpen) { g_menuOpen = false; Log("menu: closed (%s)", nm); }
                    // (38.70: the NewGameClicked arm lived here and never
                    // fired - the skip now triggers on the intro boat's
                    // measured spawn position instead, in IntroSkipApply)
                } else if (strstr(nm, "OpenPauseMenu") || strstr(nm, "MessageBox") ||
                           strstr(nm, "CanLoadGame") || strstr(nm, "CanSaveGame") ||
                           strstr(nm, "SaveSlotInfos") || strstr(nm, "BackToWindows") ||
                           strstr(nm, "LoadGameClicked")) {
                    if (!g_menuOpen) { g_menuOpen = true; Log("menu: open (%s)", nm); }
                } else if (strstr(nm, "CloseJournal")) {
                    // 34.2: the journal/powers screen measured. It announces
                    // itself as Dis_ToggleJournal and leaves as CloseJournal
                    // (both straight from the 34.1 vocab log). Close first -
                    // "CloseJournal" must not fall into the open branch.
                    if (g_menuOpen) { g_menuOpen = false; Log("menu: closed (%s)", nm); }
                } else if (strstr(nm, "ToggleJournal")) {
                    if (!g_menuOpen) { g_menuOpen = true; Log("menu: open (%s)", nm); }
                } else if (strstr(nm, "Shop") || strstr(nm, "Upgrade") ||
                           strstr(nm, "PurchasesList")) {
                    // 34.5: Req_PurchasesList is the shop's BUY screen - the
                    // 34.4 evt-vocab log finally named it (it fires the
                    // moment the shop opens; Req_UpgradesList was only the
                    // upgrade tab). OnShopClosed still ends it below.
                    // 34.1: the SHOP / UPGRADE screen (Piero) never matched any
                    // known menu event, so it rendered at gameplay fill and was
                    // cut off. Names carrying Close/Exit/Leave end it; any
                    // other shop/upgrade event means the screen is up. If the
                    // close name never fires, the stale-flag ghost test clears
                    // the flag ~1.5 s after gameplay dispatches resume.
                    if (strstr(nm, "Close") || strstr(nm, "Exit") ||
                        strstr(nm, "Leave")) {
                        if (g_menuOpen) { g_menuOpen = false; Log("menu: closed (%s)", nm); }
                    } else {
                        if (!g_menuOpen) { g_menuOpen = true; Log("menu: open (%s)", nm); }
                    }
                }
                // 34.1 diagnostic: learn the game's UI event vocabulary. Each
                // DISTINCT name that smells like a full-screen UI is logged
                // once per launch, so a screen we misclassify hands us its
                // real names in the very next log. Cheap: small ring of seen
                // names, and most events fail the strstr batch instantly.
                // 34.4: GUESSING IS OVER. A 22-second shop visit fired NOT
                // ONE event containing any of seventeen guessed words - the
                // only hit was Req_UpgradesList when the upgrade tab was
                // clicked. So: log every script event name ONCE on first
                // sight, filtered to the shapes all known UI events take
                // (Dis_*, On*, Req_*). One shop visit hands us its real
                // vocabulary; first-seen names timestamped at shop entry are
                // the candidates. Dedup is by FName INDEX - an int compare,
                // cheaper than the strstr batch this block already ran.
                // 34.5 diagnostic: BLOCK detection groundwork. "Versus" is
                // the game's name for sword blocking (Dis_VersusAlt surfaced
                // in evt-vocab). Log every occurrence, rate-limited, to learn
                // its press/release rhythm - then a standing-block trim can
                // key off it.
                // (38.47-38.53: the conversation window that parked the wrist
                // HUD lived here; the wrist HUD went with the fork in 41.0)
                if (!strcmp(nm, "OnToggleCinematicMode")) {   // 38.65
                    g_cineNow = !g_cineNow;
                    if (g_cineNow) g_cineOnMs = MaimNowMs();
                    Log("cine: %s - synthesized inputs %s",
                        g_cineNow ? "ON" : "off",
                        g_cineNow ? "PARKED (pause still works)" : "live");
                }
                // 38.79: the game is exiting for real (PreExit fires only on
                // process shutdown, never on quit-to-menu). Stand every VR
                // path down NOW so teardown can't call through freed memory
                // (the measured exit crash: EIP dededede after PreExit).
                if (!strcmp(nm, "PreExit") &&
                    !InterlockedCompareExchange(&g_gameExiting, 0, 0)) {
                    InterlockedExchange(&g_gameExiting, 1);
                    dvr::frame::set_exiting();
                    Log("shutdown: game PreExit - VR paths standing down");
                    LogFlush();          // 38.90: buffered log - land it now
                    // 41.0: the runtime layer's session teardown hooks in
                    // here (40.2's XrPaceStop joined the old pace thread).
                }
                // 38.67 BOAT-DEATH FORENSICS - log only, no behavior change.
                // Three runs died identically with three theories eliminated
                // (synthesized inputs parked, keyhole untouched, real movie
                // files restored). The event log names CLASSES but not which
                // OBJECT died or WHERE, so every event in the measured death
                // chain now prints the instance name, pointer and location,
                // and the volume events decode the pawn argument. One run of
                // this says who falls, from where, and whether it is the
                // player pawn (g_pePawn) or a scripted stand-in.
                // 38.90: the 38.67 boat-death forensics are DEV-ONLY now.
                // They fire on BaseChange/volume events - i.e. constantly,
                // once per NPC per transition - and each one cost a class
                // lookup, a name lookup and a log line (which used to be a
                // disk flush). That burst was the microstutter. The boat is
                // handled by the intro skip; turn [Overlay] DevTools=1 on
                // to get them back for the real seat-in fix.
                if (g_ovlDev &&
                   (!strcmp(nm, "PawnEnteredVolume")  ||
                    !strcmp(nm, "PawnLeavingVolume")  ||
                    !strcmp(nm, "ChooseAndTriggerDeathEvent") ||
                    !strcmp(nm, "PlayDying")          ||
                    !strcmp(nm, "NotifyKilled")       ||
                    !strcmp(nm, "PreventDeath")       ||
                    !strcmp(nm, "BaseChange")         ||
                    !strcmp(nm, "Destroyed")          ||
                    !strcmp(nm, "OnToggleHidden")     ||
                    !strcmp(nm, "OnNPCMarkForVanish") ||
                    !strcmp(nm, "Dis_ExitKeyhole")    ||
                    !strcmp(nm, "OnObjectiveAction")  ||
                    !strcmp(nm, "OnToggleCinematicMode"))) {
                    char loc[64]; strcpy(loc, "loc=?");
                    uint8_t* ob = (uint8_t*)obj;
                    if (g_actorLocFound && RangeReadable(ob + g_actorLocOff, 12)) {
                        const float* L = (const float*)(ob + g_actorLocOff);
                        if (L[0] > -1e7f && L[0] < 1e7f && L[2] > -1e7f && L[2] < 1e7f)
                            _snprintf(loc, 63, "loc=(%.0f,%.0f,%.1f)", L[0], L[1], L[2]);
                    }
                    const char* inm = RangeReadable(ob + kNameOff, 4)
                                    ? RealName(*(uint32_t*)(ob + kNameOff)) : NULL;
                    const char* ocn = ObjClassName(ob);
                    Log("boatf: %-26s on %s '%s' @%p %s%s",
                        nm, ocn ? ocn : "?", inm ? inm : "?", (void*)ob, loc,
                        (ob == g_pePawn) ? "  <== THE PLAYER PAWN" : "");
                    // the volume events carry the pawn that moved as arg 0
                    if (!strcmp(nm, "PawnEnteredVolume") ||
                        !strcmp(nm, "PawnLeavingVolume")) {
                        uint8_t* ap = (a2 && !((uintptr_t)a2 & 3) &&
                                       RangeReadable(a2, 4)) ? *(uint8_t**)a2 : NULL;
                        if (ap && !((uintptr_t)ap & 3) &&
                            RangeReadable(ap, kClassOff + 4)) {
                            char al[64]; strcpy(al, "loc=?");
                            if (g_actorLocFound && RangeReadable(ap + g_actorLocOff, 12)) {
                                const float* L2 = (const float*)(ap + g_actorLocOff);
                                if (L2[0] > -1e7f && L2[0] < 1e7f)
                                    _snprintf(al, 63, "loc=(%.0f,%.0f,%.1f)",
                                              L2[0], L2[1], L2[2]);
                            }
                            const char* an = RangeReadable(ap + kNameOff, 4)
                                           ? RealName(*(uint32_t*)(ap + kNameOff)) : NULL;
                            const char* acn = ObjClassName(ap);
                            Log("boatf:   the pawn: %s '%s' @%p %s%s",
                                acn ? acn : "?", an ? an : "?", (void*)ap, al,
                                (ap == g_pePawn) ? "  <== THE PLAYER PAWN" : "");
                        }
                    }
                }
                if (strstr(nm, "Versus")) {
                    g_lastVersusMs = MaimNowMs();   // 38.51: see dialog gate
                    static double vLastMs = 0.0;
                    static int    vSupp   = 0;
                    double vNow = MaimNowMs();
                    if (vNow - vLastMs > 100.0) {
                        if (vSupp) { Log("block-evt: (%d more suppressed)", vSupp); vSupp = 0; }
                        Log("block-evt: %s", nm);
                        vLastMs = vNow;
                    } else vSupp++;
                }
                // 36.6: FINISHER capture via substrings - DROWNED in
                // OnParticleSystemFinished ("Finish" matched it, 65 hits, the
                // suppression counter hid whatever else fired). Keep only the
                // specific family, particle noise excluded.
                if ((strstr(nm, "Assassin") || strstr(nm, "Takedown") ||
                     strstr(nm, "Execut")   || strstr(nm, "Fatal")    ||
                     strstr(nm, "Kill")) && !strstr(nm, "Particle")) {
                    static double fLastMs = 0.0;
                    static int    fSupp   = 0;
                    double fNow = MaimNowMs();
                    if (fNow - fLastMs > 100.0) {
                        if (fSupp) { Log("finisher?: (%d more suppressed)", fSupp); fSupp = 0; }
                        Log("finisher?: %s", nm);
                        fLastMs = fNow;
                    } else fSupp++;
                }
                // 36.7: FINISHER capture take 2 - the choreography must run
                // ON THE PLAYER. While the overlay toggle is on, log every
                // DISTINCT event the pawn/controller dispatch (per-name 2 s
                // dedup). Pointer compares only; a few kills with this on
                // hand us the start/end pair the gate needs.
                if (g_finCapOn && (obj == (void*)g_fpPawn ||
                                   obj == (void*)g_pcObj)) {
                    static uint32_t fcIdx[96];
                    static double   fcLast[96];
                    static int      fcN = 0;
                    double fn2 = MaimNowMs();
                    int slot = -1;
                    for (int q2 = 0; q2 < fcN; q2++)
                        if (fcIdx[q2] == nidx) { slot = q2; break; }
                    if (slot < 0 && fcN < 96) {
                        slot = fcN++; fcIdx[slot] = nidx; fcLast[slot] = 0.0;
                    }
                    if (slot >= 0 && fn2 - fcLast[slot] > 2000.0) {
                        fcLast[slot] = fn2;
                        Log("fincap: %s %s", obj == (void*)g_fpPawn ?
                            "pawn" : "pc  ", nm);
                    }
                }
                {
                    static int32_t seenIdx[512];
                    static int     seenIdxN = 0;
                    bool prefixOk =
                        (nm[0]=='D' && nm[1]=='i' && nm[2]=='s' && nm[3]=='_') ||
                        (nm[0]=='O' && nm[1]=='n') ||
                        (nm[0]=='R' && nm[1]=='e' && nm[2]=='q' && nm[3]=='_');
                    if (prefixOk) {
                        bool seen = false;
                        for (int si = 0; si < seenIdxN; si++)
                            if (seenIdx[si] == (int32_t)nidx) { seen = true; break; }
                        if (!seen && seenIdxN < 512) {
                            seenIdx[seenIdxN++] = (int32_t)nidx;
                            Log("evt-vocab: %s", nm);
                        }
                    }
                }
            }

            // any projectile that stirs while the window is open gets steered
            if (now < g_maimArmedUntil && !((uintptr_t)obj & 3) &&
                RangeReadable(obj, kClassOff + 4)) {
                const char* cn = ObjClassName((uint8_t*)obj);
                // 38.53: MAGIC AIM, measured this time. The 38.52 all-powers
                // gate never fired once (zero magic-aim lines) - the 0xbf5e4f
                // detour is inside blink's OWN function, not shared. So the
                // powers' aim has to be found, not assumed. Casting with the
                // trigger arms this very window; log the class of EVERYTHING
                // that stirs in it, first-seen once per class, so one cast of
                // possession / swarm / windblast names its spawned object.
                // If those classes are projectile-family, the proven MaimCatch
                // steering picks them up by adding them to the filter below.
                // 38.54: 38.53's ring SATURATED - 22 of its 24 slots went to
                // ambient every-frame classes (PlayerController, MusicManager,
                // Canvas...) in the first two seconds, before any cast. So the
                // casts landed in a full ring and the measurement said nothing.
                // Filter to power-shaped names, widen the ring.
                if (cn && *cn &&
                    (strstr(cn, "Proj")    || strstr(cn, "Possess") ||
                     strstr(cn, "Swarm")   || strstr(cn, "Rat")     ||
                     strstr(cn, "Blast")   || strstr(cn, "Wind")    ||
                     strstr(cn, "Power")   || strstr(cn, "Missile") ||
                     strstr(cn, "Bolt")    || strstr(cn, "Spring"))) {
                    static uint32_t stirSeen[64]; static int stirN = 0;
                    uint32_t sh = 2166136261u;
                    for (const char* q = cn; *q; q++) { sh ^= (uint8_t)*q; sh *= 16777619u; }
                    bool seen = false;
                    for (int si = 0; si < stirN; si++) if (stirSeen[si] == sh) seen = true;
                    if (!seen && stirN < 64) { stirSeen[stirN++] = sh;
                        Log("maim/stir: %s moved during the fire window", cn); }
                }
                // 38.55 SWARM AIM. Measured (3854stir log): devouring
                // swarm spawns a DisRatSwarm the instant it is cast, inside
                // this very fire window. Same law as blink: keep the game's
                // own chosen DISTANCE, swap only the DIRECTION to the
                // controller ray - horizontally, keeping the game's Z, so
                // the rats still land on the floor the game picked.
                // Possession spawned NOTHING (it is target-acquisition, not
                // a spawned object) - it is NOT handled here and saying
                // otherwise would be a lie. [MotionAim] SwarmAim=0 reverts.
                if (g_swarmAim && cn && !strcmp(cn, "DisRatSwarm") &&
                    g_pePawn && g_actorLocFound &&
                    RangeReadable(obj, g_actorLocOff + 12) &&
                    RangeReadable(g_pePawn + g_actorLocOff, 12)) {
                    static uint8_t* swWas = NULL; static double swAt = 0.0;
                    if (obj != (void*)swWas || now - swAt > 1000.0) {
                        swWas = (uint8_t*)obj; swAt = now;
                        float* sl = (float*)((uint8_t*)obj + g_actorLocOff);
                        const float* pl =
                            (const float*)(g_pePawn + g_actorLocOff);
                        float dx = sl[0]-pl[0], dy = sl[1]-pl[1];
                        float dHor = sqrtf(dx*dx + dy*dy);
                        float rel[3], dir[3];
                        if (dHor > 30.0f && MaimHandRel(rel)) {
                            MaimDirFromView(g_viewYawRad, g_viewPitchRad,
                                            rel, dir);
                            float dh = sqrtf(dir[0]*dir[0] + dir[1]*dir[1]);
                            if (dh > 0.15f) {
                                float ox = sl[0], oy = sl[1];
                                sl[0] = pl[0] + (dir[0]/dh) * dHor;
                                sl[1] = pl[1] + (dir[1]/dh) * dHor;
                                Log("maim: SWARM relocated (%.0f,%.0f) -> "
                                    "(%.0f,%.0f)  dist %.0fuu kept, Z kept",
                                    ox, oy, sl[0], sl[1], dHor);
                            }
                        }
                    }
                }
                if (cn && (!strncmp(cn, "DisProjectile", 13) || !strcmp(cn, "DisBullet") ||
                           !strncmp(cn, "DisGrenade", 10))) {
                    static uint8_t* lastObj = NULL;
                    static double   lastAt  = 0;
                    if (obj != lastObj || now - lastAt > 250.0) {
                        lastObj = (uint8_t*)obj; lastAt = now;
                        float rel[3];
                        if (MaimHandRel(rel) && RangeReadable(obj, 0x220))
                            MaimCatch((uint8_t*)obj, cn, rel, now);
                    }
                }
            }
        }
    }

    // ---- differential weapon watch (F6) ----
    if (g_wwPhase) {
        uint8_t* f = (uint8_t*)a1;
        if (f && !((uintptr_t)f & 3) && RangeReadable(f, kNameOff + 8)) {
            uint32_t nidx = *(uint32_t*)(f + kNameOff);
            if (g_wwPhase == 2) {            // count EVERYTHING in the window
                LONG hl = g_wwHistLen; if (hl > 256) hl = 256;
                int hi = -1;
                for (LONG i = 0; i < hl; i++)
                    if (g_wwHistN[i] == nidx) { hi = (int)i; break; }
                if (hi >= 0) g_wwHistC[hi]++;
                else if (hl < 256) {
                    LONG s = InterlockedIncrement(&g_wwHistLen) - 1;
                    if (s < 256) {
                        g_wwHistN[s] = nidx; g_wwHistC[s] = 1;
                        uint32_t ci = 0;
                        if (obj && !((uintptr_t)obj & 3) &&
                            RangeReadable((uint8_t*)obj, kClassOff + 4)) {
                            uint8_t* cls = *(uint8_t**)((uint8_t*)obj + kClassOff);
                            if (cls && !((uintptr_t)cls & 3) &&
                                RangeReadable(cls, kNameOff + 4))
                                ci = *(uint32_t*)(cls + kNameOff);
                        }
                        g_wwHistCls[s] = ci;
                    }
                }
            }
            LONG n = g_wwBaseN; if (n > 1024) n = 1024;
            bool known = false;
            for (LONG i = 0; i < n; i++) if (g_wwBase[i] == nidx) { known = true; break; }
            if (!known && n < 1024) {
                LONG s = InterlockedIncrement(&g_wwBaseN) - 1;
                if (s < 1024) g_wwBase[s] = nidx;
                if (g_wwPhase == 2 && g_wwLines < 200) {
                    InterlockedIncrement(&g_wwLines);
                    const char* nm = RealName(nidx);
                    const char* on = NULL;
                    if (obj && !((uintptr_t)obj & 3) && RangeReadable(obj, kClassOff + 4))
                        on = ObjClassName((uint8_t*)obj);
                    Log("weapon: %-40s on %s", nm ? nm : "?", on ? on : "?");
                }
            }
        }
    }

    if (!g_peLogging && g_idxViewRot != 0xffffffffu) return;

    uint8_t* fn = AsUFunction(a1);
    int which = 1;
    if (!fn) { fn = AsUFunction(a3); which = 3; }
    if (!fn) { fn = AsUFunction(a2); which = 2; }
    if (!fn) return;                       // no function in any slot this call

    uint32_t idx = *(uint32_t*)(fn + kNameOff);
    uint32_t idx0 = *(uint32_t*)(fn + kNameOff);
    if (g_idxViewRot == 0xffffffffu) {          // still hunting for the view hook
        const char* nm0 = RealName(idx0);
        if (nm0 && !strcmp(nm0, "ProcessViewRotation")) {
            g_idxViewRot = idx0;
            Log("script: ProcessViewRotation found - head tracking now goes "
                "through the engine's own view hook");
        }
    }
    if (!g_peLogging) return;
    LONG n = g_peSeenN; if (n > 512) n = 512;
    for (LONG i = 0; i < n; i++) if (g_peSeen[i] == idx) return;
    if (n >= 512) { g_peLogging = false; return; }
    LONG slot = InterlockedIncrement(&g_peSeenN) - 1;
    if (slot >= 512) { g_peLogging = false; return; }
    g_peSeen[slot] = idx;

    const char* name = RealName(idx);
    if (name && !strcmp(name, "ProcessViewRotation") && g_idxViewRot == 0xffffffffu) {
        g_idxViewRot = idx;
        Log("script: ProcessViewRotation found - head tracking now goes through "
            "the engine's own view hook");
    }
    const char* on = NULL;
    if (obj && !((uintptr_t)obj & 3) && RangeReadable(obj, kClassOff + 4))
        on = ObjClassName((uint8_t*)obj);
    Log("script: %-40s on %-28s [arg%d]", name ? name : "?", on ? on : "?", which);
}


static bool InstallProcessEventHook()
{
    if (g_peInstalled) return true;
    uint8_t* at = (uint8_t*)kProcessEvent;
    if (!RangeReadable(at, 8)) { Log("script: ProcessEvent address unreadable"); return false; }
    // expected prologue: push ebp; mov ebp,esp; push -1   (exactly 5 bytes)
    if (!(at[0] == 0x55 && at[1] == 0x8B && at[2] == 0xEC && at[3] == 0x6A && at[4] == 0xFF)) {
        Log("script: ProcessEvent prologue mismatch (%02x %02x %02x %02x %02x) - not hooking",
            at[0], at[1], at[2], at[3], at[4]);
        return false;
    }
    g_peStub = (uint8_t*)VirtualAlloc(NULL, 96, MEM_COMMIT | MEM_RESERVE,
                                      PAGE_EXECUTE_READWRITE);
    if (!g_peStub) { Log("script: stub alloc failed"); return false; }

    uint8_t s[96]; int n = 0;
    s[n++] = 0x9C;                                   // pushfd
    s[n++] = 0x60;                                   // pushad            (36 bytes on stack)
    // three pushes of [esp+0x30] walk backwards through arg3, arg2, arg1
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x30;  // push arg3
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x30;  // push arg2
    s[n++] = 0xFF; s[n++] = 0x74; s[n++] = 0x24; s[n++] = 0x30;  // push arg1
    s[n++] = 0x51;                                   // push ecx          = this
    s[n++] = 0xE8;                                   // call PeHandler
    { int32_t rel = (int32_t)((uintptr_t)&PeHandler - ((uintptr_t)g_peStub + n + 4));
      memcpy(s + n, &rel, 4); n += 4; }
    s[n++] = 0x83; s[n++] = 0xC4; s[n++] = 0x10;     // add esp,16
    s[n++] = 0x61;                                   // popad
    s[n++] = 0x9D;                                   // popfd
    s[n++] = 0x55;                                   // push ebp        \ the original
    s[n++] = 0x8B; s[n++] = 0xEC;                    // mov ebp,esp     | 5 bytes we
    s[n++] = 0x6A; s[n++] = 0xFF;                    // push -1         / displaced
    s[n++] = 0xE9;                                   // jmp back
    { int32_t rel = (int32_t)((kProcessEvent + 5) - ((uintptr_t)g_peStub + n + 4));
      memcpy(s + n, &rel, 4); n += 4; }
    memcpy(g_peStub, s, n);

    DWORD op;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) {
        Log("script: VirtualProtect failed"); return false;
    }
    at[0] = 0xE9;
    int32_t jrel = (int32_t)((uintptr_t)g_peStub - ((uintptr_t)at + 5));
    memcpy(at + 1, &jrel, 4);
    VirtualProtect(at, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 5);

    g_peInstalled = true;
    Log("script: ProcessEvent hooked at 0x%08x (stub %p) - listening for named events",
        (unsigned)kProcessEvent, (void*)g_peStub);
    return true;
}
