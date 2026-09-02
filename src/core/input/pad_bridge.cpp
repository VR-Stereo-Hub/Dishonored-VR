// core/input/pad_bridge.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static inline SHORT PadStick(float v)
{
    float a = fabsf(v);
    if (a < g_padDeadzone) return 0;
    float s = (a - g_padDeadzone) / (1.0f - g_padDeadzone);
    if (s > 1.0f) s = 1.0f;
    return (SHORT)((v < 0 ? -s : s) * 32767.0f);
}


static void UpdateVirtualPad()
{
    if (!g_padEnabled || !g_xrOn) { g_padActive = false; return; }
    MeleeTick();

    bool active = false;
    XINPUT_STATE xs; memset(&xs, 0, sizeof(xs));
    g_wheelHeld = false;   // re-established below when the wheel grip is held

    // 41.0: ONE input path. The controllers arrive as the runtime layer's
    // snapshot (published under g_xrCs) with the Touch semantics the Quest
    // bindings always had: left grip = power wheel, right grip = choke, A jump,
    // B stealth, X interact, Y/menu pause, stick clicks = sprint / health hold.
    // The OpenVR action set and the legacy wand path went with the OpenVR
    // backend; everything gameplay-side that only the OpenVR path used to run
    // (physical crouch pulses, the overlay pointer, the fire tracer, motion
    // aim) now runs for every controller.
    XrInpState in;
    if (g_xrCsInit) {
        EnterCriticalSection(&g_xrCs);
        in = g_xrInp;
        LeaveCriticalSection(&g_xrCs);
    } else memset(&in, 0, sizeof(in));
    if (in.active) {
        active = true;
        g_dbgRawMx = in.mv[0]; g_dbgRawMy = in.mv[1];  // 38.25 pre-shaping
        float mx = in.mv[0], my = in.mv[1], tx = in.lk[0], ty = in.lk[1];
        float hr = in.trigR, hl = in.trigL;
        WORD b = 0;
        static bool wheelWas = false, chokeWas = false;
        bool wheel = in.gripL > (wheelWas ? 0.7f : 0.9f);
        bool choke = in.gripR > (chokeWas ? 0.7f : 0.9f);
        wheelWas = wheel; chokeWas = choke;
        bool wheelHeld = wheel;
        g_wheelHeld = wheelHeld;
        if (wheelHeld)       b |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (choke)           b |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (in.a)            b |= XINPUT_GAMEPAD_A;       // jump
        bool userStealth = in.b;
        if (userStealth)     b |= XINPUT_GAMEPAD_B;       // stealth
        if (in.x)            b |= XINPUT_GAMEPAD_X;       // interact
        if (in.y || in.menu) b |= XINPUT_GAMEPAD_START;   // pause
        if (SprintBit(in.clkL)) b |= XINPUT_GAMEPAD_LEFT_THUMB; // SPRINT 38.28
        HealthElixirTick(in.clkR);                        // health hold
        {   // stick-click edges stay measured facts
            static bool sw = false, hw = false;
            if (in.clkL != sw) { sw = in.clkL;
                Log("act: L-stick click (sprint) %s", sw ? "DOWN" : "UP"); }
            if (in.clkR != hw) { hw = in.clkR;
                Log("act: R-stick click (health hold) %s", hw ? "DOWN" : "UP"); }
        }
        // 38.22: slide assist - B at full run = slide
        b = SlideAssist(b, userStealth, mx, my);
        // 32.87: physical crouch finally presses THE CROUCH BUTTON.
        // It held LEFT_THUMB, which 32.42-32.44 measured is NOT crouch -
        // the game's crouch is B (0x2000), and it is a TOGGLE, so holding
        // anything is wrong twice over. Pulse B for ~120 ms on each stance
        // transition: duck IRL -> one press -> pawn crouches; stand IRL ->
        // one press -> pawn stands. The roadmap called this fix cheap and
        // likely to just work; the pieces (detection, B, toggle semantics)
        // were all measured long ago and just never joined up.
        // 32.95: THE REFEREE IS WITHDRAWN. 32.94 made the player's
        // real-world stance the authority over the crouch toggle, and for
        // a SEATED player that is exactly backwards - the log shows every
        // button crouch being cancelled within a second by "physical up".
        // Physical crouch goes back to what 32.88 shipped and the user
        // liked: EDGE-triggered - a sustained real-world stance CHANGE
        // sends one press, and otherwise the button owns the toggle
        // completely. The one 32.94 improvement kept: gameplay is judged
        // by the fork's splice counter, not the menu flag that sticks.
        {
            static bool   crPulsed      = false;  // stance we last SENT
            static bool   crSeen        = false;  // stance we last saw
            static double crStableSince = 0.0;
            static double crLastPulse   = 0.0;
            static double crUntil       = 0.0;
            // 33.5: WHO owns the current crouch. A seated player's
            // ordinary chair posture - lean in to look, settle back -
            // crosses the duck/rise thresholds all day, and a "rise"
            // while button-crouched was a legal un-crouch pulse: Corvo
            // stood up under the boat the moment the user sat back. The
            // rule now: PHYSICAL CROUCH MAY ONLY UNDO ITS OWN CROUCHES.
            // Button crouch -> only the button stands you up; chair
            // movement is powerless. Physical duck -> physical rise
            // undoes it, the natural room-scale flow.
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
            bool aiming = g_wheelHeld ||
                          (crNow - g_blkAimSeen) < 800.0;
            if (crSeen != crPulsed &&
                (crNow - crStableSince) > 300.0 &&
                (crNow - crLastPulse)  > 1200.0 &&
                !aiming && inGameplay && !userStealth) {
                // 32.98: THE PULSE ASKS THE CYLINDER FIRST. Ducking your
                // real head while already game-crouched - which is what
                // every human does going under a boat hull - used to fire
                // a pulse at a toggle and STAND CORVO UP under the boat.
                // That was the "head getting stuck". A duck-pulse now
                // only fires if the cylinder says standing (87.5), a
                // rise-pulse only if it says crouched (65). A press that
                // would not change the stance is not sent at all. The
                // cylinder unreadable (mid-load): old behaviour, no gate.
                float chP = PawnCollisionHeight();
                bool sendIt = true;
                if (chP > 1.0f) {
                    // 33.0: THREE stances, not two. 87.5 standing, 65
                    // crouched - and 33 inside vents, a forced crawl the
                    // game controls. The rise-gate saw 33 < 76, said
                    // "crouched, ok to stand" and pressed the toggle
                    // INSIDE the vent - the reported jam. Below normal
                    // crouch height the stance is not ours to change:
                    // no pulses of any kind until the game hands it back.
                    bool gameCrouched = chP < 76.0f;
                    bool forcedLow    = chP < 50.0f;     // vents/crawls
                    sendIt = !forcedLow &&
                           ( (crSeen && !gameCrouched)   // duck: need standing
                          || (!crSeen && gameCrouched    // rise: need crouch
                              && crPhysOwned));          //   ...that WE made
                }
                crPulsed = crSeen;             // stance acknowledged either way
                if (sendIt) {
                    if (crSeen) crPhysOwned = true;    // our crouch now
                    else        crPhysOwned = false;   // undone
                    crLastPulse = crNow;
                    crUntil     = crNow + 120.0;
                    Log("crouch: physical %s (sustained) -> pulsing B "
                        "(cylinder %.1f)%s", crSeen ? "DOWN" : "up", chP,
                        crSeen ? " [physical owns this crouch]" : "");
                } else {
                    Log("crouch: physical %s but the game is already "
                        "there (cylinder %.1f) - no pulse",
                        crSeen ? "DOWN" : "up", chP);
                }
            }
            if (crNow < crUntil) b |= XINPUT_GAMEPAD_B;
        }
        // 31.1: only the RAY is computed here. The previous version called
        // ImGui::GetIO() from this thread, which runs before the overlay
        // context exists and alongside the render thread that owns it -
        // both a race and a null dereference waiting to happen, and the
        // likely reason clicking did nothing.
        {
            float rel[3];
            int ph = (g_ovlPtrHand >= 0 && g_ovlPtrHand <= 1) ? g_ovlPtrHand : 1;
            if (g_ovlPtrEnable && g_ovlVisible &&
                HandRelFull(ph, rel, NULL) && rel[2] > 0.25f) {
                g_ovlRayX = rel[0] / rel[2];
                g_ovlRayY = rel[1] / rel[2];
                g_ovlPtrValid = true;
                float trg = 0.0f;
                trg = ph ? hr : hl;
                g_ovlPtrDown = (trg > 0.4f);
            } else {
                g_ovlPtrValid = false;
                g_ovlPtrDown = false;
            }
        }
        xs.Gamepad.wButtons      = b;
        xs.Gamepad.sThumbLX      = PadStick(mx);
        xs.Gamepad.sThumbLY      = PadStick(my);
        xs.Gamepad.sThumbRX      = PadStick(tx);
        // pitch belongs to the head - EXCEPT in menus (stick navigates)
        // and while the power wheel is held open (stick points at wedges)
        xs.Gamepad.sThumbRY      = (g_inMenu || wheelHeld) ? PadStick(ty) : 0;
        xs.Gamepad.bRightTrigger = (BYTE)(hr * 255.0f);
        xs.Gamepad.bLeftTrigger  = (BYTE)(hl * 255.0f);
        if (MeleeActive()) xs.Gamepad.bRightTrigger = 255;   // sword swing
        static WORD lastB = 0xffff;
        if (b != lastB) { lastB = b; Log("pad: xbtn=0x%04x", b); }
        // 38.81: "can't use my right stick on Quest" - nothing logged the
        // right stick, so the loss point is invisible. One line per second
        // while it is pushed: raw action value vs what the game gets.
        // No line at all while pushing = the ACTION isn't delivering
        // (runtime/binding side); raw nonzero but RX 0 = our shaping ate
        // it (the flags say which); raw and RX both live = game-side.
        {
            static double rsLogMs = 0.0;
            double rsNow = MaimNowMs();
            if ((tx > 0.15f || tx < -0.15f || ty > 0.15f || ty < -0.15f) &&
                rsNow - rsLogMs > 1000.0) {
                rsLogMs = rsNow;
                Log("pad/rs: raw=(%.2f,%.2f) -> RX=%d RY=%d "
                    "(menu=%d/%d cine=%d wheel=%d)",
                    tx, ty, (int)xs.Gamepad.sThumbRX, (int)xs.Gamepad.sThumbRY,
                    (int)g_menuOpen, (int)g_inMenu,
                    (int)CineActive(), (int)g_wheelHeld);
            }
        }
        // Stage 7.2: projectile-spawn tracer - on a shot, find the bolt/
        // projectile the game just spawned and read the aim it was given.
        if (g_fireTraceEnabled) {
            static bool fireWas = false;
            bool fireNow = (hr > 0.55f) || (hl > 0.55f);
            bool edge = fireNow && !fireWas;
            fireWas = fireNow;
            SpawnTraceTick(edge);
        }
        // Stage 7.3: hand-aimed projectiles - steer the freshly spawned
        // bolt/bullet/grenade along the controller's ray.
        MotionAimTick(hl, hr);
    }

    // 30.59: MENU NAVIGATION SHAPING. The 30.58 telemetry showed the stick
    // arriving at 0.21 / 0.29 / 0.48 while scrolling - analog values wandering
    // around the game's navigation threshold, so steps fired erratically and
    // sometimes not at all ("skipping and delay"). Menus want discrete
    // presses, so inside a menu the stick becomes one clean full-scale pulse
    // per step: immediate on a fresh push, then a steady repeat while held,
    // neutral in between. Gameplay is untouched.
    // 30.60: gate on the SCRIPT signal alone. g_inMenu still folds in cursor
    // visibility, which goes stale during gameplay - and a stale flag here does
    // not merely pause the head-mouse (harmless), it turns the movement stick
    // into discrete pulses, i.e. no walking and no turning. Only the game's own
    // menu events may shape movement input.
    // 38.46: walking in the room pushes the movement stick, so the pawn goes
    // where you went - through the game's own collision, no wall clipping.
    // Never during a menu; that stick is navigation there.
    if (g_roomScaleCfg && active && !g_menuOpen && !g_inMenu &&
        !CineActive()) {
        float f = g_roomFwdM, rr = g_roomRightM;
        float len = sqrtf(f * f + rr * rr);
        if (len > g_roomDeadM && len > 0.0001f) {
            float m = (len - g_roomDeadM) * g_roomGain;
            if (m > g_roomMaxStick) m = g_roomMaxStick;
            float sx = (float)xs.Gamepad.sThumbLX / 32767.0f + (rr / len) * m;
            float sy = (float)xs.Gamepad.sThumbLY / 32767.0f + (f  / len) * m;
            float sl = sqrtf(sx * sx + sy * sy);
            if (sl > 1.0f) { sx /= sl; sy /= sl; }
            xs.Gamepad.sThumbLX = (SHORT)(sx * 32767.0f);
            xs.Gamepad.sThumbLY = (SHORT)(sy * 32767.0f);
            static double rlog = 0.0; double rn = MaimNowMs();
            if (rn - rlog > 2000.0) { rlog = rn;
                Log("roomscale: offset %.2f m (fwd %.2f, right %.2f) -> stick "
                    "%.2f", len, f, rr, m); }
        }
    }
    // 38.65: cinematic running - park the pad. Buttons except START are
    // dropped (a stray A or a chair-shuffle B pulse must never eject the
    // player from a scripted sequence again); sticks and triggers go to
    // zero. The game's own toggle giveth and taketh away.
    if (active && !g_menuOpen && !g_inMenu && CineActive()) {
        xs.Gamepad.wButtons &= XINPUT_GAMEPAD_START;
        xs.Gamepad.sThumbLX = 0; xs.Gamepad.sThumbLY = 0;
        xs.Gamepad.sThumbRX = 0; xs.Gamepad.sThumbRY = 0;
        xs.Gamepad.bLeftTrigger = 0; xs.Gamepad.bRightTrigger = 0;
    }
    // 38.82 THE DEAD RIGHT STICK. The menu flag can GHOST during gameplay
    // (measured in the user's own Quest log: "sbs: stereo (menu=1 ...)" -
    // stereo world rendering with the menu flag stuck on; the ghost
    // detector clears it, but only after seconds, and it can recur). While
    // the flag is up this block hard-zeroes the RIGHT stick and chops the
    // left into step pulses - pulses still move you, so it reads as "right
    // stick dead, left stick works". Menus are MONO by ground truth (the
    // fork's splice counter: world draws stop when a menu is really up -
    // the same signal the skc gates trust), so menu shaping now requires
    // the renderer to AGREE a menu is showing. A real menu is unchanged; a
    // ghost flag during stereo gameplay can no longer eat the sticks.
    if (g_menuOpen && active) {
        xs.Gamepad.sThumbLX = MenuStep(xs.Gamepad.sThumbLX, 0);
        xs.Gamepad.sThumbLY = MenuStep(xs.Gamepad.sThumbLY, 1);
        xs.Gamepad.sThumbRX = 0;   // one navigation axis only - a second one
        xs.Gamepad.sThumbRY = 0;   // double-steps the same list
    }

    // 38.25 crawlbox: mirror the delivered (post-shaping) movement stick for
    // the crouch/raw diag line. SHORT writes are atomic enough for a log.
    g_dbgOutLx = active ? xs.Gamepad.sThumbLX : 0;
    g_dbgOutLy = active ? xs.Gamepad.sThumbLY : 0;

    EnterCriticalSection(&g_padLock);
    if (active) {
        xs.dwPacketNumber = ++g_padPacket; g_padState = xs;
    } else if (g_padActive) {
        // controllers just dropped - neutralize so no button/stick sticks
        memset(&g_padState, 0, sizeof(g_padState));
        g_padState.dwPacketNumber = ++g_padPacket;
    }
    g_padActive = active;
    // 32.41: publish the crouch/sneak bit exactly as the game will receive it.
    // Taken from the final synthesized mask so it covers the action path, the
    // legacy wand path and the physical-crouch OR in one place.
    InterlockedExchange(&g_padBtnsPub, active ? (LONG)xs.Gamepad.wButtons : 0);
    InterlockedExchange(&g_sneakBtn,
        (active && (xs.Gamepad.wButtons & g_crouchBtnMask)) ? 1 : 0);
    LeaveCriticalSection(&g_padLock);

    // UE3 only re-evaluates cursor visibility on a REAL mouse event, so after
    // leaving a menu with the pad the cursor stays "visible" and our head-mouse
    // stays paused until the player touches the mouse. If the player is
    // actively driving the pad while we still think a menu is up, send a
    // net-zero 1-count mouse wiggle to make the game update its cursor state.
    if (g_inMenu) {
        // 30.56: REVERTED to the 30.54 nudge. Suppressing it during stick use
        // (30.55, to stop the menu highlight fighting) also stopped it from
        // clearing a stale "cursor visible" state - which is what lets head
        // tracking resume after a load - so holding the stick to walk could
        // leave head tracking dead for the rest of the session. Head tracking
        // outranks menu polish; the menu fix returns once it can be gated on a
        // trustworthy in-menu signal rather than on stick deflection.
        // 30.58: the nudge exists for exactly ONE case - a STALE cursor in
        // gameplay, where nothing else will clear the flag. Inside a real
        // (script-confirmed) menu it has no job and only causes harm: each 1px
        // move re-homes the highlight under the cursor while the player
        // scrolls. So nudge only when the script says we are NOT in a menu.
        // (30.55 suppressed it by stick deflection instead, which also killed
        // the stale-cursor rescue and took head tracking with it.)
        bool padBusy = !g_menuOpen &&
                       active && (g_padState.Gamepad.wButtons != 0 ||
                       g_padState.Gamepad.sThumbLX >  16000 ||
                       g_padState.Gamepad.sThumbLX < -16000 ||
                       g_padState.Gamepad.sThumbLY >  16000 ||
                       g_padState.Gamepad.sThumbLY < -16000);
        static int nudgeTimer = 0;
        if (++nudgeTimer >= 45) { nudgeTimer = 0; padBusy = !g_menuOpen; }
        if (padBusy) {
            INPUT in[2]; memset(in, 0, sizeof(in));
            in[0].type = INPUT_MOUSE;
            in[0].mi.dx = 1;  in[0].mi.dwFlags = MOUSEEVENTF_MOVE;
            in[1].type = INPUT_MOUSE;
            in[1].mi.dx = -1; in[1].mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(2, in, sizeof(INPUT));
        }
        // (auto-focus grab removed at user request - it was annoying and did
        // not fix the load-in stuck-tracking issue anyway.)
    }
}


// Stage 7.3: haptic feedback for MotionAim (catch confirmations, key acks)
// and the game's own rumble. Queued; the runtime lane applies it.
static void MaimHaptic(int hand, float amp, float durSec)
{
    if (!g_padHaptics || !g_xrOn) return;
    XrInpHaptic(hand, amp, durSec);
}


static DWORD WINAPI hkXInputGetState(DWORD user, XINPUT_STATE* st)
{
    // Always report a connected pad on slot 0 (neutral until the VR
    // controllers come online). The game's input system only enables its
    // gamepad path if the pad is there from the very first poll at startup.
    if (user == 0 && g_padEnabled && st) {
        InterlockedIncrement(&g_padPolls);
        EnterCriticalSection(&g_padLock);
        *st = g_padState;
        LeaveCriticalSection(&g_padLock);
        return ERROR_SUCCESS;
    }
    return g_realXIGetState ? g_realXIGetState(user, st) : ERROR_DEVICE_NOT_CONNECTED;
}


static DWORD WINAPI hkXInputSetState(DWORD user, XINPUT_VIBRATION* vib)
{
    if (user == 0 && g_padEnabled && vib) {
        // game rumble -> controller haptic pulse on the stronger motor's hand
        if (g_padHaptics) {
            WORD lm = vib->wLeftMotorSpeed, rm = vib->wRightMotorSpeed;
            WORD m = lm > rm ? lm : rm;
            if (m > 2500) {
                int h = (lm >= rm) ? 0 : 1;
                // 38.13: game rumble -> the haptic queue. Magic/power feedback
                // arrives THIS way (XInputSetState).
                MaimHaptic(h, (float)m / 65535.0f, 0.08f);
            }
        }
        return ERROR_SUCCESS;
    }
    return g_realXISetState ? g_realXISetState(user, vib) : ERROR_SUCCESS;
}


static void InstallPadHook()
{
    if (g_padHooked || !g_padEnabled) return;
    HMODULE xm = GetModuleHandleA("XINPUT1_3.dll");
    if (!xm) { Log("pad: xinput1_3.dll not loaded - no pad hook"); return; }
    g_realXIGetState = (XInputGetState_t)GetProcAddress(xm, (LPCSTR)2);
    g_realXISetState = (XInputSetState_t)GetProcAddress(xm, (LPCSTR)3);
    void** slotGet = (void**)kXIGetSlot;
    void** slotSet = (void**)kXISetSlot;
    // sanity: the IAT slots must currently point at the real functions
    if (*slotGet != (void*)g_realXIGetState || *slotSet != (void*)g_realXISetState) {
        Log("pad: IAT mismatch (get %p vs %p, set %p vs %p) - NOT hooking, tell Claude",
            *slotGet, (void*)g_realXIGetState, *slotSet, (void*)g_realXISetState);
        return;
    }
    DWORD op;
    if (!VirtualProtect((void*)kXISetSlot, sizeof(void*) * 2, PAGE_READWRITE, &op)) {
        Log("pad: VirtualProtect failed (%lu)", GetLastError());
        return;
    }
    *slotGet = (void*)hkXInputGetState;
    *slotSet = (void*)hkXInputSetState;
    VirtualProtect((void*)kXISetSlot, sizeof(void*) * 2, op, &op);
    g_padHooked = true;
    Log("pad: IAT hooked - the VR controllers will appear as a 360 pad");
}
