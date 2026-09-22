// game/dishonored/ue3/ui_state.cpp - what screen is up, asked rather than inferred (VR-62).
//
// Read-only engine access. It discovers the game's Scaleform movie-player classes and their
// properties out of GObjects, watches the live movie objects, and prints the
// gameplay verdict beside the one the mod used. The note result also drives
// fast mono transitions and menu retention. No engine memory is written. The argument is in the state block.
//
// LANES:
//   discovery  - the script lane, once per load, a bounded GObjects scan
//   latching   - the script lane, from the ProcessEvent stream, pointer compares
//   polling    - the present thread, on a cadence, plain guarded memory reads


// The script lane can replace slots while Present polls them. Publish complete
// identities under one lock; Present skips a busy scan instead of waiting for it.
static SRWLOCK g_uiTableLock = SRWLOCK_INIT;
struct UiTableUnlock {
    ~UiTableUnlock() { ReleaseSRWLockExclusive(&g_uiTableLock); }
};

static bool UiInstanceLive(const UiInst* e)
{
    return e->obj && IsLiveObject(e->obj) &&
        RangeReadable(e->obj, kClassOff + 4) &&
        RangeReadable(e->obj + kNameOff, 8) &&
        *(uint8_t**)(e->obj + kClassOff) == e->cls &&
        *(uint32_t*)(e->obj + kNameOff) == e->fname[0] &&
        *(uint32_t*)(e->obj + kNameOff + 4) == e->fname[1];
}

// Caller holds the table lock after refreshing GObjects for this generation.
static void UiResetInstances()
{
    g_uiInstN = 0;
    g_uiNoteOpen = false;
}

// Is this class object one of the movie-player classes discovery found?
static int UiClsIndex(uint8_t* cls)
{
    if (!cls) return -1;
    for (int i = 0; i < g_uiClsN; ++i) if (g_uiCls[i].obj == cls) return i;
    return -1;
}


// A movie player's own object name, for the log. Instances are named per screen
// (DisGFxMoviePlayerMainMenu_0 and so on), and the NAME is how a stale object
// from the previous level is told from the one the current level made.
static void UiObjName(uint8_t* o, char* out, size_t n)
{
    const char* nm = (o && RangeReadable(o + kNameOff, 4))
                         ? RealName(*(uint32_t*)(o + kNameOff)) : NULL;
    _snprintf(out, n, "%s", nm ? nm : "?");
    out[n - 1] = 0;
}


// Read the open bit off one instance. Returns -1 when we cannot answer, and that
// is the whole point: an unreadable object is UNKNOWN, never "closed".
static int UiReadOpen(uint8_t* o)
{
    if (!g_uiOpenMask || !o || ((uintptr_t)o & 3)) return -1;
    if (!RangeReadable(o + g_uiOpenOff, 4)) return -1;
    return (*(uint32_t*)(o + g_uiOpenOff) & g_uiOpenMask) ? 1 : 0;
}


// Append an instance if it is new. Called from the event stream and from the
// scan, so it must tolerate being handed the same object repeatedly.
static void UiAddInstance(uint8_t* obj, uint8_t* cls)
{
    // Caller holds g_uiTableLock. A dropped address is reusable, not a permanent
    // table occupant. The same address with a new FName is a new observation.
    if (!obj || !IsLiveObject(obj) || !RangeReadable(obj, kClassOff + 4) ||
        !RangeReadable(obj + kNameOff, 8) || *(uint8_t**)(obj + kClassOff) != cls) return;
    const LONG n = g_uiInstN;
    LONG slot = n;
    for (LONG i = 0; i < n; ++i) {
        if (g_uiInst[i].obj != obj) continue;
        if (UiInstanceLive(&g_uiInst[i])) return;
        slot = i;
        break;
    }
    // Repeated events validate only their own identity. A new object alone
    // pays for searching the population for a reclaimable entry.
    if (slot == n) {
        for (LONG i = 0; i < n; ++i) {
            if (!UiInstanceLive(&g_uiInst[i])) { slot = i; break; }
        }
    }
    if (slot >= UI_INST_MAX) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
            "uistate: instance table full at %d live identities - new movies cannot vote",
            (int)UI_INST_MAX);
        return;
    }
    UiInst* e = &g_uiInst[slot];
    e->obj = obj; e->cls = cls;
    e->fname[0] = *(uint32_t*)(obj + kNameOff);
    e->fname[1] = *(uint32_t*)(obj + kNameOff + 4);
    UiObjName(obj, e->name, sizeof(e->name));
    e->open = -1;
    e->gen = g_uiGen;
    e->seenMs = MaimNowMs();
    if (slot == n) InterlockedExchange(&g_uiInstN, n + 1);
    const char* cn = ObjClassName(obj);
    if (cn && strstr(cn, "MoviePlayerNote"))
        Log("uistate: watching note '%s' gen %d in slot %ld (%ld tracked)",
            e->name, e->gen, slot, g_uiInstN);
}


// The ProcessEvent stream. Cheap by construction: one bool, then at most
// UI_CLS_MAX pointer compares. It exists so a movie player CREATED after the
// scan is still watched - a pause menu built on demand would otherwise never
// appear.
static void UiPeLatch(void* obj)
{
    if (!g_uiOn || !g_uiReady || !obj || ((uintptr_t)obj & 3)) return;
    if (!RangeReadable(obj, kClassOff + 4)) return;
    AcquireSRWLockExclusive(&g_uiTableLock);
    UiTableUnlock unlock;
    uint8_t* cls = *(uint8_t**)((uint8_t*)obj + kClassOff);
    if (UiClsIndex(cls) < 0) return;
    // On-demand movies may be created after the last load scan.
    if (!IsLiveObject((uint8_t*)obj)) {
        static double refreshMs = -1000.0;
        const double now = MaimNowMs();
        if (now - refreshMs < 1000.0) return;
        refreshMs = now;
        if (!BuildLiveSet()) return;
    }
    UiAddInstance((uint8_t*)obj, cls);
}


// ---- discovery --------------------------------------------------------------
//
// TWO PASSES OVER GObjects, once per load, on the script lane.
//
// Pass 1 finds every UClass whose name says movie player. Pass 2 finds every live
// instance of one, and every UProperty declared on one, by comparing the
// property's Outer POINTER to a class we found - no name compare and no
// inheritance walk, which is the same trick FindPropOffset already uses.
//
// The point of dumping every property rather than looking one up is that a name
// we did not predict is the most useful thing a first run can return. Guessing a
// property name and reporting "not found" teaches nothing.
static void UiDiscover(void)
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || onum < 1000 || onum > 4000000) return;

    if (!BuildLiveSet()) return;
    AcquireSRWLockExclusive(&g_uiTableLock);
    UiTableUnlock unlock;
    const LONG previousN = g_uiInstN;
    UiResetInstances(); // replace the outgoing population, never append to it
    const double t0 = MaimNowMs();
    const bool first = (g_uiScans == 0);
    ++g_uiScans;
    g_uiClsN = 0; g_uiPropN = 0;
    Log("uistate: rebuild gen %d replaces %ld prior movie identities", g_uiGen, previousN);

    // Pass 1: the classes. VR-102: both passes use GObjForEach (one VirtualQuery
    // per memory region, one name lookup per class); each was a VirtualQuery per
    // object and the rescan on entering gameplay cost half a second of game thread.
    const double tLive = MaimNowMs() - t0;
    const GObjWalkStats w1 = GObjForEach(kClassOff + 4, [&](uint32_t, uint8_t* o, const char* cn) {
        if (!cn || strcmp(cn, "Class")) return true;
        const char* nm = RangeReadable(o + kNameOff, 4)
                             ? RealName(*(uint32_t*)(o + kNameOff)) : NULL;
        if (!nm || !strstr(nm, "MoviePlayer")) return true;
        if (g_uiClsN >= UI_CLS_MAX) return false;
        UiCls* c = &g_uiCls[g_uiClsN++];
        c->obj = o; c->instances = 0;
        _snprintf(c->name, sizeof(c->name), "%s", nm);
        c->name[sizeof(c->name) - 1] = 0;
        return true;
    });
    const uint32_t seen = w1.visited;

    // Pass 2: instances of those classes, and properties declared on them.
    const GObjWalkStats w2 = GObjForEach(0x80, [&](uint32_t, uint8_t* o, const char* cn) {
        uint8_t* cls = *(uint8_t**)(o + kClassOff);
        const int ci = UiClsIndex(cls);
        if (ci >= 0) { g_uiCls[ci].instances++; UiAddInstance(o, cls); return true; }
        // A property? Its Outer is the class that declares it.
        if (!cn || !strstr(cn, "Property")) return true;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (UiClsIndex(ou) < 0) return true;
        if (g_uiPropN >= UI_PROP_MAX) return true;
        const char* pn = RangeReadable(o + kNameOff, 4)
                             ? RealName(*(uint32_t*)(o + kNameOff)) : NULL;
        if (!pn) return true;
        UiProp* p = &g_uiProp[g_uiPropN++];
        p->owner = ou;
        _snprintf(p->name, sizeof(p->name), "%s", pn);
        _snprintf(p->kind, sizeof(p->kind), "%s", cn);
        p->name[sizeof(p->name) - 1] = 0;
        p->kind[sizeof(p->kind) - 1] = 0;
        p->off  = *(uint32_t*)(o + kUPropOffset);
        p->mask = strcmp(cn, "BoolProperty") ? 0 : *(uint32_t*)(o + kUBoolBitMask);
        return true;
    });

    // CHOOSE THE OPEN BIT, and say why. Preference order is exact first, because
    // "bMovieIsOpen" is the UE3 GFxUI name; anything else is a fallback and the
    // log must say the choice was a fallback so a wrong pick is visible.
    const char* picked = NULL; const char* pickWhy = "";
    uint32_t off = 0, mask = 0; const char* owner = "?";
    for (int pass = 0; pass < 2 && !picked; ++pass) {
        for (int i = 0; i < g_uiPropN; ++i) {
            UiProp* p = &g_uiProp[i];
            if (!p->mask) continue;
            const bool exact = !strcmp(p->name, "bMovieIsOpen");
            const bool loose = (strstr(p->name, "IsOpen") != NULL) ||
                               (strstr(p->name, "MovieOpen") != NULL);
            if (pass == 0 ? !exact : !loose) continue;
            picked = p->name; off = p->off; mask = p->mask;
            pickWhy = pass == 0 ? "the exact UE3 GFxUI name"
                                : "A FALLBACK - no bMovieIsOpen exists on any movie class here";
            const int oi = UiClsIndex(p->owner);
            owner = oi >= 0 ? g_uiCls[oi].name : "?";
            break;
        }
    }
    g_uiOpenOff = off; g_uiOpenMask = mask;
    _snprintf(g_uiOpenName,  sizeof(g_uiOpenName),  "%s", picked ? picked : "");
    _snprintf(g_uiOpenOwner, sizeof(g_uiOpenOwner), "%s", owner);
    g_uiOpenName[sizeof(g_uiOpenName) - 1] = 0;
    g_uiOpenOwner[sizeof(g_uiOpenOwner) - 1] = 0;

    InterlockedExchange((LONG*)&g_uiReadyFlag, 1);
    g_uiReady = true;

    Log("uistate: scan #%d over %u GObjects entries in %.0f ms (live set %.0f, walks "
        "%.0f + %.0f) - %d movie-player class(es), %d property(ies) declared on them, "
        "%ld live instance(s).",
        g_uiScans, seen, MaimNowMs() - t0, tLive, w1.ms, w2.ms, g_uiClsN, g_uiPropN,
        g_uiInstN);
    for (int i = 0; i < g_uiClsN; ++i)
        Log("uistate:   class %-36s %d instance(s)", g_uiCls[i].name, g_uiCls[i].instances);
    if (first) {
        // The full property dump, ONCE per session. This is the payload: a name
        // we did not predict cannot be looked up, only listed.
        for (int i = 0; i < g_uiPropN; ++i) {
            const int oi = UiClsIndex(g_uiProp[i].owner);
            Log("uistate:   %-14s %-28s::%-28s +0x%04x mask 0x%08x",
                g_uiProp[i].kind, oi >= 0 ? g_uiCls[oi].name : "?",
                g_uiProp[i].name, g_uiProp[i].off, g_uiProp[i].mask);
        }
        if (g_uiPropN >= UI_PROP_MAX)
            DVR_WARN("uistate: the property table filled at %d - the dump above is "
                     "TRUNCATED and a missing name may exist.", (int)UI_PROP_MAX);
    }
    if (g_uiOpenMask)
        Log("uistate: the OPEN bit is '%s' (%s +0x%04x mask 0x%08x) - %s. Every "
            "line below depends on this pick, and the way to check it is whether "
            "it CHANGES when a screen visibly opens.",
            g_uiOpenName, g_uiOpenOwner, g_uiOpenOff, g_uiOpenMask, pickWhy);
    else
        DVR_WARN("uistate: NO open/closed bool exists on any movie-player class. "
                 "The premise of this probe is dead as written, the UI state stays "
                 "UNKNOWN, and nothing below may be read as 'no menu'.");
}


// ---- the poll ---------------------------------------------------------------
//
// The present thread, on a cadence, immediately before the state machine decides.
// Guarded reads only. It answers three things and logs a change in any of them:
// which movies are open, whether one of them is a main menu, and what the
// gameplay verdict would have been.
static void UiPoll(bool pawn, bool viewLive)
{
    if (!g_uiOn || !g_uiReady) return;
    if (!TryAcquireSRWLockExclusive(&g_uiTableLock)) return;
    UiTableUnlock unlock;
    const double now = MaimNowMs();
    if (now - g_uiPollMs < 100.0) return;
    g_uiPollMs = now;
    InterlockedIncrement(&g_uiPolls);

    int openN = 0; bool mainUp = false, noteUp = false; int unknown = 0;
    char list[256]; list[0] = 0; int named = 0;
    const LONG n = g_uiInstN;
    for (LONG i = 0; i < n; ++i) {
        UiInst* e = &g_uiInst[i];
        // A retained pointer and matching class alone do not establish liveness.
        if (!UiInstanceLive(e)) {
            if (e->open != -2) {
                Log("uistate: instance '%s' (gen %d) is no longer the same live identity - "
                    "dropped. A stale movie object cannot vote on the current level.",
                    e->name, e->gen);
                e->open = -2; InterlockedIncrement(&g_uiDropped);
            }
            continue;
        }
        const int op = UiReadOpen(e->obj);
        if (op < 0) { ++unknown; continue; }
        if (op != e->open && e->open != -1)
            Log("uistate: '%s' %s %d -> %d (gen %d, %.1f s after it was first seen)",
                e->name, g_uiOpenName, e->open, op, e->gen, (now - e->seenMs) / 1000.0);
        e->open = op;
        if (!op) continue;
        ++openN;
        const int ci = UiClsIndex(e->cls);
        const char* cn = ci >= 0 ? g_uiCls[ci].name : "?";
        if (strstr(cn, "MainMenu") || strstr(cn, "StartScreen")) mainUp = true;
        if (strstr(cn, "MoviePlayerNote")) noteUp = true;   // VR-93: the book/note screen
        if (named < UI_OPENSET) {
            const size_t used = strlen(list);
            _snprintf(list + used, sizeof(list) - used, "%s%s", named ? ", " : "", cn);
            ++named;
        }
    }
    if (g_uiNoteOpen != noteUp)
        Log("uistate: note-visible %d -> %d (gen %d, %ld tracked)",
            (int)g_uiNoteOpen, (int)noteUp, g_uiGen, n);
    g_uiNoteOpen = noteUp;
    UiFlagsPoll();   // VR-93 research reporter; returns at once unless [Menu] UiFlags=1
    if (!named) _snprintf(list, sizeof(list), "%s", "none");
    list[sizeof(list) - 1] = 0;

    // THE COMPARISON. `old` is the mod's own menu belief; `proposed` replaces it
    // with the movie state and changes nothing else, so any difference in the
    // verdict is attributable to this one substitution.
    const bool known    = g_uiOpenMask != 0;
    const bool oldMenu  = g_menuOpen || g_inMenu || g_mainMenu;
    const bool newMenu  = known ? (openN > 0) : oldMenu;   // UNKNOWN keeps the old
    const bool oldV     = pawn && !oldMenu && !g_cineNow && viewLive;
    const bool newV     = pawn && !newMenu && !g_cineNow && viewLive;
    // What the verdict would be WITHOUT the one-second view hold, so the two
    // known delays are measured apart instead of together.
    const bool newVNoView = pawn && !newMenu && !g_cineNow;
    g_uiVerdictNew   = newV;
    g_uiVerdictNoView = newVNoView;

    if (openN != g_uiOpenCount || mainUp != g_uiMainMenu ||
        oldV != g_uiPropOld || newV != g_uiPropNew || strcmp(list, g_uiOpenList)) {
        g_uiOpenCount = openN; g_uiMainMenu = mainUp;
        g_uiPropOld = oldV; g_uiPropNew = newV;
        _snprintf(g_uiOpenList, sizeof(g_uiOpenList), "%s", list);
        g_uiOpenList[sizeof(g_uiOpenList) - 1] = 0;
        Log("uistate: %d movie(s) open [%s]%s%s | flags menuOpen=%d inMenu=%d "
            "mainMenu=%d | verdict old=%s proposed=%s (without the 1 s view hold: %s) "
            "-> %s",
            openN, list, mainUp ? " MAIN MENU" : "",
            unknown ? " (some instances unreadable - counted as UNKNOWN, not closed)" : "",
            (int)g_menuOpen, (int)g_inMenu, (int)g_mainMenu,
            oldV ? "TRUE" : "FALSE", newV ? "TRUE" : "FALSE",
            newVNoView ? "TRUE" : "FALSE",
            !known ? "UNKNOWN: no open bit, the proposal is just a copy of the old one"
                   : oldV == newV ? "they AGREE" : "THEY DISAGREE - this is the case to explain");
    }
}


// The script lane. One bounded scan per load and nothing else.
static void UiTick(void)
{
    if (g_uiOn && g_uiReady) UiFlagsResolve();   // VR-93 research: once, after the first scan
    if (!g_uiOn || !g_uiRescan) return;
    if (!RflNamesReady()) return;   // nothing resolves by name before the pool is up
    g_uiRescan = false;
    UiDiscover();
}


// A load began: re-scan, because the movie objects of the outgoing level are not
// the movie objects of the incoming one, and an old instance authorising a new
// level is exactly the failure this probe exists to catch.
static void UiNoteLoad(void)
{
    ++g_uiGen;
    g_uiRescan = true;
}


static bool UiCommand(const char* args)
{
    if (!args) args = "";
    if (!args[0] || !strcmp(args, "status")) {
        AcquireSRWLockExclusive(&g_uiTableLock);
        UiTableUnlock unlock;
        Log("uistate: %s | %d scan(s), %d class(es), %d prop(s), %ld instance(s), "
            "%ld dropped | open bit %s | %d open now [%s]%s | polls %ld",
            g_uiOn ? "on" : "OFF", g_uiScans, g_uiClsN, g_uiPropN, g_uiInstN,
            g_uiDropped,
            g_uiOpenMask ? g_uiOpenName : "NONE FOUND (state is UNKNOWN)",
            g_uiOpenCount, g_uiOpenList[0] ? g_uiOpenList : "none",
            g_uiMainMenu ? " MAIN MENU" : "", g_uiPolls);
        for (LONG i = 0; i < g_uiInstN; ++i) {
            const int ci = UiClsIndex(g_uiInst[i].cls);
            Log("uistate:   %-32s %-30s open=%s gen %d",
                ci >= 0 ? g_uiCls[ci].name : "?", g_uiInst[i].name,
                g_uiInst[i].open == 1 ? "yes" : g_uiInst[i].open == 0 ? "no"
                : g_uiInst[i].open == -2 ? "GONE" : "unknown", g_uiInst[i].gen);
        }
        return true;
    }
    if (!strcmp(args, "scan")) {
        g_uiRescan = true;
        Log("uistate: a scan is queued for the next script tick");
        return true;
    }
    bool b = false;
    if (DvrOnOff(args, &b)) { g_uiOn = b; Log("uistate: %s", b ? "on" : "off"); return true; }
    Log("uistate: status | scan | on | off");
    return true;
}

// ---- VR-93 research: the declared screen flags, reported --------------------
//
// GAMEPLAY_STATE.md section 9 lists flags the script dump declares for notes,
// menus, dialogue choices and the cutscene skip prompt. A declaration is not a
// measurement, so this only resolves them by name and logs each CHANGE with the
// game state beside it. It gates nothing and writes no engine memory.

static UiFlag g_uf[] = {
    // the book / note / tutorial screen
    {"DisGFxMoviePlayerNote",     "m_bNoteVisible",              "MoviePlayerNote",     "Tweaks", true},
    {"DisGFxMoviePlayerNote",     "m_bAsyncNoteFromMenu",        "MoviePlayerNote",     "Tweaks", true},
    {"DisGFxMoviePlayerNote",     "m_AsyncNoteType",             "MoviePlayerNote",     "Tweaks", false},
    // save / load screens (declared on the menu base, inherited by the pause and main menus)
    {"DisGFxMoviePlayerMenuBase", "m_bIsInLoadMenu",             "PauseMenu|MainMenu",  "Tweaks", true},
    {"DisGFxMoviePlayerMenuBase", "m_bIsInSaveMenu",             "PauseMenu|MainMenu",  "Tweaks", true},
    {"DisGFxMoviePlayerMenuBase", "m_bLoadingGame",              "PauseMenu|MainMenu",  "Tweaks", true},
    {"DisGFxMoviePlayerPauseMenu","m_bWaitingSaveLoadToStart",   "MoviePlayerPauseMenu","Tweaks", true},
    {"DisGFxMoviePlayerPauseMenu","m_bGameOver",                 "MoviePlayerPauseMenu","Tweaks", true},
    // the HUD's own view of dialogue choice, cinematics and tutorials
    {"DisGFxMoviePlayerHUD",      "m_bChoiceSelection",          "MoviePlayerHUD",      "Tweaks|HUDFX", true},
    {"DisGFxMoviePlayerHUD",      "m_bCinematicMode",            "MoviePlayerHUD",      "Tweaks|HUDFX", true},
    {"DisGFxMoviePlayerHUD",      "m_bTutorialWindowSet",        "MoviePlayerHUD",      "Tweaks|HUDFX", true},
    {"DisGFxMoviePlayerHUD",      "m_bBlockInteractionWindow",   "MoviePlayerHUD",      "Tweaks|HUDFX", true},
    // the "hold to skip" gauge over a cutscene
    {"DishonoredPlayerInput",     "m_bSkipSceneGaugeIsDisplayed","@input",              NULL,     true},
};
static const int kUfN = (int)(sizeof(g_uf) / sizeof(g_uf[0]));
static uint32_t g_ufInputOff = 0;   // PlayerController.PlayerInput


// The script lane, once, after discovery. Every name either resolves or says so.
static void UiFlagsResolve(void)
{
    if (!g_ufOn || g_ufResolved) return;
    g_ufResolved = true;
    int ok = 0;
    for (int i = 0; i < kUfN; ++i) {
        UiFlag* u = &g_uf[i];
        for (int k = 0; k < 4; ++k) u->last[k] = -1;
        if (u->isBool) u->resolved = FindBoolProp(u->declCls, u->prop, &u->off, &u->mask) && u->mask;
        else { u->off = FindPropOffset(u->declCls, u->prop); u->mask = 0; u->resolved = u->off != 0; }
        if (u->resolved) ++ok;
        Log("uiflags: %s::%s %s", u->declCls, u->prop,
            u->resolved ? (u->isBool ? "resolved" : "resolved (byte)") : "NOT FOUND - the dump's name did not resolve on this build");
        if (u->resolved)
            Log("uiflags:   +0x%04x mask 0x%08x, read on instances whose class contains '%s'",
                u->off, u->mask, u->instLike);
    }
    g_ufInputOff = FindPropOffset("PlayerController", "PlayerInput");
    Log("uiflags: %d of %d flag(s) resolved; PlayerController.PlayerInput %s. Each line below is a "
        "CHANGE with the game state beside it; a flag that never changes while its screen is "
        "used is not the flag its name suggests.", ok, kUfN,
        g_ufInputOff ? "resolved" : "NOT FOUND (the skip gauge cannot be read)");
}


// Does s contain any of the '|'-separated alternatives in lts?
static bool UiFlagAny(const char* s, const char* alts)
{
    if (!s || !alts) return false;
    char buf[64];
    const char* p = alts;
    while (*p) {
        const char* bar = strchr(p, '|');
        size_t n = bar ? (size_t)(bar - p) : strlen(p);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, p, n); buf[n] = 0;
        if (n && strstr(s, buf)) return true;
        if (!bar) break;
        p = bar + 1;
    }
    return false;
}

static int UiFlagRead(const UiFlag* u, uint8_t* o)
{
    if (!o || ((uintptr_t)o & 3) || !RangeReadable(o + u->off, 4)) return -1;
    if (u->isBool) return (*(uint32_t*)(o + u->off) & u->mask) ? 1 : 0;
    return *(uint8_t*)(o + u->off);
}

static void UiFlagNote(UiFlag* u, int slot, int v, const char* where)
{
    if (slot < 0 || slot > 3 || v == u->last[slot]) return;
    const int was = u->last[slot];
    u->last[slot] = v;
    if (was == -1) return;   // first sample is a baseline, not a change
    const LONG n = InterlockedIncrement(&u->changes);
    if (n <= 200 || (n % 50) == 0)
        Log("uiflags: %s.%s %d -> %d on %s | menuOpen=%d inMenu=%d cine=%d | change #%ld",
            u->declCls, u->prop, was, v, where, (int)g_menuOpen, (int)g_inMenu, (int)g_cineNow, (long)n);
}

// The present thread, from UiPoll's cadence.
static void UiFlagsPoll(void)
{
    if (!g_ufOn || !g_ufResolved) return;
    const LONG n = g_uiInstN;
    for (int i = 0; i < kUfN; ++i) {
        UiFlag* u = &g_uf[i];
        if (!u->resolved) continue;
        if (!strcmp(u->instLike, "@input")) {
            uint8_t* ctrl = g_peCtrl;
            if (!g_ufInputOff || !ctrl || !RangeReadable(ctrl + g_ufInputOff, 4)) continue;
            uint8_t* in = *(uint8_t**)(ctrl + g_ufInputOff);
            const char* cn = (in && !((uintptr_t)in & 3)) ? ObjClassName(in) : NULL;
            if (!cn || !strstr(cn, "PlayerInput")) continue;
            UiFlagNote(u, 0, UiFlagRead(u, in), cn);
            continue;
        }
        int slot = 0;
        for (LONG k = 0; k < n && slot < 4; ++k) {
            UiInst* e = &g_uiInst[k];
            if (e->open == -2 || !e->obj) continue;
            const int ci = UiClsIndex(e->cls);
            const char* cn = ci >= 0 ? g_uiCls[ci].name : NULL;
            if (!cn || !UiFlagAny(cn, u->instLike) || UiFlagAny(cn, u->notLike)) continue;
            const int v = UiFlagRead(u, e->obj);
            if (v >= 0) UiFlagNote(u, slot, v, e->name);
            ++slot;
        }
    }
}
