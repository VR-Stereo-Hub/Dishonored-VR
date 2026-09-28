// VR-79: an object hidden from one eye vanishes from both.
//
// UE3 culls with hardware occlusion queries. Each primitive's query history
// (last frame's result, the next query to issue) lives in the VIEW STATE, and
// the local player owns one: LocalPlayer.ViewState. `reentry` draws the scene
// twice per tick through the same local player, so both eyes read and write
// the same history and one eye's result culls the other's draw: cover an NPC's
// head with the sword in the left eye and it is gone from the right.
//
// [Stereo] Occlusion (live `occlusion native|pereye|off`):
//   native  the engine as shipped (the default; one history for both eyes).
//   pereye  the RIGHT eye gets its own view state. The mod allocates a second
//           one with the engine's own allocator (the same call the LocalPlayer
//           constructor makes) and puts it in LocalPlayer.ViewState for pass 2
//           only, restoring the left eye's straight after. Each eye then tests
//           and culls against its own depth and its own previous frame: an
//           object either eye can see is drawn in that eye, and culling still
//           saves the draws it saved before.
//   off     no occlusion culling at all: UE3's GIgnoreAllOcclusionQueries, the
//           TOGGLEOCCLUSION console switch. Correct, but every hidden draw is
//           paid for (measured laggier in the headset, 2026-09-24).
// Derivations: ENGINE_NOTES "VR-79". Every address is byte-verified first and a
// mismatch refuses (logged) and leaves the engine native.
//
// LANE: the game thread. The view is built from LocalPlayer.ViewState inside
// the viewport draw call that scene_draw.cpp makes for each pass, so the swap
// brackets exactly pass 2's call. The render thread only ever sees the pointer
// the view copied.

enum OcclMode { OCCL_NATIVE = 0, OCCL_PEREYE = 1, OCCL_OFF = 2 };
static std::atomic<int> g_occlMode{OCCL_NATIVE};
static int g_occlSwitchOk = 0, g_occlAllocOk = 0;   // 0 unchecked, 1 verified, -1 refused
static uint8_t* g_occlLp = NULL;        // the local player the swap last used
static void* g_occlLeft = NULL;         // its own view state (pass 1, the left eye)
static void* g_occlRight = NULL;        // ours (pass 2, the right eye), allocated once
static bool g_occlSwapped = false;      // pass 2 is running on the right eye's state
static uint32_t g_occlPcPlayerOff = 0;
static LONG g_occlSwaps = 0, g_occlRefused = 0;
static unsigned g_occlAttempts=0,g_occlRestores=0,g_occlRestoreRefused=0;
static const char* g_occlWhy="startup";
static CtIdentity g_occlOwners[2],g_occlSwapOwner;
static unsigned g_occlEpoch=0,g_occlSwapEpoch=0;
static LONG g_occlLoad=-1,g_occlSwapLoad=-1;
static bool g_occlHaveOwners=false;
static uint64_t g_occlOwnerRetry=0;

static void OcclBeat() {
    DVR_LOG_EVERY_MS(::dvr::log::Cat::present,::dvr::log::Level::Info,5000,
        "occlusion/pereye: attempts=%u swapped=%ld restored=%u restoreRefused=%u reason=%s "
        "eventController=%p scanController=%p localPlayer=%p left=%p right=%p load=%ld epoch=%u "
        "(cumulative; attempts without swaps means per-eye culling is NOT active)",
        g_occlAttempts,g_occlSwaps,g_occlRestores,g_occlRestoreRefused,g_occlWhy,
        g_peCtrl,g_pcObj,g_occlLp,g_occlLeft,g_occlRight,g_occlLoad,g_occlEpoch);
}

static const char* OcclModeName(int m) { return m == OCCL_PEREYE ? "pereye" : m == OCCL_OFF ? "off" : "native"; }

static bool OcclBytes(uintptr_t at, const uint8_t* want, size_t n)
{
    return RangeReadable((const void*)at, n) && !memcmp((const void*)at, want, n);
}

static bool OcclSwitchVerify()
{
    if (g_occlSwitchOk) return g_occlSwitchOk > 0;
    const bool a = OcclBytes(kOcclReaderViewSetup, kOcclReaderViewSetupBytes, sizeof(kOcclReaderViewSetupBytes));
    const bool b = OcclBytes(kOcclReaderDepthPass, kOcclReaderDepthPassBytes, sizeof(kOcclReaderDepthPassBytes));
    const bool g = RangeReadable((const void*)kIgnoreAllOcclusionQueries, 4);
    g_occlSwitchOk = a && b && g ? 1 : -1;
    Log("occlusion: engine switch %s (view-setup reader %s, depth-pass reader %s, switch %s)",
        g_occlSwitchOk > 0 ? "verified" : "REFUSED - `off` is unavailable on this build",
        a ? "ok" : "MISMATCH", b ? "ok" : "MISMATCH", g ? "readable" : "UNREADABLE");
    return g_occlSwitchOk > 0;
}

static bool OcclAllocVerify()
{
    if (g_occlAllocOk) return g_occlAllocOk > 0;
    const bool f = OcclBytes(kAllocateViewState, kAllocateViewStatePrefix, sizeof(kAllocateViewStatePrefix));
    const bool c = OcclBytes(kLocalPlayerAllocSite, kLocalPlayerAllocSiteBytes, sizeof(kLocalPlayerAllocSiteBytes));
    g_occlAllocOk = f && c ? 1 : -1;
    Log("occlusion: view-state allocator %s (allocator 0x%08X %s, LocalPlayer constructor's call and +0x%02X store %s)",
        g_occlAllocOk > 0 ? "verified" : "REFUSED - `pereye` is unavailable on this build",
        (unsigned)kAllocateViewState, f ? "ok" : "MISMATCH", (unsigned)kLocalPlayerViewStateOff, c ? "ok" : "MISMATCH");
    return g_occlAllocOk > 0;
}

static void OcclSetSwitch(bool ignoreQueries)
{
    if (!OcclSwitchVerify()) return;
    uint32_t cur = 0;
    SafeRead32(kIgnoreAllOcclusionQueries, &cur);
    if ((cur != 0) == ignoreQueries) return;
    *(volatile uint32_t*)kIgnoreAllOcclusionQueries = ignoreQueries ? 1u : 0u;
    Log("occlusion: engine switch %u -> %u", cur, ignoreQueries ? 1u : 0u);
}

static void OcclusionApply(int mode, const char* who)
{
    if (mode == OCCL_PEREYE && !OcclAllocVerify()) mode = OCCL_NATIVE;
    if (mode == OCCL_OFF && !OcclSwitchVerify()) mode = OCCL_NATIVE;
    g_occlMode.store(mode);
    OcclSetSwitch(mode == OCCL_OFF);
    Log("occlusion: mode %s (%s) | %s", OcclModeName(mode), who,
        mode == OCCL_PEREYE ? "each eye culls with its own view state: an object either eye can see is drawn in that eye"
        : mode == OCCL_OFF  ? "no occlusion culling: nothing is hidden, every hidden draw is paid for"
                            : "engine as shipped: one view state for both eyes, one eye can hide objects from the other (VR-79)");
}

static bool OcclParseMode(const char* s, int* out)
{
    if (!s) return false;
    if (!_stricmp(s, "native") || !strcmp(s, "on") || !strcmp(s, "1")) { *out = OCCL_NATIVE; return true; }
    if (!_stricmp(s, "pereye") || !_stricmp(s, "per-eye")) { *out = OCCL_PEREYE; return true; }
    if (!_stricmp(s, "off") || !strcmp(s, "0")) { *out = OCCL_OFF; return true; }
    return false;
}

static void OcclusionConfigure(const char* ini)
{
    char v[32] = "";
    GetPrivateProfileStringA("Stereo", "Occlusion", "native", v, sizeof(v), ini);
    int m = OCCL_NATIVE;
    if (!OcclParseMode(v, &m)) Log("occlusion: [Stereo] Occlusion='%s' unknown (native|pereye|off) - native", v);
    OcclusionApply(m, "[Stereo] Occlusion");
}

// The local player behind the live player controller, and its ViewState field.
static uint8_t* OcclLocalPlayer()
{
    // The controller from the event stream owns gameplay after a save load.
    // A still-live scan controller can belong to the old world or menu.
    const unsigned epoch=UiSurfaceEpoch();
    const bool edge=!g_occlHaveOwners || g_occlLoad!=g_mkLoadEvents || g_occlEpoch!=epoch ||
        g_occlOwners[0].value.obj!=g_peCtrl || !ChSlot(g_occlOwners[0]) || !ChSlot(g_occlOwners[1]);
    if(edge) {
        const auto now=GetTickCount64();
        if(now<g_occlOwnerRetry){g_occlWhy="waiting for owner refresh";return NULL;}
        g_occlOwnerRetry=now+1000;
        if(!BuildLiveSet()){g_occlWhy="live-object refresh failed";return NULL;}
    }
    if(!IsLiveObject(g_peCtrl)){g_occlWhy="event controller is not live";return NULL;}
    if (!g_occlPcPlayerOff) {
        g_occlPcPlayerOff = FindPropOffset("PlayerController", "Player");
        const uint32_t vs = FindPropOffset("LocalPlayer", "ViewState");
        if (vs != kLocalPlayerViewStateOff) {
            if (!InterlockedExchange(&g_occlRefused, 1))
                Log("occlusion/pereye: REFUSED - LocalPlayer.ViewState resolves to +0x%X, expected +0x%X",
                    vs, (unsigned)kLocalPlayerViewStateOff);
            g_occlPcPlayerOff = 0;g_occlWhy="ViewState reflection mismatch";
            return NULL;
        }
    }
    if (!g_occlPcPlayerOff || !RangeReadable(g_peCtrl + g_occlPcPlayerOff, 4)) {
        g_occlWhy="controller Player field unavailable";return NULL;
    }
    uint8_t* lp = *(uint8_t**)(g_peCtrl + g_occlPcPlayerOff);
    const bool newPlayer=g_occlOwners[1].value.obj!=lp;
    if(!edge && newPlayer) {
        const auto now=GetTickCount64();
        if(now<g_occlOwnerRetry){g_occlWhy="waiting for LocalPlayer refresh";return NULL;}
        g_occlOwnerRetry=now+1000;
        if(!BuildLiveSet()){g_occlWhy="new LocalPlayer refresh failed";return NULL;}
    }
    if (!IsLiveObject(lp)) {g_occlWhy="event controller LocalPlayer not live";return NULL;}
    const char* cn = ObjClassName(lp);
    if (!cn || !strstr(cn, "LocalPlayer") || !RangeReadable(lp + kLocalPlayerViewStateOff, 4)) {
        g_occlWhy="LocalPlayer layout unavailable";return NULL;
    }
    if(edge || newPlayer) {
        // Refresh for a newly referenced LocalPlayer too, then capture its live
        // GObjects slot and class/FName identity. Never trust pointer retention.
        CtIdentity next[2];
        if(!ChCapture(g_peCtrl,&next[0]) || !ChCapture(lp,&next[1])) {
            g_occlHaveOwners=false;g_occlWhy="owner identity capture failed";return NULL;
        }
        g_occlOwners[0]=next[0];g_occlOwners[1]=next[1];g_occlHaveOwners=true;
        g_occlEpoch=epoch;g_occlLoad=g_mkLoadEvents;
        Log("occlusion/pereye: owners revalidated eventController=%p LocalPlayer=%p load=%ld epoch=%u (scan=%p)",
            g_peCtrl,lp,g_occlLoad,g_occlEpoch,g_pcObj);
    }
    if(!ChSlot(g_occlOwners[0]) || !ChSlot(g_occlOwners[1])) {
        g_occlWhy="owner identity no longer live";return NULL;
    }
    g_occlOwnerRetry=0;
    return lp;
}

// Called by scene_draw.cpp right before pass 2's viewport draw.
static void OcclusionPass2Begin()
{
    if (g_occlMode.load() != OCCL_PEREYE) return;
    ++g_occlAttempts;
    struct BeatOnExit {~BeatOnExit(){OcclBeat();}} beat;
    if(g_occlRefused){g_occlWhy="verified layout/allocator refused";return;}
    uint8_t* lp = OcclLocalPlayer();
    if (!lp) return;
    void** field = (void**)(lp + kLocalPlayerViewStateOff);
    void* cur = *field;
    if (!cur) {g_occlWhy="LocalPlayer has no ViewState";return;}                                   // no view state: the engine is not drawing this player
    if (lp != g_occlLp || (cur != g_occlLeft && cur != g_occlRight)) {
        if (g_occlLp && lp != g_occlLp)
            Log("occlusion/pereye: local player changed %p -> %p; adopting its view state", (void*)g_occlLp, (void*)lp);
        else if (g_occlLeft && cur != g_occlLeft)
            Log("occlusion/pereye: the engine replaced the left eye's view state %p -> %p; adopting it", g_occlLeft, cur);
        g_occlLp = lp; g_occlLeft = cur;
    }
    if (cur == g_occlRight) {g_occlWhy="right state still installed, restore was missed";return;}                      // a restore was missed; never swap the right eye's state in twice
    if (!g_occlRight) {
        g_occlRight = ((void* (__cdecl*)())kAllocateViewState)();
        Log("occlusion/pereye: allocated the right eye's view state %p with the engine's allocator (left eye keeps %p, "
            "LocalPlayer %p +0x%X)", g_occlRight, g_occlLeft, (void*)lp, (unsigned)kLocalPlayerViewStateOff);
        if (!g_occlRight) { InterlockedExchange(&g_occlRefused, 1); Log("occlusion/pereye: REFUSED - the allocator returned null"); return; }
    }
    // Allocation can call engine code: validate the exact live owners again.
    if(g_peCtrl!=g_occlOwners[0].value.obj || !ChSlot(g_occlOwners[0]) || !ChSlot(g_occlOwners[1]) ||
       *(uint8_t**)(g_peCtrl+g_occlPcPlayerOff)!=lp || *field!=cur) {
        g_occlWhy="owner or ViewState changed before swap";return;
    }
    g_occlSwapOwner=g_occlOwners[1];g_occlSwapEpoch=UiSurfaceEpoch();g_occlSwapLoad=g_mkLoadEvents;
    *field = g_occlRight;
    g_occlSwapped = true;g_occlWhy="right eye has separate state";
    InterlockedIncrement(&g_occlSwaps);
}

// Called by scene_draw.cpp right after pass 2's viewport draw, success or fault.
static void OcclusionPass2End()
{
    if (!g_occlSwapped) return;
    g_occlSwapped = false;
    // Revalidate retained identity even when the pointer did not change. A
    // load/menu inside the draw requires a current object table before restore.
    const bool edge=g_occlSwapEpoch!=UiSurfaceEpoch() || g_occlSwapLoad!=g_mkLoadEvents;
    if((edge && !BuildLiveSet()) || !ChSlot(g_occlSwapOwner)) {
        ++g_occlRestoreRefused;g_occlWhy="restore refused: LocalPlayer identity changed";
        Log("occlusion/pereye: %s",g_occlWhy);return;
    }
    auto* lp=(uint8_t*)g_occlSwapOwner.value.obj;
    void** field=(void**)(lp+kLocalPlayerViewStateOff);
    if(RangeReadable(field,sizeof(void*)) && *field==g_occlRight) {
        *field=g_occlLeft;++g_occlRestores;
    } else {
        ++g_occlRestoreRefused;g_occlWhy="restore refused: engine changed ViewState";
        Log("occlusion/pereye: %s; field left untouched",g_occlWhy);
    }
}

static int OcclusionModeGet() { return g_occlMode.load(); }
static void OcclusionModeSet(int mode, const char* who)
{
    OcclusionApply(mode, who);
    ConfigWriteKey("Stereo", "Occlusion", OcclModeName(g_occlMode.load()), who);
}

static bool OcclusionCommand(const char* args)
{
    int m = 0;
    if (OcclParseMode(args, &m)) {
        OcclusionApply(m, "the seam");
        ConfigWriteKey("Stereo", "Occlusion", OcclModeName(g_occlMode.load()), "the seam");
        return true;
    }
    uint32_t sw = 0;
    const bool read = g_occlSwitchOk > 0 && SafeRead32(kIgnoreAllOcclusionQueries, &sw);
    Log("occlusion: mode %s | per-eye swaps %ld left %p right %p refused %ld | engine switch %s | "
        "usage: occlusion native|pereye|off", OcclModeName(g_occlMode.load()), g_occlSwaps, g_occlLeft, g_occlRight,
        g_occlRefused, read ? (sw ? "1 (queries off)" : "0") : "unread");
    return true;
}
