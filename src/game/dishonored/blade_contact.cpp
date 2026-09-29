// game/dishonored/blade_contact.cpp - included by src/mod/dishonoredvr.cpp (unity build).
// VR-173: THE HELD BLADE IN THE GAME'S WORLD.
//
// What the player would get from it: a sword attack timed by the blade reaching
// something. The blade is known in the hand (hands/blade_axis.cpp). This file carries it
// into the game's world units on the script lane, which is where the engine can be asked
// what lies along it.
//
// SCRIPT LANE ONLY, and read-only: nothing here writes an engine field.
//
// PREREQUISITE 3, THE BRIDGE, CHECKED BY TWO ROUTES TO ONE POINT ([Blade] World, off):
//
//   the XR route    the sword hand's grip pose -> the palm-frame blade -> XR LOCAL metres
//                   -> dvr::fireaim::point_to_world, the arithmetic every hand-aimed
//                   feature's ray origin already takes
//   the draw route  the same palm-frame blade, carried the way the DRAW went and back
//                   across the weapon path's coordinate bridge (hands/blade_axis.cpp)
//
// The draw route holds no headset pose and the XR route holds no draw transform. If the
// bridge has the wrong scale, the wrong anchor or the wrong frames, they part.
//
// COST. Off: two atomic reads per script dispatch. On: once per blade frame (a hand
// sample), a handful of dot products; the comparison line is printed twice a second.
#include "game/dishonored/fire_aim_math.h"
#include "game/dishonored/blade_math.h"

#ifdef DVR_CAT
#undef DVR_CAT
#endif
#define DVR_CAT ::dvr::log::Cat::melee

static std::atomic<bool> g_bwOn{false};          // [Blade] World
// TWO DELIBERATE ERRORS, so the comparison can be shown to disagree. Neither is saved, and
// both are instrument-only: nothing but this file's own world blade reads them.
//   anchor render   anchor the XR route on the last render sample instead of the game
//                   camera. That sample is whichever scene draw uploaded last (an eye, or
//                   a shadow pass), which is why the hand rays left it (VR-181).
//   skew <percent>  scale the XR route's world units by this much. At 5 percent a point
//                   80 uu from the head must come out 4 uu from where the draw put it.
static std::atomic<bool>  g_bwAnchorRender{false};
static std::atomic<float> g_bwSkewPct{0.0f};

struct BwSide { unsigned n = 0; double sumTip = 0, sumBase = 0; float maxTip = 0, maxBase = 0; float lastTip = 0, lastBase = 0; };
struct BwStats {
    BwSide eye[2];
    unsigned frames = 0, refused = 0, stale = 0;
    float lastXrTip[3] = {}, lastXrBase[3] = {}, lastCamera[3] = {};
    float lastLenUU = 0;
    char  why[160] = "off";
} g_bwS;

// The blade's base and tip in game world units, from the blade frame the present lane
// published. `why` names a refusal. Script lane.
static bool BladeWorld(const dvr::hands::BladeFrame& bf, float* baseW, float* tipW, float* cameraW, const char** why)
{
    if (!bf.ok) { *why = bf.why; return false; }
    const uint64_t now = GetTickCount64();
    if (now < bf.sampleMs || now - bf.sampleMs > 100) { *why = "the blade frame is more than 100 ms old"; return false; }
    float camera[3];
    // The anchor every hand ray uses (interact_aim.cpp): the game camera's own base, or
    // the render sample when [Aim] HandRayGameAnchor=0.
    const bool wantGame = g_hrGameAnchor.load() && !g_bwAnchorRender.load();
    if (!(wantGame && GameCameraAnchor(camera)) && !dvr::camera::render_pos_world(camera)) {
        *why = "no world camera position"; return false;
    }
    dvr::fireaim::Frames fr;
    if (!dvr::fireaim::frames(bf.headQuat, g_viewYawRad, g_viewPitchRad, fr)) {
        *why = "the head looks straight up or down: no horizon to build the frames from"; return false;
    }
    if (!std::isfinite(g_posScaleUU) || g_posScaleUU < 1 || g_posScaleUU > 400) { *why = "the world scale is out of range"; return false; }
    const float scale = g_posScaleUU * (1.0f + 0.01f * g_bwSkewPct.load());
    dvr::fireaim::point_to_world(fr, bf.headPos, bf.baseXr, camera, scale, baseW);
    dvr::fireaim::point_to_world(fr, bf.headPos, bf.tipXr, camera, scale, tipW);
    if (!dvr::fireaim::finite3(baseW) || !dvr::fireaim::finite3(tipW)) { *why = "a world point is not finite"; return false; }
    if (cameraW) memcpy(cameraW, camera, 12);
    return true;
}

static void BwTick()
{
    static uint32_t lastGen = 0;
    const dvr::hands::BladeFrame bf = dvr::hands::blade_frame();
    if (bf.ok && bf.handGen == lastGen) return;        // one comparison per hand sample
    lastGen = bf.handGen;
    float baseW[3], tipW[3], cam[3]; const char* why = "";
    if (!BladeWorld(bf, baseW, tipW, cam, &why)) {
        ++g_bwS.refused;
        _snprintf_s(g_bwS.why, sizeof(g_bwS.why), _TRUNCATE, "%s", why);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000, "blade/world: no world blade - %s", why);
        return;
    }
    ++g_bwS.frames;
    memcpy(g_bwS.lastXrTip, tipW, 12); memcpy(g_bwS.lastXrBase, baseW, 12); memcpy(g_bwS.lastCamera, cam, 12);
    { const float d[3] = { tipW[0]-baseW[0], tipW[1]-baseW[1], tipW[2]-baseW[2] }; g_bwS.lastLenUU = dvr::blade::len3(d); }
    const dvr::hands::BladeSnapshot s = dvr::hands::blade_snapshot(g_waSwordHand);
    const uint64_t now = GetTickCount64();
    bool any = false;
    for (int e = 0; e < 2; ++e) {
        if (!s.drawnOk[e] || now < s.drawnMs[e] || now - s.drawnMs[e] > 100) continue;
        any = true;
        const float dt[3] = { tipW[0]-s.drawnTipWorld[e][0], tipW[1]-s.drawnTipWorld[e][1], tipW[2]-s.drawnTipWorld[e][2] };
        const float db[3] = { baseW[0]-s.drawnBaseWorld[e][0], baseW[1]-s.drawnBaseWorld[e][1], baseW[2]-s.drawnBaseWorld[e][2] };
        BwSide& b = g_bwS.eye[e];
        b.lastTip = dvr::blade::len3(dt); b.lastBase = dvr::blade::len3(db);
        ++b.n; b.sumTip += b.lastTip; b.sumBase += b.lastBase;
        if (b.lastTip > b.maxTip) b.maxTip = b.lastTip;
        if (b.lastBase > b.maxBase) b.maxBase = b.lastBase;
    }
    if (!any) { ++g_bwS.stale; _snprintf_s(g_bwS.why, sizeof(g_bwS.why), _TRUNCATE, "no draw of the sword in the last 100 ms to compare with"); }
    else _snprintf_s(g_bwS.why, sizeof(g_bwS.why), _TRUNCATE, "comparing");
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 500,
        "blade/world: XR route tip (%.1f %.1f %.1f) base (%.1f %.1f %.1f) uu, %.1f uu apart | draw route, left eye tip (%.1f %.1f %.1f) "
        "%s, right eye tip (%.1f %.1f %.1f) %s | XR against draw: left tip %.2f base %.2f uu, right tip %.2f base %.2f uu | head in the "
        "world (%.1f %.1f %.1f), view yaw %.2f pitch %.2f deg, %.0f uu per metre, live tip %.4f m off the constant | 100 uu = 1 m",
        tipW[0], tipW[1], tipW[2], baseW[0], baseW[1], baseW[2], g_bwS.lastLenUU,
        s.drawnTipWorld[0][0], s.drawnTipWorld[0][1], s.drawnTipWorld[0][2], s.drawnOk[0] ? "ok" : "NONE",
        s.drawnTipWorld[1][0], s.drawnTipWorld[1][1], s.drawnTipWorld[1][2], s.drawnOk[1] ? "ok" : "NONE",
        g_bwS.eye[0].lastTip, g_bwS.eye[0].lastBase, g_bwS.eye[1].lastTip, g_bwS.eye[1].lastBase,
        cam[0], cam[1], cam[2], g_viewYawRad * 57.29578f, g_viewPitchRad * 57.29578f, g_posScaleUU, bf.liveApartM);
}

static void BwReport()
{
    const BwSide& l = g_bwS.eye[0]; const BwSide& r = g_bwS.eye[1];
    if (g_bwAnchorRender.load() || g_bwSkewPct.load() != 0.0f)
        DVR_WARN("blade/world: A DELIBERATE ERROR IS ON (anchor %s, skew %+.1f %%): the figures below are expected to DISAGREE",
                 g_bwAnchorRender.load() ? "RENDER sample" : "game camera", g_bwSkewPct.load());
    Log("blade/world: %s | %u blade frame(s) carried, %u refused, %u with no fresh draw | XR route against the draw route, "
        "LEFT eye's draw: tip mean %.2f max %.2f uu, base mean %.2f max %.2f uu over %u; RIGHT eye's: tip mean %.2f max %.2f uu, "
        "base mean %.2f max %.2f uu over %u | last XR tip (%.1f %.1f %.1f), blade %.1f uu | %s. The two routes share the "
        "palm-frame blade and nothing else: a wrong scale, anchor or frame shows here. 0 over 0 means nothing was compared, "
        "not that they agree",
        g_bwOn.load() ? "ON" : "off", g_bwS.frames, g_bwS.refused, g_bwS.stale,
        l.n ? l.sumTip / l.n : 0.0, l.maxTip, l.n ? l.sumBase / l.n : 0.0, l.maxBase, l.n,
        r.n ? r.sumTip / r.n : 0.0, r.maxTip, r.n ? r.sumBase / r.n : 0.0, r.maxBase, r.n,
        g_bwS.lastXrTip[0], g_bwS.lastXrTip[1], g_bwS.lastXrTip[2], g_bwS.lastLenUU, g_bwS.why);
}


// ---- PREREQUISITE 4: THE ENGINE'S OWN TRACE ([Blade] Trace, off) ----------------------
//
// The question put to the engine: what lies along this line? It is answered by the
// world's single line check, the function the script-callable Actor.Trace runs, with the
// flags Actor.Trace composes for "trace actors too". So the blade agrees with the game's
// own collision by construction: what stops a bolt stops the blade.
//
// WHY NOT THROUGH ProcessEvent, which is how the mod calls every other engine function.
// It was built that way first and every call came back unanswered: the selftest below
// printed four FAILs. ProcessEvent returns at once for a function that carries a NATIVE
// INDEX (its own test of the word at function +0x84), and Trace is one of the numbered
// natives. The natives the mod does call that way (SetHidden, TransformFromBoneSpace) have
// no number. So the line check itself is called, the way the thunk calls it.
//
// WHAT IS VERIFIED BEFORE THE FIRST CALL, and refused on any mismatch (BtResolve):
//   * the function object named Trace, declared on Actor, found by class and name in ONE
//     pass over the object table, holds the address of the thunk the offline native
//     registration table gives for AActor::execTrace. That ties the address to a NAME;
//   * that thunk loads the world pointer and calls the line check at the two places the
//     disassembly says, byte for byte;
//   * the line check begins with the bytes it had when its seven arguments were read.
// Every address is in patterns.h with its derivation in ENGINE_NOTES.
//
// The hit actor is validated against the live-object table before anything is read
// through it, is never kept across a tick, and is never written to. Nothing here walks the
// object table after the resolve. Script lane only.

static std::atomic<bool> g_btOn{false};          // [Blade] Trace
static std::atomic<int>  g_btSelfTestReq{0};     // the seam's selftest word, present lane -> script lane
static std::atomic<int>  g_btResolveReq{0};      // the seam's resolve word: try again after a miss
static std::atomic<int>  g_btViewReq{0};         // the seam's view word: one trace along the view, this many uu
static std::atomic<int>  g_btScanReq{0};         // the seam's scan word: a fan of traces about the view, half angle in degrees

struct BtLayout {
    bool tried = false, ok = false;
    uint8_t* fn = nullptr;         // the function object named Actor.Trace
    int thunkAt = -1;              // where in it the thunk's address sits
    unsigned nativeIndex = 0;      // why ProcessEvent refuses it
    double walkMs = 0;
    char why[200] = "not resolved yet";
} g_btL;

// The engine's FCheckResult as the thunk builds it on its own stack, and what it reads back.
struct BtCheckResult {
    void*    next;                 // +0x00
    uint8_t* actor;                // +0x04  the answer: null = nothing was hit
    float    location[3];          // +0x08
    float    normal[3];            // +0x14
    float    time;                 // +0x20  1.0 going in
    int32_t  item;                 // +0x24  -1 going in
    uint32_t rest[7];              // +0x28  zero going in ...
    int32_t  levelIndex;           // +0x44  ... but this one, -1
    uint32_t startPenetrating;     // +0x48
    uint32_t pad[13];              // room past the engine's 0x4C bytes; never read
};
typedef int (__thiscall *PFN_WorldLineCheck)(void* world, BtCheckResult* hit, void* sourceActor, const float* end,
                                             const float* start, uint32_t traceFlags, const float* extent, void* sourceLight);

static bool BtResolve()
{
    if (g_btResolveReq.exchange(0)) { g_btL = BtLayout{}; }
    if (g_btL.tried) return g_btL.ok;
    if (!RflNamesReady()) return false;                  // not a miss: the name table is not there yet
    g_btL.tried = true;
    auto no = [&](const char* fmt, ...) {
        va_list ap; va_start(ap, fmt); _vsnprintf_s(g_btL.why, sizeof(g_btL.why), _TRUNCATE, fmt, ap); va_end(ap);
        DVR_WARN("blade/trace: NOT RESOLVED - %s. The blade cannot ask the engine anything; the miss is kept and nothing "
                 "walks the object table again until the seam's 'blade trace resolve'", g_btL.why);
        return false;
    };
    // 1. the code, byte for byte
    static const uint8_t kEntry[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x08, 0x8D, 0xF5, 0x00, 0x64, 0xA1,
                                      0x00, 0x00, 0x00, 0x00, 0x50, 0x83, 0xEC, 0x14, 0x53, 0x56, 0x57, 0xA1 };
    if (!RangeReadable((void*)kWorldLineCheck, sizeof(kEntry)) || memcmp((void*)kWorldLineCheck, kEntry, sizeof(kEntry)))
        return no("the line check at %p does not begin with the bytes it was derived from", (void*)kWorldLineCheck);
    uint8_t load[6] = { 0x8B, 0x0D }; const uint32_t world = (uint32_t)kGWorldPtr; memcpy(load + 2, &world, 4);
    if (!RangeReadable((void*)kTraceThunkWorldLoad, 6) || memcmp((void*)kTraceThunkWorldLoad, load, 6))
        return no("the trace thunk does not load the world pointer %p at %p", (void*)kGWorldPtr, (void*)kTraceThunkWorldLoad);
    if (!RangeReadable((void*)kTraceThunkCall, 5) || *(uint8_t*)kTraceThunkCall != 0xE8)
        return no("the trace thunk has no call at %p", (void*)kTraceThunkCall);
    int32_t rel; memcpy(&rel, (void*)(kTraceThunkCall + 1), 4);
    if ((uintptr_t)(kTraceThunkCall + 5 + rel) != kWorldLineCheck)
        return no("the trace thunk's call at %p goes to %p, not to the line check %p", (void*)kTraceThunkCall,
                  (void*)(uintptr_t)(kTraceThunkCall + 5 + rel), (void*)kWorldLineCheck);
    // 2. the NAME: the function object called Trace, declared on Actor, in one pass
    const uint32_t fi = FindNameIdx("Trace"), ci = FindNameIdx("Actor");
    if (fi == 0xffffffffu || ci == 0xffffffffu) return no("'Trace' or 'Actor' is not in the name table");
    uint8_t* fn = nullptr; int fns = 0;
    const GObjWalkStats st = GObjForEach(0xC0, [&](uint32_t, uint8_t* o, const char* cn) {
        if (!cn || *(uint32_t*)(o + kNameOff) != fi || strcmp(cn, "Function")) return true;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (ou && !((uintptr_t)ou & 3) && RangeReadable(ou, kNameOff + 4) && *(uint32_t*)(ou + kNameOff) == ci) { fn = o; ++fns; }
        return true;
    });
    g_btL.walkMs = st.ms;
    if (!st.ran) return no("the object table could not be read");
    if (fns != 1) return no("%d functions named Trace are declared on Actor, wanted exactly 1", fns);
    for (int off = 0x40; off + 4 <= 0xC0; off += 4)
        if (*(uint32_t*)(fn + off) == (uint32_t)kTraceThunk) { g_btL.thunkAt = off; break; }
    if (g_btL.thunkAt < 0)
        return no("the function object named Actor.Trace (%p) does not hold the thunk address %p: the address does not belong to this name on this build",
                  fn, (void*)kTraceThunk);
    g_btL.nativeIndex = *(uint16_t*)(fn + kUFuncNativeIdxOff);
    g_btL.fn = fn; g_btL.ok = true;
    _snprintf_s(g_btL.why, sizeof(g_btL.why), _TRUNCATE, "resolved");
    Log("blade/trace: RESOLVED - the function object named Actor.Trace (%p, found by class and name in one pass over %u objects, "
        "%.1f ms, once) holds the thunk %p at +0x%x; the thunk loads the world pointer and calls the line check %p where the "
        "disassembly says; the line check begins with its 24 derived bytes. Native index %u: ProcessEvent refuses a function "
        "with one (%s), so the line check is called the way the thunk calls it. World object now %p",
        fn, st.visited, st.ms, (void*)kTraceThunk, g_btL.thunkAt, (void*)kWorldLineCheck, g_btL.nativeIndex,
        g_btL.nativeIndex ? "this is why the first build's calls came back unanswered" : "ZERO here, which the first build's failure does not agree with",
        *(void**)kGWorldPtr);
    return true;
}

struct BtHit {
    bool hit = false;
    uint8_t* actor = nullptr;      // valid for THIS tick only; never stored past the caller
    float loc[3] = {}, normal[3] = {};
    float dist = 0;                // start to the hit, uu
    float offLine = 0;             // how far the hit lies from the traced segment, uu
    double us = 0;                 // what the call cost
};
static LONG g_btCalls = 0, g_btNoReturn = 0;

// One line trace, start to end, world units. false = the call could not be made or did not
// answer; why names it. A miss is an answer: true with hit=false.
static bool BtTrace(uint8_t* pawn, const float* start, const float* end, BtHit* out, const char** why)
{
    if (!g_btL.ok) { *why = g_btL.why; return false; }
    if (!pawn || !IsLiveObject(pawn)) { *why = "the player's pawn is not a live object"; return false; }
    if (!dvr::fireaim::finite3(start) || !dvr::fireaim::finite3(end)) { *why = "a trace end is not finite"; return false; }
    if (!RangeReadable((void*)kGWorldPtr, 4)) { *why = "the world pointer cannot be read"; return false; }
    void* world = *(void**)kGWorldPtr;
    if (!world || ((uintptr_t)world & 3) || !RangeReadable(world, 8)) { *why = "there is no world object"; return false; }
    BtCheckResult r; memset(&r, 0, sizeof(r));
    const uint8_t* unanswered = (const uint8_t*)(uintptr_t)0xFFFFFFFFu;
    r.actor = (uint8_t*)unanswered;          // the engine writes null or an actor; this value means it wrote neither
    r.time = 1.0f; r.item = -1; r.levelIndex = -1;
    const float sentinel = -1.0e30f; r.location[0] = sentinel;
    const float extent[3] = { 0, 0, 0 };      // a line, not a box
    LARGE_INTEGER c0, c1; QueryPerformanceCounter(&c0);
    ((PFN_WorldLineCheck)kWorldLineCheck)(world, &r, pawn, end, start, kTraceFlagsActors, extent, nullptr);
    QueryPerformanceCounter(&c1);
    InterlockedIncrement(&g_btCalls);
    BtHit h;
    h.us = g_qpcFreq ? (double)(c1.QuadPart - c0.QuadPart) * 1.0e6 / (double)g_qpcFreq : 0.0;
    if (r.actor == unanswered) { InterlockedIncrement(&g_btNoReturn); *why = "the line check did not write its answer"; return false; }
    h.actor = r.actor;
    h.hit = h.actor != nullptr;
    if (h.hit) {
        memcpy(h.loc, r.location, 12);
        memcpy(h.normal, r.normal, 12);
        if (h.loc[0] == sentinel || !dvr::fireaim::finite3(h.loc) || !dvr::fireaim::finite3(h.normal)) {
            *why = "an actor came back with no hit location"; return false;
        }
        const float d[3] = { end[0]-start[0], end[1]-start[1], end[2]-start[2] };
        const float e[3] = { h.loc[0]-start[0], h.loc[1]-start[1], h.loc[2]-start[2] };
        const float len = dvr::blade::len3(d);
        h.dist = dvr::blade::len3(e);
        if (len > 1e-3f) {
            const float t = (e[0]*d[0] + e[1]*d[1] + e[2]*d[2]) / len;
            const float p[3] = { e[0]-d[0]/len*t, e[1]-d[1]/len*t, e[2]-d[2]/len*t };
            h.offLine = dvr::blade::len3(p);
        }
    }
    *out = h;
    return true;
}

// What was hit. The verdict is kept per CLASS pointer: the ancestry walk reads each class
// in the chain with a checked read, which is a system call, and a fight hits the same few
// classes thousands of times.
enum BtKind { kBtMiss = 0, kBtCharacter, kBtWorld, kBtOther, kBtUnverified };
static const char* BtKindName(int k) {
    static const char* n[] = { "nothing", "a CHARACTER", "the WORLD", "another actor", "an actor that could NOT be verified as live" };
    return k >= 0 && k <= kBtUnverified ? n[k] : "?";
}
struct BtClass { uint8_t* cls; int kind; bool breakable; char name[64]; LONG hits; };
static BtClass g_btClass[48]; static int g_btClassN = 0;
static double  g_btLastRebuildMs = 0;

// Does the class, or one it derives from, carry `part` in its name? The walk PossIsA makes,
// asked a different question. Once per class, never per trace.
static bool BtAncestryNamed(uint8_t* actor, const char* part, char* which, size_t cap)
{
    if (!actor || !RangeReadable(actor + kClassOff, 4)) return false;
    uint8_t* cls = *(uint8_t**)(actor + kClassOff);
    for (int depth = 0; cls && depth < 32; ++depth) {
        const char* n = PossStructName(cls);
        if (!n || !strcmp(n, "Object")) return false;
        if (strstr(n, part)) { _snprintf_s(which, cap, _TRUNCATE, "%s", n); return true; }
        if (!RangeReadable(cls + kSuperFieldOff, 4)) return false;
        cls = *(uint8_t**)(cls + kSuperFieldOff);
    }
    return false;
}

static int BtClassify(uint8_t* actor, const char** nameOut, bool* breakableOut = nullptr)
{
    *nameOut = "?";
    if (breakableOut) *breakableOut = false;
    if (!actor) return kBtMiss;
    if (!IsLiveObject(actor)) {
        // A pawn spawned since the table was built is a live object the table has not seen.
        const double now = MaimNowMs();
        if (now - g_btLastRebuildMs >= 1000.0) { g_btLastRebuildMs = now; BuildLiveSet(); }
        if (!IsLiveObject(actor)) return kBtUnverified;
    }
    if (!RangeReadable(actor + kClassOff, 4)) return kBtUnverified;
    uint8_t* cls = *(uint8_t**)(actor + kClassOff);
    for (int i = 0; i < g_btClassN; ++i) if (g_btClass[i].cls == cls) {
        InterlockedIncrement(&g_btClass[i].hits); *nameOut = g_btClass[i].name;
        if (breakableOut) *breakableOut = g_btClass[i].breakable;
        return g_btClass[i].kind;
    }
    const char* cn = ObjClassName(actor);
    int kind = kBtOther;
    // WHAT BREAKS. The only evidence so far is the vocabulary (PLAN-contact-sword 7.4): the
    // planks that block a doorway are 'DishonoredBreakableNavBlock'. So a class counts as
    // something that breaks when it, or a class it derives from, says so in its name. That
    // is a rule about names and is reported as one: the line below prints which name.
    char by[64] = "";
    const bool breakable = BtAncestryNamed(actor, "Breakable", by, sizeof(by));
    if (breakableOut) *breakableOut = breakable;
    const int pawn = PossIsA(actor, "Pawn");
    if (pawn == 1) kind = kBtCharacter;
    else if (cn && (!strcmp(cn, "WorldInfo") || PossIsA(actor, "Brush") == 1 || PossIsA(actor, "StaticMeshActor") == 1 ||
                    PossIsA(actor, "StaticMeshCollectionActor") == 1 || PossIsA(actor, "InterpActor") == 1)) kind = kBtWorld;
    if (g_btClassN < (int)(sizeof(g_btClass) / sizeof(g_btClass[0]))) {
        BtClass& c = g_btClass[g_btClassN++];
        c.cls = cls; c.kind = kind; c.hits = 1; c.breakable = breakable;
        _snprintf_s(c.name, sizeof(c.name), _TRUNCATE, "%s", cn ? cn : "?");
        *nameOut = c.name;
        // THE VOCABULARY: every class the blade has ever touched, once, with what it was
        // taken for. "Breakable" is defined from this list and not before it.
        Log("blade/trace: first contact with class '%s' -> %s%s%s%s (Pawn ancestry %d; class %d of %d kept)",
            c.name, BtKindName(kind), breakable ? ", something that BREAKS (its class chain names '" : "", by, breakable ? "')" : "",
            pawn, g_btClassN, (int)(sizeof(g_btClass) / sizeof(g_btClass[0])));
    } else *nameOut = cn ? cn : "?";
    return kind;
}

// ---- the self-test: known answers, each able to be wrong ------------------------------
//
// WHAT THE FLOOR TEST CAN AND CANNOT DEMAND. On flat ground the floor is exactly the
// capsule's half height under the pawn's location. The first run of this test stood on the
// sewer save's ledge and read 76.0 crouched and 99.2 standing against half heights of 65.0
// and 87.5: 11 uu more both times, on a surface whose normal leans (z 0.98). The capsule
// rests on its rim there, so the floor under its CENTRE is lower. "About the half height"
// is therefore a band, never less than the half height and up to 25 uu more, and the
// tests that pin the trace are the ones a slope cannot move: the same floor from 50 uu
// higher must be 50 uu further, to the unit, and the same floor from the other stance must
// be at the same height in the world while the pawn itself has moved.
static struct { bool have = false; float floorZ = 0, half = 0; } g_btLastSelf;

static void BtSelfTest()
{
    uint8_t* pawn = PawnForCollision();
    const float half = PawnCollisionHeight();
    if (!pawn || !g_actorLocOff || !RangeReadable(pawn + g_actorLocOff, 12) || half < 0) {
        DVR_WARN("blade/trace: SELFTEST NOT RUN - pawn %p, Actor.Location offset 0x%x, capsule half height %.1f (each has to be known)",
                 pawn, g_actorLocOff, half);
        return;
    }
    float loc[3]; memcpy(loc, pawn + g_actorLocOff, 12);
    int pass = 0, fail = 0, skipped = 0; const char* why = "";
    auto say = [&](int ok, const char* name, const char* fmt, ...) {
        char t[340]; va_list ap; va_start(ap, fmt); _vsnprintf_s(t, sizeof(t), _TRUNCATE, fmt, ap); va_end(ap);
        if (ok < 0) ++skipped; else if (ok) ++pass; else ++fail;
        Log("blade/trace: selftest %-28s %s  %s", name, ok < 0 ? "SKIP" : ok ? "PASS" : "FAIL", t);
    };
    float floorDist = -1, floorZ = 0;
    // 1. Straight down, past the feet: the floor, facing up, on the traced line, at about
    //    the capsule's half height (never less; more on a slope or at an edge).
    {
        const float end[3] = { loc[0], loc[1], loc[2] - (half + 60.0f) }; BtHit h; const char* cn = "?";
        const bool ran = BtTrace(pawn, loc, end, &h, &why);
        const int kind = ran && h.hit ? BtClassify(h.actor, &cn) : kBtMiss;
        const bool ok = ran && h.hit && h.dist >= half - 2.0f && h.dist <= half + 25.0f && h.normal[2] > 0.7f && h.offLine < 1.0f;
        if (ran && h.hit) { floorDist = h.dist; floorZ = h.loc[2]; }
        say(ok, "floor under the feet",
            "%s: hit %d at %.1f uu (want %.1f to %.1f: the capsule's half height, and up to 25 more where the ground leans), normal z "
            "%.2f (want over 0.7), %.2f uu off the line, the floor is at z %.1f, class '%s' = %s, %.0f us",
            ran ? "ran" : why, (int)(ran && h.hit), h.dist, half - 2.0f, half + 25.0f, h.normal[2], h.offLine, h.loc[2], cn, BtKindName(kind), h.us);
    }
    // 2. The same line STOPPING 10 uu short of where test 1 found the floor: nothing. A
    //    trace that says "hit" whatever it is asked passes test 1 and fails this one.
    if (floorDist > 12.0f) {
        const float end[3] = { loc[0], loc[1], loc[2] - (floorDist - 10.0f) }; BtHit h;
        const bool ran = BtTrace(pawn, loc, end, &h, &why);
        say(ran && !h.hit, "stops short of the floor", "%s: hit %d at %.1f uu (want NO hit: the trace ends 10 uu above the floor test 1 found at %.1f)",
            ran ? "ran" : why, (int)(ran && h.hit), h.dist, floorDist);
    } else say(0, "stops short of the floor", "test 1 found no floor to stop short of");
    // 3. Inside the player's own capsule, upward: nothing. The pawn the trace is made for
    //    must not be its own obstacle, or the blade would always be touching something.
    {
        const float end[3] = { loc[0], loc[1], loc[2] + (half - 10.0f) }; BtHit h; const char* cn = "?";
        const bool ran = BtTrace(pawn, loc, end, &h, &why);
        const bool self = ran && h.hit && h.actor == pawn;
        if (ran && h.hit) BtClassify(h.actor, &cn);
        say(ran && !self, "the player is not a target", "%s: hit %d%s%s (want no hit on the player's own pawn; a low ceiling is allowed and named)",
            ran ? "ran" : why, (int)(ran && h.hit), self ? " ON THE PLAYER'S OWN PAWN" : ran && h.hit ? " on class " : "", ran && h.hit && !self ? cn : "");
    }
    // 4. The same floor from 50 uu higher: 50 uu further, to the unit. A trace that returns
    //    a constant, or a start that is not honoured, cannot do this.
    if (floorDist > 0) {
        const float start[3] = { loc[0], loc[1], loc[2] + 50.0f };
        const float end[3] = { loc[0], loc[1], loc[2] - (half + 60.0f) }; BtHit h;
        const bool ran = BtTrace(pawn, start, end, &h, &why);
        say(ran && h.hit && fabsf(h.dist - (floorDist + 50.0f)) <= 1.0f, "the floor from 50 uu up", "%s: hit %d at %.1f uu (want %.1f +/- 1)",
            ran ? "ran" : why, (int)(ran && h.hit), h.dist, floorDist + 50.0f);
    } else say(0, "the floor from 50 uu up", "test 1 found no floor");
    // 5. The same floor from the OTHER stance: the pawn's own location moves with the
    //    capsule (87.5 standing, 65 crouched), the floor does not.
    if (floorDist > 0 && g_btLastSelf.have && fabsf(g_btLastSelf.half - half) > 10.0f)
        say(fabsf(g_btLastSelf.floorZ - floorZ) <= 1.5f, "the floor across stances",
            "the floor is at z %.1f with a half height of %.1f; the last selftest found it at z %.1f with %.1f (want the same floor within 1.5 uu)",
            floorZ, half, g_btLastSelf.floorZ, g_btLastSelf.half);
    else say(-1, "the floor across stances", "%s", !g_btLastSelf.have ? "no earlier selftest to compare with: run it again in the other stance"
                                                                       : "the last selftest was in the same stance: toggle the crouch and run it again");
    if (floorDist > 0) { g_btLastSelf.have = true; g_btLastSelf.floorZ = floorZ; g_btLastSelf.half = half; }
    Log("blade/trace: SELFTEST %s - %d passed, %d failed, %d skipped, capsule half height %.1f uu (87.5 standing, 65 crouched), pawn "
        "at (%.1f %.1f %.1f)", fail ? "FAILED" : "PASSED", pass, fail, skipped, half, loc[0], loc[1], loc[2]);
}

// ---- one trace along the view: what is THAT? --------------------------------------
// An instrument for the question the blade cannot reach on a ledge: does the line check
// report a CHARACTER as one? Point the head at a guard, ask, read the class.
static void BtViewProbe(int lengthUU)
{
    uint8_t* pawn = PawnForCollision();
    float cam[3];
    if (!GameCameraAnchor(cam) && !dvr::camera::render_pos_world(cam)) { DVR_WARN("blade/trace: view probe NOT RUN - no world camera position"); return; }
    const float cp = cosf(g_viewPitchRad), F[3] = { cp * cosf(g_viewYawRad), cp * sinf(g_viewYawRad), sinf(g_viewPitchRad) };
    const float end[3] = { cam[0] + F[0] * lengthUU, cam[1] + F[1] * lengthUU, cam[2] + F[2] * lengthUU };
    BtHit h; const char* why = ""; const char* cn = "";
    if (!BtTrace(pawn, cam, end, &h, &why)) { DVR_WARN("blade/trace: view probe NOT RUN - %s", why); return; }
    const int kind = h.hit ? BtClassify(h.actor, &cn) : kBtMiss;
    Log("blade/trace: VIEW PROBE %d uu along the view (yaw %.2f pitch %.2f deg) from (%.1f %.1f %.1f): %s%s%s%s at %.1f uu, hit "
        "(%.1f %.1f %.1f) normal (%.2f %.2f %.2f), %.0f us",
        lengthUU, g_viewYawRad * 57.29578f, g_viewPitchRad * 57.29578f, cam[0], cam[1], cam[2], BtKindName(kind),
        h.hit ? " '" : "", cn, h.hit ? "'" : "", h.dist, h.loc[0], h.loc[1], h.loc[2], h.normal[0], h.normal[1], h.normal[2], h.us);
}

// ---- a fan of traces about the view: what is out there? -----------------------------
// An instrument, one shot, asked for by hand. 41 x 41 traces (1681) spread over a cone
// about the view: every class they touch is named once (the vocabulary), and the nearest
// CHARACTER, if any, is reported with where it is. It costs tens of milliseconds ONCE and
// is never run by anything but the seam word.
static void BtViewScan(int halfDeg)
{
    uint8_t* pawn = PawnForCollision();
    float cam[3];
    if (!GameCameraAnchor(cam) && !dvr::camera::render_pos_world(cam)) { DVR_WARN("blade/trace: scan NOT RUN - no world camera position"); return; }
    const int n = 41; const float reach = 4000.0f;
    unsigned kinds[kBtUnverified + 1] = {}; unsigned ran = 0, refused = 0;
    float bestDist = 1e30f, bestYaw = 0, bestPitch = 0, bestLoc[3] = {}; char bestCls[64] = "";
    LARGE_INTEGER c0, c1; QueryPerformanceCounter(&c0);
    for (int iy = 0; iy < n; ++iy) for (int ix = 0; ix < n; ++ix) {
        const float dy = ((float)ix / (n - 1) * 2.0f - 1.0f) * halfDeg * 0.0174532925f;
        const float dp = ((float)iy / (n - 1) * 2.0f - 1.0f) * halfDeg * 0.0174532925f;
        const float yaw = g_viewYawRad + dy;
        float pitch = g_viewPitchRad + dp; pitch = pitch > 1.5f ? 1.5f : pitch < -1.5f ? -1.5f : pitch;
        const float cp = cosf(pitch), F[3] = { cp * cosf(yaw), cp * sinf(yaw), sinf(pitch) };
        const float end[3] = { cam[0] + F[0] * reach, cam[1] + F[1] * reach, cam[2] + F[2] * reach };
        BtHit h; const char* why = ""; const char* cn = "";
        if (!BtTrace(pawn, cam, end, &h, &why)) { ++refused; continue; }
        ++ran;
        const int kind = h.hit ? BtClassify(h.actor, &cn) : kBtMiss;
        ++kinds[kind];
        if (kind == kBtCharacter && h.dist < bestDist) {
            bestDist = h.dist; bestYaw = dy * 57.29578f; bestPitch = dp * 57.29578f;
            memcpy(bestLoc, h.loc, 12); _snprintf_s(bestCls, sizeof(bestCls), _TRUNCATE, "%s", cn);
        }
    }
    QueryPerformanceCounter(&c1);
    const double ms = g_qpcFreq ? (double)(c1.QuadPart - c0.QuadPart) * 1000.0 / (double)g_qpcFreq : 0.0;
    Log("blade/trace: SCAN %d traces over +/-%d deg about the view, %.0f uu each, in %.1f ms (%.1f us each), %u refused: nothing %u, "
        "a CHARACTER %u, the world %u, another actor %u, unverified %u", ran, halfDeg, reach, ms, ran ? ms * 1000.0 / ran : 0.0, refused,
        kinds[kBtMiss], kinds[kBtCharacter], kinds[kBtWorld], kinds[kBtOther], kinds[kBtUnverified]);
    if (kinds[kBtCharacter])
        Log("blade/trace: SCAN nearest character: class '%s' at %.1f uu, %+.1f deg of yaw and %+.1f deg of pitch off the view, hit (%.1f %.1f %.1f)",
            bestCls, bestDist, bestYaw, bestPitch, bestLoc[0], bestLoc[1], bestLoc[2]);
    else
        Log("blade/trace: SCAN found NO character in the cone. That is an answer about the cone, not about the trace: look at a "
            "capture to see whether one stood in it");
}

// ---- the blade, traced ------------------------------------------------------------
// The published answer is dvr::hands::BladeTouch (aim_ray.h): the motion sword reads it on
// the present lane. Its kinds are this file's, value for value.
static_assert((int)dvr::hands::kTouchNothing == (int)kBtMiss && (int)dvr::hands::kTouchCharacter == (int)kBtCharacter &&
              (int)dvr::hands::kTouchWorld == (int)kBtWorld && (int)dvr::hands::kTouchOther == (int)kBtOther &&
              (int)dvr::hands::kTouchUnverified == (int)kBtUnverified, "the published kinds are the trace's kinds");
typedef dvr::hands::BladeTouch BladeContact;
static SRWLOCK g_bcLock = SRWLOCK_INIT;
static BladeContact g_bcPub;
static BladeContact BladeContactSnapshot() {
    BladeContact c; AcquireSRWLockShared(&g_bcLock); c = g_bcPub; ReleaseSRWLockShared(&g_bcLock); return c;
}
namespace dvr::hands { BladeTouch blade_touch() { return BladeContactSnapshot(); } }
static bool BtWanted() { return g_btOn.load(std::memory_order_relaxed) || dvr::hands::blade_demanded(); }
struct BtStats {
    unsigned traces = 0, hits[kBtUnverified + 1] = {}, refused = 0, apart = 0;
    unsigned sweeps = 0, sweepHits = 0, aheads = 0, aheadHits = 0;   // the two extra segments, and how often each found something the blade's own had not
    float us[256] = {}; unsigned usN = 0;
    float lagMs[256] = {}; unsigned lagN = 0;     // hand sample to answer
    char  why[160] = "off";
} g_btS;
static float BtPercentile(const float* v, unsigned n, float p) {
    if (!n) return 0;
    float t[256]; const unsigned m = n < 256 ? n : 256;
    memcpy(t, v, m * sizeof(float));
    for (unsigned i = 1; i < m; ++i) { const float x = t[i]; unsigned j = i; while (j && t[j-1] > x) { t[j] = t[j-1]; --j; } t[j] = x; }
    unsigned k = (unsigned)(p * m); if (k >= m) k = m - 1;
    return t[k];
}

// How far the live tip may leave the latched blade before the blade stops being "where the
// hand is" (prerequisite 2: at rest and in physical swings it reads 0.0000 m; while the
// game plays a clip on the hand, up to 1.45 m). Two centimetres is a bound between the two
// populations, not a tuned number.
static const float kBtLiveAgreeM = 0.02f;

// A tip cannot travel further than this between two hand samples, or over a lead: 400 uu is
// 4 m, which at 90 samples a second would be 360 m/s. Longer is a blade that was re-placed.
static const float kBtMaxStepUU = 400.0f;

static void BtTick()
{
    static uint32_t lastGen = 0;
    static bool havePrev = false; static float prevTipW[3] = {}; static int64_t prevQpc = 0;
    const dvr::hands::BladeFrame bf = dvr::hands::blade_frame();
    if (bf.ok && bf.handGen == lastGen) return;        // one blade per hand sample
    lastGen = bf.handGen;
    BladeContact c; c.handGen = bf.handGen; c.sampleMs = bf.sampleMs;
    auto refuse = [&](const char* why) {
        ++g_btS.refused; c.why = why; havePrev = false;
        _snprintf_s(g_btS.why, sizeof(g_btS.why), _TRUNCATE, "%s", why);
        AcquireSRWLockExclusive(&g_bcLock); g_bcPub = c; ReleaseSRWLockExclusive(&g_bcLock);
    };
    if (!BtResolve()) { refuse(g_btL.why); return; }
    float baseW[3], tipW[3]; const char* why = "";
    if (!BladeWorld(bf, baseW, tipW, nullptr, &why)) { refuse(why); return; }
    if (!bf.liveOk || bf.liveApartM > kBtLiveAgreeM || GetTickCount64() - bf.liveMs > 100) {
        ++g_btS.apart;
        refuse("the drawn sword is not where the hand's blade is (the game is animating the hand, or the sword is not drawn)");
        return;
    }
    uint8_t* pawn = PawnForCollision();
    BtHit h;
    if (!BtTrace(pawn, baseW, tipW, &h, &why)) { refuse(why); return; }
    auto fill = [](const BtHit& hit, dvr::hands::BladeHit& out) {
        const char* cn = ""; bool breakable = false;
        out.asked = true;
        out.kind = hit.hit ? BtClassify(hit.actor, &cn, &breakable) : kBtMiss;
        out.breakable = breakable; out.distUU = hit.hit ? hit.dist : 0.0f;
        _snprintf_s(out.cls, sizeof(out.cls), _TRUNCATE, "%s", cn);
    };
    fill(h, c.blade);
    const char* cn = c.blade.cls;
    c.ok = true;
    { const float d[3] = { tipW[0]-baseW[0], tipW[1]-baseW[1], tipW[2]-baseW[2] }; c.bladeUU = dvr::blade::len3(d); }
    // THE TWO EXTRA SEGMENTS, from the tip's own step since the last hand sample. Each is
    // one more line check (7 to 8 us). Either failing to run leaves the blade's answer
    // standing: they add to it, they never replace it.
    {
        const double dtMs = havePrev && g_qpcFreq ? (double)(bf.pubQpc - prevQpc) * 1000.0 / (double)g_qpcFreq : 0.0;
        if (havePrev && dtMs >= 4.0 && dtMs <= 100.0) {
            const float d[3] = { tipW[0]-prevTipW[0], tipW[1]-prevTipW[1], tipW[2]-prevTipW[2] };
            const float step = dvr::blade::len3(d);
            const char* w2 = "";
            if (step > 1.0f && step < kBtMaxStepUU) {
                BtHit s;
                if (BtTrace(pawn, prevTipW, tipW, &s, &w2)) {
                    fill(s, c.sweep); c.sweepUU = step; ++g_btS.sweeps;
                    if (c.sweep.kind != kBtMiss && c.blade.kind == kBtMiss) ++g_btS.sweepHits;
                }
                const float lead = dvr::hands::blade_lead_ms();
                const float reach = step * (float)(lead / dtMs);
                if (lead > 0.0f && reach > 1.0f && reach < kBtMaxStepUU) {
                    const float k = (float)(lead / dtMs);
                    const float end[3] = { tipW[0] + d[0]*k, tipW[1] + d[1]*k, tipW[2] + d[2]*k };
                    BtHit a;
                    if (BtTrace(pawn, tipW, end, &a, &w2)) {
                        fill(a, c.ahead); c.aheadUU = reach; c.leadMs = lead; ++g_btS.aheads;
                        if (c.ahead.kind != kBtMiss && c.blade.kind == kBtMiss && c.sweep.kind == kBtMiss) ++g_btS.aheadHits;
                    }
                }
            }
        }
        memcpy(prevTipW, tipW, sizeof(prevTipW)); prevQpc = bf.pubQpc; havePrev = true;
    }
    c.tracedMs = GetTickCount64();
    c.why = "traced";
    ++g_btS.traces; ++g_btS.hits[c.blade.kind];
    g_btS.us[g_btS.usN++ % 256] = (float)h.us;
    {   // from the present lane publishing this blade to the script lane holding its answer
        LARGE_INTEGER q; QueryPerformanceCounter(&q);
        const double ms = g_qpcFreq && bf.pubQpc ? (double)(q.QuadPart - bf.pubQpc) * 1000.0 / (double)g_qpcFreq : 0.0;
        g_btS.lagMs[g_btS.lagN++ % 256] = (float)ms;
    }
    _snprintf_s(g_btS.why, sizeof(g_btS.why), _TRUNCATE, "tracing");
    static int kindWas = -1; static char clsWas[64] = "";
    if (c.blade.kind != kindWas || strcmp(clsWas, cn)) {
        kindWas = c.blade.kind; strncpy_s(clsWas, cn, _TRUNCATE);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 200,
            "blade/trace: the blade touches %s%s%s%s%s - %.1f uu from its base along %.1f uu of blade, the hit %.2f uu off the blade's "
            "line, normal (%.2f %.2f %.2f); base (%.1f %.1f %.1f) tip (%.1f %.1f %.1f); %.0f us, %u ms after the hand sample",
            BtKindName(c.blade.kind), h.hit ? " '" : "", cn, h.hit ? "'" : "", c.blade.breakable ? " (it breaks)" : "", c.blade.distUU,
            c.bladeUU, h.offLine, h.normal[0], h.normal[1],
            h.normal[2], baseW[0], baseW[1], baseW[2], tipW[0], tipW[1], tipW[2], h.us, (unsigned)(c.tracedMs - c.sampleMs));
    }
    AcquireSRWLockExclusive(&g_bcLock); g_bcPub = c; ReleaseSRWLockExclusive(&g_bcLock);
}

static void BtReport()
{
    const unsigned n = g_btS.usN < 256 ? g_btS.usN : 256, m = g_btS.lagN < 256 ? g_btS.lagN : 256;
    Log("blade/trace: %s, %s | %u blade(s) traced: nothing %u, a character %u, the world %u, another actor %u, unverified %u | the "
        "tip's path since the last sample traced %u time(s) and found something the blade had not %u time(s); the path ahead "
        "(lead %.0f ms, 0 = never asked) %u and %u | refused %u "
        "(%u because the drawn sword had left the hand's blade) | cost per call: median %.0f p95 %.0f max %.0f us over the last %u | "
        "blade published to answer: median %.1f p95 %.1f max %.1f ms over the last %u | engine calls %ld, unanswered %ld | %s. Every "
        "count is 0 while the trace is off or the blade is not latched",
        g_btOn.load() ? "ON" : dvr::hands::blade_demanded() ? "ON for the motion sword (Detector=contact)" : "off",
        g_btL.ok ? "function resolved" : g_btL.why, g_btS.traces, g_btS.hits[kBtMiss],
        g_btS.hits[kBtCharacter], g_btS.hits[kBtWorld], g_btS.hits[kBtOther], g_btS.hits[kBtUnverified],
        g_btS.sweeps, g_btS.sweepHits, dvr::hands::blade_lead_ms(), g_btS.aheads, g_btS.aheadHits, g_btS.refused, g_btS.apart,
        BtPercentile(g_btS.us, n, 0.5f), BtPercentile(g_btS.us, n, 0.95f), BtPercentile(g_btS.us, n, 1.0f), n,
        BtPercentile(g_btS.lagMs, m, 0.5f), BtPercentile(g_btS.lagMs, m, 0.95f), BtPercentile(g_btS.lagMs, m, 1.0f), m,
        g_btCalls, g_btNoReturn, g_btS.why);
    char line[900]; int at = 0;
    for (int i = 0; i < g_btClassN && at < (int)sizeof(line) - 100; ++i)
        at += _snprintf_s(line + at, sizeof(line) - at, _TRUNCATE, " %s=%s%s x%ld;", g_btClass[i].name, BtKindName(g_btClass[i].kind),
                          g_btClass[i].breakable ? " that breaks" : "", g_btClass[i].hits);
    Log("blade/trace: classes touched so far (%d):%s", g_btClassN, g_btClassN ? line : " none");
}

// The script tick runs on EVERY ProcessEvent dispatch. Never nested, and once per frame
// at most (TRAPS, VR-182).
static void BladeContactTick()
{
    if (!g_bwOn.load(std::memory_order_relaxed) && !BtWanted() &&
        !g_btSelfTestReq.load(std::memory_order_relaxed) && !g_btViewReq.load(std::memory_order_relaxed) &&
        !g_btScanReq.load(std::memory_order_relaxed)) return;
    // Our own engine call re-enters the script tick. Everything this file does stops here.
    static bool inside = false;
    if (inside) return;
    static uint32_t lastFrame = 0xffffffffu;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame == lastFrame) return;
    lastFrame = frame;
    inside = true;
    if (g_bwOn.load()) BwTick();
    if (g_btSelfTestReq.load() && DvrGameplayVerdict() && BtResolve()) { g_btSelfTestReq.store(0); BtSelfTest(); }
    if (g_btViewReq.load() && DvrGameplayVerdict() && BtResolve()) { BtViewProbe(g_btViewReq.exchange(0)); }
    if (g_btScanReq.load() && DvrGameplayVerdict() && BtResolve()) { BtViewScan(g_btScanReq.exchange(0)); }
    if (BtWanted() && DvrGameplayVerdict()) BtTick();
    inside = false;
}

static bool BladeContactCommand(const char* a, const char* b, const char* c)
{
    if (!strcmp(a, "world")) {
        if (!strcmp(b, "on") || !strcmp(b, "off")) {
            g_bwOn.store(!strcmp(b, "on"));
            Log("blade/world: %s - %s", g_bwOn.load() ? "ON" : "off", g_bwOn.load()
                ? "the held blade is carried into world units each hand sample and compared with where the renderer drew it; it needs `blade on` and a latched blade"
                : "nothing is carried");
            return true;
        }
        if (!strcmp(b, "reset")) { g_bwS = BwStats{}; Log("blade/world: the comparison is cleared"); return true; }
        if (!strcmp(b, "anchor") && (!strcmp(c, "render") || !strcmp(c, "game"))) {
            g_bwAnchorRender.store(!strcmp(c, "render"));
            Log("blade/world: the XR route is anchored on the %s%s", g_bwAnchorRender.load() ? "LAST RENDER SAMPLE" : "game camera",
                g_bwAnchorRender.load() ? " - a DELIBERATE error for the instrument, never saved" : " (as every hand ray is)");
            return true;
        }
        if (!strcmp(b, "skew") && *c) {
            float v = (float)atof(c); v = v < -50 ? -50 : v > 50 ? 50 : v;
            g_bwSkewPct.store(v);
            Log("blade/world: the XR route's world scale is off by %+.1f %% - a DELIBERATE error for the instrument, never saved; 0 puts it back", v);
            return true;
        }
        BwReport();
        return true;
    }
    if (!strcmp(a, "trace")) {
        if (!strcmp(b, "on") || !strcmp(b, "off")) {
            g_btOn.store(!strcmp(b, "on"));
            Log("blade/trace: %s - %s", g_btOn.load() ? "ON" : "off", g_btOn.load()
                ? "once per hand sample the engine is asked what lies along the held blade, base to tip; it needs the blade measurement on and a latched blade. Read-only"
                : "the engine is not asked anything");
            return true;
        }
        if (!strcmp(b, "selftest")) { g_btSelfTestReq.store(1); Log("blade/trace: selftest asked for; it runs on the script lane at the next gameplay tick"); return true; }
        if (!strcmp(b, "scan")) {
            int n = *c ? atoi(c) : 30; n = n < 2 ? 2 : n > 80 ? 80 : n;
            g_btScanReq.store(n); Log("blade/trace: a scan of +/-%d deg about the view is asked for (1681 traces, once)", n); return true;
        }
        if (!strcmp(b, "view")) {
            int n = *c ? atoi(c) : 3000; n = n < 50 ? 50 : n > 20000 ? 20000 : n;
            g_btViewReq.store(n); Log("blade/trace: one trace of %d uu along the view is asked for", n); return true;
        }
        if (!strcmp(b, "resolve"))  { g_btResolveReq.store(1); Log("blade/trace: the function is looked up again at the next use"); return true; }
        if (!strcmp(b, "reset"))    { g_btS = BtStats{}; Log("blade/trace: the counts are cleared"); return true; }
        BtReport();
        return true;
    }
    Log("blade: world on|off|status|reset | world anchor game|render | world skew <percent> | trace on|off|status|selftest|view [uu]|scan [deg]|resolve|reset");
    return true;
}
static void BladeContactConfigure(const char* ini)
{
    g_bwOn.store(IniFloat(ini, "Blade", "World", 0) != 0.0f);
    g_btOn.store(IniFloat(ini, "Blade", "Trace", 0) != 0.0f);
    Log("config: [Blade] World=%d Trace=%d - the held blade carried into the game's world units on the script lane; World "
        "checks it against where the renderer drew it, Trace asks the engine's own line trace what lies along it (VR-173). "
        "Both read-only; off, nothing runs", (int)g_bwOn.load(), (int)g_btOn.load());
}
static void BladeContactSave(const char* ini)
{
    WritePrivateProfileStringA("Blade", "World", g_bwOn.load() ? "1" : "0", ini);
    WritePrivateProfileStringA("Blade", "Trace", g_btOn.load() ? "1" : "0", ini);
}
static void BladeContactStatus(dvr::status::Writer& w)
{
    w.obj("bladeWorld");
    w.kv("on", g_bwOn.load());
    w.kv("frames", (unsigned long)g_bwS.frames); w.kv("refused", (unsigned long)g_bwS.refused);
    w.kv("noFreshDraw", (unsigned long)g_bwS.stale);
    const char* name[2] = { "left", "right" };
    for (int e = 0; e < 2; ++e) {
        const BwSide& s = g_bwS.eye[e];
        w.obj(name[e]);
        w.kv("n", (unsigned long)s.n);
        w.kv("tipMeanUU", s.n ? s.sumTip / s.n : 0.0); w.kv("tipMaxUU", (double)s.maxTip);
        w.kv("baseMeanUU", s.n ? s.sumBase / s.n : 0.0); w.kv("baseMaxUU", (double)s.maxBase);
        w.end_obj();
    }
    w.kv("bladeUU", (double)g_bwS.lastLenUU);
    w.kv("why", g_bwS.why);
    w.end_obj();
    const BladeContact c = BladeContactSnapshot();
    const unsigned n = g_btS.usN < 256 ? g_btS.usN : 256, m = g_btS.lagN < 256 ? g_btS.lagN : 256;
    w.obj("bladeTrace");
    w.kv("on", g_btOn.load()); w.kv("demanded", dvr::hands::blade_demanded());
    w.kv("resolved", g_btL.ok); w.kv("nativeIndex", (unsigned long)g_btL.nativeIndex);
    w.kv("sweeps", (unsigned long)g_btS.sweeps); w.kv("sweepFoundMore", (unsigned long)g_btS.sweepHits);
    w.kv("aheads", (unsigned long)g_btS.aheads); w.kv("aheadFoundMore", (unsigned long)g_btS.aheadHits);
    w.kv("traces", (unsigned long)g_btS.traces); w.kv("refused", (unsigned long)g_btS.refused);
    w.kv("swordNotOnTheHand", (unsigned long)g_btS.apart);
    w.kv("nothing", (unsigned long)g_btS.hits[kBtMiss]); w.kv("character", (unsigned long)g_btS.hits[kBtCharacter]);
    w.kv("world", (unsigned long)g_btS.hits[kBtWorld]); w.kv("other", (unsigned long)g_btS.hits[kBtOther]);
    w.kv("unverified", (unsigned long)g_btS.hits[kBtUnverified]);
    w.kv("usMedian", (double)BtPercentile(g_btS.us, n, 0.5f)); w.kv("usP95", (double)BtPercentile(g_btS.us, n, 0.95f));
    w.kv("lagMsMedian", (double)BtPercentile(g_btS.lagMs, m, 0.5f)); w.kv("lagMsP95", (double)BtPercentile(g_btS.lagMs, m, 0.95f));
    w.kv("lagMsMax", (double)BtPercentile(g_btS.lagMs, m, 1.0f));
    w.kv("touching", BtKindName(c.ok ? c.blade.kind : kBtMiss)); w.kv("touchingClass", c.blade.cls);
    w.kv("touchingBreaks", c.blade.breakable);
    w.kv("touchingAtUU", (double)c.blade.distUU);
    w.kv("why", g_btS.why);
    w.end_obj();
}

#undef DVR_CAT
