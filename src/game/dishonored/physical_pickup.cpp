// game/dishonored/physical_pickup.cpp - loot picked up by reaching for it.
//
// Reach a hand to a lootable item and squeeze that hand's grip: the item is picked up. The
// Interact button keeps working exactly as before, at any range the game allows.
//
// Nothing here picks anything up by itself, and no engine field is written. Three small parts:
//
//   1. THE LIST. Every actor whose class derives from DisPickup_Base (coins, elixirs, keys, bone
//      charms, ammunition, notes), or is a DisProjectile_Arrow (a bolt to recover), found by an
//      INCREMENTAL pass over GObjects: a couple of thousand slots a frame, so a full sweep takes
//      well under a second and no frame pays for a whole walk. An entry is used only while its
//      GObjects slot still holds the same pointer with the same class.
//   2. THE TARGET. Each frame the nearest listed item within reach of either hand becomes the
//      target, and interact_aim.cpp's two bridges hand the ENGINE's own interaction trace a ray
//      from the head to that item instead of the pointing ray. The engine still traces,
//      validates, highlights and prompts: the highlight is the game's, and an item the game
//      would refuse stays refused. A target the engine does not focus within a quarter second
//      is dropped for a few seconds (a looted item that is still an actor, something in the way).
//   3. THE GRIP. While the engine's focused actor IS the target and a hand is within reach, a
//      press of that hand's PHYSICAL grip is swallowed (the power wheel or the block bound to
//      it does not fire) and Interact is pressed for a moment instead (pad_bridge.cpp).
//
// Books, notes and audio logs (DisAbstractItemPickup and its children) are larger than a coin and
// are measured from their origin like everything else, so they have a longer reach of their own;
// and when one is opened by a grip, the page attaches to the hand that opened it (hud_layout's
// reading panel; the Interact button keeps the left hand).
//
// Doors (DisDoor) can be opened the same way, behind their own switch: a door is measured from
// its collision box, not its origin (the hinge), and the trace looks at the point of the box
// nearest the hand.
//
// What the GAME highlights is what the grip takes: if the engine focuses another listed item
// within reach than the one aimed at, that item becomes the target. A book or note opened by a
// grip is then left alone for a while after reading ends, and the trace tries a few points
// around the next one, so the lower of two stacked books can be reached.
//
// [Aim] PhysicalPickup (default 1), PhysicalPickupReachCm (30), PhysicalPickupBookReachCm (45),
// PhysicalDoors (1), PhysicalDoorReachCm (20). F10 > Aim. Seam: `pickup`.
// Script lane for 1 and 2 (the interaction bridges run on the same game thread); the pad
// bridge reads two atomics.

#define DVR_CAT ::dvr::log::Cat::script

static std::atomic<bool>     g_ppOn{true};
static std::atomic<float>    g_ppReachM{0.30f};
static std::atomic<float>    g_ppBookReachM{0.45f};   // books, notes, audio logs
static std::atomic<bool>     g_ppTargetReadable{false};   // the target is one of those (for the pad bridge)
static std::atomic<bool>     g_ppDoorsOn{true};       // [Aim] PhysicalDoors
static std::atomic<float>    g_ppDoorReachM{0.20f};   // from the hand to the door's collision box
static std::atomic<uint32_t> g_ppReadyMask{0};        // bit h: hand h may pick the target up now
static std::atomic<uint64_t> g_ppReadyMs{0};          // when the mask was last written
static std::atomic<uint32_t> g_ppFired{0}, g_ppSwallowed{0};

enum : uint32_t { kPpLoot = 0, kPpReadable = 1, kPpDoor = 2 };   // readable: a book, note or audio log
struct PpEntry { uint8_t* obj; uint32_t idx; uint8_t* cls; uint32_t kind; };
// Two fixed arrays, swapped when a sweep completes. Plain data on purpose: the loops that fill
// and read them run under a structured exception handler (see PpSweepSlice), which cannot share
// a function with C++ objects that unwind.
constexpr uint32_t kPpMax = 4096;
static PpEntry  g_ppArrA[kPpMax], g_ppArrB[kPpMax];
static PpEntry* g_ppList = g_ppArrA; static uint32_t g_ppListN = 0;
static PpEntry* g_ppBuild = g_ppArrB; static uint32_t g_ppBuildN = 0;
// class pointer -> lootable, direct mapped; a collision only recomputes.
struct PpClassSlot { uint8_t* cls; uint8_t verdict; };
static PpClassSlot g_ppClass[8192];
// name index -> "is one of the base names", direct mapped (0 = empty, 1 = no, 2 = loot,
// 3 = readable loot: DisAbstractItemPickup, 4 = a door: DisDoor).
// A class name is turned into text ONCE; after that the chain walk compares integers.
struct PpNameSlot { uint32_t idx; uint8_t verdict; };
static PpNameSlot g_ppName[8192];
static uint32_t g_ppNameBudget = 0;                    // text lookups left this frame
static uint32_t g_ppCursor = 0, g_ppSweeps = 0, g_ppFaults = 0;
static double   g_ppSweepStartMs = 0, g_ppLastSweepMs = 0;
// This module's own cost a frame, and how often it was not small.
static double   g_ppCostUsSum = 0, g_ppCostUsMax = 0; static uint32_t g_ppCostN = 0, g_ppCostOver250 = 0, g_ppCostOver1000 = 0;
static uint32_t g_ppSliceCut = 0;                      // slices ended by the time budget
// The NEAR list: the listed items within a few metres of the camera, refreshed by a rotating
// pass over the whole list (a chunk a frame). The per-frame distance test reads only these, so
// its cost does not grow with the number of pickups in a level.
constexpr uint32_t kPpNearMax = 128;
static PpEntry  g_ppNearA[kPpNearMax], g_ppNearB[kPpNearMax];
static PpEntry* g_ppNear = g_ppNearA; static uint32_t g_ppNearN = 0;
static PpEntry* g_ppNearBuild = g_ppNearB; static uint32_t g_ppNearBuildN = 0;
static uint32_t g_ppNearCursor = 0;
static const PpEntry* g_ppNearSrc = NULL;              // the list the rotation is walking
// The last 8 frames: this module's cost and the time since the frame before. Printed when the
// re-entry gate reports a camera-silent single draw, so the log says whether a slow frame of OURS
// came before it or not (it records the gaps with pickup switched off too, where the cost reads 0).
static float    g_ppRingCostUs[8] = {}, g_ppRingGapMs[8] = {};
static uint32_t g_ppRingAt = 0;

// The target (game thread only).
static uint8_t* g_ppTarget = NULL;
static uint32_t g_ppTargetIdx = 0;
static float    g_ppTargetLoc[3] = {};
static int      g_ppTargetHand = -1;
static double   g_ppTargetSinceMs = 0, g_ppFocusedLastMs = 0;
static bool     g_ppTargetFocused = false;
static uint8_t* g_ppBlock = NULL; static double g_ppBlockUntilMs = 0, g_ppBlockForMs = 0;   // doubles while the same item keeps refusing
static uint32_t g_ppTargetKind = kPpLoot;
// The aim: which point the trace looks at and from where. Variant 0 is the head looking at the
// target; while the game has not focused the target the variants are walked, three frames each.
static int      g_ppAimVariant = 0; static uint32_t g_ppAimFrames = 0;
static float    g_ppAimHand[3] = {}; static bool g_ppAimHandOk = false;
// Books and notes opened by a grip, left alone until a while after reading ends.
struct PpDone { uint8_t* obj; double untilMs; bool reading; };
static PpDone   g_ppDone[4] = {};
static uint8_t* g_ppLastTarget = NULL; static uint32_t g_ppLastTargetKind = kPpLoot; static double g_ppLastTargetMs = 0;
static uint32_t g_ppAdopted = 0;                       // times the game's own focus replaced the aimed target
static volatile LONG g_ppRayDriven = 0;

static bool PickupEnabled() { return g_ppOn.load(); }

static bool PpGameplay()
{
    return g_ppOn.load() && !g_gamepadOnly && CylTruthLive() && !g_menuOpen && !g_inMenu && !g_mainMenu && !g_cineNow;
}

// WHY THESE LOOPS ARE GUARDED INSTEAD OF CHECKED. The first build asked "is this readable"
// once per object (RegionMemo, which costs a VirtualQuery whenever the next object lies in
// another memory region - nearly always). 2000 objects a frame came to about 2.4 ms of the
// game thread every tick (pe/cost 160 us an event against 54), the camera-silent gate fired
// about once every two seconds instead of once in five minutes, and each firing left the
// RIGHT eye without an image for a present: a visible flicker (FLICKER_REFERENCE, 2026-10-05).
// GObjects entries are live objects on this thread - the collector runs on it too - so the
// loops read them directly and a structured exception handler is the backstop for a pointer
// that is not. Plain data only inside them.

// Is this name one of the lootable base names? 3 yes and readable, 2 yes, 1 no, 0 not known yet
// (the frame's budget of text lookups is spent: ask again in a later sweep).
static uint8_t PpNameVerdict(uint32_t nameIdx)
{
    PpNameSlot& slot = g_ppName[(nameIdx * 2654435761u) >> 19];
    if (slot.idx == nameIdx && slot.verdict) return slot.verdict;
    if (!g_ppNameBudget) return 0;
    --g_ppNameBudget;
    const char* n = RealName(nameIdx);
    const uint8_t v = !n ? 1 : !strcmp(n, "DisAbstractItemPickup") ? 3 : !strcmp(n, "DisDoor") ? 4
                    : (!strcmp(n, "DisPickup_Base") || !strcmp(n, "DisProjectile_Arrow")) ? 2 : 1;
    slot.idx = nameIdx; slot.verdict = v;
    return v;
}

// Does this class derive from a lootable base? Walks the SuperField chain, cached per class.
// 3 yes and readable, 2 yes, 1 no, 0 not known yet. Called only from inside the guarded loops.
static uint8_t PpClassLootableRaw(uint8_t* cls)
{
    PpClassSlot& slot = g_ppClass[(((uintptr_t)cls >> 4) * 2654435761u) >> 19];
    if (slot.cls == cls) return slot.verdict;
    uint8_t v = 1;
    uint8_t* c = cls;
    for (int depth = 0; c && depth < 32; ++depth) {
        if (((uintptr_t)c & 3) || (uintptr_t)c < 0x10000) break;
        const uint8_t nv = PpNameVerdict(*(uint32_t*)(c + kNameOff));
        if (nv == 0) return 0;                          // unknown: not cached, asked again next sweep
        if (nv >= 2) { v = nv; break; }                 // the readable base is met before DisPickup_Base on the way up
        c = *(uint8_t**)(c + kSuperFieldOff);
    }
    slot.cls = cls; slot.verdict = v;
    return v;
}

// One slice of the sweep. Returns the new count; *fault says the handler ran; *next is where the
// slice stopped (the time budget can end it early: the clock is read every 64 slots).
static uint32_t PpSweepSlice(void** objs, uint32_t from, uint32_t to, PpEntry* out, uint32_t n, bool* fault,
                             uint32_t* next, int64_t qpcDeadline)
{
    uint32_t i = from;
    __try {
        for (; i < to; ++i) {
            if ((i & 63u) == 63u) {
                LARGE_INTEGER t; QueryPerformanceCounter(&t);
                if (t.QuadPart >= qpcDeadline) { ++g_ppSliceCut; break; }
            }
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || (uintptr_t)o < 0x10000) continue;
            uint8_t* cls = *(uint8_t**)(o + kClassOff);
            if (!cls || ((uintptr_t)cls & 3) || (uintptr_t)cls < 0x10000) continue;
            const uint8_t v = PpClassLootableRaw(cls);
            if (v < 2) continue;
            if (n < kPpMax) { out[n].obj = o; out[n].idx = i; out[n].cls = cls; out[n].kind = v == 3 ? kPpReadable : v == 4 ? kPpDoor : kPpLoot; ++n; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { *fault = true; i = to; }
    *next = i;
    return n;
}

// The incremental sweep: `budget` GObjects slots a call.
static void PpSweep(void** objs, uint32_t num, uint32_t budget, double nowMs, int64_t qpcDeadline)
{
    if (g_ppCursor >= num) g_ppCursor = 0;
    if (g_ppCursor == 0) { g_ppBuildN = 0; g_ppSweepStartMs = nowMs; }
    const uint32_t end = (num - g_ppCursor > budget) ? g_ppCursor + budget : num;
    bool fault = false; uint32_t next = end;
    g_ppNameBudget = 24;                                 // class names turned into text this frame, at most
    g_ppBuildN = PpSweepSlice(objs, g_ppCursor, end, g_ppBuild, g_ppBuildN, &fault, &next, qpcDeadline);
    if (fault) ++g_ppFaults;                             // the rest of this slice is skipped until the next sweep
    g_ppCursor = next;
    if (g_ppCursor >= num) {
        g_ppCursor = 0;
        PpEntry* t = g_ppList; g_ppList = g_ppBuild; g_ppBuild = t;
        g_ppListN = g_ppBuildN; g_ppBuildN = 0;
        g_ppLastSweepMs = nowMs - g_ppSweepStartMs;
        // A class pointer can be reused by another class after a level change; the verdicts
        // are cheap to rebuild, so they do not outlive a few dozen sweeps.
        if ((g_ppSweeps & 63u) == 63u) { memset(g_ppClass, 0, sizeof(g_ppClass)); memset(g_ppName, 0, sizeof(g_ppName)); }
        if (++g_ppSweeps == 1)
            Log("pickup: first sweep done - %u lootable actor(s) among %u objects in %.0f ms of game time "
                "(up to %u slots and 60 us a frame, guarded direct reads, %u fault(s); a first sweep is slow on purpose: "
                "it may turn only 24 class names into text a frame)",
                g_ppListN, num, g_ppLastSweepMs, budget, g_ppFaults);
    }
}

// One chunk of the rotating near-list pass: entries still alive and within `radius2` of the camera
// (doors by their origin, the hinge, so they get `doorRadius2`).
static uint32_t PpNearSlice(void** objs, uint32_t num, const PpEntry* list, uint32_t from, uint32_t to, uint32_t locOff,
                            const float* camera, float radius2, float doorRadius2, PpEntry* out, uint32_t n, bool* fault)
{
    __try {
        for (uint32_t k = from; k < to; ++k) {
            const PpEntry& e = list[k];
            if (e.idx >= num || (uint8_t*)objs[e.idx] != e.obj) continue;
            if (*(uint8_t**)(e.obj + kClassOff) != e.cls) continue;
            float loc[3]; memcpy(loc, e.obj + locOff, 12);
            const float dx = loc[0] - camera[0], dy = loc[1] - camera[1], dz = loc[2] - camera[2];
            if (!(dx * dx + dy * dy + dz * dz <= (e.kind == kPpDoor ? doorRadius2 : radius2))) continue;
            if (n < kPpNearMax) out[n++] = e;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { *fault = true; }
    return n;
}

// What the per-frame pass needs to know, and what it found. Plain data.
struct PpQuery {
    uint32_t locOff, hiddenOff, hiddenMask, collOff, boundsOff, focusOff;
    float    reach[3];                 // by kind, game units
    bool     doors;
    uint8_t* held; uint8_t* blocked; uint8_t* done[4]; uint8_t* pc;
};
struct PpCand { uint8_t* obj; uint32_t idx; uint32_t kind; float aim[3]; float d2[2]; int hand; float keep; };
struct PpPick { PpCand best; PpCand focused; uint8_t* focus; bool fault; };

// The nearest listed item within reach of a hand, the engine's focused actor, and that actor's own
// entry if it is listed and within reach (so the game's choice can be taken over the aimed one).
static void PpNearest(void** objs, uint32_t num, const PpEntry* list, uint32_t n, const PpQuery* q,
                      const float (*hand)[3], const bool* handOk, PpPick* out)
{
    out->best.obj = NULL; out->focused.obj = NULL; out->focus = NULL; out->fault = false;
    float bestScore = 0;
    __try {
        if (q->pc && !((uintptr_t)q->pc & 3)) out->focus = *(uint8_t**)(q->pc + q->focusOff);
        for (uint32_t k = 0; k < n; ++k) {
            const PpEntry& e = list[k];
            if (e.idx >= num || (uint8_t*)objs[e.idx] != e.obj) continue;      // the slot moved on: not this actor any more
            if (*(uint8_t**)(e.obj + kClassOff) != e.cls) continue;
            if (e.kind == kPpDoor && !q->doors) continue;
            if (e.obj == q->done[0] || e.obj == q->done[1] || e.obj == q->done[2] || e.obj == q->done[3]) continue;
            const bool blockedNow = e.obj == q->blocked;        // not aimed at for now; the game's own focus on it still counts
            if (q->hiddenMask && (*(uint32_t*)(e.obj + q->hiddenOff) & q->hiddenMask)) continue;
            float loc[3]; memcpy(loc, e.obj + q->locOff, 12);
            if (!(loc[0] == loc[0]) || !(loc[1] == loc[1]) || !(loc[2] == loc[2])) continue;   // NaN
            // A door is its collision box (the origin is the hinge); everything else is its origin.
            bool box = false; float bo[3] = {}, be[3] = {};
            if (e.kind == kPpDoor && q->collOff && q->boundsOff) {
                uint8_t* comp = *(uint8_t**)(e.obj + q->collOff);
                if (comp && !((uintptr_t)comp & 3) && (uintptr_t)comp >= 0x10000) {
                    memcpy(bo, comp + q->boundsOff, 12); memcpy(be, comp + q->boundsOff + 12, 12);
                    box = bo[0] == bo[0] && be[0] == be[0] && be[0] >= 0 && be[1] >= 0 && be[2] >= 0 &&
                          be[0] < 2000 && be[1] < 2000 && be[2] < 2000;
                }
            }
            const float own = q->reach[e.kind], keep = own * 1.25f;            // the held target's hysteresis
            const float lim = (e.obj == q->held ? keep : own), lim2 = lim * lim;
            float d2h[2] = { 1e30f, 1e30f }, aimh[2][3] = {}; int nearHand = -1;
            for (int h = 0; h < 2; ++h) {
                if (!handOk[h]) continue;
                float pt[3];
                for (int a = 0; a < 3; ++a) {
                    if (!box) { pt[a] = loc[a]; continue; }
                    const float lo = bo[a] - be[a], hi = bo[a] + be[a];
                    pt[a] = hand[h][a] < lo ? lo : hand[h][a] > hi ? hi : hand[h][a];   // the box's nearest point
                }
                const float dx = pt[0] - hand[h][0], dy = pt[1] - hand[h][1], dz = pt[2] - hand[h][2];
                d2h[h] = dx * dx + dy * dy + dz * dz;
                for (int a = 0; a < 3; ++a) aimh[h][a] = box ? pt[a] + (bo[a] - pt[a]) * 0.08f : pt[a];   // a little inside the box
                if (nearHand < 0 || d2h[h] < d2h[nearHand]) nearHand = h;
            }
            if (nearHand < 0 || !(d2h[nearHand] <= lim2)) {
                // the engine's own focus is taken with the hysteresis reach as well
                if (e.obj != out->focus || nearHand < 0 || !(d2h[nearHand] <= keep * keep)) continue;
            }
            PpCand c; c.obj = e.obj; c.idx = e.idx; c.kind = e.kind; c.hand = nearHand; c.keep = keep;
            memcpy(c.aim, aimh[nearHand], 12); c.d2[0] = d2h[0]; c.d2[1] = d2h[1];
            if (e.obj == out->focus) out->focused = c;
            if (blockedNow || !(d2h[nearHand] <= lim2)) continue;
            // the held target keeps the choice unless another item is clearly nearer
            const float score = d2h[nearHand] * (e.obj == q->held ? 0.6f : 1.0f);
            if (!out->best.obj || score < bestScore) { out->best = c; bestScore = score; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { out->best.obj = NULL; out->focused.obj = NULL; out->fault = true; }
}

// A hand's position in game world units: the grip pose, scaled about the head the way the drawn
// hand is, through the same head-to-world mapping the published aim ray uses.
static bool PpHandWorld(int hand, const float camera[3], float out[3])
{
#if DVR_WITH_OPENXR
    const uint64_t now = GetTickCount64();
    const dvr::vr::HandAimSample s = dvr::vr::input_hand_aim_sample(hand);
    dvr::vr::HeadPose head;
    if (!s.gripValid || !dvr::vr::peek_head_pose(head)) return false;
    dvr::aim::FireFrame f;
    f.headValid = true;
    f.headPos[0] = head.px; f.headPos[1] = head.py; f.headPos[2] = head.pz;
    f.headQuat[0] = head.qx; f.headQuat[1] = head.qy; f.headQuat[2] = head.qz; f.headQuat[3] = head.qw;
    f.distanceM = 1.0f;
    float pos[3] = { s.gripPos[0], s.gripPos[1], s.gripPos[2] };
    const dvr::hands::TrimSnapshot cal = dvr::hands::trim_snapshot(hand);
    if (cal.ok && std::isfinite(cal.handToWorldScale) && cal.handToWorldScale > 0.2f && cal.handToWorldScale < 5.0f)
        for (int i = 0; i < 3; ++i) pos[i] = f.headPos[i] + cal.handToWorldScale * (pos[i] - f.headPos[i]);
    f.ray = dvr::aim::from_pose(hand, true, pos, s.gripQuat, s.generation, s.stampMs, now);
    dvr::fireaim::Solution sol;
    if (!dvr::fireaim::solve(f, now, g_viewYawRad, g_viewPitchRad, camera, g_posScaleUU, camera, sol)) return false;
    memcpy(out, sol.origin, 12);
    return true;
#else
    (void)hand; (void)camera; (void)out;
    return false;
#endif
}

// interact_aim.cpp asks this before its own ray: while a target is held, the engine's
// interaction trace looks at the target. Variant 0 looks from the head at the target's point;
// the others (walked only while the game has not focused the target) look from the hand, and at
// points 7 units to each side and above: enough to find the exposed edge of a book that lies
// under another one.
static bool PickupRay(float* origin, float* dir)
{
    if (!g_ppTarget || !PpGameplay()) return false;
    float from[3];
    if (!GameCameraAnchor(from)) return false;
    static const float kOff[6][3] = { {0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1} };
    const int v = g_ppAimVariant;
    const float* o = kOff[(v >> 1) % 6];
    const float to[3] = { g_ppTargetLoc[0] + o[0] * 7.0f, g_ppTargetLoc[1] + o[1] * 7.0f, g_ppTargetLoc[2] + o[2] * 7.0f };
    if ((v & 1) && g_ppAimHandOk) {
        const float hx = to[0] - g_ppAimHand[0], hy = to[1] - g_ppAimHand[1], hz = to[2] - g_ppAimHand[2];
        if (hx * hx + hy * hy + hz * hz > 36.0f) memcpy(from, g_ppAimHand, 12);   // not from a hand already on the item
    }
    float d[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
    if (!dvr::fireaim::normalize(d)) return false;
    memcpy(origin, from, 12); memcpy(dir, d, 12);
    InterlockedIncrement(&g_ppRayDriven);
    return true;
}

static void PpDropTarget(const char* why, double nowMs, bool block)
{
    if (g_ppTarget) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 500,
            "pickup: target released (%s) after %.0f ms, engine focus on it %s", why, nowMs - g_ppTargetSinceMs,
            g_ppTargetFocused ? "YES" : "never");
        if (block) {
            g_ppBlockForMs = (g_ppBlock == g_ppTarget && g_ppBlockForMs > 0) ? (g_ppBlockForMs < 30000.0 ? g_ppBlockForMs * 2.0 : 60000.0) : 3000.0;
            g_ppBlock = g_ppTarget; g_ppBlockUntilMs = nowMs + g_ppBlockForMs;
        }
    }
    g_ppTarget = NULL; g_ppTargetHand = -1; g_ppTargetFocused = false;
    g_ppReadyMask.store(0);
}

// Script lane, once a frame.
static void PhysicalPickupTick()
{
    static uint32_t lastFrame = 0xffffffffu;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame == lastFrame) return;
    lastFrame = frame;
    const double now = MaimNowMs();
    {
        static double lastNow = 0; static uint32_t silentWas = 0xffffffffu;
        g_ppRingAt = (g_ppRingAt + 1) & 7;
        g_ppRingGapMs[g_ppRingAt] = lastNow > 0 ? (float)(now - lastNow) : 0.0f;
        g_ppRingCostUs[g_ppRingAt] = 0.0f;
        lastNow = now;
        if (silentWas == 0xffffffffu) silentWas = g_sdSkipSilent;
        if (g_sdSkipSilent != silentWas) {
            silentWas = g_sdSkipSilent;
            const float* g = g_ppRingGapMs; const float* c = g_ppRingCostUs; const uint32_t a = g_ppRingAt;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 400,
                "pickup/silent: camera-silent single draw #%u. The 8 game frames up to now, oldest first - gap since the "
                "frame before in ms: %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f | this module's cost in us: %.0f %.0f %.0f %.0f "
                "%.0f %.0f %.0f (the newest frame has not run yet). Pickup %s. A long gap with small costs is not this module",
                (unsigned)silentWas,
                g[(a + 1) & 7], g[(a + 2) & 7], g[(a + 3) & 7], g[(a + 4) & 7], g[(a + 5) & 7], g[(a + 6) & 7], g[(a + 7) & 7], g[a],
                c[(a + 1) & 7], c[(a + 2) & 7], c[(a + 3) & 7], c[(a + 4) & 7], c[(a + 5) & 7], c[(a + 6) & 7], c[(a + 7) & 7],
                g_ppOn.load() ? "ON" : "off");
        }
    }
    {   // A grip just opened a book or note: it is not a target again until 15 s after reading ends,
        // so what lies under it can be reached. Runs outside gameplay too (the page opens at once).
        static uint32_t firedWas = 0; static double lastGameplayMs = 0;
        const uint32_t fired = g_ppFired.load();
        if (fired != firedWas) {
            firedWas = fired;
            if (g_ppLastTarget && g_ppLastTargetKind == kPpReadable && now - g_ppLastTargetMs < 600.0) {
                int slot = 0;
                for (int i = 1; i < 4; ++i) if (g_ppDone[i].untilMs < g_ppDone[slot].untilMs) slot = i;
                g_ppDone[slot].obj = g_ppLastTarget; g_ppDone[slot].untilMs = now + 4000.0; g_ppDone[slot].reading = true;
                Log("pickup: a book or note was opened by a grip - it is not a target again until 15 s after the reading ends, "
                    "so an item under it can be reached");
            }
        }
        const bool gameplayNow = PpGameplay();
        if (gameplayNow) {
            if (now - lastGameplayMs > 300.0)                  // back from a page, a menu or a load
                for (PpDone& d : g_ppDone) if (d.reading) { d.reading = false; d.untilMs = now + 15000.0; }
            lastGameplayMs = now;
        } else {
            for (PpDone& d : g_ppDone) if (d.reading) d.untilMs = now + 15000.0;   // still reading
        }
    }
    if (!PpGameplay()) { if (g_ppTarget) PpDropTarget("not in gameplay, or off", now, false); g_ppReadyMask.store(0); return; }

    // This module's own cost, every frame it does work: the number the first build lacked.
    struct PpCost {
        LARGE_INTEGER f, a;
        PpCost() { QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a); }
        ~PpCost() {
            LARGE_INTEGER b; QueryPerformanceCounter(&b);
            const double us = (double)(b.QuadPart - a.QuadPart) * 1e6 / (double)f.QuadPart;
            g_ppCostUsSum += us; ++g_ppCostN; if (us > g_ppCostUsMax) g_ppCostUsMax = us;
            g_ppRingCostUs[g_ppRingAt] = (float)us;
            if (us > 250.0) ++g_ppCostOver250;
            if (us > 1000.0) ++g_ppCostOver1000;
        }
    } cost;

    void** objs = *(void***)kGObjHdr;                       // the image's own data: always readable
    const uint32_t num = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || num < 1000 || num > 4000000) { if (g_ppTarget) PpDropTarget("no object table", now, false); return; }
    dvr::crash::probe_begin();                              // a fault inside the guarded loops is not a crash
    PpSweep(objs, num, 500, now, cost.a.QuadPart + cost.f.QuadPart * 60 / 1000000);   // at most 60 us of sweep a frame
    dvr::crash::probe_end();

    static uint32_t locOff = 0, focusOff = 0, hiddenOff = 0, hiddenMask = 0, collOff = 0, boundsOff = 0; static bool hiddenAsked = false;
    if (!locOff)   locOff   = RflOffsetOf("Actor", "Location");
    if (!hiddenAsked && locOff) {
        hiddenAsked = true;
        if (!FindBoolProp("Actor", "bHidden", &hiddenOff, &hiddenMask)) { hiddenOff = 0; hiddenMask = 0; }
        collOff = RflOffsetOf("Actor", "CollisionComponent");
        boundsOff = RflOffsetOf("PrimitiveComponent", "Bounds");
        Log("pickup: Actor.Location at +0x%X, Actor.bHidden %s (a hidden pickup is never a target), Actor.CollisionComponent "
            "+0x%X and PrimitiveComponent.Bounds +0x%X (a door is measured from that box; 0 = not resolved, the hinge is used)",
            locOff, hiddenMask ? "resolved" : "NOT resolved - hidden items are left to the game's refusal", collOff, boundsOff);
    }
    if (!focusOff) focusOff = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairActor");
    float camera[3], hand[2][3];
    bool handOk[2] = { false, false };
    if (!locOff || !focusOff || !GameCameraAnchor(camera)) { if (g_ppTarget) PpDropTarget("no camera or offsets", now, false); return; }
    for (int h = 0; h < 2; ++h) handOk[h] = PpHandWorld(h, camera, hand[h]);
    if (!handOk[0] && !handOk[1]) { if (g_ppTarget) PpDropTarget("no tracked hand", now, false); return; }

    PpQuery q;
    q.locOff = locOff; q.hiddenOff = hiddenOff; q.hiddenMask = hiddenMask; q.collOff = collOff; q.boundsOff = boundsOff; q.focusOff = focusOff;
    q.reach[kPpLoot] = g_ppReachM.load() * g_posScaleUU;          // game units
    q.reach[kPpReadable] = g_ppBookReachM.load() * g_posScaleUU;
    q.reach[kPpDoor] = g_ppDoorReachM.load() * g_posScaleUU;
    q.doors = g_ppDoorsOn.load();
    q.held = g_ppTarget; q.blocked = now < g_ppBlockUntilMs ? g_ppBlock : NULL; q.pc = g_peCtrl;
    for (int i = 0; i < 4; ++i) q.done[i] = now < g_ppDone[i].untilMs ? g_ppDone[i].obj : NULL;
    PpPick pick;
    dvr::crash::probe_begin();
    {   // the rotating near-list pass: 192 listed items a frame, those within 2.5 m of the camera kept (doors 4 m)
        if (g_ppNearSrc != g_ppList || g_ppNearCursor > g_ppListN) { g_ppNearSrc = g_ppList; g_ppNearCursor = 0; g_ppNearBuildN = 0; }
        const uint32_t to = (g_ppListN - g_ppNearCursor > 192u) ? g_ppNearCursor + 192u : g_ppListN;
        const float radius = 2.5f * g_posScaleUU, doorRadius = 4.0f * g_posScaleUU;
        bool fault = false;
        g_ppNearBuildN = PpNearSlice(objs, num, g_ppList, g_ppNearCursor, to, locOff, camera, radius * radius, doorRadius * doorRadius,
                                     g_ppNearBuild, g_ppNearBuildN, &fault);
        if (fault) ++g_ppFaults;
        g_ppNearCursor = to;
        if (g_ppNearCursor >= g_ppListN) {
            PpEntry* t = g_ppNear; g_ppNear = g_ppNearBuild; g_ppNearBuild = t;
            g_ppNearN = g_ppNearBuildN; g_ppNearBuildN = 0; g_ppNearCursor = 0;
        }
    }
    PpNearest(objs, num, g_ppNear, g_ppNearN, &q, hand, handOk, &pick);
    dvr::crash::probe_end();
    if (pick.fault) ++g_ppFaults;

    // What the game highlights is what the grip takes: its own focus, when that is a listed item
    // within reach, wins over the item this module would have aimed at.
    const bool adopt = pick.focused.obj && pick.focused.obj != g_ppTarget;
    const PpCand* c = pick.focused.obj ? &pick.focused : pick.best.obj ? &pick.best : NULL;
    if (!c) { if (g_ppTarget) PpDropTarget("nothing within reach", now, false); return; }

    static const char* const kKind[3] = { "", " (a book or note: its own reach, and the page goes to the hand that opens it)",
                                          " (a door: measured from its collision box)" };
    if (c->obj != g_ppTarget) {
        if (g_ppTarget) PpDropTarget(adopt ? "the game focused another item within reach: that one is taken" : "a nearer item", now, false);
        if (adopt) ++g_ppAdopted;
        g_ppTarget = c->obj; g_ppTargetIdx = c->idx; g_ppTargetSinceMs = now; g_ppFocusedLastMs = 0; g_ppTargetFocused = false;
        g_ppAimVariant = 0; g_ppAimFrames = 0;
        const char* cn = ObjClassName(c->obj);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 400,
            "pickup: target %s%s at %.0f uu (%.0f cm) from the %s hand, reach %.0f cm%s - the interaction trace now looks at it; "
            "the grip takes it once the game focuses it",
            cn ? cn : "?", kKind[c->kind], sqrtf(c->d2[c->hand]), sqrtf(c->d2[c->hand]) / g_posScaleUU * 100.0f,
            c->hand ? "RIGHT" : "LEFT", q.reach[c->kind] / g_posScaleUU * 100.0f,
            adopt ? " (the game's own choice)" : "");
    }
    memcpy(g_ppTargetLoc, c->aim, 12);
    g_ppTargetHand = c->hand; g_ppTargetKind = c->kind;
    g_ppTargetReadable.store(c->kind == kPpReadable);
    g_ppAimHandOk = handOk[c->hand]; if (g_ppAimHandOk) memcpy(g_ppAimHand, hand[c->hand], 12);
    g_ppLastTarget = c->obj; g_ppLastTargetKind = c->kind; g_ppLastTargetMs = now;

    // The engine's verdict: is its focused actor our target?
    uint8_t* focus = pick.focus;
    if (focus == g_ppTarget) { g_ppFocusedLastMs = now; g_ppTargetFocused = true; g_ppAimFrames = 0; }
    else {
        // Not focused: try the next aim (three frames each, twelve of them), then give up for a while.
        if (++g_ppAimFrames >= 3) { g_ppAimFrames = 0; g_ppAimVariant = (g_ppAimVariant + 1) % 12; }
        if (now - (g_ppFocusedLastMs > 0 ? g_ppFocusedLastMs : g_ppTargetSinceMs) > 700.0) {
            PpDropTarget("the game did not focus it from any of the aims (looted, hidden, covered or blocked) - left alone for 3 s, "
                         "doubling each time", now, true);
            return;
        }
    }
    uint32_t mask = 0;
    if (focus == g_ppTarget)
        for (int h = 0; h < 2; ++h) if (handOk[h] && c->d2[h] <= c->keep * c->keep) mask |= 1u << h;
    g_ppReadyMask.store(mask);
    g_ppReadyMs.store(GetTickCount64());

    static double nextBeat = 0;
    if (now >= nextBeat) {
        nextBeat = now + 10000;
        Log("pickup: beat - %u lootable actor(s) listed (%u within 2.5 m), sweep %u took %.0f ms of game time, trace driven %ld time(s), "
            "grips swallowed %u, Interact pressed %u, reach %.0f cm | own cost %.1f us a frame, max %.0f, over 250 us in %u and "
            "over 1000 us in %u of %u frames (a frame of several ms on the game thread alone is what starves an eye: both "
            "counts must read 0 or close to it), slices cut by the 60 us budget %u, guarded-read faults %u | doors %s, "
            "the game's own focus taken over the aimed item %u time(s)",
            g_ppListN, g_ppNearN, g_ppSweeps, g_ppLastSweepMs, (long)g_ppRayDriven, g_ppSwallowed.load(), g_ppFired.load(),
            g_ppReachM.load() * 100.0f, g_ppCostN ? g_ppCostUsSum / g_ppCostN : 0.0, g_ppCostUsMax, g_ppCostOver250,
            g_ppCostOver1000, g_ppCostN, g_ppSliceCut, g_ppFaults, g_ppDoorsOn.load() ? "ON" : "off", g_ppAdopted);
        g_ppCostUsSum = 0; g_ppCostUsMax = 0; g_ppCostN = 0; g_ppCostOver250 = 0; g_ppCostOver1000 = 0; g_ppSliceCut = 0;
    }
}

// Pad bridge (present thread). `raw` is the PHYSICAL snapshot about to be remapped: a grip that
// starts a pickup is zeroed in it until released, so whatever is bound to that grip does not
// fire. Returns true while Interact should be held down.
static bool PickupPadFilter(dvr::vr::InputSnapshot& raw, bool blocked)
{
    static bool was[2] = { false, false }, swallow[2] = { false, false };
    static uint64_t pressUntil = 0;
    const uint64_t now = GetTickCount64();
    const bool fresh = now - g_ppReadyMs.load() <= 250;
    const uint32_t mask = (g_ppOn.load() && !blocked && fresh) ? g_ppReadyMask.load() : 0;
    float* grip[2] = { &raw.gripL, &raw.gripR };
    for (int h = 0; h < 2; ++h) {
        const bool down = *grip[h] > (was[h] ? 0.7f : 0.9f);
        if (down && !was[h] && (mask & (1u << h))) {
            swallow[h] = true; pressUntil = now + 130;
            g_ppSwallowed.fetch_add(1); g_ppFired.fetch_add(1);
            const bool page = g_ppTargetReadable.load();
            if (page) dvr::hudlayout::note_opened_by_hand(h);     // the reading panel attaches to this hand
            Log("pickup: %s grip pressed with the target in reach and focused - grip swallowed, Interact pressed for 130 ms%s",
                h ? "RIGHT" : "LEFT", page ? "; a book or note: its page is asked onto this hand" : "");
        }
        if (!down) swallow[h] = false;
        was[h] = down;
        if (swallow[h]) *grip[h] = 0.0f;
    }
    return now < pressUntil;
}

static void PickupSet(bool on, const char* who)
{
    g_ppOn.store(on);
    if (on) InteractAimInstall();                       // the two bridges carry the target's ray
    if (!on) g_ppReadyMask.store(0);
    Log("pickup: physical pickup %s (%s) - reach %.0f cm, books and notes %.0f cm. Reach a hand to loot and squeeze its "
        "grip; the Interact button is unchanged. Bridges first-pass=%s selector=%s",
        on ? "ON" : "off", who, g_ppReachM.load() * 100.0f, g_ppBookReachM.load() * 100.0f, g_iaFirstDet.on ? "ready" : "NOT installed",
        g_iaSelDet.on ? "ready" : "NOT installed");
}

static void PickupSetReachCm(float cm)
{
    if (!std::isfinite(cm)) return;
    cm = cm < 10.0f ? 10.0f : cm > 80.0f ? 80.0f : cm;
    g_ppReachM.store(cm / 100.0f);
}
static float PickupReachCm() { return g_ppReachM.load() * 100.0f; }
static void PickupSetBookReachCm(float cm)
{
    if (!std::isfinite(cm)) return;
    cm = cm < 10.0f ? 10.0f : cm > 100.0f ? 100.0f : cm;
    g_ppBookReachM.store(cm / 100.0f);
}
static float PickupBookReachCm() { return g_ppBookReachM.load() * 100.0f; }
static bool PickupDoorsEnabled() { return g_ppDoorsOn.load(); }
static void PickupSetDoors(bool on, const char* who)
{
    g_ppDoorsOn.store(on);
    Log("pickup: doors by grabbing %s (%s) - a hand within %.0f cm of a door's collision box and a grip opens or closes it; "
        "needs physical pickup on (now %s)", on ? "ON" : "off", who, g_ppDoorReachM.load() * 100.0f, g_ppOn.load() ? "ON" : "off");
}
static void PickupSetDoorReachCm(float cm)
{
    if (!std::isfinite(cm)) return;
    cm = cm < 5.0f ? 5.0f : cm > 80.0f ? 80.0f : cm;
    g_ppDoorReachM.store(cm / 100.0f);
}
static float PickupDoorReachCm() { return g_ppDoorReachM.load() * 100.0f; }

static void PickupConfigure(const char* ini)
{
    PickupSetReachCm(IniFloat(ini, "Aim", "PhysicalPickupReachCm", 30));
    PickupSetBookReachCm(IniFloat(ini, "Aim", "PhysicalPickupBookReachCm", 45));
    PickupSetDoorReachCm(IniFloat(ini, "Aim", "PhysicalDoorReachCm", 20));
    PickupSetDoors(IniFloat(ini, "Aim", "PhysicalDoors", 1) != 0.0f, "ini [Aim] PhysicalDoors");
    PickupSet(IniFloat(ini, "Aim", "PhysicalPickup", 1) != 0.0f, "ini [Aim] PhysicalPickup");
}

static bool PickupCommand(const char* args)
{
    bool b = false; float cm = 0;
    if (!strncmp(args, "doors ", 6) && DvrOnOff(args + 6, &b)) {
        PickupSetDoors(b, "seam");
        ConfigWriteKey("Aim", "PhysicalDoors", b ? "1" : "0", "the seam");
        return true;
    }
    if (sscanf(args, "doorreach %f", &cm) == 1) {
        PickupSetDoorReachCm(cm);
        char v[16]; _snprintf(v, sizeof(v), "%.0f", PickupDoorReachCm()); v[sizeof(v) - 1] = 0;
        ConfigWriteKey("Aim", "PhysicalDoorReachCm", v, "the seam");
        return true;
    }
    if (DvrOnOff(args, &b)) {
        PickupSet(b, "seam");
        ConfigWriteKey("Aim", "PhysicalPickup", b ? "1" : "0", "the seam");
        return true;
    }
    if (sscanf(args, "reach %f", &cm) == 1) {
        PickupSetReachCm(cm);
        char v[16]; _snprintf(v, sizeof(v), "%.0f", PickupReachCm()); v[sizeof(v) - 1] = 0;
        ConfigWriteKey("Aim", "PhysicalPickupReachCm", v, "the seam");
    } else if (sscanf(args, "bookreach %f", &cm) == 1) {
        PickupSetBookReachCm(cm);
        char v[16]; _snprintf(v, sizeof(v), "%.0f", PickupBookReachCm()); v[sizeof(v) - 1] = 0;
        ConfigWriteKey("Aim", "PhysicalPickupBookReachCm", v, "the seam");
    }
    Log("pickup: on|off, reach <cm>, bookreach <cm>, doors on|off, doorreach <cm> (now %s, reach %.0f cm, books and notes %.0f cm, "
        "doors %s at %.0f cm) | listed %u, sweeps %u (last %.0f ms), target %s (hand %d, focused %s), "
        "trace driven %ld, grips swallowed %u, Interact pressed %u, ready mask %u",
        g_ppOn.load() ? "ON" : "off", PickupReachCm(), PickupBookReachCm(), g_ppDoorsOn.load() ? "ON" : "off", PickupDoorReachCm(),
        g_ppListN, g_ppSweeps, g_ppLastSweepMs,
        g_ppTarget ? "held" : "none", g_ppTargetHand, g_ppTargetFocused ? "yes" : "no", (long)g_ppRayDriven,
        g_ppSwallowed.load(), g_ppFired.load(), g_ppReadyMask.load());
    return true;
}
