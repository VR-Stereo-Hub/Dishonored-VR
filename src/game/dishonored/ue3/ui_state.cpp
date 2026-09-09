// game/dishonored/ue3/ui_state.cpp - what screen is up, asked rather than inferred (VR-62).
//
// Read-only. It discovers the game's Scaleform movie-player classes and their
// properties out of GObjects, watches the live movie objects, and prints the
// gameplay verdict it WOULD have produced beside the one the mod used. It gates
// nothing and writes no engine memory. The argument is in the state block.
//
// LANES:
//   discovery  - the script lane, once per load, a bounded GObjects scan
//   latching   - the script lane, from the ProcessEvent stream, pointer compares
//   polling    - the present thread, on a cadence, plain guarded memory reads


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
    const LONG n = g_uiInstN;
    for (LONG i = 0; i < n; ++i) if (g_uiInst[i].obj == obj) return;
    if (n >= UI_INST_MAX) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
            "uistate: instance table full at %d - further movie players are not "
            "watched, so an 'open' count from here is a FLOOR and not a total.",
            (int)UI_INST_MAX);
        return;
    }
    UiInst* e = &g_uiInst[n];
    e->obj = obj; e->cls = cls;
    UiObjName(obj, e->name, sizeof(e->name));
    e->open = -1;
    e->gen = g_uiGen;
    e->seenMs = MaimNowMs();
    InterlockedExchange(&g_uiInstN, n + 1);   // published last
}


// The ProcessEvent stream. Cheap by construction: one bool, then at most
// UI_CLS_MAX pointer compares. It exists so a movie player CREATED after the
// scan is still watched - a pause menu built on demand would otherwise never
// appear.
static void UiPeLatch(void* obj)
{
    if (!g_uiOn || !g_uiReady || !obj || ((uintptr_t)obj & 3)) return;
    if (!RangeReadable(obj, kClassOff + 4)) return;
    uint8_t* cls = *(uint8_t**)((uint8_t*)obj + kClassOff);
    if (UiClsIndex(cls) < 0) return;
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

    const double t0 = MaimNowMs();
    const bool first = (g_uiScans == 0);
    ++g_uiScans;
    g_uiClsN = 0; g_uiPropN = 0;

    // Pass 1: the classes.
    uint32_t seen = 0;
    for (uint32_t i = 0; i < onum; ++i) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        ++seen;
        const char* cn = ObjClassName(o);
        if (!cn || strcmp(cn, "Class")) continue;
        const char* nm = RangeReadable(o + kNameOff, 4)
                             ? RealName(*(uint32_t*)(o + kNameOff)) : NULL;
        if (!nm || !strstr(nm, "MoviePlayer")) continue;
        if (g_uiClsN >= UI_CLS_MAX) break;
        UiCls* c = &g_uiCls[g_uiClsN++];
        c->obj = o; c->instances = 0;
        _snprintf(c->name, sizeof(c->name), "%s", nm);
        c->name[sizeof(c->name) - 1] = 0;
    }

    // Pass 2: instances of those classes, and properties declared on them.
    for (uint32_t i = 0; i < onum; ++i) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        uint8_t* cls = *(uint8_t**)(o + kClassOff);
        const int ci = UiClsIndex(cls);
        if (ci >= 0) { g_uiCls[ci].instances++; UiAddInstance(o, cls); continue; }
        // A property? Its Outer is the class that declares it.
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "Property")) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (UiClsIndex(ou) < 0) continue;
        if (g_uiPropN >= UI_PROP_MAX) continue;
        const char* pn = RangeReadable(o + kNameOff, 4)
                             ? RealName(*(uint32_t*)(o + kNameOff)) : NULL;
        if (!pn) continue;
        UiProp* p = &g_uiProp[g_uiPropN++];
        p->owner = ou;
        _snprintf(p->name, sizeof(p->name), "%s", pn);
        _snprintf(p->kind, sizeof(p->kind), "%s", cn);
        p->name[sizeof(p->name) - 1] = 0;
        p->kind[sizeof(p->kind) - 1] = 0;
        p->off  = *(uint32_t*)(o + kUPropOffset);
        p->mask = strcmp(cn, "BoolProperty") ? 0 : *(uint32_t*)(o + kUBoolBitMask);
    }

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

    Log("uistate: scan #%d over %u GObjects entries in %.0f ms - %d movie-player "
        "class(es), %d property(ies) declared on them, %ld live instance(s).",
        g_uiScans, seen, MaimNowMs() - t0, g_uiClsN, g_uiPropN, g_uiInstN);
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
    const double now = MaimNowMs();
    if (now - g_uiPollMs < 100.0) return;
    g_uiPollMs = now;
    InterlockedIncrement(&g_uiPolls);

    int openN = 0; bool mainUp = false; int unknown = 0;
    char list[256]; list[0] = 0; int named = 0;
    const LONG n = g_uiInstN;
    for (LONG i = 0; i < n; ++i) {
        UiInst* e = &g_uiInst[i];
        // Has the object gone? A destroyed movie must not keep voting. The class
        // pointer still reading back as the class we latched is the cheap test;
        // it costs no scan and catches freed or recycled memory.
        if (!e->obj || !RangeReadable(e->obj, kClassOff + 4) ||
            *(uint8_t**)(e->obj + kClassOff) != e->cls) {
            if (e->open != -2) {
                Log("uistate: instance '%s' (gen %d) no longer reads as its class - "
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
        if (named < UI_OPENSET) {
            const size_t used = strlen(list);
            _snprintf(list + used, sizeof(list) - used, "%s%s", named ? ", " : "", cn);
            ++named;
        }
    }
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
