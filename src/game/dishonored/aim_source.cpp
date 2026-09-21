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
static void AimSourceTick()
{
    SpawnCensusTick();   // VR-166: the spawn-site census rides the same arming
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
struct SpwCall { uint32_t ret; uint8_t* obj; };
static SpwCall g_spwRing[64];
static volatile LONG g_spwHead = 0, g_spwTail = 0, g_spwHits = 0;

extern "C" void __cdecl SpawnCensusHook(uint8_t* obj, uint32_t ret)
{
    InterlockedIncrement(&g_spwHits);
    const LONG h = g_spwHead;
    if (h - g_spwTail >= 64) return;
    g_spwRing[h % 64] = { ret, obj };
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
        push eax
        push ecx
        call SpawnCensusHook
        add esp, 8
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
