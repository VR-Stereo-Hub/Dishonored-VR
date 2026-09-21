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
static bool AimSourceProbeOn() { return g_asrcOn; }
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
static void PowerAimCensusTick();
static void AimSourceTick()
{
    SpawnCensusTick();   // VR-166: the spawn-site census rides the same arming
    if (g_asrcOn) TraceCensusTick();   // VR-166: and so does the trace census
    if (g_asrcOn) PowerAimCensusTick(); // VR-44: where the powers read their aim
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

// ---- VR-44: where the powers read their aim (power aim census, READ-ONLY) ----------
// Static reading (ENGINE_NOTES "Where the powers read their aim") predicts two seams:
// Windblast aims from its routine 0x00BF9570, and Possession and Devouring Swarm take their
// aim from UsePower's aim-assist search 0x00C12B00. A third site says who else asks:
// the player-camera accessor 0x00B515C0, which seven power-component call sites use to
// fetch the camera whose POV (+0x330/+0x33C) they aim from. Each hook copies the return
// address and the CLASS pointers of ECX/ESI/EDI into a ring. It never calls the engine,
// and no object pointer outlives the hook. The script lane names the classes (class
// objects outlive their instances) and logs each new (site, caller, class) once, then a
// 5 s count per key while calls arrive. What would kill the prediction: aiming Possession
// or Swarm moves no count at 0x00C12B00, or aiming Windblast moves none at 0x00BF9570.
// Armed by [Aim] SourceProbe.
namespace {
enum { kPaCam = 0, kPaWind = 1, kPaAssist = 2 };
struct PaCall { uint32_t site, ret; void* cls[3]; uint32_t item; };
PaCall g_paRing[256];
volatile LONG g_paHead = 0, g_paTail = 0, g_paHits[3] = {0, 0, 0}, g_paDropped = 0;
dvr::hooks::Detour g_paDet[3];
void* PaClassOf(uint32_t p)
{
    uint8_t* o = (uint8_t*)(uintptr_t)p;
    if (!o || ((uintptr_t)o & 3) || !RangeReadable(o + kClassOff, 4)) return nullptr;
    void* c = *(void**)(o + kClassOff);
    return (c && !((uintptr_t)c & 3) && RangeReadable((uint8_t*)c + kNameOff, 4)) ? c : nullptr;
}
}
static uint32_t g_paBackCam = (uint32_t)(kCamAccessor + sizeof(kCamAccessorBytes));
static uint32_t g_paBackWind = (uint32_t)(kWindblastAim + sizeof(kWindblastAimBytes));
static uint32_t g_paBackAssist = (uint32_t)(kPowerAssist + sizeof(kPowerAssistBytes));

// regs: the pushad block (EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX), then EFLAGS, then ret.
extern "C" void __cdecl PowerAimHook(uint32_t site, const uint32_t* regs)
{
    const uint32_t ret = regs[9];
    // The camera accessor has 95 callers; only item and power code is of interest.
    if (site == kPaCam && !(ret >= 0x00BE0000 && ret < 0x00C60000)) return;
    InterlockedIncrement(&g_paHits[site]);
    const LONG h = g_paHead;
    if (h - g_paTail >= 256) { InterlockedIncrement(&g_paDropped); return; }
    PaCall& c = g_paRing[h % 256];
    c.site = site; c.ret = ret;
    c.cls[0] = PaClassOf(regs[6]); c.cls[1] = PaClassOf(regs[1]); c.cls[2] = PaClassOf(regs[0]);
    c.item = 0;
    if (site == kPaAssist) {                                // the context's item, +0x3C
        const uint8_t* ctx = (const uint8_t*)(uintptr_t)regs[6];
        if (ctx && !((uintptr_t)ctx & 3) && RangeReadable(ctx + 0x3C, 4)) {
            const uint8_t* it = *(const uint8_t* const*)(ctx + 0x3C);
            if (it && !((uintptr_t)it & 3) && RangeReadable(it + kNameOff, 4))
                c.item = *(const uint32_t*)(it + kNameOff);
        }
    }
    InterlockedExchange(&g_paHead, h + 1);
}

extern "C" __declspec(naked) void PowerAimStubCam(void)
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
        push edx
        push 0
        call PowerAimHook
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        mov eax, dword ptr [ecx+26Ch]   ; displaced: 8b 81 6c 02 00 00
        jmp dword ptr [g_paBackCam]
    }
}
extern "C" __declspec(naked) void PowerAimStubWind(void)
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
        push edx
        push 1
        call PowerAimHook
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp                        ; displaced: 55 8b ec 83 ec 18
        mov ebp, esp
        sub esp, 18h
        jmp dword ptr [g_paBackWind]
    }
}
extern "C" __declspec(naked) void PowerAimStubAssist(void)
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
        push edx
        push 2
        call PowerAimHook
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebx                        ; displaced: 53 8b dc 83 ec 08
        mov ebx, esp
        sub esp, 8
        jmp dword ptr [g_paBackAssist]
    }
}

static void PowerAimCensusTick()
{
    static bool tried = false;
    if (!tried) {
        tried = true;
        dvr::hooks::detour_install(g_paDet[0], "power/census cam", kCamAccessor, kCamAccessorBytes,
                                   sizeof(kCamAccessorBytes), (void*)&PowerAimStubCam);
        dvr::hooks::detour_install(g_paDet[1], "power/census windblast", kWindblastAim,
                                   kWindblastAimBytes, sizeof(kWindblastAimBytes),
                                   (void*)&PowerAimStubWind);
        dvr::hooks::detour_install(g_paDet[2], "power/census assist", kPowerAssist,
                                   kPowerAssistBytes, sizeof(kPowerAssistBytes),
                                   (void*)&PowerAimStubAssist);
        Log("power/census: ARMED - camera accessor %s, Windblast aim %s, UsePower aim-assist %s. "
            "READ-ONLY. Aim each power in turn: the sites whose counts move while a power is "
            "aimed are where it reads its aim",
            g_paDet[0].on ? "hooked" : "NOT hooked", g_paDet[1].on ? "hooked" : "NOT hooked",
            g_paDet[2].on ? "hooked" : "NOT hooked");
    }
    static const char* const kSite[3] = { "camera accessor 0x00B515C0", "Windblast aim 0x00BF9570",
                                          "UsePower aim-assist 0x00C12B00" };
    struct Key { uint32_t site, ret; void* cls[3]; uint32_t item; long n, lastN; };
    static Key keys[64]; static int nKeys = 0;
    auto clsName = [](void* c) -> const char* {
        return c ? RealName(*(uint32_t*)((uint8_t*)c + kNameOff)) : nullptr;
    };
    while (g_paTail < g_paHead) {
        const PaCall c = g_paRing[g_paTail % 256];
        InterlockedIncrement(&g_paTail);
        int k = 0;
        while (k < nKeys && !(keys[k].site == c.site && keys[k].ret == c.ret &&
                              keys[k].cls[0] == c.cls[0] && keys[k].cls[1] == c.cls[1] &&
                              keys[k].item == c.item)) ++k;
        if (k == nKeys) {
            if (nKeys >= 64) continue;
            Key& n = keys[nKeys++];
            n.site = c.site; n.ret = c.ret; n.item = c.item; n.n = 0; n.lastN = 0;
            n.cls[0] = c.cls[0]; n.cls[1] = c.cls[1]; n.cls[2] = c.cls[2];
            const char* a = clsName(c.cls[0]); const char* b = clsName(c.cls[1]);
            const char* d = clsName(c.cls[2]); const char* it = c.item ? RealName(c.item) : nullptr;
            Log("power/census: NEW %s <- caller 0x%08X | ECX %s, ESI %s, EDI %s%s%s",
                kSite[c.site], c.ret, a ? a : "-", b ? b : "-", d ? d : "-",
                it ? " | item " : "", it ? it : "");
        }
        ++keys[k].n;
    }
    static double next = 0;
    const double now = MaimNowMs();
    if (now < next || !nKeys) return;
    next = now + 5000;
    for (int k = 0; k < nKeys; ++k) {
        Key& s = keys[k];
        if (s.n == s.lastN) continue;
        const char* a = clsName(s.cls[0]); const char* b = clsName(s.cls[1]);
        const char* it = s.item ? RealName(s.item) : nullptr;
        Log("power/census: %s <- 0x%08X moved +%ld in 5 s (total %ld, dropped %ld) | ECX %s, "
            "ESI %s%s%s", kSite[s.site], s.ret, s.n - s.lastN, s.n, (long)g_paDropped,
            a ? a : "-", b ? b : "-", it ? " | item " : "", it ? it : "");
        s.lastN = s.n;
    }
}
