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
    if (g_hmOn) HmScanTicks();
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


// WHICH CONTROLS ACTUALLY TICK?
//
// Phase 1 answered its question and the answer was not the expected one.
// LookAtControl_LeftHand has ControlStrength 1.0, but its ControlTickTag sat
// at 10 for an entire session while we wrote to it 8,500 times a second, and
// its own apply flags were clear. UE3 stamps that tag when a control is
// EVALUATED, so a tag that never advances means the control is never
// evaluated - and a write to a control nobody evaluates cannot move anything,
// at any cadence, with any flags.
//
// That also retires the reframing this phase was built on. The old finding
// that 9,000 writes a second "outrun the recompute" was not a race being lost.
// The control is inert.
//
// So find the ones that are not. Every SkelControl in GObjects, sampled a
// second apart: the ones whose tag ADVANCES are the controls the engine is
// actually evaluating, and they are the only ones worth writing to. Read-only.
static void HmScanTicks(void)
{
    // HM_SCAN_CAP is the size of the COMPARISON TABLE, not a limit on the
    // walk. The 2026-09-07 reading "64 SkelControl objects in GObjects" was
    // this array's old capacity of 64 reported as a population: the loop
    // stopped as soon as the table filled, so it never reached the rest of
    // GObjects and the census was the cap. The walk below now always runs to
    // the end of the array, and a saturated table is stated on the line.
    enum { HM_SCAN_CAP = 1024 };
    static uint8_t* prevObj[HM_SCAN_CAP];
    static int      prevTag[HM_SCAN_CAP];
    static int      prevN = 0;
    static double   next = 0.0;
    static uint8_t* curObj[HM_SCAN_CAP];
    static int      curTag[HM_SCAN_CAP];

    const double now = MaimNowMs();
    if (now < next) return;
    next = now + 1000.0;

    const uint32_t oName = PrOff("SkelControlBase", "ControlName");
    const uint32_t oStr  = PrOff("SkelControlBase", "ControlStrength");
    const uint32_t oTag  = PrOff("SkelControlBase", "ControlTickTag");
    if (!oTag || !RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;

    int curN = 0;
    int live = 0, total = 0, untracked = 0;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x120)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "SkelControl")) continue;
        total++;
        const int tag = *(int*)(o + oTag);
        // Past the table's end the object is still COUNTED - the census is
        // the whole array - but it cannot be compared next second, and the
        // summary says how many are in that state.
        if (curN < HM_SCAN_CAP) { curObj[curN] = o; curTag[curN] = tag; curN++; }
        else                    { untracked++; }

        for (int k = 0; k < prevN; k++) {
            if (prevObj[k] != o || prevTag[k] == tag) continue;
            float str = -1.0f;
            if (oStr && RangeReadable(o + oStr, 4)) memcpy(&str, o + oStr, 4);
            const char* nm = (oName && RangeReadable(o + oName, 4))
                             ? RealName(*(uint32_t*)(o + oName)) : NULL;
            // WHOSE control is it? A control's Outer is its AnimTree and
            // that tree's Outer is the SkeletalMeshComponent it belongs to.
            // This is the walk the WORKING 38.x drive used to decide a
            // control was the player's (skelcontrol.cpp:210-226) - and it is
            // the step this phase skipped by reading the pawn's named
            // m_pLookAtControl_* pointers instead. Those hang off the PAWN's
            // Mesh, which need not be the first-person view model the split
            // actually cuts: same skeleton asset, so every socket matched and
            // nothing looked wrong.
            uint8_t* tree = NULL; uint8_t* comp = NULL;
            if (RangeReadable(o + kOuterOff, 4)) tree = *(uint8_t**)(o + kOuterOff);
            if (tree && !((uintptr_t)tree & 3) && RangeReadable(tree + kOuterOff, 4))
                comp = *(uint8_t**)(tree + kOuterOff);
            const char* compCls = (comp && !((uintptr_t)comp & 3) &&
                                   RangeReadable(comp, 0x40)) ? ObjClassName(comp) : NULL;
            uint8_t* pawnMesh = NULL;
            {
                const uint32_t mo = PrOff("Pawn", "Mesh");
                if (mo && g_pePawn && RangeReadable(g_pePawn + mo, 4))
                    pawnMesh = *(uint8_t**)(g_pePawn + mo);
            }
            Log("handmove/ticks: LIVE  %p '%s' class '%s' tag %d -> %d, "
                "strength %.3f | owner component %p '%s' - %s",
                (void*)o, nm ? nm : "?", cn, prevTag[k], tag, str,
                (void*)comp, compCls ? compCls : "?",
                (comp && comp == pawnMesh)
                    ? "the SAME component as the pawn's Mesh"
                    : "a DIFFERENT component from the pawn's Mesh - which is "
                      "where the named LookAtControls came from");
            live++;
            break;
        }
    }
    // The population this counter ran over is on the line with it: %u objects
    // walked out of the %u GObjects claims, and how many SkelControls were
    // past the comparison table. A zero live count means nothing without them.
    Log("handmove/ticks: %d SkelControl object(s) out of %u GObjects entries "
        "walked (FULL sweep), %d tracked for comparison, %d NOT tracked "
        "(table full - raise HM_SCAN_CAP), %d advanced their tick tag in the "
        "last second. hands=%d gamepadOnly=%d. A control whose tag does NOT "
        "advance is not evaluated, and writing to it cannot move anything - "
        "which is what the three named LookAtControls did while we wrote to "
        "them 8500 times a second. A live one on a DIFFERENT component from "
        "the pawn's Mesh means this phase wrote to the wrong object and the "
        "native lane is alive; live=0 with untracked=0 closes it.",
        total, onum, curN, untracked, live,
        g_skcDrive ? 1 : 0, g_gamepadOnly ? 1 : 0);
    memcpy(prevObj, curObj, sizeof(uint8_t*) * curN);
    memcpy(prevTag, curTag, sizeof(int) * curN);
    prevN = curN;
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
