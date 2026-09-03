// game/dishonored/scene_probe.cpp - the instruments that derive the scene-draw
// root (S2b, task one). Included by the unity build; all default off; every
// instrument is a counter or a one-shot, driven by the `reentry <verb>` words
// (commands.cpp). Nothing here patches code: it observes, and what it finds
// goes to patterns.h with its derivation in ENGINE_NOTES.
//
// The method is BioShock Infinite's session 40 (bioshock-1-vr-mod, camera.cpp
// 390-582): a caller census at the camera write names the once-per-tick
// dispatcher, a one-shot stack scrape from that dispatch names everything
// above it (RtlCaptureStackBackTrace is cut short by frame-pointer-omitted
// frames, so the raw call-preceded scrape walks the whole chain), and the
// static tool (tools\pe-xref.ps1) only has to CONFIRM entries, never guess
// them. Static walking failed twice on that engine; derive live.
//
//   reentry census on|off|report   return addresses of the ProcessEvent callers
//                                  that dispatch ProcessViewRotation, with the
//                                  present delta per report (the once-per-tick
//                                  caller's count matches the presents)
//   reentry stack event <name>     one-shot scrape from the next dispatch of
//                                  that script event (PostRender / DrawHUD fire
//                                  INSIDE the viewport draw in UE3)
//   reentry stack caller <hex>     one-shot scrape from the next dispatch whose
//                                  ProcessEvent caller returns to <hex>
//   reentry stack present          one-shot scrape from the next Present (the
//                                  present thread's chain: a root that presents
//                                  in its own tail shows here)
//   reentry probe <hex> [len]      SEH-guarded hex dump at an address, and the
//                                  target of an E8 call found there
//   reentry status                 thread ids, arms, census summary

#include <intrin.h>

static const int kSpCallerSlots = 16;
struct SpCaller { uint32_t addr; uint32_t count; uint32_t lastDump; };
static SpCaller g_spCallers[kSpCallerSlots];
static int      g_spCallerN = 0;
static volatile LONG g_spCensusOn = 0;
static uint32_t g_spCensusPresents = 0;   // presents at the last report
static uint32_t g_spCensusDispatches = 0, g_spCensusUnverified = 0;
static DWORD    g_spScriptTid = 0;
static volatile LONG g_spStackArm = 0;    // 0 none, 1 event, 2 caller, 3 present
static char     g_spStackEvent[48] = "";
static uint32_t g_spStackCaller = 0;
static uint32_t g_spScrapes = 0;

static inline bool SpInImage(uint32_t v) { return v >= kModBase + 0x1000 && v < kModEnd; }

// The bytes before a return address decode as a call? Heuristic (a data
// dword can false-positive), fine for a derivation instrument - every hit is
// confirmed offline before anything hooks it. Returns the call form, 0 = no.
static uint32_t SpCallForm(uint32_t ret)
{
    if (!RangeReadable((void*)(ret - 7), 7)) return 0;
    const uint8_t* p = (const uint8_t*)ret;
    if (p[-5] == 0xE8) return 0xE8;                              // call rel32
    if (p[-6] == 0xFF && p[-5] == 0x15) return 0xFF15;            // call [m32]
    if (p[-2] == 0xFF && (p[-1] & 0xF8) == 0xD0) return 0xFFD0;   // call reg
    if (p[-2] == 0xFF && (p[-1] & 0xF8) == 0x10) return 0xFF10;   // call [reg]
    if (p[-3] == 0xFF && (p[-2] & 0xF8) == 0x50) return 0xFF50;   // call [reg+d8]
    if (p[-6] == 0xFF && (p[-5] & 0xF8) == 0x90) return 0xFF90;   // call [reg+d32]
    if (p[-7] == 0xFF && p[-6] == 0x14) return 0xFF14;            // call [sib]
    return 0;
}

// The target of an E8 at ret-5 (the function the frame ENTERED): the chain
// names candidate entries directly, no function-start heuristic needed.
static uint32_t SpCallTarget(uint32_t ret)
{
    if (SpCallForm(ret) != 0xE8) return 0;
    int32_t rel = *(const int32_t*)(ret - 4);
    return (uint32_t)((int32_t)ret + rel);
}

// The raw scrape: every dword from the anchor upward that is an image address
// whose preceding bytes decode as a call. POD-only, SEH-guarded (a stack
// read past the top faults; the guard answers "fewer hits", never a crash).
struct SpHit { uint32_t ret; uint32_t form; uint32_t target; };
static int SpScrapeStack(const void* anchor, SpHit* out, int maxOut)
{
    int n = 0;
    __try {
        const uint32_t* sp = (const uint32_t*)anchor;
        for (int i = 0; i < 0x800 && n < maxOut; ++i) {
            const uint32_t v = sp[i];
            if (!SpInImage(v)) continue;
            const uint32_t form = SpCallForm(v);
            if (!form) continue;
            if (n && out[n - 1].ret == v) continue;   // a value pushed twice
            out[n].ret = v; out[n].form = form; out[n].target = form == 0xE8 ? SpCallTarget(v) : 0;
            ++n;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return n;
}

static void SpDoScrape(const char* why, const void* anchor)
{
    ++g_spScrapes;
    void* frames[32] = {};
    const USHORT nf = RtlCaptureStackBackTrace(0, 32, frames, NULL);
    Log("reentry/stack: one-shot (%s) on thread %lu - RtlCaptureStackBackTrace %u frames "
        "(image addresses as 0x........; 'out' = outside the image):", why, GetCurrentThreadId(), nf);
    for (USHORT i = 0; i < nf; ++i) {
        const uint32_t v = (uint32_t)(uintptr_t)frames[i];
        if (SpInImage(v))
            Log("reentry/stack:   #%u 0x%08x (call form %X%s%08x)", i, v, SpCallForm(v),
                SpCallForm(v) == 0xE8 ? ", enters 0x" : ", target 0x", SpCallTarget(v));
        else
            Log("reentry/stack:   #%u out (%p)", i, frames[i]);
    }
    SpHit hits[40];
    const int nh = SpScrapeStack(anchor, hits, 40);
    Log("reentry/stack: scrape from %p: %d call-preceded image dwords, walk-up order (heuristic; "
        "an E8 hit names the function that frame ENTERED):", anchor, nh);
    for (int i = 0; i < nh; ++i) {
        if (hits[i].form == 0xE8)
            Log("reentry/stack:   ret 0x%08x  call rel32 -> enters 0x%08x", hits[i].ret, hits[i].target);
        else
            Log("reentry/stack:   ret 0x%08x  call form %X (indirect: the entry is in a register/vtable)",
                hits[i].ret, hits[i].form);
    }
}

// PeHandler: called for every dispatch with the ProcessEvent CALLER's return
// address (the dword the hand-built stub left above its pushad/pushfd frame)
// and the event's name index. Cheap: pointer compares unless armed.
static void SceneProbeOnDispatch(uint32_t callerRet, uint32_t nameIdx, bool isViewRot, const void* anchor, void* obj)
{
    if (!g_spScriptTid) g_spScriptTid = GetCurrentThreadId();
    if (isViewRot && InterlockedCompareExchange(&g_spCensusOn, 0, 0)) {
        ++g_spCensusDispatches;
        uint32_t key = callerRet;
        if (!SpInImage(callerRet) || !SpCallForm(callerRet)) { key = 0xFFFFFFFFu; ++g_spCensusUnverified; }
        int slot = -1;
        for (int i = 0; i < g_spCallerN; ++i) if (g_spCallers[i].addr == key) { slot = i; break; }
        if (slot < 0 && g_spCallerN < kSpCallerSlots) {
            slot = g_spCallerN++;
            g_spCallers[slot].addr = key; g_spCallers[slot].count = 0; g_spCallers[slot].lastDump = 0;
        }
        if (slot >= 0) ++g_spCallers[slot].count;
    }
    const LONG arm = InterlockedCompareExchange(&g_spStackArm, 0, 0);
    if (arm == 1) {
        const char* nm = RealName(nameIdx);
        if (nm && !strcmp(nm, g_spStackEvent) && InterlockedCompareExchange(&g_spStackArm, 0, 1) == 1) {
            char why[160];
            const char* cn = (obj && !((uintptr_t)obj & 3) && RangeReadable(obj, kClassOff + 4)) ? ObjClassName((uint8_t*)obj) : NULL;
            _snprintf(why, sizeof(why), "event %s on %s @%p, ProcessEvent caller 0x%08x", nm, cn ? cn : "?", obj, callerRet);
            why[sizeof(why) - 1] = 0;
            SpDoScrape(why, anchor);
        }
    } else if (arm == 2) {
        if (callerRet == g_spStackCaller && InterlockedCompareExchange(&g_spStackArm, 0, 2) == 2) {
            char why[160];
            const char* cn = (obj && !((uintptr_t)obj & 3) && RangeReadable(obj, kClassOff + 4)) ? ObjClassName((uint8_t*)obj) : NULL;
            _snprintf(why, sizeof(why), "caller 0x%08x, event %s on %s", callerRet, RealName(nameIdx) ? RealName(nameIdx) : "?", cn ? cn : "?");
            why[sizeof(why) - 1] = 0;
            SpDoScrape(why, anchor);
        }
    }
}

// Present thread, once per present.
static void SceneProbePresentTick()
{
    if (InterlockedCompareExchange(&g_spStackArm, 0, 3) == 3) {
        int anchor = 0;
        SpDoScrape("present", &anchor);
    }
}

static void SceneProbeReport()
{
    const uint32_t presents = g_frame;
    const uint32_t dp = presents - g_spCensusPresents;
    Log("reentry/census: %s - %lu ProcessViewRotation dispatches (%lu with an unverifiable caller), "
        "presents +%lu since the last report (a caller whose delta matches the presents is the "
        "once-per-tick scene path); script tid %lu, present tid %lu%s",
        g_spCensusOn ? "ON" : "off", (unsigned long)g_spCensusDispatches, (unsigned long)g_spCensusUnverified,
        (unsigned long)dp, (unsigned long)g_spScriptTid, (unsigned long)g_presentTid,
        (g_spScriptTid && g_presentTid) ? (g_spScriptTid == g_presentTid ? " (SAME thread: the game presents from its own tick)"
                                                                          : " (different threads: a render thread presents)") : "");
    for (int i = 0; i < g_spCallerN; ++i) {
        SpCaller& c = g_spCallers[i];
        if (c.addr == 0xFFFFFFFFu)
            Log("reentry/census:   caller ? (not call-preceded / outside the image) count=%lu delta=%lu",
                (unsigned long)c.count, (unsigned long)(c.count - c.lastDump));
        else
            Log("reentry/census:   caller ret 0x%08x (call form %X%s%08x) count=%lu delta=%lu",
                c.addr, SpCallForm(c.addr), SpCallForm(c.addr) == 0xE8 ? ", calls 0x" : ", target 0x",
                SpCallTarget(c.addr), (unsigned long)c.count, (unsigned long)(c.count - c.lastDump));
        c.lastDump = c.count;
    }
    g_spCensusPresents = presents;
}

static void SceneProbeDump(uint32_t addr, uint32_t len)
{
    if (len < 8) len = 32;
    if (len > 256) len = 256;
    if (!SpInImage(addr) || !RangeReadable((void*)addr, len)) {
        Log("reentry/probe: 0x%08x is not readable inside the image (0x%08x..0x%08x)", addr, (unsigned)kModBase, (unsigned)kModEnd);
        return;
    }
    char hex[3 * 256 + 8];
    const uint8_t* p = (const uint8_t*)addr;
    for (uint32_t i = 0; i < len; ++i) _snprintf(hex + i * 3, 4, "%02x ", p[i]);
    hex[len * 3] = 0;
    Log("reentry/probe: 0x%08x [%lu]: %s", addr, (unsigned long)len, hex);
    if (p[0] == 0xE8) {
        const int32_t rel = *(const int32_t*)(p + 1);
        Log("reentry/probe: E8 at 0x%08x calls 0x%08x", addr, (uint32_t)((int32_t)addr + 5 + rel));
    }
    // A common prologue, named for the reader: push ebp/mov ebp,esp; or the
    // SEH prologue 6A FF 68 imm32 64 A1 00000000 (push -1; push handler; mov eax,fs:[0]).
    if (p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC) Log("reentry/probe: prologue push ebp; mov ebp,esp");
    else if (p[0] == 0x6A && p[1] == 0xFF && p[2] == 0x68 && p[7] == 0x64 && p[8] == 0xA1)
        Log("reentry/probe: SEH prologue (push -1; push 0x%08x; mov eax,fs:[0]) - a function entry", *(const uint32_t*)(p + 3));
    // The first `ret imm16` in the next 4 KB names the stack-arg count (imm/4)
    // when this is the entry and the function has one epilogue.
    for (uint32_t i = 0; i < 0x1000 && RangeReadable((void*)(addr + i), 3); ++i) {
        if (p[i] == 0xC2 && p[i + 2] == 0x00) {
            Log("reentry/probe: first ret imm16 at +0x%x: ret %u (%u stack arg(s) if this is the entry's epilogue)",
                i, p[i + 1], p[i + 1] / 4);
            break;
        }
        if (p[i] == 0xC3 && i > 0 && (p[i - 1] == 0x5D || p[i - 1] == 0x5E || p[i - 1] == 0xC9)) {
            Log("reentry/probe: first plain ret at +0x%x (no stack args, or cdecl)", i);
            break;
        }
    }
}

// Walk BACK from an address inside a function to a plausible entry: a
// prologue (push ebp; mov ebp,esp / the SEH form / push esi-edi + sub esp)
// preceded by padding (CC), a ret (C3 / C2 imm16) or a jmp. A heuristic - on
// a frameless function it lands on the previous entry (Infinite's warning) -
// so every answer is confirmed by pe-xref (a virtual entry has 0 E8 callers
// and a .rdata vtable reference) before it goes near patterns.h.
static void SceneProbeFindStart(uint32_t inside)
{
    if (!SpInImage(inside)) { Log("reentry/findstart: 0x%08x is outside the image", inside); return; }
    int found = 0;
    for (uint32_t a = inside; a > inside - 0x2000 && a > kModBase + 0x1000 && found < 4; --a) {
        if (!RangeReadable((void*)(a - 4), 16)) break;
        const uint8_t* p = (const uint8_t*)a;
        const bool pro = (p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC) ||
                         (p[0] == 0x6A && p[1] == 0xFF && p[2] == 0x68 && p[7] == 0x64 && p[8] == 0xA1) ||
                         (p[0] == 0x56 && p[1] == 0x8B && p[2] == 0xF1) ||          // push esi; mov esi,ecx (thiscall)
                         (p[0] == 0x83 && p[1] == 0xEC) || (p[0] == 0x81 && p[1] == 0xEC);   // sub esp,imm
        if (!pro) continue;
        const uint8_t b1 = p[-1], b3 = p[-3];
        const bool boundary = b1 == 0xCC || b1 == 0xC3 || b1 == 0x90 || b3 == 0xC2 ||
                              (p[-5] == 0xE9) || (p[-2] == 0xEB);
        if (!boundary) continue;
        ++found;
        Log("reentry/findstart: candidate entry 0x%08x (%s; byte before %02x) - confirm with pe-xref "
            "(-TargetRva %x): 0 E8 callers + a .rdata ref = a virtual entry",
            a, p[0] == 0x55 ? "push ebp" : p[0] == 0x6A ? "SEH prologue" : p[0] == 0x56 ? "push esi" : "sub esp",
            b1, a - kModBase);
    }
    if (!found) Log("reentry/findstart: no prologue-with-boundary within 8 KB below 0x%08x", inside);
}

static bool SceneProbeCommand(const char* args)
{
    char sub[16] = "", a1[48] = "", a2[48] = "";
    const int n = sscanf(args, "%15s %47s %47s", sub, a1, a2);
    if (n < 1 || !strcmp(sub, "status")) {
        Log("reentry/probe: census %s, stack arm %ld (%s), scrapes %lu, script tid %lu, present tid %lu",
            g_spCensusOn ? "ON" : "off", (long)g_spStackArm,
            g_spStackArm == 1 ? g_spStackEvent : g_spStackArm == 2 ? "caller" : g_spStackArm == 3 ? "present" : "none",
            (unsigned long)g_spScrapes, (unsigned long)g_spScriptTid, (unsigned long)g_presentTid);
        return true;
    }
    if (!strcmp(sub, "census")) {
        if (!strcmp(a1, "on")) { InterlockedExchange(&g_spCensusOn, 1); g_spCensusPresents = g_frame; Log("reentry/census: ON - counting the ProcessEvent callers that dispatch ProcessViewRotation"); return true; }
        if (!strcmp(a1, "off")) { InterlockedExchange(&g_spCensusOn, 0); SceneProbeReport(); return true; }
        SceneProbeReport();
        return true;
    }
    if (!strcmp(sub, "stack")) {
        if (!strcmp(a1, "event") && a2[0]) {
            strncpy(g_spStackEvent, a2, sizeof(g_spStackEvent) - 1);
            g_spStackEvent[sizeof(g_spStackEvent) - 1] = 0;
            InterlockedExchange(&g_spStackArm, 1);
            Log("reentry/stack: armed for the next dispatch of script event '%s'", g_spStackEvent);
            return true;
        }
        if (!strcmp(a1, "caller") && a2[0]) {
            g_spStackCaller = (uint32_t)strtoul(a2, NULL, 16);
            InterlockedExchange(&g_spStackArm, 2);
            Log("reentry/stack: armed for the next dispatch whose ProcessEvent caller returns to 0x%08x", g_spStackCaller);
            return true;
        }
        if (!strcmp(a1, "present")) { InterlockedExchange(&g_spStackArm, 3); Log("reentry/stack: armed for the next Present"); return true; }
        if (!strcmp(a1, "off")) { InterlockedExchange(&g_spStackArm, 0); return true; }
        Log("reentry: stack event <name> | stack caller <hex> | stack present | stack off");
        return true;
    }
    if (!strcmp(sub, "probe") && a1[0]) {
        SceneProbeDump((uint32_t)strtoul(a1, NULL, 16), a2[0] ? (uint32_t)strtoul(a2, NULL, 0) : 32);
        return true;
    }
    if (!strcmp(sub, "findstart") && a1[0]) {
        SceneProbeFindStart((uint32_t)strtoul(a1, NULL, 16));
        return true;
    }
    return false;
}
