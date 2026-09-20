// game/dishonored/cam_modifiers.cpp - VR-165: WHICH camera modifier is still
// swinging? Read-only. Included by src/mod/dishonoredvr.cpp after
// ue3/reflect.cpp, whose name-keyed resolver this uses.
//
// THE MEASUREMENT THIS EXISTS TO SETTLE. A playtester came off a chain and the
// camera went on swinging as if still on it, for about 23 seconds. Measured
// from their log: the engine's player state machine was clean
// (Climb -> Jump -> Falling -> Walk), OUR positional request was flat in every
// window (0-1 reversals, under 5 uu of range), and the GAME's own camera
// oscillated at ~8 Hz with up to 413 uu of range - five times the amplitude
// and twelve times the reversal rate of ordinary walking. So the swing is a
// game-side camera modifier that is not releasing, and no clamp on our own
// writer can touch it.
//
// What is NOT known is WHICH modifier. DishonoredCamera.ini carries Lean,
// Dodge, Shake, Recoil, HitReact, PhysicalReact, BumpSmoother and the
// DisableArmFollow pair, and the master state is clean, so nothing can be keyed
// off that. Guessing one to clamp is how two levers went wrong on 2026-09-20.
// This names the owner instead.
//
// HOW IT READS. `Camera.ModifierList` is a UE3 `array<CameraModifier>`, and
// each `CameraModifier` carries Alpha, TargetAlpha, Priority and bDisabled.
// Every offset comes from the name-keyed resolver (RflOffsetOf), never from a
// hardcoded number: a name outlives a game rebuild and this project has paid
// for copied offsets before. A property that cannot be resolved is reported as
// unresolved on the line rather than read as a zero.
//
// It writes nothing. It retains no pointer across frames.

namespace {

// Ships ON: the tester plays in a headset and cannot reach a prompt, so a probe
// that has to be asked for is one that never runs. [Diagnostics] CamModProbe.
bool     g_cmOn        = true;
double   g_cmNextMs    = 0.0;
double   g_cmLastMoveMs= 0.0;   // when the camera last moved a lot (the swing)
float    g_cmLastZ     = 0.0f;
bool     g_cmHaveZ     = false;
int      g_cmSwingRuns = 0;     // consecutive fast reversals seen
float    g_cmPrevDelta = 0.0f;
int      g_cmReversals = 0;
double   g_cmWindowMs  = 0.0;
float    g_cmZMin=0.0f, g_cmZMax=0.0f;

const int kCmPeriodMs = 1000;   // one table per second while it has something to say

// Offsets, resolved once by NAME. 0 = unresolved, and the line says so.
uint32_t g_cmOffList=0, g_cmOffAlpha=0, g_cmOffTarget=0, g_cmOffPrio=0;
uint32_t g_cmOffDisabled=0, g_cmMaskDisabled=0;
bool     g_cmResolved=false;

void CmResolve()
{
    if (g_cmResolved) return;
    g_cmResolved = true;
    g_cmOffList   = RflOffsetOf("Camera", "ModifierList");
    g_cmOffAlpha  = RflOffsetOf("CameraModifier", "Alpha");
    g_cmOffTarget = RflOffsetOf("CameraModifier", "TargetAlpha");
    g_cmOffPrio   = RflOffsetOf("CameraModifier", "Priority");
    FindBoolProp("CameraModifier", "bDisabled", &g_cmOffDisabled, &g_cmMaskDisabled);
    Log("cammod: resolved by name - Camera.ModifierList +0x%x | CameraModifier Alpha +0x%x "
        "TargetAlpha +0x%x Priority +0x%x bDisabled +0x%x/0x%x. A 0 here is UNRESOLVED, "
        "not an offset of zero, and every row below will say so.",
        g_cmOffList, g_cmOffAlpha, g_cmOffTarget, g_cmOffPrio, g_cmOffDisabled, g_cmMaskDisabled);
}

// The camera's Z, for correlating a modifier's weight against the actual
// swing. This reads the SAME name-resolved CameraCache POV location that
// cinematic_trace prints as `cachePos`, so the number in this table and the
// number the oscillation was measured from are the same quantity. No new
// offset is introduced here; if the trace has not resolved its layout yet,
// this reports no Z rather than reading a guessed address.
bool CmCameraZ(uint8_t* cam, float* z)
{
    if (!cam || !g_ctLayout || !g_ctCache) return false;
    float loc[3] = {};
    if (!CtRead(cam, g_ctCache + g_ctPov + g_ctLoc, loc, sizeof(loc))) return false;
    *z = loc[2];
    return true;
}

} // namespace

// Called from the script lane next to the other per-tick readers.
static void CamModTick()
{
    if (!g_cmOn) return;
    uint8_t* cam = g_camObj;
    if (!cam || !IsLiveObject(cam)) return;
    CmResolve();
    if (!g_cmOffList) return;      // nothing to walk; CmResolve already said so

    const double now = MaimNowMs();

    // Track the swing itself, so the table can be printed WITH the thing it is
    // meant to explain rather than next to it. A reversal only counts when the
    // step is big enough to be motion rather than noise.
    float z = 0.0f;
    if (CmCameraZ(cam, &z)) {
        if (g_cmHaveZ) {
            const float d = z - g_cmLastZ;
            if (g_cmPrevDelta * d < 0.0f && fabsf(g_cmPrevDelta) > 1.0f) ++g_cmReversals;
            if (fabsf(d) > 0.05f) g_cmPrevDelta = d;
            if (z < g_cmZMin) g_cmZMin = z;
            if (z > g_cmZMax) g_cmZMax = z;
        } else { g_cmZMin = g_cmZMax = z; g_cmWindowMs = now; }
        g_cmLastZ = z; g_cmHaveZ = true;
    }

    if (now < g_cmNextMs) return;
    g_cmNextMs = now + kCmPeriodMs;

    const double win = now - g_cmWindowMs;
    const float  amp = g_cmZMax - g_cmZMin;
    const int    rev = g_cmReversals;
    // Reset the window for the next second.
    g_cmReversals = 0; g_cmWindowMs = now; g_cmZMin = g_cmZMax = g_cmLastZ;

    // The measured chain swing was ~8 reversals/s with tens of uu of range;
    // ordinary walking was a fraction of that. Print the table while it is
    // swinging, and once more when it stops, so the release is visible too.
    const bool swinging = (rev >= 4 && amp > 15.0f);
    if (swinging) { g_cmLastMoveMs = now; ++g_cmSwingRuns; }
    const bool justStopped = !swinging && g_cmSwingRuns > 0 && (now - g_cmLastMoveMs) < 3000.0;
    if (!swinging && !justStopped) { if (!swinging) g_cmSwingRuns = 0; return; }
    if (!swinging) g_cmSwingRuns = 0;

    uint8_t* data = NULL; int32_t num = 0;
    if (!RflArrayAt(cam, g_cmOffList, &data, &num) || !data || num <= 0 || num > 64) {
        Log("cammod: %s - ModifierList unreadable or empty (num=%d) on camera %p; "
            "the swing cannot be attributed to a modifier from here",
            swinging ? "SWINGING" : "settled", (int)num, (void*)cam);
        return;
    }

    Log("cammod: ---- %s: %d reversals, %.1f uu of camera Z in %.0f ms, %d modifier(s) ----",
        swinging ? "SWINGING" : "SETTLED (the swing just stopped)", rev, amp, win, (int)num);
    for (int i = 0; i < num; ++i) {
        uint8_t* m = *(uint8_t**)(data + i * sizeof(void*));
        if (!m || ((uintptr_t)m & 3) || !RangeReadable(m, kClassOff + 4)) {
            Log("cammod:   [%d] unreadable entry %p", i, (void*)m);
            continue;
        }
        const char* cn = ObjClassName(m);
        char alpha[24] = "unresolved", targ[24] = "unresolved", prio[16] = "unresolved";
        char dis[16] = "unresolved";
        if (g_cmOffAlpha  && RangeReadable(m + g_cmOffAlpha, 4))
            _snprintf(alpha, sizeof(alpha), "%.3f", *(const float*)(m + g_cmOffAlpha));
        if (g_cmOffTarget && RangeReadable(m + g_cmOffTarget, 4))
            _snprintf(targ, sizeof(targ), "%.3f", *(const float*)(m + g_cmOffTarget));
        if (g_cmOffPrio   && RangeReadable(m + g_cmOffPrio, 1))
            _snprintf(prio, sizeof(prio), "%u", (unsigned)*(const uint8_t*)(m + g_cmOffPrio));
        if (g_cmOffDisabled && g_cmMaskDisabled && RangeReadable(m + g_cmOffDisabled, 4))
            _snprintf(dis, sizeof(dis), "%d",
                      (*(const uint32_t*)(m + g_cmOffDisabled) & g_cmMaskDisabled) ? 1 : 0);
        Log("cammod:   [%d] %-44s alpha=%s target=%s prio=%s disabled=%s",
            i, cn ? cn : "(no class name)", alpha, targ, prio, dis);
    }
    Log("cammod: ---- end. A modifier whose alpha stays UP on the SETTLED line is not the "
        "owner; the owner is one that is still up while SWINGING and falls when it settles. "
        "Every alpha reading 'unresolved' means read the resolver line above, not this table. ----");
}

static void CamModConfigure(const char* ini)
{
    g_cmOn = IniFloat(ini, "Diagnostics", "CamModProbe", 1) != 0.0f;
    Log("cammod: camera-modifier probe %s ([Diagnostics] CamModProbe) - read-only, prints only "
        "while the camera is actually swinging", g_cmOn ? "ON" : "off");
}

static bool CamModCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        g_cmOn = b;
        ConfigWriteKey("Diagnostics", "CamModProbe", b ? "1" : "0", "the seam");
        Log("cammod: %s (seam)", b ? "ON" : "off");
        return true;
    }
    Log("cammod: on|off (now %s). Read-only: it names which camera modifier is still "
        "weighted while the camera swings, and writes nothing.", g_cmOn ? "ON" : "off");
    return true;
}
