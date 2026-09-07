// game/dishonored/hands/hand_move.cpp - VR-33 phase 1. See state chunk 58.
//
// Writes a constant translation to LookAtControl_LeftHand and restores
// everything it touched. This is the ticket's first write to the skeleton.

// The control we are driving, or NULL. Resolved fresh each tick so a level
// load or a replaced pawn cannot leave us writing into a freed object.
static uint8_t* HmControl(void)
{
    if (!g_pePawn) return NULL;
    const uint32_t off = PrOff("DishonoredPlayerPawn", "m_pLookAtControl_LeftHand");
    if (!off || !RangeReadable(g_pePawn + off, 4)) return NULL;
    uint8_t* c = *(uint8_t**)(g_pePawn + off);
    if (!c || ((uintptr_t)c & 3) || !RangeReadable(c, 0x120)) return NULL;
    // It must still be the kind of control whose fields we are about to write.
    if (!BqReceiverIsA(c, "SkelControlSingleBone")) return NULL;
    return c;
}


// Put back exactly what was there. Called on OFF, on losing the control, and
// on shutdown - an experiment that cannot be undone is not bounded.
static void HmRestore(void)
{
    if (!g_hmSaved || !g_hmCtl) { g_hmSaved = false; g_hmCtl = NULL; return; }
    if (RangeReadable(g_hmCtl, 0x120)) {
        memcpy(g_hmCtl + kSkcTrans, g_hmOrigTrans, 12);
        *(uint8_t*)(g_hmCtl + kSkcTSpace) = g_hmOrigSpace;
        *(uint32_t*)(g_hmCtl + kSkcBools) = g_hmOrigBools;
        Log("handmove: RESTORED - translation (%.2f %.2f %.2f), space %u, "
            "bools 0x%X put back on %p",
            g_hmOrigTrans[0], g_hmOrigTrans[1], g_hmOrigTrans[2],
            g_hmOrigSpace, g_hmOrigBools, (void*)g_hmCtl);
    }
    g_hmSaved = false;
    g_hmCtl = NULL;
}


static void HmTick(void)
{
    if (g_hmStepReq) {
        g_hmStepReq = 0;
        const int next = (g_hmState + 1) % HM_STATES;
        if (next == HM_OFF) HmRestore();
        g_hmState = next;
        Log("handmove: >>> %s <<< (%.1f uu on axis %d). The ZERO step is the "
            "control: if the hand moves THERE, the write path is clobbering "
            "the control's own translation input and no later reading means "
            "what it appears to.",
            kHmStateName[g_hmState], g_hmAmount, g_hmAxis);
    }

    if (!g_hmOn) { if (g_hmSaved) HmRestore(); return; }

    uint8_t* c = HmControl();
    if (!c) {
        if (g_hmSaved) {
            Log("handmove: the control is gone - dropping ownership without "
                "restoring, because the object it belonged to is no longer "
                "there to restore into");
            g_hmSaved = false; g_hmCtl = NULL;
        }
        return;
    }
    if (g_hmCtl && c != g_hmCtl) { g_hmSaved = false; g_hmCtl = NULL; }
    if (g_hmState == HM_OFF) { if (g_hmSaved) HmRestore(); return; }

    // Capture ONCE, before the first write, so the restore is genuine.
    if (!g_hmSaved) {
        memcpy(g_hmOrigTrans, c + kSkcTrans, 12);
        g_hmOrigSpace = *(uint8_t*)(c + kSkcTSpace);
        g_hmOrigBools = *(uint32_t*)(c + kSkcBools);
        g_hmSaved = true;
        g_hmCtl = c;
        Log("handmove: took ownership of %p - saved translation (%.2f %.2f "
            "%.2f), space %u (LEFT AS THE GAME SET IT, not chosen by us), "
            "bools 0x%X. apply=%d add=%d before we touch anything.",
            (void*)c, g_hmOrigTrans[0], g_hmOrigTrans[1], g_hmOrigTrans[2],
            g_hmOrigSpace, g_hmOrigBools,
            (g_hmOrigBools & kSkcApplyTrans) ? 1 : 0,
            (g_hmOrigBools & kSkcAddTrans) ? 1 : 0);
    }

    // The write. The offset REPLACES the control's translation input - it is
    // not added to it - which is precisely why the zero step exists.
    float v[3] = { 0.0f, 0.0f, 0.0f };
    if (g_hmState == HM_PLUS)  v[g_hmAxis] =  g_hmAmount;
    if (g_hmState == HM_MINUS) v[g_hmAxis] = -g_hmAmount;
    memcpy(c + kSkcTrans, v, 12);
    // The space is NOT changed: whatever the game had is what this is
    // expressed in, and choosing one would make the result mean something
    // different from what the control normally does.
    uint32_t b = *(uint32_t*)(c + kSkcBools);
    b |= kSkcApplyTrans;
    b |= kSkcAddTrans;         // additive, so the authored pose is the base
    *(uint32_t*)(c + kSkcBools) = b;
    g_hmWrites++;

    const int tag = RangeReadable(c + 0xA4, 4) ? *(int*)(c + 0xA4) : -1;
    const double now = MaimNowMs();
    if (now >= g_hmNextReport) {
        g_hmNextReport = now + 3000.0;
        Log("handmove: %s | wrote (%.1f %.1f %.1f) in the game's own space %u "
            "| %u write(s) since the last line | ControlTickTag %d (was %d - "
            "logged as an OBSERVATION only: a changed tag does not establish "
            "when the field was read, nor that nothing wrote it afterwards)",
            kHmStateName[g_hmState], v[0], v[1], v[2], g_hmOrigSpace,
            g_hmWrites, tag, g_hmLastTag);
        g_hmWrites = 0;
        g_hmLastTag = tag;
    }
}


static bool HmCommand(const char* args)
{
    if (args) {
        while (*args == ' ') args++;
        if (!strncmp(args, "on", 2))       g_hmOn = true;
        else if (!strncmp(args, "off", 3)) { g_hmOn = false; g_hmStepReq = 0; }
        else if (!strncmp(args, "step", 4)) g_hmStepReq = 1;
        else if (!strncmp(args, "axis", 4)) {
            int a = 0; if (sscanf(args + 4, "%d", &a) == 1 && a >= 0 && a < 3) g_hmAxis = a;
        } else if (!strncmp(args, "uu", 2)) {
            float f = 0; if (sscanf(args + 2, "%f", &f) == 1) g_hmAmount = f;
        }
    }
    Log("handmove: %s, state %s, %.1f uu on axis %d, %s. Ctrl+Num2 steps "
        "off -> zero -> plus -> minus -> off.",
        g_hmOn ? "ENABLED" : "disabled", kHmStateName[g_hmState],
        g_hmAmount, g_hmAxis,
        g_hmSaved ? "owns the control (originals saved)" : "owns nothing");
    return true;
}
