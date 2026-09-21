// game/dishonored/aim_source.cpp - VR-166: WHICH ITEMS SHARE THE POWER AIM HELPER?
// A read-only probe. It writes nothing the game reads.
//
// Blink's shipped redirect (VR-36, kBlkDirHook 0xbf55a3) sits on the instruction
// right after a call to a small helper at kAimSrcHelper. The helper has exactly
// three direct callers in the image: Blink's (0xbf559e) and two more
// (0xb75b26, 0xb82e73), and each caller reads the returned 12-byte vector the
// same way. If the other two belong to other powers or thrown items, one seam
// with a per-item policy can aim all of them from the hand - which is what
// VR-166 needs. Static reading could not name the two callers (no vtable
// references, no clean prologue), so this asks the running game.
//
// What it records per call: the caller's return address, the class and name of
// the object in ECX (the helper is thiscall - Blink's call site does
// `mov ecx, esi` first), and the input vector (arg 2, which the helper's first
// instructions read as floats), with its angle off the VIEW. An input that
// sits on the view says the helper is fed head aim, and replacing that input is
// the redirect.
//
// What would kill the hypothesis: only Blink's caller ever appears, or the
// input vector does not follow the view. Either answer prints.
//
// Bounded: armed by [Aim] SourceProbe (ships 0 in code), the hook is a counter
// and a copy into a small ring; lines print from the script lane at most once
// per new (caller, class) pair plus a 5 s summary while calls arrive.

#define DVR_CAT ::dvr::log::Cat::script

static dvr::hooks::Detour g_asrcDet;
static bool g_asrcOn = false;                 // [Aim] SourceProbe
static uint32_t g_asrcRet = (uint32_t)(kAimSrcHelper + sizeof(kAimSrcHelperBytes));

struct AsrcCall { uint32_t ret; uint8_t* self; float in[3]; bool inOk; };
static const int kAsrcRing = 64;
static AsrcCall g_asrcRing[kAsrcRing];
static volatile LONG g_asrcHead = 0, g_asrcTail = 0, g_asrcHits = 0, g_asrcDropped = 0;

// Hook context: no engine calls, no allocation, no logging. Copy and leave.
extern "C" void __cdecl AimSrcHook(uint8_t* self, uint32_t ret, const float* in)
{
    InterlockedIncrement(&g_asrcHits);
    const LONG h = g_asrcHead;
    if (h - g_asrcTail >= kAsrcRing) { InterlockedIncrement(&g_asrcDropped); return; }
    AsrcCall& c = g_asrcRing[h % kAsrcRing];
    c.ret = ret; c.self = self; c.inOk = false;
    if (in && !((uintptr_t)in & 3) && RangeReadable((void*)in, 12)) {
        memcpy(c.in, in, 12); c.inOk = true;
    }
    InterlockedExchange(&g_asrcHead, h + 1);
}

// Entry of the helper: `push ebp; mov ebp,esp; mov eax,[ebp+0Ch]` (6 bytes).
// At entry [esp] is the return address, [esp+8] the input vector pointer and
// ECX the object. After pushfd/pushad/xmm save (4 + 0x20 + 0x80 bytes) those
// sit at [esp+0A4h], [esp+0ACh] and the saved ECX at [esp+98h].
extern "C" __declspec(naked) void AimSrcStub(void)
{
    __asm {
        pushfd
        pushad
        sub    esp, 80h
        movups [esp+00h], xmm0
        movups [esp+10h], xmm1
        movups [esp+20h], xmm2
        movups [esp+30h], xmm3
        movups [esp+40h], xmm4
        movups [esp+50h], xmm5
        movups [esp+60h], xmm6
        movups [esp+70h], xmm7
        mov    eax, [esp+0A4h]            ; return address
        mov    edx, [esp+0ACh]            ; arg 2: the input vector
        mov    ecx, [esp+98h]             ; ECX at entry: the object
        push   edx
        push   eax
        push   ecx
        call   AimSrcHook
        add    esp, 0Ch
        movups xmm0, [esp+00h]
        movups xmm1, [esp+10h]
        movups xmm2, [esp+20h]
        movups xmm3, [esp+30h]
        movups xmm4, [esp+40h]
        movups xmm5, [esp+50h]
        movups xmm6, [esp+60h]
        movups xmm7, [esp+70h]
        add    esp, 80h
        popad
        popfd
        push   ebp                        ; stolen: 55
        mov    ebp, esp                   ; stolen: 8b ec
        mov    eax, [ebp+0Ch]             ; stolen: 8b 45 0c
        jmp    dword ptr [g_asrcRet]
    }
}

static void AimSourceSet(bool on, const char* who)
{
    g_asrcOn = on;
    if (on && !g_asrcDet.on) {
        dvr::hooks::detour_install(g_asrcDet, "aimsrc", kAimSrcHelper, kAimSrcHelperBytes,
                                   sizeof(kAimSrcHelperBytes), (void*)&AimSrcStub);
    } else if (!on && g_asrcDet.on) {
        dvr::hooks::detour_remove(g_asrcDet, "aimsrc");
    }
    Log("aimsrc: %s (%s), hook %s at 0x%08X. READ-ONLY: names every object that asks the "
        "shared power-aim helper for a vector, and whether that vector is the view",
        on ? "ON" : "off", who, g_asrcDet.on ? "installed" : "NOT installed",
        (unsigned)kAimSrcHelper);
}

static void AimSourceConfigure(const char* ini)
{
    AimSourceSet(IniFloat(ini, "Aim", "SourceProbe", 0) != 0.0f, "ini [Aim] SourceProbe");
}

static bool AimSourceCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) { AimSourceSet(b, "seam"); ConfigWriteKey("Aim", "SourceProbe", b ? "1" : "0", "the seam"); return true; }
    Log("aimsrc: on|off (now %s), %ld calls, %ld dropped from a full ring",
        g_asrcOn ? "ON" : "off", (long)g_asrcHits, (long)g_asrcDropped);
    return true;
}

// Script lane. Drains the ring and names what it saw.
static void SpawnCensusTick();
static void TraceCensusTick();
static void RazorWatchTick();
static void AimSourceTick()
{
    SpawnCensusTick();   // VR-166: the spawn-site census rides the same arming
    if (g_asrcOn) TraceCensusTick();   // VR-166: and so does the trace census
    if (g_asrcOn) RazorWatchTick();    // VR-166: and the razor placement write-watch
    if (!g_asrcDet.on) return;
    // VR-166: the power aim fields the scripts declare. Property offsets live in the
    // packages, not the image, so they are resolved here once in gameplay; the log line
    // is what lets their native WRITERS be found offline (disasm-rva.py disp <offset>).
    static bool resolved = false;
    if (!resolved && CylTruthLive()) {
        resolved = true;
        static const char* const kProps[][2] = {
            { "DishonoredActivePowerComponent_WindBlast", "m_vOrigin" },
            { "DishonoredActivePowerComponent_WindBlast", "m_vDirection" },
            { "DishonoredActivePowerComponent", "m_TargetPoint" },
            { "DishonoredActivePowerComponent", "m_pTargetActor" },
            { "DishonoredActivePowerComponent", "m_pSuggestedTarget" },
            { "DishonoredActivePowerComponent_Possess", "m_PossessTarget" },
            { "DishonoredActivePowerComponent_Possess", "m_pHighlightedTarget" },
        };
        for (auto& p : kProps)
            Log("aimsrc/props: %s.%s at +0x%04x (0 = did not resolve) - READ-ONLY, for the "
                "offline writer search", p[0], p[1], RflOffsetOf(p[0], p[1]));
    }
    struct Seen { uint32_t ret; void* cls; long n; float worstDeg, bestDeg; char name[64]; };
    static Seen seen[32]; static int nSeen = 0;
    static double nextSummary = 0;
    const float cp = cosf(g_viewPitchRad), view[3] = { cp * cosf(g_viewYawRad),
                                                       cp * sinf(g_viewYawRad),
                                                       sinf(g_viewPitchRad) };
    while (g_asrcTail < g_asrcHead) {
        const AsrcCall c = g_asrcRing[g_asrcTail % kAsrcRing];
        InterlockedIncrement(&g_asrcTail);
        const bool obj = c.self && !((uintptr_t)c.self & 3) && LooksLikeObj(c.self);
        void* cls = obj ? *(void**)(c.self + kClassOff) : NULL;
        float deg = -1, len = 0;
        if (c.inOk) {
            len = sqrtf(c.in[0]*c.in[0] + c.in[1]*c.in[1] + c.in[2]*c.in[2]);
            if (len > 1e-4f && std::isfinite(len)) {
                float dot = (c.in[0]*view[0] + c.in[1]*view[1] + c.in[2]*view[2]) / len;
                dot = dot > 1 ? 1 : (dot < -1 ? -1 : dot);
                deg = acosf(dot) * 57.29578f;
            }
        }
        int k = 0;
        while (k < nSeen && !(seen[k].ret == c.ret && seen[k].cls == cls)) ++k;
        if (k == nSeen && nSeen < 32) {
            const char* cn = cls ? ObjClassName(c.self) : NULL;
            Seen& ns = seen[nSeen++]; ns = { c.ret, cls, 0, -1, 999, {} };
            _snprintf(ns.name, sizeof(ns.name) - 1, "%s", cn ? cn : "not a UObject");
            const char* on = (cls && RangeReadable(c.self + kNameOff, 4))
                           ? RealName(*(uint32_t*)(c.self + kNameOff)) : NULL;
            Log("aimsrc: NEW caller 0x%08X object %s '%s' | input (%.2f,%.2f,%.2f) len %.2f, "
                "%.1f deg off the view (near 0 = the helper is fed head aim; -1 = unreadable)",
                c.ret, cn ? cn : "?", on ? on : "?", c.in[0], c.in[1], c.in[2], len, deg);
        }
        if (k < nSeen) {
            Seen& s = seen[k]; ++s.n;
            if (deg >= 0) { if (deg > s.worstDeg) s.worstDeg = deg; if (deg < s.bestDeg) s.bestDeg = deg; }
        }
    }
    const double now = MaimNowMs();
    if (now >= nextSummary && nSeen) {
        nextSummary = now + 5000;
        static long lastHits = -1;
        if (g_asrcHits != lastHits) {
            lastHits = g_asrcHits;
            for (int k = 0; k < nSeen; ++k)
                Log("aimsrc: summary caller 0x%08X %s calls %ld, off-view %.1f..%.1f deg "
                    "(total %ld, dropped %ld)", seen[k].ret, seen[k].name, seen[k].n,
                    seen[k].bestDeg > 900 ? -1.0f : seen[k].bestDeg, seen[k].worstDeg,
                    (long)g_asrcHits, (long)g_asrcDropped);
        }
    }
}

// ---- VR-166: who spawns what (SpawnActor caller census, READ-ONLY) ----------
// The spring razor's throw did not pass through either throw seam, and static reading
// could not find its spawn site. SpawnActor (0x00C66070) is where every projectile is
// born: this records (caller, ECX's object name) pairs - ECX is what the call sites
// load before the call (a class/archetype object) - and names each NEW pair once. A
// razor throw then names its own caller. Spawns are events, not per-frame work.
static dvr::hooks::Detour g_spwDet;
static uint32_t g_spwRet = (uint32_t)(kSpawnActor + sizeof(kSpawnActorBytes));
struct SpwCall { uint32_t ret; uint8_t* obj; float loc[3]; bool locOk; };
static SpwCall g_spwRing[64];
static volatile LONG g_spwHead = 0, g_spwTail = 0, g_spwHits = 0;

extern "C" void __cdecl SpawnCensusHook(uint8_t* obj, uint32_t ret, const uint32_t* args)
{
    InterlockedIncrement(&g_spwHits);
    const LONG h = g_spwHead;
    if (h - g_spwTail >= 64) return;
    // args[0] = the return address's slot + 4: (0, FName[8], &Location, &Rotation, ...)
    SpwCall c = { ret, obj, {}, false };
    const float* loc = (args && RangeReadable((void*)args, 16)) ? (const float*)(uintptr_t)args[3] : nullptr;
    if (loc && RangeReadable((void*)loc, 12)) { memcpy(c.loc, loc, 12); c.locOk = true; }
    g_spwRing[h % 64] = c;
    InterlockedExchange(&g_spwHead, h + 1);
}

extern "C" __declspec(naked) void SpawnCensusStub(void)
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        mov eax, [edx+24h]          ; return address
        mov ecx, [edx+18h]          ; ECX at entry
        lea ebx, [edx+28h]          ; the first stack argument
        push ebx
        push eax
        push ecx
        call SpawnCensusHook
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp                    ; displaced: 55 8b ec 33 c0
        mov ebp, esp
        xor eax, eax
        jmp dword ptr [g_spwRet]
    }
}

static void SpawnCensusTick()
{
    if (!g_asrcOn) return;
    if (!g_spwDet.on) {
        static bool tried = false;
        if (tried) return;
        tried = true;
        dvr::hooks::detour_install(g_spwDet, "spawn/census", kSpawnActor, kSpawnActorBytes,
                                   sizeof(kSpawnActorBytes), (void*)&SpawnCensusStub);
        return;
    }
    struct Seen { uint32_t ret; uint32_t name; };
    static Seen seen[96]; static int nSeen = 0;
    while (g_spwTail < g_spwHead) {
        const SpwCall c = g_spwRing[g_spwTail % 64];
        InterlockedIncrement(&g_spwTail);
        uint32_t nm = 0;
        if (c.obj && !((uintptr_t)c.obj & 3) && RangeReadable(c.obj + kNameOff, 4)) nm = *(uint32_t*)(c.obj + kNameOff);
        // VR-166: every razor placement - where it landed, and how far that point is
        // from the HEAD ray and from the HAND ray. Whichever it sits on is the ray the
        // placement follows. Perpendicular distances, in world units.
        const char* nmS = nm ? RealName(nm) : nullptr;
        if (c.locOk && nmS && strstr(nmS, "SpringRazor")) {
            float cam[3], ho[3], hd[3]; const char* why = nullptr;
            const bool camOk = dvr::camera::render_pos_world(cam);
            const bool handOk = HandRayWorld(ho, hd, &why);
            const float cp = cosf(g_viewPitchRad);
            const float vd[3] = { cp * cosf(g_viewYawRad), cp * sinf(g_viewYawRad), sinf(g_viewPitchRad) };
            auto perp = [](const float* o, const float* d, const float* p, float* along) {
                const float v[3] = { p[0] - o[0], p[1] - o[1], p[2] - o[2] };
                const float t = v[0] * d[0] + v[1] * d[1] + v[2] * d[2];
                *along = t;
                const float q[3] = { v[0] - t * d[0], v[1] - t * d[1], v[2] - t * d[2] };
                return sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
            };
            float aH = 0, aC = 0;
            const float dHead = camOk ? perp(cam, vd, c.loc, &aH) : -1;
            const float dHand = handOk ? perp(ho, hd, c.loc, &aC) : -1;
            Log("razor/place: landed at (%.0f,%.0f,%.0f) - %.0f uu off the HEAD ray (%.0f along), "
                "%.0f uu off the HAND ray (%.0f along)%s. The smaller offset is the ray placement follows",
                c.loc[0], c.loc[1], c.loc[2], dHead, aH, dHand, aC, handOk ? "" : " [hand ray unavailable]");
        }
        int k = 0;
        while (k < nSeen && !(seen[k].ret == c.ret && seen[k].name == nm)) ++k;
        if (k < nSeen || nSeen >= 96) continue;
        seen[nSeen++] = { c.ret, nm };
        const char* on = nm ? RealName(nm) : nullptr;
        Log("spawn/census: NEW caller 0x%08X spawns via '%s' (ECX %p) - READ-ONLY; the caller that "
            "appears only when the spring razor is thrown is its spawn site",
            c.ret, on ? on : "?", (void*)c.obj);
    }
}

// ---- VR-166: who traces (trace caller census, READ-ONLY) ---------------------
// Placing a spring razor spawns it from 0x00C3BA21 at a point computed upstream -
// presumably a placement trace. This names every (entry, caller, object class) pair
// that reaches the three controller camera-trace helpers or AActor::execTrace (the
// script's Trace()), once each. The pairs that appear only while the razor is out are
// its placement trace. Four entries share one ring; each stub tags its hook id.
static dvr::hooks::Detour g_trDet[4];
static uint32_t g_trRet[4] = { (uint32_t)(kTraceHelperA + 6), (uint32_t)(kTraceHelperB + 6),
                               (uint32_t)(kTraceHelperC + 6), (uint32_t)(kExecTrace + 9) };
struct TrCall { uint32_t id, ret; uint8_t* obj; };
static TrCall g_trRing[128];
static volatile LONG g_trHead = 0, g_trTail = 0;
extern "C" void __cdecl TraceCensusHook(uint32_t id, uint8_t* obj, uint32_t ret)
{
    const LONG h = g_trHead;
    if (h - g_trTail >= 128) return;
    g_trRing[h % 128] = { id, ret, obj };
    InterlockedExchange(&g_trHead, h + 1);
}
extern "C" __declspec(naked) void TraceStubA(void)
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        mov eax, [edx+24h]          ; return address
        mov ecx, [edx+18h]          ; ECX at entry
        push eax
        push ecx
        push 0
        call TraceCensusHook
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp
        mov ebp, esp
        mov eax, [ebp+34h]
        jmp dword ptr [g_trRet + 0]
    }
}
extern "C" __declspec(naked) void TraceStubB(void)
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        mov eax, [edx+24h]          ; return address
        mov ecx, [edx+18h]          ; ECX at entry
        push eax
        push ecx
        push 1
        call TraceCensusHook
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp
        mov ebp, esp
        sub esp, 60h
        jmp dword ptr [g_trRet + 4]
    }
}
extern "C" __declspec(naked) void TraceStubC(void)
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        mov eax, [edx+24h]          ; return address
        mov ecx, [edx+18h]          ; ECX at entry
        push eax
        push ecx
        push 2
        call TraceCensusHook
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp
        mov ebp, esp
        sub esp, 60h
        jmp dword ptr [g_trRet + 8]
    }
}
extern "C" __declspec(naked) void TraceStubD(void)
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        mov eax, [edx+24h]          ; return address
        mov ecx, [edx+18h]          ; ECX at entry
        push eax
        push ecx
        push 3
        call TraceCensusHook
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp
        mov ebp, esp
        sub esp, 0E4h
        jmp dword ptr [g_trRet + 12]
    }
}

static void TraceCensusTick()
{
    static const char* const kNames[4] = { "camtrace A 0x00AA5100", "camtrace B 0x00AA60D0",
                                           "camtrace C 0x00AA5FF0", "execTrace 0x006D0ED0" };
    static bool tried = false;
    if (!tried) {
        tried = true;
        dvr::hooks::detour_install(g_trDet[0], "trace/census A", kTraceHelperA, kTraceHelperABytes, 6, (void*)&TraceStubA);
        dvr::hooks::detour_install(g_trDet[1], "trace/census B", kTraceHelperB, kTraceHelperBCBytes, 6, (void*)&TraceStubB);
        dvr::hooks::detour_install(g_trDet[2], "trace/census C", kTraceHelperC, kTraceHelperBCBytes, 6, (void*)&TraceStubC);
        dvr::hooks::detour_install(g_trDet[3], "trace/census D", kExecTrace, kExecTraceBytes, 9, (void*)&TraceStubD);
        return;
    }
    struct Seen { uint32_t id, ret; void* cls; };
    static Seen seen[160]; static int nSeen = 0;
    while (g_trTail < g_trHead) {
        const TrCall c = g_trRing[g_trTail % 128];
        InterlockedIncrement(&g_trTail);
        const bool obj = c.obj && LooksLikeObj(c.obj);
        void* cls = obj ? *(void**)(c.obj + kClassOff) : nullptr;
        int k = 0;
        while (k < nSeen && !(seen[k].id == c.id && seen[k].ret == c.ret && seen[k].cls == cls)) ++k;
        if (k < nSeen || nSeen >= 160) continue;
        seen[nSeen++] = { c.id, c.ret, cls };
        Log("trace/census: NEW %s <- caller 0x%08X on %s - READ-ONLY; the callers that appear only "
            "while the spring razor is out are its placement trace",
            kNames[c.id & 3], c.ret, obj ? ObjClassName(c.obj) : "not a UObject");
    }
}

// ---- VR-166: who writes the razor's placement point (hardware write-watch, READ-ONLY) --
// The razor spawns at its context's +0xB8 (location; +0xC4 the normal), read at
// 0x00C3B81A inside the placement routine 0x00C3B570, and it lands on the HEAD ray
// (measured: 6-10 uu off it, 28-91 uu off the hand's). Static reading did not find the
// writer, so DR0 watches +0xB8 for writes on every thread and a vectored handler records
// each writing EIP with the first three return addresses on its stack. Armed once, on
// the first live DisItemContext_UseSpringRazor; reported after 20 s or 16 writers.
// The legacy src/legacy/aim_watch.cpp is the same technique.
#include <tlhelp32.h>
namespace {
struct RwRec { uint32_t eip, n, ret[3]; };
RwRec g_rwRecs[16];
volatile LONG g_rwN = 0, g_rwTotal = 0;
uintptr_t g_rwAddr = 0;
PVOID g_rwVeh = nullptr;
double g_rwArmedMs = 0;
bool g_rwDone = false;
}
static LONG CALLBACK RazorWatchVeh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    if (!(c->Dr6 & 0x1) || !g_rwAddr) return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    InterlockedIncrement(&g_rwTotal);
    const uint32_t eip = (uint32_t)c->Eip;
    LONG n = g_rwN; if (n > 16) n = 16;
    for (LONG i = 0; i < n; ++i) if (g_rwRecs[i].eip == eip) { ++g_rwRecs[i].n; return EXCEPTION_CONTINUE_EXECUTION; }
    const LONG idx = InterlockedIncrement(&g_rwN) - 1;
    if (idx < 16) {
        RwRec& r = g_rwRecs[idx]; r.eip = eip; r.n = 1; r.ret[0] = r.ret[1] = r.ret[2] = 0;
        const uint32_t* sp = (const uint32_t*)c->Esp; int got = 0;
        for (int k = 0; k < 48 && got < 3; ++k) {
            if (!RangeReadable((void*)(sp + k), 4)) break;
            const uint32_t v = sp[k];
            if (v >= 0x401000 && v < 0xF40000) r.ret[got++] = v;
        }
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
// The placement routine's entry: remember the context it runs on. Read-only.
static uint8_t* volatile g_rwCtx = nullptr;
static dvr::hooks::Detour g_rpDet;
static uint32_t g_rpRet = (uint32_t)(kRazorPlace + sizeof(kRazorPlaceBytes));
extern "C" void __cdecl RazorPlaceHook(uint8_t* self) { if (!g_rwCtx) g_rwCtx = self; }
extern "C" __declspec(naked) void RazorPlaceStub(void)
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        mov ecx, [edx+18h]          ; ECX at entry: the razor context
        push ecx
        call RazorPlaceHook
        add esp, 4
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebx                    ; displaced: 53 8b dc 83 ec 08
        mov ebx, esp
        sub esp, 8
        jmp dword ptr [g_rpRet]
    }
}
static void RazorWatchApply(bool enable)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te; te.dwSize = sizeof(te);
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        const bool other = te.th32ThreadID != self;
        HANDLE th = other ? OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID)
                          : GetCurrentThread();
        if (!th) continue;
        if (other) SuspendThread(th);
        CONTEXT c; memset(&c, 0, sizeof(c)); c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(th, &c)) {
            if (enable) { c.Dr0 = (DWORD)g_rwAddr; c.Dr7 = (c.Dr7 & ~(0x3u | (0xFu << 16))) | 0x1u | (0x1u << 16) | (0x3u << 18); }
            else        { c.Dr0 = 0; c.Dr7 &= ~(0x3u | (0xFu << 16)); }
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            SetThreadContext(th, &c);
        }
        if (other) { ResumeThread(th); CloseHandle(th); }
    } while (Thread32Next(snap, &te));
    CloseHandle(snap);
}
static void RazorWatchTick()
{
    if (!g_rpDet.on) {
        static bool tried = false;
        if (!tried) { tried = true;
            dvr::hooks::detour_install(g_rpDet, "razor/place", kRazorPlace, kRazorPlaceBytes,
                                       sizeof(kRazorPlaceBytes), (void*)&RazorPlaceStub); }
    }
    if (g_rwDone) return;
    const double now = MaimNowMs();
    if (g_rwAddr) {
        if (now - g_rwArmedMs < 60000.0 && g_rwN < 16) return;
        RazorWatchApply(false);
        LONG n = g_rwN; if (n > 16) n = 16;
        Log("razor/watch: %ld write(s) to the placement point, %ld writer(s) - READ-ONLY; the writer that "
            "fires while aiming the razor is the placement code:", (long)g_rwTotal, (long)n);
        for (LONG i = 0; i < n; ++i)
            Log("razor/watch:   eip=0x%08X hits=%u ret=0x%08X 0x%08X 0x%08X", g_rwRecs[i].eip, g_rwRecs[i].n,
                g_rwRecs[i].ret[0], g_rwRecs[i].ret[1], g_rwRecs[i].ret[2]);
        g_rwAddr = 0; g_rwDone = true;
        return;
    }
    // The object the placement routine actually runs on, captured at its entry (build 611
    // armed on the class-named template instead and saw 0 writes).
    uint8_t* ctx = g_rwCtx;
    if (!ctx || !RangeReadable(ctx + 0xB8, 12)) return;
    if (!g_rwVeh) g_rwVeh = AddVectoredExceptionHandler(1, RazorWatchVeh);
    g_rwAddr = (uintptr_t)(ctx + 0xB8); g_rwArmedMs = now; g_rwN = 0; g_rwTotal = 0;
    RazorWatchApply(true);
    Log("razor/watch: ARMED on the placing context %s %p +0xB8 for 60 s - place the razor again",
        LooksLikeObj(ctx) ? ObjClassName(ctx) : "?", (void*)ctx);
}
