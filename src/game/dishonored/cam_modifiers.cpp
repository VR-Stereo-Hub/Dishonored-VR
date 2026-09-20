// game/dishonored/cam_modifiers.cpp - VR-165: WHICH camera modifier is still
// swinging? Read-only. Included by src/mod/dishonoredvr.cpp after
// ue3/reflect.cpp, whose name-keyed resolver this uses.
//
// THE MEASUREMENT THIS EXISTS TO SETTLE. A playtester came off a chain and the
// camera went on swinging as if still on it, for about 23 seconds. Measured
// from their log: the engine's player state machine was clean
// (Climb -> Jump -> Falling -> Walk), OUR positional request was flat in every
// window (0-1 reversals, under 5 uu of range), and the GAME's own camera
// oscillated at ~8 Hz with up to 413 uu of range - five times the amplitude of
// ordinary walking. So the swing is game-side and no clamp on our own writer
// can touch it.
//
// RESULT, run 2 (2026-09-20, the user reproduced it by ungrabbing the chain
// with X rather than jumping off): **THE MODIFIER STACK IS NOT THE OWNER.**
// The probe caught the swing (199.7 uu while SWINGING, 6.3 uu once SETTLED)
// and `Camera.ModifierList` held exactly ONE entry throughout:
//
//   [0] CameraModifier_CameraShake  alpha=0.000 target=0.000 prio=127 disabled=0
//
// Zero weight, identical swinging and settled. So the suspects from
// DishonoredCamera.ini - Lean, Dodge, Shake, BumpSmoother, the DisableArmFollow
// pair - are ELIMINATED: those sections configure classes that never enter the
// runtime modifier stack at all. The swing lives in the camera's own POV
// update, not the modifier chain. Do not come back to ModifierList for this.
//
// The probe is kept because that elimination is the result, and because it is
// what will show a modifier arriving if one ever does.
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
float    g_cmExtreme   = 0.0f;   // running extreme of the current leg
int      g_cmLeg       = 0;      // -1 falling, +1 rising, 0 unknown
int      g_cmReversals = 0;
double   g_cmWindowMs  = 0.0;
float    g_cmZMin=0.0f, g_cmZMax=0.0f;

const int   kCmPeriodMs = 1000;   // one table per second while it has something to say
// A direction change only counts once the camera has actually travelled this
// far back from the leg's extreme. Below this it is sampling noise, not motion.
const float kCmTurnUu   = 2.0f;

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
    // CORRECTED after run 2. This used to count a reversal per ProcessEvent
    // tick, and ProcessEvent runs far faster than any camera motion - so it
    // reported "249 reversals in 1010 ms", which is not a frequency of
    // anything and cannot be compared against the ~8 Hz the offline analysis
    // measured. A counter whose units are "per dispatch" is not evidence about
    // motion.
    //
    // Count DIRECTION CHANGES OF A REAL EXCURSION instead: a reversal is only
    // counted once the camera has travelled kCmTurnUu since the last one, so
    // the number is half-cycles of actual movement and dividing by the window
    // gives a rate in Hz that means something. Sampling rate no longer changes
    // the answer.
    float z = 0.0f;
    if (CmCameraZ(cam, &z)) {
        if (g_cmHaveZ) {
            const float d = z - g_cmLastZ;
            if (d > 0.0f) { if (z > g_cmExtreme) g_cmExtreme = z; }
            else if (d < 0.0f) { if (z < g_cmExtreme) g_cmExtreme = z; }
            // A turn: we have moved back from the running extreme by more than
            // the threshold, in the opposite sense to the leg we were on.
            const float back = z - g_cmExtreme;
            if (g_cmLeg >= 0 && back < -kCmTurnUu) { ++g_cmReversals; g_cmLeg = -1; g_cmExtreme = z; }
            else if (g_cmLeg <= 0 && back > kCmTurnUu) { ++g_cmReversals; g_cmLeg = 1; g_cmExtreme = z; }
            if (z < g_cmZMin) g_cmZMin = z;
            if (z > g_cmZMax) g_cmZMax = z;
        } else { g_cmZMin = g_cmZMax = g_cmExtreme = z; g_cmWindowMs = now; g_cmLeg = 0; }
        g_cmLastZ = z; g_cmHaveZ = true;
    }

    if (now < g_cmNextMs) return;
    g_cmNextMs = now + kCmPeriodMs;

    const double win = now - g_cmWindowMs;
    const float  amp = g_cmZMax - g_cmZMin;
    const int    rev = g_cmReversals;
    // Reset the window for the next second.
    g_cmReversals = 0; g_cmWindowMs = now; g_cmZMin = g_cmZMax = g_cmLastZ;

    // ~8 Hz with tens of uu was the measured chain swing; walking was far below
    // it. Print while swinging, and once more when it stops, so the release is
    // visible too - the release is the half that would identify an owner.
    const double hz = win > 0.0 ? (rev * 0.5) / (win / 1000.0) : 0.0;
    const bool swinging = (hz >= 2.0 && amp > 15.0f);
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

    Log("cammod: ---- %s: %.1f Hz (%d turns of >%.0f uu in %.0f ms), %.1f uu of camera Z, %d modifier(s) ----",
        swinging ? "SWINGING" : "SETTLED (the swing just stopped)",
        win > 0.0 ? (rev * 0.5) / (win / 1000.0) : 0.0, rev, kCmTurnUu, win, amp, (int)num);
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
