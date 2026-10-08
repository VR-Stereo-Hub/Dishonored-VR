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
//   3. THE GRIP. While the engine's focused actor is a listed thing within a palm's reach (the
//      target, or another listed thing the trace or the reticle landed on: PpGate), a press of
//      that hand's PHYSICAL grip is swallowed (the power wheel or the block bound to it does not
//      fire) and Interact is pressed for a moment instead (pad_bridge.cpp).
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
// The wider list (step 2 of PLAN-physical-interaction.md): every class that declares the game's
// DisInteractableInterface, in groups, each found by its base name on the class chain:
//   loot      DisPickup_Base, DisProjectile (bolts and darts to recover), DisRiverKrust, DisUpgrade
//   carry     DishonoredMovable, DisGrenade, DisWhiskeyBottle, DisMovableLimb, DisDLC07SkeletalMovable
//   usable    DishonoredUsableObject (levers, valves, switches; DisDoor keeps its own group),
//             DisProjectileLauncher, DisClimbable (chains), the placed traps (spring razor, arc
//             mine, tripwire)
// and NEVER a pawn or a talk target: DishonoredPawn, Pawn, DisDialogInanimateDummy, DisSpeaker_PA,
// GameCrowdAgent and DisTrigger stop the chain walk with a "no", so talking, chokes and takedowns
// cannot be offered. Carry and usable things are measured from their collision box, like doors.
// While a movable is carried nothing is targeted (the grip goes back to its own action); while a
// body is carried only the hand on the free lane is (the upper-body lane holds the body).
// A lever or valve that wants Interact HELD gets it for as long as the grip stays down on it.
//
// Tried and taken out again (2026-10-05, reported worse in the headset): taking the game's own
// focus as the target, leaving a grip-opened book out for a while, and walking several aims at
// an unfocused target, all meant to reach the lower of two stacked books. The lower book is
// opened by pointing at it. ENGINE_NOTES, "Physical pickup".
//
// [Aim] PhysicalPickup (default 1), PhysicalPickupReachCm (60), PhysicalPickupBookReachCm (74),
// PhysicalDoors (1), PhysicalDoorReachCm (20), PhysicalCarry (1), PhysicalUsables (1); carry and
// usable things share the door reach. F10 > Aim. Seam: `pickup`.
// Script lane for 1 and 2 (the interaction bridges run on the same game thread); the pad
// bridge reads two atomics.

#define DVR_CAT ::dvr::log::Cat::script

static std::atomic<bool>     g_ppOn{true};
static std::atomic<float>    g_ppReachM{0.45f};
static std::atomic<float>    g_ppBookReachM{0.55f};   // books, notes, audio logs
static std::atomic<bool>     g_ppTargetReadable{false};   // the target is one of those (for the pad bridge)
static std::atomic<bool>     g_ppDoorsOn{true};       // [Aim] PhysicalDoors
static std::atomic<float>    g_ppDoorReachM{0.35f};   // from the palm to the box of a door, carried thing or usable
static std::atomic<bool>     g_ppCarryOn{true};       // [Aim] PhysicalCarry: things carried and thrown
static std::atomic<bool>     g_ppUsablesOn{true};     // [Aim] PhysicalUsables: levers, switches, chains, traps
static std::atomic<uintptr_t> g_ppTargetId{0};        // the target (for the pad bridge's hold)
static std::atomic<bool>     g_ppTargetHold{false};   // the target is a usable: Interact follows the grip
static std::atomic<uint32_t> g_ppReadyMask{0};        // bit h: hand h may pick the target up now
static std::atomic<uint64_t> g_ppReadyMs{0};          // when the mask was last written
static std::atomic<uint32_t> g_ppFired{0}, g_ppSwallowed{0};

// readable: a book, note or audio log. Kinds from kPpDoor on are measured from their collision box.
enum : uint32_t { kPpLoot = 0, kPpReadable = 1, kPpDoor = 2, kPpCarry = 3, kPpUsable = 4, kPpKinds = 5 };
static const char* const kPpKindName[kPpKinds] = { "loot", "book", "door", "carry", "usable" };
static uint32_t g_ppTargetsByKind[kPpKinds] = {};      // targets taken, by kind, since the last beat
static uint32_t g_ppPawnTargets = 0;                   // targets whose class chain names a pawn or talk class: must stay 0
static uint32_t g_ppPawnTargetsTotal = 0;
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
// 3 = readable loot: DisAbstractItemPickup, 4 = a door: DisDoor, 5 = carry, 6 = usable,
// 7 = EXCLUDED: a pawn or talk class, which ends the walk with a no). Kind = verdict - 2.
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
// Objects the engine did not focus when aimed at: left out for a moment so the next nearest can
// be tried. Short and per object (it used to be one object, 3 s doubling to a minute: an item
// refused once while the hand passed at a bad angle then stayed dead, which felt finicky).
struct PpBlocked { uint8_t* obj; double untilMs; };
static PpBlocked g_ppBlocked[8] = {};
static void PpBlock(uint8_t* obj, double nowMs, double forMs)
{
    int slot = 0;
    for (int i = 0; i < 8; ++i) {
        if (g_ppBlocked[i].obj == obj) { slot = i; break; }
        if (g_ppBlocked[i].untilMs < g_ppBlocked[slot].untilMs) slot = i;
    }
    g_ppBlocked[slot].obj = obj; g_ppBlocked[slot].untilMs = nowMs + forMs;
}
static float g_ppTargetFrom[3] = {};                    // where the engine's check traces FROM: the target hand's palm
static uint32_t g_ppTargetKind = kPpLoot;
// THE GRIP'S OBJECT (2026-10-07): what a grip takes is whatever the ENGINE focuses (Interact acts on
// the focus), so the pad bridge reads the focused item's kind, not the target's. Published by PpGate.
static std::atomic<uint32_t> g_ppGripKind{kPpLoot};
// Why each hand's grip is or is not taken right now, with the values, for the pad bridge's refused-
// grip line (present thread). Class names come from GNames and stay valid; written by PpGate only.
static std::atomic<const char*> g_ppGateWhy[2] = { {"not asked yet"}, {"not asked yet"} };
static std::atomic<uintptr_t>   g_ppDiagTarget{0}, g_ppDiagFocus{0};
static std::atomic<const char*> g_ppDiagTargetCls{""}, g_ppDiagFocusCls{""};
static std::atomic<int>         g_ppDiagTargetHand{-1}, g_ppDiagFocusKind{-1};   // -1: not listed
static std::atomic<float>       g_ppDiagFocusCm[2] = { {-1.0f}, {-1.0f} }, g_ppDiagReachCm{0.0f};
static uint32_t g_ppGateByFocus = 0;                   // gate passes on a focused thing that is not the target, since the beat
// 2026-10-05: the hand whose grip last pressed Interact on a carryable, and when (throw_aim.cpp makes it
// the carry hand if a carry starts within 2 s).
static std::atomic<int> g_ppCarryGripHand{-1};
static std::atomic<uint64_t> g_ppCarryGripMs{0};
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

// The base names, by verdict (see g_ppName): the classes that declare DisInteractableInterface in
// the script corpus, grouped. The first base met walking UP the chain wins, so DisDoor is met
// before DishonoredUsableObject and DisAbstractItemPickup before DisPickup_Base.
static uint8_t PpBaseVerdict(const char* n)
{
    if (!n) return 1;
    static const char* const kExcluded[] = { "DishonoredPawn", "Pawn", "DisDialogInanimateDummy", "DisSpeaker_PA",
                                             "GameCrowdAgent", "DisTrigger" };
    for (const char* x : kExcluded) if (!strcmp(n, x)) return 7;
    if (!strcmp(n, "DisAbstractItemPickup")) return 3;
    if (!strcmp(n, "DisDoor")) return 4;
    static const char* const kLoot[] = { "DisPickup_Base", "DisProjectile", "DisRiverKrust", "DisUpgrade" };
    for (const char* x : kLoot) if (!strcmp(n, x)) return 2;
    static const char* const kCarry[] = { "DishonoredMovable", "DisGrenade", "DisWhiskeyBottle", "DisMovableLimb",
                                          "DisDLC07SkeletalMovable" };
    for (const char* x : kCarry) if (!strcmp(n, x)) return 5;
    static const char* const kUsable[] = { "DishonoredUsableObject", "DisProjectileLauncher", "DisClimbable",
                                           "DisGadget_SpringRazorPlaced", "DisDLC06Gadget_ArcMinePlaced", "DisTripwire" };
    for (const char* x : kUsable) if (!strcmp(n, x)) return 6;
    return 1;
}

// Is this name one of the base names? The verdict above, or 0 not known yet (the frame's budget of
// text lookups is spent: ask again in a later sweep).
static uint8_t PpNameVerdict(uint32_t nameIdx)
{
    PpNameSlot& slot = g_ppName[(nameIdx * 2654435761u) >> 19];
    if (slot.idx == nameIdx && slot.verdict) return slot.verdict;
    if (!g_ppNameBudget) return 0;
    --g_ppNameBudget;
    const uint8_t v = PpBaseVerdict(RealName(nameIdx));
    slot.idx = nameIdx; slot.verdict = v;
    return v;
}

// The independent check behind the pawn counter: does any class on this chain have "Pawn" in its
// name, or is it a talk class? Text compares, so it does not share the verdict table it checks.
// Called once per new target. Returns the name that matched, or NULL.
static const char* PpChainNamesPawnRaw(uint8_t* cls)
{
    __try {
        for (int depth = 0; cls && depth < 32; ++depth) {
            if (((uintptr_t)cls & 3) || (uintptr_t)cls < 0x10000) break;
            const char* n = RealName(*(uint32_t*)(cls + kNameOff));
            if (n && (strstr(n, "Pawn") || !strcmp(n, "DisDialogInanimateDummy") || !strcmp(n, "DisSpeaker_PA"))) return n;
            cls = *(uint8_t**)(cls + kSuperFieldOff);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return NULL;
}

// Does this class derive from a listed base? Walks the SuperField chain, cached per class.
// The verdict of the first base met (2..6 listed, 7 excluded), 1 no, 0 not known yet. Called only
// from inside the guarded loops.
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
        if (nv >= 2) { v = nv; break; }                 // the first base met on the way up decides (an excluded one says no)
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
            if (v < 2 || v > 6) continue;
            if (n < kPpMax) { out[n].obj = o; out[n].idx = i; out[n].cls = cls; out[n].kind = v - 2u; ++n; }
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
            Log("pickup: first sweep done - %u interactable actor(s) among %u objects in %.0f ms of game time "
                "(up to %u slots and 60 us a frame, guarded direct reads, %u fault(s); a first sweep is slow on purpose: "
                "it may turn only 24 class names into text a frame)",
                g_ppListN, num, g_ppLastSweepMs, budget, g_ppFaults);
    }
}

// One chunk of the rotating near-list pass: entries still alive and within `radius2` of the camera
// (box-measured kinds by their origin, which can be a hinge or an end, so they get `doorRadius2`).
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
            if (!(dx * dx + dy * dy + dz * dz <= (e.kind >= kPpDoor ? doorRadius2 : radius2))) continue;
            if (n < kPpNearMax) out[n++] = e;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { *fault = true; }
    return n;
}

// What the per-frame pass needs to know, and what it found. Plain data.
struct PpQuery {
    uint32_t locOff, hiddenOff, hiddenMask, collOff, boundsOff, focusOff;
    float    reach[kPpKinds];          // by kind, game units
    bool     kindOn[kPpKinds];         // by kind: its F10 switch
    uint8_t* held; uint8_t* pc;
    uint8_t* blocked[8];
};
struct PpCand { uint8_t* obj; uint32_t idx; uint32_t kind; float aim[3]; float d2[2]; int hand; float keep; };
// The engine's focused actor as the list sees it: listed (an interactable of an enabled kind in the
// near list) or not, and each palm's distance to its box. The grip gate reads this (2026-10-07).
struct PpFocus { uint8_t* obj; bool listed; uint32_t kind; float d2[2]; };
struct PpPick { PpCand best; uint8_t* focus; PpFocus f; bool fault; };

// EACH HAND'S nearest listed thing within reach, the better of the two as the pick, and the
// engine's focused actor. Distances run from the PALM to the nearest point of the thing's
// collision box (Half-Life: Alyx's rule: the surface your hand is near, not a centre), and are
// compared as a fraction of that kind's reach so a door and a coin compete fairly.
static void PpNearest(void** objs, uint32_t num, const PpEntry* list, uint32_t n, const PpQuery* q,
                      const float (*hand)[3], const bool* handOk, PpPick* out)
{
    out->best.obj = NULL; out->focus = NULL; out->fault = false;
    out->f.obj = NULL; out->f.listed = false; out->f.kind = 0; out->f.d2[0] = out->f.d2[1] = 1e30f;
    PpCand bestH[2]; float scoreH[2] = { 1e30f, 1e30f }; bestH[0].obj = bestH[1].obj = NULL;
    __try {
        if (q->pc && !((uintptr_t)q->pc & 3)) out->focus = *(uint8_t**)(q->pc + q->focusOff);
        out->f.obj = out->focus;
        for (uint32_t k = 0; k < n; ++k) {
            const PpEntry& e = list[k];
            if (e.idx >= num || (uint8_t*)objs[e.idx] != e.obj) continue;      // the slot moved on: not this actor any more
            if (*(uint8_t**)(e.obj + kClassOff) != e.cls) continue;
            if (e.kind >= kPpKinds || !q->kindOn[e.kind]) continue;
            bool blocked = false;
            for (int b = 0; b < 8; ++b) if (q->blocked[b] == e.obj) blocked = true;
            const bool isFocus = e.obj == out->focus;
            if (blocked && !isFocus) continue;                                  // a blocked item the engine focuses is still measured
            if (q->hiddenMask && (*(uint32_t*)(e.obj + q->hiddenOff) & q->hiddenMask)) continue;
            float loc[3]; memcpy(loc, e.obj + q->locOff, 12);
            if (!(loc[0] == loc[0]) || !(loc[1] == loc[1]) || !(loc[2] == loc[2])) continue;   // NaN
            // Every kind is its collision box (a door's origin is the hinge, a book's its corner); a
            // missing or absurd box degrades to the origin.
            bool box = false; float bo[3] = {}, be[3] = {};
            if (q->collOff && q->boundsOff) {
                uint8_t* comp = *(uint8_t**)(e.obj + q->collOff);
                if (comp && !((uintptr_t)comp & 3) && (uintptr_t)comp >= 0x10000) {
                    memcpy(bo, comp + q->boundsOff, 12); memcpy(be, comp + q->boundsOff + 12, 12);
                    box = bo[0] == bo[0] && be[0] == be[0] && be[0] >= 0 && be[1] >= 0 && be[2] >= 0 &&
                          be[0] < 2000 && be[1] < 2000 && be[2] < 2000;
                }
            }
            const float own = q->reach[e.kind], keep = own * 1.25f;            // the held target's hysteresis
            const float lim = (e.obj == q->held ? keep : own);
            float d2h[2] = { 1e30f, 1e30f }, aimh[2][3] = {};
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
            }
            if (isFocus) { out->f.listed = true; out->f.kind = e.kind; out->f.d2[0] = d2h[0]; out->f.d2[1] = d2h[1]; }
            if (blocked) continue;                                              // measured for the gate, never a target
            for (int h = 0; h < 2; ++h) {
                if (!handOk[h] || !(d2h[h] <= lim * lim)) continue;
                // a fraction of the reach; the held target is favoured so the pick does not flicker
                const float score = sqrtf(d2h[h]) / own * (e.obj == q->held ? 0.8f : 1.0f);
                if (score >= scoreH[h]) continue;
                PpCand& c = bestH[h];
                c.obj = e.obj; c.idx = e.idx; c.kind = e.kind; c.hand = h; c.keep = keep;
                memcpy(c.aim, aimh[h], 12); c.d2[0] = d2h[0]; c.d2[1] = d2h[1];
                scoreH[h] = score;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { out->fault = true; return; }
    const int w = !bestH[0].obj ? 1 : !bestH[1].obj ? 0 : scoreH[0] <= scoreH[1] ? 0 : 1;
    if (!bestH[w].obj) return;
    out->best = bestH[w];
}

// A hand's PALM in game world units: the grip pose, scaled about the head the way the drawn hand
// is, through the same head-to-world mapping the published aim ray uses, then 7 cm along the
// hand's own forward (the grip pose sits in the controller handle, behind the fingers).
static const float kPpPalmM = 0.07f;
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
    float d[3] = { sol.direction[0], sol.direction[1], sol.direction[2] };
    const bool dirOk = dvr::fireaim::normalize(d);
    for (int i = 0; i < 3; ++i) out[i] = sol.origin[i] + (dirOk ? d[i] * kPpPalmM * g_posScaleUU : 0.0f);
    return true;
#else
    (void)hand; (void)camera; (void)out;
    return false;
#endif
}

// The reach a focused thing is measured against: its kind's own, with the held target's 1.25
// hysteresis only when it IS the target.
static float PpFocusReach(const PpFocus& f, const PpQuery& q)
{
    return f.listed && f.kind < kPpKinds ? q.reach[f.kind] * (f.obj == q.held ? 1.25f : 1.0f) : 0.0f;
}

// The nearer palm within reach of the engine's focused actor when the list holds it, else -1.
static int PpFocusHand(const PpPick& pick, const PpQuery& q, const bool* handOk)
{
    const PpFocus& f = pick.f;
    if (!f.obj || !f.listed || f.kind >= kPpKinds || !q.kindOn[f.kind]) return -1;
    const float lim = PpFocusReach(f, q);
    int best = -1;
    for (int h = 0; h < 2; ++h)
        if (handOk[h] && f.d2[h] <= lim * lim && (best < 0 || f.d2[h] < f.d2[best])) best = h;
    return best;
}

// THE GRIP GATE (2026-10-07). A grip takes what the ENGINE focuses, because Interact acts on the
// focus. It used to require the focus to BE the mod's target, and a headset run showed the two
// apart for whole seconds: the game focusing an elixir (its prompt on screen) while the target was
// a usable whose box held the palm, or while there was no target at all (the elixir dropped and
// left out for a second after the palm's own trace to it had found nothing); neither grip did
// anything. Now a grip is taken when the focus is a listed thing within that palm's reach:
//   * the focus IS the target: the hand that picked it, as before;
//   * the focus is another listed thing (or there is no target): the nearer palm in its reach.
// Never both hands (2026-10-05: both ready at once hid the crossbow in the other hand). Nothing
// unlisted counts: a pawn, a talk target or a class the list does not hold is never offered.
static void PpGate(const PpPick& pick, const PpQuery& q, const bool* handOk, double now)
{
    (void)now;
    const PpFocus& f = pick.f;
    const int fHand = PpFocusHand(pick, q, handOk);
    uint32_t mask = 0;
    if (f.obj && f.obj == g_ppTarget && g_ppTargetHand >= 0) mask = 1u << g_ppTargetHand;
    else if (fHand >= 0) { mask = 1u << fHand; ++g_ppGateByFocus; }
    if (mask) {
        g_ppGripKind.store(f.kind);
        g_ppTargetReadable.store(f.kind == kPpReadable);
        g_ppTargetHold.store(f.kind == kPpUsable);
        g_ppTargetId.store((uintptr_t)f.obj);
    } else {
        g_ppTargetHold.store(false);
        g_ppTargetId.store((uintptr_t)g_ppTarget);
    }
    g_ppReadyMask.store(mask);
    g_ppReadyMs.store(GetTickCount64());
    GrabReadyPublish(mask);                                  // the ready hand (mesh_split.cpp): eligibility with hysteresis

    // The refused-grip line's values. Names are looked up when a pointer CHANGES, not every tick.
    static uint8_t* lastFocus = (uint8_t*)1; static uint8_t* lastTarget = (uint8_t*)1;
    if (f.obj != lastFocus) {
        lastFocus = f.obj;
        const char* cn = f.obj && LooksLikeObj(f.obj) ? ObjClassName(f.obj) : NULL;
        g_ppDiagFocusCls.store(cn ? cn : (f.obj ? "?" : "none"));
    }
    if (g_ppTarget != lastTarget) {
        lastTarget = g_ppTarget;
        const char* cn = g_ppTarget ? ObjClassName(g_ppTarget) : NULL;
        g_ppDiagTargetCls.store(cn ? cn : (g_ppTarget ? "?" : "none"));
    }
    g_ppDiagFocus.store((uintptr_t)f.obj); g_ppDiagTarget.store((uintptr_t)g_ppTarget);
    g_ppDiagTargetHand.store(g_ppTarget ? g_ppTargetHand : -1);
    g_ppDiagFocusKind.store(f.listed ? (int)f.kind : -1);
    const float lim = PpFocusReach(f, q);
    for (int h = 0; h < 2; ++h) g_ppDiagFocusCm[h].store(f.listed && handOk[h] ? sqrtf(f.d2[h]) / g_posScaleUU * 100.0f : -1.0f);
    g_ppDiagReachCm.store(lim / g_posScaleUU * 100.0f);
    for (int h = 0; h < 2; ++h) {
        const char* why;
        if (mask & (1u << h)) why = f.obj == g_ppTarget ? "taken: the focus is the target" : "taken: the focus is a listed thing in this palm's reach";
        else if (!f.obj) why = g_ppTarget ? "the engine focuses nothing (the target is not focused)" : "no target and the engine focuses nothing";
        else if (!f.listed) why = "the engine focuses an actor the list does not hold (not an interactable kind that is on, or a component or child of one)";
        else if (!q.kindOn[f.kind]) why = "the focused thing's kind is switched off";
        else if (!handOk[h]) why = "this hand is not tracked or not free (a carried body's lane)";
        else if (!(f.d2[h] <= lim * lim)) why = "the focused thing is out of this palm's reach";
        else why = "the other hand is the one offered (the target's hand, or the nearer palm)";
        g_ppGateWhy[h].store(why);
    }
}

// interact_aim.cpp asks this before its own ray: while a target is held, the engine's
// interaction trace runs from the target hand's PALM (pulled back 15 cm along the ray, so it never
// starts inside the thing) to the target's point. From the eyes, a book under another book was
// hidden by the top one; from the hand that reaches for it, it is the first thing the ray meets.
static bool PickupRay(float* origin, float* dir)
{
    if (!g_ppTarget || !PpGameplay()) return false;
    float d[3] = { g_ppTargetLoc[0] - g_ppTargetFrom[0], g_ppTargetLoc[1] - g_ppTargetFrom[1], g_ppTargetLoc[2] - g_ppTargetFrom[2] };
    if (!dvr::fireaim::normalize(d)) return false;
    const float back = 0.15f * g_posScaleUU;
    for (int i = 0; i < 3; ++i) origin[i] = g_ppTargetFrom[i] - d[i] * back;
    memcpy(dir, d, 12);
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
            PpBlock(g_ppTarget, nowMs, 1000.0);
        }
    }
    g_ppTarget = NULL; g_ppTargetHand = -1; g_ppTargetFocused = false;
    g_ppTargetId.store(0); g_ppTargetHold.store(false);
    g_ppReadyMask.store(0);
}

// What the player's own animation lanes say the hands may do. A carried movable: nothing (the
// grip is the game's again: throw or drop). A carried body: only the hand whose lane is free
// (lane 1 is the upper body and right arm, lane 2 the left arm).
static void PpCarryGate(bool* carryingMovable, bool handFree[2])
{
    *carryingMovable = false; handFree[0] = handFree[1] = true;
    const auto s = dvr::anim::snapshot();
    if (!s.valid) return;
    for (int i = 0; i < 3; ++i) if (!strcmp(s.state[i], "StatePlayerGrabMovable")) *carryingMovable = true;
    auto corpse = [](const char* st) { return !strcmp(st, "StatePlayerGrabCorpse") || !strcmp(st, "StatePlayerCarryCorpseIdle"); };
    if (corpse(s.state[1])) handFree[1] = false;
    if (corpse(s.state[2])) handFree[0] = false;
    static int was = -1;
    const int now = (*carryingMovable ? 4 : 0) | (handFree[0] ? 0 : 1) | (handFree[1] ? 0 : 2);
    if (now != was) {
        was = now;
        Log("pickup: carry gate - %s (lanes: upper %s, arm %s)",
            *carryingMovable ? "carrying an object: nothing is targeted and the grip keeps its own action (throw, drop)"
            : (!handFree[0] && !handFree[1]) ? "a body on both lanes: no hand may interact"
            : !handFree[1] ? "carrying a body: only the LEFT hand may interact (the upper-body lane holds it)"
            : !handFree[0] ? "carrying a body: only the RIGHT hand may interact (the arm lane holds it)"
            : "hands free", s.state[1], s.state[2]);
    }
}

// `pickup near`: the nearest listed things to the camera, with where they sit in the HEAD's frame
// (metres right, up and forward of the camera, yaw only) and how far each hand is, so a test can
// put a hand on a real item. Game thread, on request; a diagnostic, never per frame.
static std::atomic<bool> g_ppNearReq{false};
struct PpNearRow { uint8_t* obj; uint32_t kind; float d2; float loc[3]; };
static uint32_t PpNearCollect(void** objs, uint32_t num, const float* camera, uint32_t locOff, uint32_t collOff,
                              uint32_t boundsOff, PpNearRow* out, uint32_t maxOut)
{
    uint32_t n = 0;
    __try {
        for (uint32_t k = 0; k < g_ppListN; ++k) {
            const PpEntry& e = g_ppList[k];
            if (e.idx >= num || (uint8_t*)objs[e.idx] != e.obj || *(uint8_t**)(e.obj + kClassOff) != e.cls) continue;
            float loc[3]; memcpy(loc, e.obj + locOff, 12);
            if (e.kind >= kPpDoor && collOff && boundsOff) {        // the box centre for box-measured kinds
                uint8_t* comp = *(uint8_t**)(e.obj + collOff);
                if (comp && !((uintptr_t)comp & 3) && (uintptr_t)comp >= 0x10000) memcpy(loc, comp + boundsOff, 12);
            }
            const float dx = loc[0] - camera[0], dy = loc[1] - camera[1], dz = loc[2] - camera[2];
            const float d2 = dx * dx + dy * dy + dz * dz;
            // keep the nearest maxOut (insertion into a small sorted array)
            uint32_t at = n < maxOut ? n : maxOut;
            while (at > 0 && out[at - 1].d2 > d2) { if (at < maxOut) out[at] = out[at - 1]; --at; }
            if (at < maxOut) { out[at].obj = e.obj; out[at].kind = e.kind; out[at].d2 = d2; memcpy(out[at].loc, loc, 12); if (n < maxOut) ++n; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return n;
}
static void PpLogNear(void** objs, uint32_t num, const float* camera, uint32_t locOff, uint32_t collOff, uint32_t boundsOff,
                      const float (*hand)[3], const bool* handOk)
{
    PpNearRow rows[10];
    dvr::crash::probe_begin();
    const uint32_t n = PpNearCollect(objs, num, camera, locOff, collOff, boundsOff, rows, 10);
    dvr::crash::probe_end();
    const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad), m = g_posScaleUU > 1 ? g_posScaleUU : 50.0f;
    Log("pickup/near: %u listed, the %u nearest to the camera (%.0f %.0f %.0f), view yaw %.1f deg, %.1f uu per m - "
        "head frame in metres (right, up, forward), and each hand's distance:", g_ppListN, n, camera[0], camera[1], camera[2],
        g_viewYawRad * 57.2958f, m);
    for (uint32_t i = 0; i < n; ++i) {
        const float dx = rows[i].loc[0] - camera[0], dy = rows[i].loc[1] - camera[1], dz = rows[i].loc[2] - camera[2];
        const float fwd = (dx * cy + dy * sy) / m, right = (-dx * sy + dy * cy) / m, up = dz / m;
        float dh[2] = { -1, -1 };
        for (int h = 0; h < 2; ++h) if (handOk[h]) {
            const float hx = rows[i].loc[0] - hand[h][0], hy = rows[i].loc[1] - hand[h][1], hz = rows[i].loc[2] - hand[h][2];
            dh[h] = sqrtf(hx * hx + hy * hy + hz * hz) / m;
        }
        const char* cn = ObjClassName(rows[i].obj);
        Log("pickup/near: #%u %s [%s] %.2f m - right %+.2f up %+.2f forward %+.2f | left hand %.2f m, right hand %.2f m%s",
            i, cn ? cn : "?", rows[i].kind < kPpKinds ? kPpKindName[rows[i].kind] : "?", sqrtf(rows[i].d2) / m, right, up, fwd,
            dh[0], dh[1], rows[i].obj == g_ppTarget ? " (the TARGET)" : "");
    }
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
    {
        bool carrying = false, free[2];
        PpCarryGate(&carrying, free);
        if (carrying) { if (g_ppTarget) PpDropTarget("an object is carried", now, false); g_ppReadyMask.store(0); return; }
        for (int h = 0; h < 2; ++h) handOk[h] = handOk[h] && free[h];
        if (!handOk[0] && !handOk[1]) { if (g_ppTarget) PpDropTarget("no free hand (a body is carried)", now, false); g_ppReadyMask.store(0); return; }
    }

    PpQuery q;
    q.locOff = locOff; q.hiddenOff = hiddenOff; q.hiddenMask = hiddenMask; q.collOff = collOff; q.boundsOff = boundsOff; q.focusOff = focusOff;
    q.reach[kPpLoot] = g_ppReachM.load() * g_posScaleUU;          // game units
    q.reach[kPpReadable] = g_ppBookReachM.load() * g_posScaleUU;
    q.reach[kPpDoor] = g_ppDoorReachM.load() * g_posScaleUU;
    q.reach[kPpCarry] = q.reach[kPpDoor];                          // box-measured: the door reach
    q.reach[kPpUsable] = q.reach[kPpDoor];
    q.kindOn[kPpLoot] = q.kindOn[kPpReadable] = true;
    q.kindOn[kPpDoor] = g_ppDoorsOn.load();
    q.kindOn[kPpCarry] = g_ppCarryOn.load();
    q.kindOn[kPpUsable] = g_ppUsablesOn.load();
    q.held = g_ppTarget; q.pc = g_peCtrl;
    for (int b = 0; b < 8; ++b) q.blocked[b] = now < g_ppBlocked[b].untilMs ? g_ppBlocked[b].obj : NULL;
    PpPick pick;
    dvr::crash::probe_begin();
    {   // the rotating near-list pass: 192 listed items a frame, those within 2.5 m of the camera kept (box kinds 4 m)
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

    // The beat, every 10 s of gameplay whether or not anything is in reach (it sat after the
    // "nothing within reach" return, so a run that never came near loot printed no cost at all).
    static double nextBeat = 0;
    if (now >= nextBeat) {
        nextBeat = now + 10000;
        uint32_t byKind[kPpKinds] = {};
        for (uint32_t k = 0; k < g_ppListN; ++k) if (g_ppList[k].kind < kPpKinds) ++byKind[g_ppList[k].kind];
        Log("pickup: beat - %u interactable actor(s) listed (loot %u, books %u, doors %u, carry %u, usable %u; %u within 2.5 m), "
            "sweep %u took %.0f ms of game time, trace driven %ld time(s), targets taken loot %u books %u doors %u carry %u usable %u, "
            "grips swallowed %u, Interact pressed %u, gate passes on a focus that is not the target %u, reach %.0f cm | own cost %.1f us a frame, max %.0f, over 250 us in %u and "
            "over 1000 us in %u of %u frames (a frame of several ms on the game thread alone is what starves an eye: both "
            "counts must read 0 or close to it), slices cut by the 60 us budget %u, guarded-read faults %u | doors %s, carry %s, "
            "usables %s | pawn or talk targets refused %u (%u this run; counts targets whose class chain names a pawn or a talk "
            "class, read by text: must read 0)",
            g_ppListN, byKind[0], byKind[1], byKind[2], byKind[3], byKind[4], g_ppNearN, g_ppSweeps, g_ppLastSweepMs,
            (long)g_ppRayDriven, g_ppTargetsByKind[0], g_ppTargetsByKind[1], g_ppTargetsByKind[2], g_ppTargetsByKind[3],
            g_ppTargetsByKind[4], g_ppSwallowed.load(), g_ppFired.load(), g_ppGateByFocus,
            g_ppReachM.load() * 100.0f, g_ppCostN ? g_ppCostUsSum / g_ppCostN : 0.0, g_ppCostUsMax, g_ppCostOver250,
            g_ppCostOver1000, g_ppCostN, g_ppSliceCut, g_ppFaults, g_ppDoorsOn.load() ? "ON" : "off",
            g_ppCarryOn.load() ? "ON" : "off", g_ppUsablesOn.load() ? "ON" : "off", g_ppPawnTargets, g_ppPawnTargetsTotal);
        g_ppCostUsSum = 0; g_ppCostUsMax = 0; g_ppCostN = 0; g_ppCostOver250 = 0; g_ppCostOver1000 = 0; g_ppSliceCut = 0;
        memset(g_ppTargetsByKind, 0, sizeof(g_ppTargetsByKind)); g_ppPawnTargets = 0; g_ppGateByFocus = 0;
    }
    if (g_ppNearReq.exchange(false)) PpLogNear(objs, num, camera, locOff, collOff, boundsOff, hand, handOk);

    const PpCand* c = pick.best.obj ? &pick.best : NULL;
    if (!c) { if (g_ppTarget) PpDropTarget("nothing within reach", now, false); PpGate(pick, q, handOk, now); return; }

    static const char* const kKind[kPpKinds] = { "", " (a book or note: its own reach, and the page goes to the hand that opens it)",
                                                 " (a door: measured from its collision box)",
                                                 " (carry or throw: measured from its collision box)",
                                                 " (a lever, switch, chain or trap: measured from its collision box; a held grip holds Interact)" };
    if (c->obj != g_ppTarget) {
        if (g_ppTarget) PpDropTarget("a nearer item", now, false);
        // The pawn check, by a different route than the list (text, not the verdict table). It must
        // read 0; a hit is refused and blocked for a minute, and says which name matched.
        dvr::crash::probe_begin();
        const char* pawn = PpChainNamesPawnRaw(*(uint8_t**)(c->obj + kClassOff));
        dvr::crash::probe_end();
        if (pawn) {
            ++g_ppPawnTargets; ++g_ppPawnTargetsTotal;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000, "pickup: REFUSED a target whose class chain names %s (%s) - a pawn or talk target must never be offered; "
                 "the list should have excluded it", pawn, ObjClassName(c->obj));
            PpBlock(c->obj, now, 60000.0);
            PpGate(pick, q, handOk, now);
            return;
        }
        g_ppTarget = c->obj; g_ppTargetIdx = c->idx; g_ppTargetSinceMs = now; g_ppFocusedLastMs = 0; g_ppTargetFocused = false;
        ++g_ppTargetsByKind[c->kind];
        const char* cn = ObjClassName(c->obj);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 400,
            "pickup: target %s [%s]%s at %.0f uu (%.0f cm) from the %s palm, reach %.0f cm - the interaction trace now runs from "
            "that palm to it; the grip takes it once the game focuses it",
            cn ? cn : "?", kPpKindName[c->kind], kKind[c->kind], sqrtf(c->d2[c->hand]), sqrtf(c->d2[c->hand]) / g_posScaleUU * 100.0f,
            c->hand ? "RIGHT" : "LEFT", q.reach[c->kind] / g_posScaleUU * 100.0f);
    }
    memcpy(g_ppTargetLoc, c->aim, 12);
    memcpy(g_ppTargetFrom, hand[c->hand], 12);
    g_ppTargetHand = c->hand; g_ppTargetKind = c->kind;

    // The engine's verdict: is its focused actor our target? A focus on ANOTHER listed thing in a
    // palm's reach also keeps the target: the trace aimed at the target is what produced that focus,
    // and dropping the target would swap the trace and lose it (PpGate takes the grip for it).
    uint8_t* focus = pick.focus;
    if (focus == g_ppTarget) { g_ppFocusedLastMs = now; g_ppTargetFocused = true; }
    else if (PpFocusHand(pick, q, handOk) < 0 &&
             now - (g_ppFocusedLastMs > 0 ? g_ppFocusedLastMs : g_ppTargetSinceMs) > 250.0) {
        // Aimed at it for a quarter second and the game did not take it: not usable now.
        PpDropTarget("the game did not focus it (looted, hidden, covered or not usable) - left out for 1 s, the next nearest is tried", now, true);
        PpGate(pick, q, handOk, now);
        return;
    }
    PpGate(pick, q, handOk, now);
}

// Pad bridge (present thread). `raw` is the PHYSICAL snapshot about to be remapped: a grip that
// starts a pickup is zeroed in it until released, so whatever is bound to that grip does not
// fire. Returns true while Interact should be held down: 130 ms for a press, and on a usable (a
// valve, a lever that wants Interact held) for as long as the grip stays down AND the same target
// is still focused and in that hand's reach. The hold ends the moment the target is lost, because
// Interact held with nothing focused sheathes the weapon in this game.
static bool PickupPadFilter(dvr::vr::InputSnapshot& raw, bool blocked)
{
    static bool was[2] = { false, false }, swallow[2] = { false, false }, holding[2] = { false, false };
    static uintptr_t pressedOn[2] = { 0, 0 };
    static uint64_t pressUntil = 0, holdSince[2] = { 0, 0 };
    const uint64_t now = GetTickCount64();
    const bool fresh = now - g_ppReadyMs.load() <= 250;
    const uint32_t mask = (g_ppOn.load() && !blocked && fresh) ? g_ppReadyMask.load() : 0;
    float* grip[2] = { &raw.gripL, &raw.gripR };
    for (int h = 0; h < 2; ++h) {
        const bool down = *grip[h] > (was[h] ? 0.7f : 0.9f);
        if (down && !was[h] && (mask & (1u << h))) {
            swallow[h] = true; pressUntil = now + 130;
            GrabAnimNotify(h, "a pickup grip");                  // the hand closes (mesh_split.cpp, THE GRAB)
            pressedOn[h] = g_ppTargetHold.load() ? g_ppTargetId.load() : 0;
            holding[h] = pressedOn[h] != 0; holdSince[h] = now;
            g_ppSwallowed.fetch_add(1); g_ppFired.fetch_add(1);
            if (g_ppGripKind.load() == kPpCarry) { g_ppCarryGripHand.store(h); g_ppCarryGripMs.store(now); }
            const bool page = g_ppTargetReadable.load();
            if (page) dvr::hudlayout::note_opened_by_hand(h);     // the reading panel attaches to this hand
            Log("pickup: %s grip pressed with the target in reach and focused - grip swallowed, Interact pressed for 130 ms%s",
                h ? "RIGHT" : "LEFT", page ? "; a book or note: its page is asked onto this hand" : "");
        }
        if (down && !was[h] && !(mask & (1u << h)) && (g_ppDiagTarget.load() || g_ppDiagFocus.load())) {
            // A grip pressed near something and NOT taken: both pointers, their classes and the reason,
            // so the next run is arithmetic. A component or child of the target shows as an unlisted focus.
            const int fk = g_ppDiagFocusKind.load();
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 300,
                "pickup: %s grip NOT taken - %s | target %s %p (%s hand), engine focus %s %p (%s%s), this palm %.0f cm from it, "
                "reach %.0f cm",
                h ? "RIGHT" : "LEFT",
                !g_ppOn.load() ? "physical pickup is off" : blocked ? "the overlay, a menu or a cinematic blocks it"
                : !fresh ? "the gate is stale (no gameplay tick in 250 ms: not in gameplay, no tracked hand, or a carry)"
                : g_ppGateWhy[h].load(),
                g_ppDiagTargetCls.load(), (void*)g_ppDiagTarget.load(),
                g_ppDiagTargetHand.load() == 1 ? "RIGHT" : g_ppDiagTargetHand.load() == 0 ? "LEFT" : "no",
                g_ppDiagFocusCls.load(), (void*)g_ppDiagFocus.load(),
                fk < 0 ? "not listed" : kPpKindName[fk < (int)kPpKinds ? fk : 0],
                g_ppDiagFocus.load() && g_ppDiagFocus.load() == g_ppDiagTarget.load() ? ", the target" : "",
                (double)g_ppDiagFocusCm[h].load(), (double)g_ppDiagReachCm.load());
        }
        if (!down) swallow[h] = false;
        was[h] = down;
        GrabAnimGrip(h, swallow[h]);                             // the fist stays closed while this grip is held
        if (swallow[h]) *grip[h] = 0.0f;
        if (holding[h]) {
            const bool keep = swallow[h] && (mask & (1u << h)) && g_ppTargetId.load() == pressedOn[h];
            if (!keep) {
                holding[h] = false;
                if (now - holdSince[h] > 130)
                    Log("pickup: %s grip held Interact on a usable for %llu ms - ended by %s", h ? "RIGHT" : "LEFT",
                        (unsigned long long)(now - holdSince[h]), !swallow[h] ? "the grip's release"
                        : g_ppTargetId.load() != pressedOn[h] ? "the target changing or being lost" : "the hand leaving reach or the focus");
            }
        }
    }
    return now < pressUntil || holding[0] || holding[1];
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
static bool PickupCarryEnabled() { return g_ppCarryOn.load(); }
static void PickupSetCarry(bool on, const char* who)
{
    g_ppCarryOn.store(on);
    Log("pickup: carry and throw by grabbing %s (%s) - bottles, tanks, grenades, limbs: a hand within %.0f cm of the "
        "collision box and a grip picks it up; needs physical pickup on (now %s)", on ? "ON" : "off", who,
        g_ppDoorReachM.load() * 100.0f, g_ppOn.load() ? "ON" : "off");
}
static bool PickupUsablesEnabled() { return g_ppUsablesOn.load(); }
static void PickupSetUsables(bool on, const char* who)
{
    g_ppUsablesOn.store(on);
    Log("pickup: levers, switches, chains and traps by grabbing %s (%s) - a hand within %.0f cm of the collision box and "
        "a grip uses it, held for as long as the grip is; needs physical pickup on (now %s)", on ? "ON" : "off", who,
        g_ppDoorReachM.load() * 100.0f, g_ppOn.load() ? "ON" : "off");
}

static void PickupConfigure(const char* ini)
{
    PickupSetReachCm(IniFloat(ini, "Aim", "PhysicalPickupReachCm", 45));
    PickupSetBookReachCm(IniFloat(ini, "Aim", "PhysicalPickupBookReachCm", 55));
    PickupSetDoorReachCm(IniFloat(ini, "Aim", "PhysicalDoorReachCm", 35));
    PickupSetDoors(IniFloat(ini, "Aim", "PhysicalDoors", 1) != 0.0f, "ini [Aim] PhysicalDoors");
    PickupSetCarry(IniFloat(ini, "Aim", "PhysicalCarry", 1) != 0.0f, "ini [Aim] PhysicalCarry");
    PickupSetUsables(IniFloat(ini, "Aim", "PhysicalUsables", 1) != 0.0f, "ini [Aim] PhysicalUsables");
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
    if (!strcmp(args, "near")) { g_ppNearReq.store(true); Log("pickup: near - the next gameplay tick lists the nearest things"); return true; }
    if (!strncmp(args, "carry ", 6) && DvrOnOff(args + 6, &b)) {
        PickupSetCarry(b, "seam");
        ConfigWriteKey("Aim", "PhysicalCarry", b ? "1" : "0", "the seam");
        return true;
    }
    if (!strncmp(args, "usables ", 8) && DvrOnOff(args + 8, &b)) {
        PickupSetUsables(b, "seam");
        ConfigWriteKey("Aim", "PhysicalUsables", b ? "1" : "0", "the seam");
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
    Log("pickup: on|off, reach <cm>, bookreach <cm>, doors on|off, doorreach <cm>, carry on|off, usables on|off (now %s, reach "
        "%.0f cm, books and notes %.0f cm, doors %s at %.0f cm, carry %s, usables %s, both at the door reach) | listed %u, "
        "sweeps %u (last %.0f ms), target %s [%s] (hand %d, focused %s), trace driven %ld, grips swallowed %u, Interact "
        "pressed %u, ready mask %u, pawn or talk targets refused this run %u",
        g_ppOn.load() ? "ON" : "off", PickupReachCm(), PickupBookReachCm(), g_ppDoorsOn.load() ? "ON" : "off", PickupDoorReachCm(),
        g_ppCarryOn.load() ? "ON" : "off", g_ppUsablesOn.load() ? "ON" : "off",
        g_ppListN, g_ppSweeps, g_ppLastSweepMs,
        g_ppTarget ? "held" : "none", g_ppTarget && g_ppTargetKind < kPpKinds ? kPpKindName[g_ppTargetKind] : "-",
        g_ppTargetHand, g_ppTargetFocused ? "yes" : "no", (long)g_ppRayDriven,
        g_ppSwallowed.load(), g_ppFired.load(), g_ppReadyMask.load(), g_ppPawnTargetsTotal);
    return true;
}
